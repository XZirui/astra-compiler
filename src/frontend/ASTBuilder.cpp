#include "astra/frontend/ASTBuilder.h"

#include "astra/ast/Program.h"
#include "astra/basic/FloatParse.h"
#include "astra/parser/AstraParser.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/ErrorHandling.h>

#include <optional>

namespace astra::frontend {

/// Return the first non-null argument. The alternatives of a rule are
/// mutually exclusive, so exactly one is non-null and this picks the
/// alternative that was actually matched.
static antlr4::tree::ParseTree *available() { return nullptr; }

template <typename... Args>
static antlr4::tree::ParseTree *available(antlr4::tree::ParseTree *First,
                                          Args... Rest) {
  if (First) {
    return First;
  }
  return available(Rest...);
}

/// Return the `Token` of the first non-null `TerminalNode` in `Nodes`.
/// The alternatives are mutually exclusive, so exactly one is non-null.
/// This picks the operator token out of an alternative list.
template <typename... Args>
static antlr4::Token *getToken(antlr4::tree::TerminalNode *First,
                               Args... Rest) {
  return static_cast<antlr4::tree::TerminalNode *>(available(First, Rest...))
      ->getSymbol();
}

/// Map a visibility modifier token to the corresponding `ast::Visibility`.
static ast::Visibility
getVisibility(AstraParser::VisibilityModifierContext *Mod) {
  switch (
      getToken(Mod->PUBLIC(), Mod->PRIVATE(), Mod->PROTECTED())->getType()) {
  case AstraParser::PUBLIC:
    return ast::Visibility::Public;
  case AstraParser::PRIVATE:
    return ast::Visibility::Private;
  case AstraParser::PROTECTED:
    return ast::Visibility::Protected;
  default:
    // The token types above are the only visibility modifiers the grammar
    // allows.
    llvm_unreachable("Unknown visibility modifier.");
  }
}

/// Map a binary operator token to the corresponding `ast::Op`.
static ast::Op getBinaryOp(size_t TokenType) {
  switch (TokenType) {
  case AstraParser::ADD:
    return ast::Op::Add;
  case AstraParser::SUB:
    return ast::Op::Sub;
  case AstraParser::MULT:
    return ast::Op::Mult;
  case AstraParser::DIV:
    return ast::Op::Div;
  case AstraParser::MOD:
    return ast::Op::Mod;
  case AstraParser::EQ:
    return ast::Op::Eq;
  case AstraParser::NEQ:
    return ast::Op::Neq;
  case AstraParser::LT:
    return ast::Op::Lt;
  case AstraParser::GT:
    return ast::Op::Gt;
  case AstraParser::LE:
    return ast::Op::Le;
  case AstraParser::GE:
    return ast::Op::Ge;
  case AstraParser::LSHIFT:
    return ast::Op::LShift;
  case AstraParser::BIT_AND:
    return ast::Op::BitAnd;
  case AstraParser::BIT_OR:
    return ast::Op::BitOr;
  case AstraParser::BIT_XOR:
    return ast::Op::BitXor;
  default:
    // The token types above are the only binary operators the grammar allows.
    llvm_unreachable("Unknown binary operator.");
  }
}

/// Append `CodePoint` (BMP) to `Out` as UTF-8.
static void appendUtf8(llvm::SmallVectorImpl<char> &Out, uint32_t CodePoint) {
  if (CodePoint < 0x80) {
    Out.push_back(static_cast<char>(CodePoint));
  } else if (CodePoint < 0x800) {
    Out.push_back(static_cast<char>(0xC0 | (CodePoint >> 6)));
    Out.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
  } else {
    Out.push_back(static_cast<char>(0xE0 | (CodePoint >> 12)));
    Out.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
    Out.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
  }
}

/// Decode the escape sequence starting at `Body[I]` (which must be a
/// backslash) and advance `I` past it. Returns the decoded code point, or
/// `std::nullopt` (with a diagnostic on `Range`) for an invalid escape.
static std::optional<uint32_t> decodeEscape(llvm::StringRef Body, size_t &I,
                                            basic::DiagnosticsEngine &Diags,
                                            llvm::SMRange Range) {
  assert(Body[I] == '\\');
  auto Esc = Body[I + 1];
  switch (Esc) {
  case 't':
    I += 2;
    return 0x09;
  case 'b':
    I += 2;
    return 0x08;
  case 'n':
    I += 2;
    return 0x0A;
  case 'r':
    I += 2;
    return 0x0D;
  case '\'':
    I += 2;
    return 0x27;
  case '"':
    I += 2;
    return 0x22;
  case '\\':
    I += 2;
    return 0x5C;
  case '$':
    I += 2;
    return 0x24;
  case 'u': {
    auto Hex = Body.substr(I + 2, 4);
    unsigned CodePoint;
    if (Hex.size() != 4 || Hex.getAsInteger(16, CodePoint)) {
      Diags.report(Range, llvm::SourceMgr::DK_Error,
                   "invalid \\u escape sequence");
      return std::nullopt;
    }
    I += 6;
    return CodePoint;
  }
  default:
    Diags.report(Range, llvm::SourceMgr::DK_Error, "invalid escape sequence");
    return std::nullopt;
  }
}

/// Decode `Body` (string literal content without quotes) into `Out` as UTF-8
/// bytes. Returns false and reports a diagnostic on the first invalid escape.
static bool decodeStringBody(llvm::StringRef Body,
                             llvm::SmallVectorImpl<char> &Out,
                             basic::DiagnosticsEngine &Diags,
                             llvm::SMRange Range) {
  for (size_t I = 0; I < Body.size();) {
    if (Body[I] == '\\') {
      auto CodePoint = decodeEscape(Body, I, Diags, Range);
      if (!CodePoint) {
        return false;
      }
      appendUtf8(Out, *CodePoint);
    } else {
      Out.push_back(Body[I]);
      ++I;
    }
  }
  return true;
}

/// Decode the single-character content `Body` (char literal without quotes)
/// into `CodePoint`. Returns false and reports a diagnostic on invalid input.
static bool decodeCharValue(llvm::StringRef Body, uint32_t &CodePoint,
                            basic::DiagnosticsEngine &Diags,
                            llvm::SMRange Range) {
  if (Body.front() == '\\') {
    size_t I = 0;
    auto Decoded = decodeEscape(Body, I, Diags, Range);
    if (!Decoded) {
      return false;
    }
    if (I != Body.size()) {
      // The lexer only accepts one escape, so this is unreachable for
      // lexed tokens; guard anyway.
      Diags.report(Range, llvm::SourceMgr::DK_Error,
                   "invalid character literal");
      return false;
    }
    CodePoint = *Decoded;
    return true;
  }

  // A raw byte, or a single UTF-8 sequence.
  unsigned Lead = static_cast<unsigned char>(Body[0]);
  if (Body.size() == 1) {
    CodePoint = Lead;
    return true;
  }
  size_t Length;
  uint32_t Value;
  if ((Lead & 0xE0) == 0xC0) {
    Length = 2;
    Value = Lead & 0x1F;
  } else if ((Lead & 0xF0) == 0xE0) {
    Length = 3;
    Value = Lead & 0x0F;
  } else if ((Lead & 0xF8) == 0xF0) {
    Length = 4;
    Value = Lead & 0x07;
  } else {
    Diags.report(Range, llvm::SourceMgr::DK_Error, "invalid character literal");
    return false;
  }
  if (Body.size() != Length) {
    Diags.report(Range, llvm::SourceMgr::DK_Error, "invalid character literal");
    return false;
  }
  for (size_t J = 1; J < Length; ++J) {
    unsigned char Byte = static_cast<unsigned char>(Body[J]);
    if ((Byte & 0xC0) != 0x80) {
      Diags.report(Range, llvm::SourceMgr::DK_Error,
                   "invalid character literal");
      return false;
    }
    Value = (Value << 6) | (Byte & 0x3F);
  }
  CodePoint = Value;
  return true;
}

/// Map a prefix unary operator token to the corresponding `ast::Op`.
static ast::Op getPrefixUnaryOp(size_t TokenType) {
  switch (TokenType) {
  case AstraParser::ADD:
    return ast::Op::Add;
  case AstraParser::SUB:
    return ast::Op::Sub;
  case AstraParser::NOT:
    return ast::Op::Not;
  case AstraParser::BIT_NOT:
    return ast::Op::BitNot;
  default:
    // The token types above are the only prefix operators the grammar allows.
    llvm_unreachable("Unknown prefix unary operator.");
  }
}

ast::Declaration *ASTBuilder::getDecl(antlr4::tree::ParseTree *Tree) {
  return std::any_cast<ast::Declaration *>(visit(Tree));
}

ast::Type *ASTBuilder::getType(antlr4::tree::ParseTree *Tree) {
  return std::any_cast<ast::Type *>(visit(Tree));
}

ast::Statement *ASTBuilder::getStmt(antlr4::tree::ParseTree *Tree) {
  return std::any_cast<ast::Statement *>(visit(Tree));
}

ast::Block *ASTBuilder::getBlock(antlr4::tree::ParseTree *Tree) {
  return std::any_cast<ast::Block *>(visit(Tree));
}

ast::Expr *ASTBuilder::getExpr(antlr4::tree::ParseTree *Tree) {
  return std::any_cast<ast::Expr *>(visit(Tree));
}

std::any ASTBuilder::visitFile(AstraParser::FileContext *Ctx) {
  const auto &TopLevelObjects = Ctx->topLevelObject();
  auto *Result = Arena.allocate<ast::Program>();
  Result->Range = getRange(Ctx);

  for (auto *Obj : TopLevelObjects) {
    Result->Objects.push_back(std::any_cast<ast::TopLevelObject *>(visit(Obj)));
  }
  return Result;
}

std::any
ASTBuilder::visitTopLevelObject(AstraParser::TopLevelObjectContext *Ctx) {
  // TODO other kinds
  auto *Result = Arena.allocate<ast::TopLevelObject>();
  Result->Decl = getDecl(Ctx->declaration());
  Result->Range = getRange(Ctx);

  return Result;
}

std::any ASTBuilder::visitDeclaration(AstraParser::DeclarationContext *Ctx) {
  auto *Decl = getDecl(
      available(Ctx->functionDecl(), Ctx->variableDecl(), Ctx->classDecl()));
  if (auto *Mod = Ctx->visibilityModifier()) {
    Decl->Vis = getVisibility(Mod);
  }
  // No modifier keeps the default `Public`.
  return static_cast<ast::Declaration *>(Decl);
}

std::any ASTBuilder::visitFunctionDecl(AstraParser::FunctionDeclContext *Ctx) {
  auto *Result = Arena.allocate<ast::FunctionDecl>();
  Result->Range = getRange(Ctx);

  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  Result->Body = std::any_cast<ast::Block *>(visit(Ctx->block()));
  Result->ReturnType = getType(Ctx->type());

  const auto &ParamList = Ctx->parameter();
  for (auto *Param : ParamList) {
    Result->Parameters.push_back(std::any_cast<ast::Parameter *>(visit(Param)));
  }

  return static_cast<ast::Declaration *>(Result);
}

std::any ASTBuilder::visitClassDecl(AstraParser::ClassDeclContext *Ctx) {
  auto *Result = Arena.allocate<ast::ClassDecl>();
  Result->Range = getRange(Ctx);

  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  if (auto *TypeParams = Ctx->typeParameters()) {
    for (auto *Param : TypeParams->typeParameter()) {
      auto *TypeParam = Arena.allocate<ast::TypeParam>();
      TypeParam->Range = getRange(Param);
      TypeParam->Name = getIdentifier(Param->IDENTIFIER());
      if (auto *DefaultType = Param->type()) {
        TypeParam->DefaultType = getType(DefaultType);
      }
      Result->TypeParams.push_back(TypeParam);
    }
  }
  if (auto *Body = Ctx->classBody()) {
    for (auto *Member : Body->declaration()) {
      Result->Members.push_back(getDecl(Member));
    }
  }

  // std::any requires the exact type: ClassDecl* would not match
  // any_cast<Declaration*>.
  return static_cast<ast::Declaration *>(Result);
}

std::any ASTBuilder::visitParameter(AstraParser::ParameterContext *Ctx) {
  auto *Result = Arena.allocate<ast::Parameter>();
  Result->Range = getRange(Ctx);

  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  Result->ParamType = getType(Ctx->type());
  if (Ctx->expression()) {
    Result->DefaultValue = getExpr(Ctx->expression());
  }

  return Result;
}

std::any ASTBuilder::visitVariableDecl(AstraParser::VariableDeclContext *Ctx) {
  auto *Result = Arena.allocate<ast::VarDecl>();
  Result->Range = getRange(Ctx);

  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  if (Ctx->type()) {
    Result->VarType = getType(Ctx->type());
  }

  if (Ctx->expression()) {
    Result->Value = getExpr(Ctx->expression());
  }

  Result->IsMutable = (Ctx->VAR() != nullptr);
  return static_cast<ast::Declaration *>(Result);
}

std::any ASTBuilder::visitType(AstraParser::TypeContext *Ctx) {
  if (Ctx->functionType()) {
    return visit(Ctx->functionType());
  }

  auto *Result =
      getType(available(Ctx->parenType(), Ctx->typeRef(), Ctx->builtinType()));

  // Wrap the type in one `ArrayType` per bracket pair. The rightmost pair
  // ends up as the outermost `ArrayType`.
  if (const auto &Expressions = Ctx->expression(); !Expressions.empty()) {
    auto N = Expressions.size();
    for (size_t I = 0; I < N; ++I) {
      auto *ArrayTy = Arena.allocate<ast::ArrayType>();

      // The range covers the type plus the enclosing bracket pair.
      ArrayTy->Range = getRange(Ctx->getStart(), Ctx->RBRACKET(I)->getSymbol());
      ArrayTy->ElementType = Result;
      ArrayTy->Size = getExpr(Expressions[I]);
      Result = ArrayTy;
    }
  }
  return Result;
}

std::any ASTBuilder::visitParenType(AstraParser::ParenTypeContext *Ctx) {
  // Parens are transparent. They introduce no node, just like `visitParenExpr`.
  return visit(Ctx->type());
}

std::any ASTBuilder::visitTypeRef(AstraParser::TypeRefContext *Ctx) {
  auto *Result = Arena.allocate<ast::TypeRef>();
  Result->Range = getRange(Ctx);
  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  if (auto *TypeArgs = Ctx->typeArguments()) {
    // An empty list (`<>`) is valid: default type parameters fill it in.
    Result->ExplicitTypeArgs = true;
    if (auto *ArgList = TypeArgs->typeArgument()) {
      for (auto *Arg : ArgList->type()) {
        Result->TypeArgs.push_back(getType(Arg));
      }
    }
  }
  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitFunctionType(AstraParser::FunctionTypeContext *Ctx) {
  auto *Result = Arena.allocate<ast::FunctionType>();
  Result->Range = getRange(Ctx);
  Result->ReturnType = getType(Ctx->type());
  if (auto *ParamList = Ctx->paramTypeList()) {
    for (auto *Param : ParamList->type()) {
      Result->Parameters.push_back(getType(Param));
    }
  }

  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitBuiltinType(AstraParser::BuiltinTypeContext *Ctx) {
  auto *Result = Arena.allocate<ast::BuiltinType>();
  Result->Range = getRange(Ctx);

  auto BuiltinTy =
      getToken(Ctx->VOID(), Ctx->BOOL(), Ctx->INT(), Ctx->LONG(), Ctx->FLOAT(),
               Ctx->DOUBLE(), Ctx->CHAR(), Ctx->STRING())
          ->getType();
  switch (BuiltinTy) {
  case AstraParser::VOID:
    Result->Value = ast::BuiltinType::Void;
    break;
  case AstraParser::BOOL:
    Result->Value = ast::BuiltinType::Bool;
    break;
  case AstraParser::INT:
    Result->Value = ast::BuiltinType::Int;
    break;
  case AstraParser::LONG:
    Result->Value = ast::BuiltinType::Long;
    break;
  case AstraParser::FLOAT:
    Result->Value = ast::BuiltinType::Float;
    break;
  case AstraParser::DOUBLE:
    Result->Value = ast::BuiltinType::Double;
    break;
  case AstraParser::CHAR:
    Result->Value = ast::BuiltinType::Char;
    break;
  case AstraParser::STRING:
    Result->Value = ast::BuiltinType::String;
    break;
  default:
    // The token types above are the only builtin types the grammar allows.
    llvm_unreachable("Unknown builtin type.");
  }
  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitBlock(AstraParser::BlockContext *Ctx) {
  auto *Result = Arena.allocate<ast::Block>();
  Result->Range = getRange(Ctx);

  const auto &Statements = Ctx->statement();
  for (auto *Statement : Statements) {
    Result->Statements.push_back(getStmt(Statement));
  }
  return Result;
}

std::any ASTBuilder::visitStatement(AstraParser::StatementContext *Ctx) {
  return visit(available(Ctx->declStatement(), Ctx->assignment(),
                         Ctx->controlStatement(), Ctx->exprStmt()));
}

std::any
ASTBuilder::visitDeclStatement(AstraParser::DeclStatementContext *Ctx) {
  auto *Result = Arena.allocate<ast::DeclStatement>();
  Result->Range = getRange(Ctx);
  Result->Decl = getDecl(Ctx->declaration());
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitAssignment(AstraParser::AssignmentContext *Ctx) {
  auto *Result = Arena.allocate<ast::AssignmentStmt>();
  Result->Range = getRange(Ctx);
  Result->LHS = getExpr(Ctx->postfixUnaryExpr());
  Result->RHS = getExpr(Ctx->expression());

  auto *OpTree = Ctx->assignmentOperator();
  // Compound assignments such as `+=` fold into the plain operator value.
  // The distinction is lost, so codegen cannot rely on it.
  auto Op = getToken(OpTree->ASSIGNMENT(), OpTree->ADD_ASSIGNMENT(),
                     OpTree->SUB_ASSIGNMENT(), OpTree->MULT_ASSIGNMENT(),
                     OpTree->DIV_ASSIGNMENT(), OpTree->MOD_ASSIGNMENT(),
                     OpTree->BIT_AND_ASSIGNMENT(), OpTree->BIT_OR_ASSIGNMENT(),
                     OpTree->BIT_XOR_ASSIGNMENT(), OpTree->LSHIFT_ASSIGNMENT(),
                     OpTree->RSHIFT_ASSIGNMENT())
                ->getType();
  switch (Op) {
  case AstraParser::ASSIGNMENT:
    Result->Operator = ast::Op::Assignment;
    break;
  case AstraParser::ADD_ASSIGNMENT:
    Result->Operator = ast::Op::Add;
    break;
  case AstraParser::SUB_ASSIGNMENT:
    Result->Operator = ast::Op::Sub;
    break;
  case AstraParser::MULT_ASSIGNMENT:
    Result->Operator = ast::Op::Mult;
    break;
  case AstraParser::DIV_ASSIGNMENT:
    Result->Operator = ast::Op::Div;
    break;
  case AstraParser::MOD_ASSIGNMENT:
    Result->Operator = ast::Op::Mod;
    break;
  case AstraParser::BIT_AND_ASSIGNMENT:
    Result->Operator = ast::Op::BitAnd;
    break;
  case AstraParser::BIT_OR_ASSIGNMENT:
    Result->Operator = ast::Op::BitOr;
    break;
  case AstraParser::BIT_XOR_ASSIGNMENT:
    Result->Operator = ast::Op::BitXor;
    break;
  case AstraParser::LSHIFT_ASSIGNMENT:
    Result->Operator = ast::Op::LShift;
    break;
  case AstraParser::RSHIFT_ASSIGNMENT:
    Result->Operator = ast::Op::RShift;
    break;
  default:
    // The token types above are the only assignment operators the grammar
    // allows.
    llvm_unreachable("Unknown assignment operator.");
  }

  return static_cast<ast::Statement *>(Result);
}

std::any
ASTBuilder::visitControlStatement(AstraParser::ControlStatementContext *Ctx) {
  return visit(available(Ctx->forStmt(), Ctx->forEachStmt(), Ctx->whileStmt(),
                         Ctx->doWhileStmt(), Ctx->ifStmt(),
                         Ctx->tryStatement()));
}

std::any ASTBuilder::visitForStmt(AstraParser::ForStmtContext *Ctx) {
  auto *Result = Arena.allocate<ast::ForStmt>();
  Result->Range = getRange(Ctx);

  if (auto *InitDecls = Ctx->variableDecls()) {
    for (auto *Stmt : InitDecls->variableDecl()) {
      Result->InitStmts.push_back(getDecl(Stmt));
    }
  }
  if (Ctx->expression()) {
    Result->Condition = getExpr(Ctx->expression());
  }
  if (Ctx->forUpdate()) {
    Result->Update = getStmt(Ctx->forUpdate());
  }
  Result->Body = getBlock(Ctx->block());

  // std::any requires the exact type: ForStmt* would not match
  // any_cast<Statement*>.
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitForUpdate(AstraParser::ForUpdateContext *Ctx) {
  // The update is either an assignment or a plain expression.
  // The latter is wrapped in an `ExprStmt`.
  if (Ctx->assignment()) {
    return getStmt(Ctx->assignment());
  }
  auto *Result = Arena.allocate<ast::ExprStmt>();
  Result->Range = getRange(Ctx);
  Result->Expression = getExpr(Ctx->expression());
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitForEachStmt(AstraParser::ForEachStmtContext *Ctx) {
  auto *Result = Arena.allocate<ast::ForEachStmt>();
  Result->Range = getRange(Ctx);

  Result->VarName = getText(Ctx->IDENTIFIER());
  Result->Scope = getExpr(Ctx->expression());
  Result->Body = getBlock(Ctx->block());

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitWhileStmt(AstraParser::WhileStmtContext *Ctx) {
  auto *Result = Arena.allocate<ast::WhileStmt>();
  Result->Range = getRange(Ctx);

  Result->Condition = getExpr(Ctx->expression());
  Result->Body = getBlock(Ctx->block());

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitDoWhileStmt(AstraParser::DoWhileStmtContext *Ctx) {
  auto *Result = Arena.allocate<ast::DoWhileStmt>();
  Result->Range = getRange(Ctx);

  Result->Body = getBlock(Ctx->block());
  Result->Condition = getExpr(Ctx->expression());

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitIfStmt(AstraParser::IfStmtContext *Ctx) {
  auto *Result = Arena.allocate<ast::IfStmt>();
  Result->Range = getRange(Ctx);

  Result->Condition = getExpr(Ctx->expression());
  Result->Then = getBlock(Ctx->block(0));
  if (Ctx->ELSE()) {
    // An `else if` chain nests `IfStmt`s, while a plain `else` stores the
    // trailing block directly in `Else`.
    if (auto *ElseIf = Ctx->ifStmt()) {
      Result->Else = getStmt(ElseIf);
    } else {
      Result->Else = getBlock(Ctx->block(1));
    }
  }

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitTryStatement(AstraParser::TryStatementContext *Ctx) {
  auto *Result = Arena.allocate<ast::TryStmt>();
  Result->Range = getRange(Ctx);

  Result->Body = getBlock(Ctx->block(0));
  for (auto *Clause : Ctx->catchClause()) {
    Result->CatchClauses.push_back(
        std::any_cast<ast::CatchClause *>(visit(Clause)));
  }
  if (Ctx->FINALLY()) {
    Result->Finally = getBlock(Ctx->block().back());
  }
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitCatchClause(AstraParser::CatchClauseContext *Ctx) {
  auto *Result = Arena.allocate<ast::CatchClause>();
  Result->Range = getRange(Ctx);

  auto *Param = Arena.allocate<ast::Parameter>();
  Param->Range = getRange(Ctx);
  Param->Name = getIdentifier(Ctx->IDENTIFIER());
  Param->ParamType = getType(Ctx->type());
  // Catch parameters have no default value.
  Result->Param = Param;

  Result->Body = getBlock(Ctx->block());
  return Result;
}

std::any ASTBuilder::visitExprStmt(AstraParser::ExprStmtContext *Ctx) {
  auto *Result = Arena.allocate<ast::ExprStmt>();
  Result->Range = getRange(Ctx);
  Result->Expression = getExpr(Ctx->expression());
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitExpression(AstraParser::ExpressionContext *Ctx) {
  return visit(Ctx->disjunction());
}

std::any ASTBuilder::visitDisjunction(AstraParser::DisjunctionContext *Ctx) {
  return foldLeftAssoc(Ctx->conjunction(), Ctx->DISJ(),
                       [](auto *) { return ast::Op::Disj; });
}

std::any ASTBuilder::visitConjunction(AstraParser::ConjunctionContext *Ctx) {
  return foldLeftAssoc(Ctx->equality(), Ctx->CONJ(),
                       [](auto *) { return ast::Op::Conj; });
}

std::any ASTBuilder::visitEquality(AstraParser::EqualityContext *Ctx) {
  // The grammar allows at most one operator, so `foldLeftAssoc` degenerates
  // to a single `BinaryExpr` (or plain passthrough when there's no operator).
  return foldLeftAssoc(Ctx->comparison(), std::vector{Ctx->equalityOperator()},
                       [](AstraParser::EqualityOperatorContext *OpCtx) {
                         auto *Tok = getToken(OpCtx->EQ(), OpCtx->NEQ());
                         return getBinaryOp(Tok->getType());
                       });
}

std::any ASTBuilder::visitComparison(AstraParser::ComparisonContext *Ctx) {
  // `foo<Int>>(x)`: a second `>` blocks the argument list, so the
  // generic-call reading (preferred when `>` is directly followed by `(`)
  // cannot close the type argument list. Report the error and degrade to a
  // best-effort `CallExpr` so the rest of the program still parses.
  if (Ctx->typeArguments()) {
    Diags.report(getRange(Ctx->GT()->getSymbol(), Ctx->GT()->getSymbol()),
                 llvm::SourceMgr::DK_Error,
                 "expected '(' after type argument list");
    auto *Result = Arena.allocate<ast::CallExpr>();
    Result->Range = getRange(Ctx);
    Result->Callee = getExpr(Ctx->infixExpr(0));
    Result->ExplicitTypeArgs = true;
    if (auto *ArgList = Ctx->typeArguments()->typeArgument()) {
      for (auto *Arg : ArgList->type()) {
        Result->TypeArgs.push_back(getType(Arg));
      }
    }
    if (auto *Args = Ctx->valueArguments()) {
      for (auto *Arg : Args->valueArgument()) {
        Result->Arguments.push_back(getExpr(Arg->expression()));
      }
    }
    return static_cast<ast::Expr *>(Result);
  }

  const auto &Subs = Ctx->infixExpr();
  const auto &Ops = Ctx->comparisonOperator();
  auto GetOp = [](AstraParser::ComparisonOperatorContext *OpCtx) {
    auto *Tok = getToken(OpCtx->LT(), OpCtx->GT(), OpCtx->LE(), OpCtx->GE());
    return getBinaryOp(Tok->getType());
  };
  if (Ops.size() < 2) {
    // At most one operator: the chain degenerates to a `BinaryExpr`, or a
    // plain passthrough when there is no operator.
    return foldLeftAssoc(Subs, Ops, GetOp);
  }

  // Two or more operators form a comparison chain, e.g. `a < b <= c`. This
  // has the math semantics `a < b && b <= c` (each middle operand is
  // evaluated once). The expansion is left to semantic analysis. All
  // operators must point in the same direction.
  auto *Result = Arena.allocate<ast::ComparisonChainExpr>();
  Result->Range = getRange(Ctx);
  bool Mixed = false;
  std::optional<bool> Ascending;
  for (size_t I = 0; I < Subs.size(); ++I) {
    Result->Operands.push_back(getExpr(Subs[I]));
    if (I < Ops.size()) {
      auto Op = GetOp(Ops[I]);
      Result->Operators.push_back(Op);
      bool IsAscending = (Op == ast::Op::Lt || Op == ast::Op::Le);
      if (Ascending) {
        Mixed |= (*Ascending != IsAscending);
      } else {
        Ascending = IsAscending;
      }
    }
  }
  if (Mixed) {
    Diags.report(getRange(Ctx), llvm::SourceMgr::DK_Error,
                 "comparison chain with mixed operators");
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitInfixExpr(AstraParser::InfixExprContext *Ctx) {
  // `a in b is T` is left-folded: `(a in b) is T`. The `in`/`is` operators
  // can be mixed arbitrarily. Merge them by source position to keep their
  // order without RTTI-based children type tests.
  struct InfixOp {
    size_t Start; // token start offset in the source
    bool IsIn;
    size_t Index; // index within inOperator()/isOperator()
  };
  llvm::SmallVector<InfixOp> Ops;
  const auto &InOps = Ctx->inOperator();
  for (size_t I = 0; I < InOps.size(); ++I) {
    Ops.push_back({InOps[I]->getStart()->getStartIndex(), true, I});
  }
  const auto &IsOps = Ctx->isOperator();
  for (size_t J = 0; J < IsOps.size(); ++J) {
    Ops.push_back({IsOps[J]->getStart()->getStartIndex(), false, J});
  }
  llvm::sort(Ops, [](const InfixOp &A, const InfixOp &B) {
    return A.Start < B.Start;
  });

  const auto &ElvisExprs = Ctx->elvisExpr();
  auto *Result = getExpr(ElvisExprs.front());
  size_t InIdx = 0, IsIdx = 0;
  for (const auto &Op : Ops) {
    if (Op.IsIn) {
      auto *Binary = Arena.allocate<ast::BinaryExpr>();
      Binary->Range = getRange(ElvisExprs.front()->getStart(),
                               ElvisExprs[++InIdx]->getStop());
      Binary->Operator = ast::Op::In;
      Binary->LHS = Result;
      Binary->RHS = getExpr(ElvisExprs[InIdx]);
      Result = Binary;
    } else /*Is expr*/ {
      auto *Is = Arena.allocate<ast::IsExpr>();
      Is->Range =
          getRange(ElvisExprs.front()->getStart(), Ctx->type(IsIdx)->getStop());
      Is->Operand = Result;
      Is->CheckType = getType(Ctx->type(IsIdx));
      Result = Is;
      ++IsIdx;
    }
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitElvisExpr(AstraParser::ElvisExprContext *Ctx) {
  return foldLeftAssoc(Ctx->bitwiseOr(), Ctx->elvisOperator(),
                       [](auto *) { return ast::Op::Elvis; });
}

std::any ASTBuilder::visitBitwiseOr(AstraParser::BitwiseOrContext *Ctx) {
  return foldLeftAssoc(Ctx->bitwiseXor(), Ctx->BIT_OR(),
                       [](auto *) { return ast::Op::BitOr; });
}

std::any ASTBuilder::visitBitwiseXor(AstraParser::BitwiseXorContext *Ctx) {
  return foldLeftAssoc(Ctx->bitwiseAnd(), Ctx->BIT_XOR(),
                       [](auto *) { return ast::Op::BitXor; });
}

std::any ASTBuilder::visitBitwiseAnd(AstraParser::BitwiseAndContext *Ctx) {
  return foldLeftAssoc(Ctx->bitwiseShift(), Ctx->BIT_AND(),
                       [](auto *) { return ast::Op::BitAnd; });
}

std::any ASTBuilder::visitBitwiseShift(AstraParser::BitwiseShiftContext *Ctx) {
  return foldLeftAssoc(
      Ctx->addition(), Ctx->bitwiseShiftOperator(),
      [this](AstraParser::BitwiseShiftOperatorContext *OpCtx) {
        if (OpCtx->LSHIFT()) {
          return ast::Op::LShift;
        }
        // `>>` lexes as two `GT` tokens (there is no `RSHIFT` token). Only a
        // physically adjacent pair is a right shift. `a > > b`, with
        // whitespace or a comment between the two `>`, is reported as an error.
        auto *First = OpCtx->GT(0);
        auto *Second = OpCtx->GT(1);
        if (Second->getSymbol()->getStartIndex() !=
            First->getSymbol()->getStopIndex() + 1) {
          Diags.report(getRange(First->getSymbol(), Second->getSymbol()),
                       llvm::SourceMgr::DK_Error,
                       "right shift requires two adjacent '>' tokens");
        }
        return ast::Op::RShift;
      });
}

std::any ASTBuilder::visitAddition(AstraParser::AdditionContext *Ctx) {
  const auto &Subs = Ctx->multiplication();
  const auto &Ops = Ctx->additionOperator();
  return foldLeftAssoc(Subs, Ops,
                       [](AstraParser::AdditionOperatorContext *OpCtx) {
                         auto *Tok = getToken(OpCtx->ADD(), OpCtx->SUB());
                         return getBinaryOp(Tok->getType());
                       });
}

std::any
ASTBuilder::visitMultiplication(AstraParser::MultiplicationContext *Ctx) {
  const auto &Subs = Ctx->asExpr();
  const auto &Ops = Ctx->multiplicationOperator();
  return foldLeftAssoc(
      Subs, Ops, [](AstraParser::MultiplicationOperatorContext *OpCtx) {
        auto *Tok = getToken(OpCtx->MULT(), OpCtx->DIV(), OpCtx->MOD());
        return getBinaryOp(Tok->getType());
      });
}

std::any ASTBuilder::visitAsExpr(AstraParser::AsExprContext *Ctx) {
  auto *Operand = getExpr(Ctx->prefixUnaryExpr());
  if (!Ctx->asOperator()) {
    return static_cast<ast::Expr *>(Operand);
  }
  auto *Result = Arena.allocate<ast::AsExpr>();
  Result->Range = getRange(Ctx);
  Result->Operand = Operand;
  Result->NullSafe = Ctx->asOperator()->QUEST() != nullptr;
  Result->TargetType = getType(Ctx->type());
  return static_cast<ast::Expr *>(Result);
}

std::any
ASTBuilder::visitPrefixUnaryExpr(AstraParser::PrefixUnaryExprContext *Ctx) {
  auto *Result = getExpr(Ctx->postfixUnaryExpr());
  const auto &Prefixes = Ctx->unaryPrefix();
  // The rightmost prefix operator binds the closest, so fold from right to
  // left, e.g. `!-a` -> `UnaryExpr(Not, UnaryExpr(Sub, a))`.
  for (auto It = Prefixes.rbegin(); It != Prefixes.rend(); ++It) {
    auto *OpToken = getToken((*It)->prefixUnaryOperator()->ADD(),
                             (*It)->prefixUnaryOperator()->SUB(),
                             (*It)->prefixUnaryOperator()->NOT(),
                             (*It)->prefixUnaryOperator()->BIT_NOT());
    auto *Unary = Arena.allocate<ast::UnaryExpr>();
    Unary->Range = getRange(OpToken, Ctx->postfixUnaryExpr()->getStop());
    Unary->Operator = getPrefixUnaryOp(OpToken->getType());
    Unary->Operand = Result;
    Result = Unary;
  }
  return static_cast<ast::Expr *>(Result);
}

std::any
ASTBuilder::visitPostfixUnaryExpr(AstraParser::PostfixUnaryExprContext *Ctx) {
  auto *Result = getExpr(Ctx->primaryExpr());
  // Suffixes apply in source order, each wrapping the previous result.
  // For example, `f(x).y` becomes `MemberExpr(CallExpr(f, x), y)`.
  for (auto *Postfix : Ctx->unaryPostfix()) {
    if (auto *Call = Postfix->callSuffix()) {
      auto *CallExpr = Arena.allocate<ast::CallExpr>();
      CallExpr->Range = getRange(Ctx->getStart(), Postfix->getStop());
      CallExpr->Callee = Result;
      if (auto *TypeArgs = Call->typeArguments()) {
        // An empty list (`<>`) is valid: default type parameters fill it in.
        CallExpr->ExplicitTypeArgs = true;
        if (auto *ArgList = TypeArgs->typeArgument()) {
          for (auto *Arg : ArgList->type()) {
            CallExpr->TypeArgs.push_back(getType(Arg));
          }
        }
      }
      if (auto *Args = Call->valueArguments()) {
        for (auto *Arg : Args->valueArgument()) {
          // TODO named arguments and spread
          CallExpr->Arguments.push_back(getExpr(Arg->expression()));
        }
      }
      Result = CallExpr;
    } else if (auto *Index = Postfix->indexingSuffix()) {
      auto *IndexExpr = Arena.allocate<ast::IndexExpr>();
      IndexExpr->Range = getRange(Ctx->getStart(), Postfix->getStop());
      IndexExpr->Base = Result;
      IndexExpr->Index = getExpr(Index->expression());
      Result = IndexExpr;
    } else if (auto *Nav = Postfix->navigationSuffix()) {
      auto *Member = Arena.allocate<ast::MemberExpr>();
      Member->Range = getRange(Ctx->getStart(), Postfix->getStop());
      Member->Base = Result;
      Member->Member = getText(Nav->IDENTIFIER());
      Member->NullSafe = Nav->memberAccessOperator()->QUEST_DOT() != nullptr;
      Result = Member;
    }
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitPrimaryExpr(AstraParser::PrimaryExprContext *Ctx) {
  if (Ctx->IDENTIFIER()) {
    auto *Result = Arena.allocate<ast::VarExpr>();
    Result->Range = getRange(Ctx);
    Result->Name = getIdentifier(Ctx->IDENTIFIER());
    return static_cast<ast::Expr *>(Result);
  }
  return visit(available(Ctx->parenExpr(), Ctx->literalConstant(),
                         Ctx->collectionLiteral(), Ctx->thisLiteral(),
                         Ctx->ifExpression(), Ctx->jumpExpression()));
}

std::any ASTBuilder::visitParenExpr(AstraParser::ParenExprContext *Ctx) {
  // Parens are transparent: they introduce no AST node.
  return visit(Ctx->expression());
}

std::any
ASTBuilder::visitLiteralConstant(AstraParser::LiteralConstantContext *Ctx) {
  if (Ctx->BOOLEAN_LITERAL()) {
    auto *Result = Arena.allocate<ast::BoolLiteral>();
    Result->Range = getRange(Ctx);
    Result->Value = getText(Ctx->BOOLEAN_LITERAL()) == "true";
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->NULL_LITERAL()) {
    auto *Result = Arena.allocate<ast::NullLiteral>();
    Result->Range = getRange(Ctx);
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->INTEGER_LITERAL()) {
    auto *Result = Arena.allocate<ast::IntLiteral>();
    Result->Range = getRange(Ctx);
    auto Text = getText(Ctx->INTEGER_LITERAL());
    unsigned Radix = 10;
    if (Text.starts_with("0x") || Text.starts_with("0X")) {
      Radix = 16;
      Text = Text.drop_front(2);
    } else if (Text.starts_with("0b") || Text.starts_with("0B")) {
      Radix = 2;
      Text = Text.drop_front(2);
    } else if (Text.starts_with("0o") || Text.starts_with("0O")) {
      Radix = 8;
      Text = Text.drop_front(2);
    }
    llvm::APInt Raw;
    if (Text.getAsInteger(Radix, Raw)) {
      // Unreachable for tokens the lexer accepted; degrade instead of crash.
      Diags.report(Result->Range, llvm::SourceMgr::DK_Error,
                   "invalid integer literal");
      return static_cast<ast::Expr *>(Result);
    }
    // The literal must fit in a signed 64-bit `long`. `getAsInteger`
    // over-allocates the APInt width (roughly 4 bits per digit), so check the
    // value itself: anything with 64 or more active bits exceeds `LONG_MAX`.
    if (Raw.getActiveBits() > 63) {
      Diags.report(Result->Range, llvm::SourceMgr::DK_Error,
                   "integer literal is too large for `long`");
      return static_cast<ast::Expr *>(Result);
    }
    // The value fits in 63 bits, so converting to a full 64-bit APInt is
    // exact whether the parsed width was smaller (`zext`) or larger
    // (`trunc`; the high bits are zero). The signed `APSInt` interpretation
    // then never sign-extends or truncates the value.
    if (Raw.getBitWidth() > 64) {
      Raw = Raw.trunc(64);
    }
    Result->Value = llvm::APSInt(Raw.zext(64), /*IsUnsigned=*/false);
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->STRING_LITERAL()) {
    auto *Result = Arena.allocate<ast::StringLiteral>();
    Result->Range = getRange(Ctx);
    // The token is closed (unterminated strings never reach the builder),
    // so stripping the quotes is safe.
    auto Body = getText(Ctx->STRING_LITERAL()).drop_front().drop_back();
    llvm::SmallVector<char, 16> Decoded;
    if (decodeStringBody(Body, Decoded, Diags, Result->Range)) {
      Result->Value =
          Arena.allocateCopy(llvm::StringRef(Decoded.data(), Decoded.size()));
    } else {
      // Keep the raw text so the node stays usable; the diagnostic was
      // already reported.
      Result->Value = Arena.allocateCopy(Body);
    }
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->CHAR_LITERAL()) {
    auto *Result = Arena.allocate<ast::CharLiteral>();
    Result->Range = getRange(Ctx);
    auto Body = getText(Ctx->CHAR_LITERAL()).drop_front().drop_back();
    decodeCharValue(Body, Result->Value, Diags, Result->Range);
    return static_cast<ast::Expr *>(Result);
  }

  // FLOAT_LITERAL or DOUBLE_LITERAL.
  auto *Result = Arena.allocate<ast::FloatLiteral>();
  Result->Range = getRange(Ctx);
  auto Text = getText(Ctx->FLOAT_LITERAL() ? Ctx->FLOAT_LITERAL()
                                           : Ctx->DOUBLE_LITERAL());
  llvm::APFloat Value(Ctx->FLOAT_LITERAL() ? llvm::APFloat::IEEEsingle()
                                           : llvm::APFloat::IEEEdouble());
  if (Ctx->FLOAT_LITERAL()) {
    Text = Text.drop_back(); // strip the trailing 'f'/'F' suffix
  }
  auto Status = basic::convertFloatString(Value, Text,
                                          llvm::APFloat::rmNearestTiesToEven);
  if (Status == llvm::APFloat::opInvalidOp) {
    // Unreachable for tokens the lexer accepted; degrade instead of crash.
    Diags.report(Result->Range, llvm::SourceMgr::DK_Error,
                 "invalid floating-point literal");
    return static_cast<ast::Expr *>(Result);
  }
  if (Status & (llvm::APFloat::opOverflow | llvm::APFloat::opUnderflow)) {
    Diags.report(Result->Range, llvm::SourceMgr::DK_Error,
                 "floating-point literal is out of range");
  }
  Result->Value = std::move(Value);
  return static_cast<ast::Expr *>(Result);
}

std::any
ASTBuilder::visitCollectionLiteral(AstraParser::CollectionLiteralContext *Ctx) {
  auto *Result = Arena.allocate<ast::CollectionExpr>();
  Result->Range = getRange(Ctx);
  for (auto *Element : Ctx->expression()) {
    Result->Elements.push_back(getExpr(Element));
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitThisLiteral(AstraParser::ThisLiteralContext *Ctx) {
  auto *Result = Arena.allocate<ast::ThisExpr>();
  Result->Range = getRange(Ctx);
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitIfExpression(AstraParser::IfExpressionContext *Ctx) {
  auto *Result = Arena.allocate<ast::IfExpr>();
  Result->Range = getRange(Ctx);

  Result->Condition = getExpr(Ctx->expression(0));
  Result->Then = getExpr(Ctx->expression(1));
  if (Ctx->ELSE()) {
    Result->Else = getExpr(Ctx->expression(2));
  }
  return static_cast<ast::Expr *>(Result);
}

std::any
ASTBuilder::visitJumpExpression(AstraParser::JumpExpressionContext *Ctx) {
  if (Ctx->THROW()) {
    auto *Result = Arena.allocate<ast::ThrowExpr>();
    Result->Range = getRange(Ctx);
    Result->Content = getExpr(Ctx->expression());
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->RETURN()) {
    auto *Result = Arena.allocate<ast::ReturnExpr>();
    Result->Range = getRange(Ctx);
    if (Ctx->expression()) {
      Result->Value = getExpr(Ctx->expression());
    }
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->CONTINUE()) {
    auto *Result = Arena.allocate<ast::ContinueExpr>();
    Result->Range = getRange(Ctx);
    if (Ctx->label()) {
      Result->Pos = std::any_cast<ast::Label *>(visit(Ctx->label()));
    }
    return static_cast<ast::Expr *>(Result);
  }
  // The remaining alternative is BREAK.
  auto *Result = Arena.allocate<ast::BreakExpr>();
  Result->Range = getRange(Ctx);
  if (Ctx->label()) {
    Result->Pos = std::any_cast<ast::Label *>(visit(Ctx->label()));
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitLabel(AstraParser::LabelContext *Ctx) {
  auto *Result = Arena.allocate<ast::Label>();
  Result->Range = getRange(Ctx);
  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  return Result;
}

} // namespace astra::frontend
