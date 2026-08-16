#include "astra/frontend/ASTBuilder.h"

#include "astra/ast/Program.h"
#include "astra/basic/FloatParse.h"
#include "astra/parser/AstraParser.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/ErrorHandling.h>

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

/// Map a binary operator token to the corresponding `ast::Op`.
static ast::Op getBinaryOp(int TokenType) {
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
  case AstraParser::RSHIFT:
    return ast::Op::RShift;
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

/// Map a prefix unary operator token to the corresponding `ast::Op`.
static ast::Op getPrefixUnaryOp(int TokenType) {
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
  auto *Result = ASTContext.allocate<ast::Program>();
  Result->Range = getRange(Ctx);

  for (auto *Obj : TopLevelObjects) {
    Result->Objects.push_back(std::any_cast<ast::TopLevelObject *>(visit(Obj)));
  }
  return Result;
}

std::any
ASTBuilder::visitTopLevelObject(AstraParser::TopLevelObjectContext *Ctx) {
  // TODO other kinds
  auto *Result = ASTContext.allocate<ast::TopLevelObject>();
  Result->Decl = getDecl(Ctx->declaration());
  Result->Range = getRange(Ctx);

  return Result;
}

std::any ASTBuilder::visitDeclaration(AstraParser::DeclarationContext *Ctx) {
  return visit(available(Ctx->functionDecl(), Ctx->variableDecl()));
}

std::any ASTBuilder::visitFunctionDecl(AstraParser::FunctionDeclContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::FunctionDecl>();
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

std::any ASTBuilder::visitParameter(AstraParser::ParameterContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::Parameter>();
  Result->Range = getRange(Ctx);

  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  Result->Type = getType(Ctx->type());
  if (Ctx->expression()) {
    Result->DefaultValue = getExpr(Ctx->expression());
  }

  return Result;
}

std::any ASTBuilder::visitVariableDecl(AstraParser::VariableDeclContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::VarDecl>();
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
      auto *ArrayTy = ASTContext.allocate<ast::ArrayType>();

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
  auto *Result = ASTContext.allocate<ast::TypeRef>();
  Result->Range = getRange(Ctx);
  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitFunctionType(AstraParser::FunctionTypeContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::FunctionType>();
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
  auto *Result = ASTContext.allocate<ast::BuiltinType>();
  Result->Range = getRange(Ctx);

  auto BuiltinTy = getToken(Ctx->VOID(), Ctx->BOOL(), Ctx->INT(), Ctx->LONG(),
                            Ctx->FLOAT(), Ctx->DOUBLE())
                       ->getType();
  switch (BuiltinTy) {
  case AstraParser::VOID:
    Result->Type = ast::BuiltinType::Void;
    break;
  case AstraParser::BOOL:
    Result->Type = ast::BuiltinType::Bool;
    break;
  case AstraParser::INT:
    Result->Type = ast::BuiltinType::Int;
    break;
  case AstraParser::LONG:
    Result->Type = ast::BuiltinType::Long;
    break;
  case AstraParser::FLOAT:
    Result->Type = ast::BuiltinType::Float;
    break;
  case AstraParser::DOUBLE:
    Result->Type = ast::BuiltinType::Double;
    break;
  default:
    // The token types above are the only builtin types the grammar allows.
    llvm_unreachable("Unknown builtin type.");
  }
  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitBlock(AstraParser::BlockContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::Block>();
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
  auto *Result = ASTContext.allocate<ast::DeclStatement>();
  Result->Range = getRange(Ctx);
  Result->Declaration = getDecl(Ctx->declaration());
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitAssignment(AstraParser::AssignmentContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::AssignmentStmt>();
  Result->Range = getRange(Ctx);
  Result->LHS = getExpr(Ctx->postfixUnaryExpr());
  Result->RHS = getExpr(Ctx->expression());

  auto *OpTree = Ctx->assignmentOperator();
  // Compound assignments such as `+=` fold into the plain operator value.
  // The distinction is lost, so codegen cannot rely on it.
  auto Op = getToken(OpTree->ASSIGNMENT(), OpTree->ADD_ASSIGNMENT(),
                     OpTree->SUB_ASSIGNMENT(), OpTree->MULT_ASSIGNMENT(),
                     OpTree->DIV_ASSIGNMENT(), OpTree->MOD_ASSIGNMENT())
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
                         Ctx->doWhileStmt(), Ctx->ifStmt()));
}

std::any ASTBuilder::visitForStmt(AstraParser::ForStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::ForStmt>();
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
  auto *Result = ASTContext.allocate<ast::ExprStmt>();
  Result->Range = getRange(Ctx);
  Result->Expression = getExpr(Ctx->expression());
  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitForEachStmt(AstraParser::ForEachStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::ForEachStmt>();
  Result->Range = getRange(Ctx);

  Result->VarName = getText(Ctx->IDENTIFIER());
  Result->Scope = getExpr(Ctx->expression());
  Result->Body = getBlock(Ctx->block());

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitWhileStmt(AstraParser::WhileStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::WhileStmt>();
  Result->Range = getRange(Ctx);

  Result->Condition = getExpr(Ctx->expression());
  Result->Body = getBlock(Ctx->block());

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitDoWhileStmt(AstraParser::DoWhileStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::DoWhileStmt>();
  Result->Range = getRange(Ctx);

  Result->Body = getBlock(Ctx->block());
  Result->Condition = getExpr(Ctx->expression());

  return static_cast<ast::Statement *>(Result);
}

std::any ASTBuilder::visitIfStmt(AstraParser::IfStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::IfStmt>();
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

std::any ASTBuilder::visitExprStmt(AstraParser::ExprStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::ExprStmt>();
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
  // Same as `visitEquality`: at most one comparison operator.
  return foldLeftAssoc(Ctx->infixExpr(), std::vector{Ctx->comparisonOperator()},
                       [](AstraParser::ComparisonOperatorContext *OpCtx) {
                         auto *Tok = getToken(OpCtx->LT(), OpCtx->GT(),
                                              OpCtx->LE(), OpCtx->GE());
                         return getBinaryOp(Tok->getType());
                       });
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
      auto *Binary = ASTContext.allocate<ast::BinaryExpr>();
      Binary->Range = getRange(ElvisExprs.front()->getStart(),
                               ElvisExprs[++InIdx]->getStop());
      Binary->Operator = ast::Op::In;
      Binary->LHS = Result;
      Binary->RHS = getExpr(ElvisExprs[InIdx]);
      Result = Binary;
    } else /*Is expr*/ {
      auto *Is = ASTContext.allocate<ast::IsExpr>();
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
  return foldLeftAssoc(Ctx->addition(), Ctx->bitwiseShiftOperator(),
                       [](AstraParser::BitwiseShiftOperatorContext *OpCtx) {
                         return OpCtx->LSHIFT() ? ast::Op::LShift
                                                : ast::Op::RShift;
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
  auto *Result = ASTContext.allocate<ast::AsExpr>();
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
    auto *Unary = ASTContext.allocate<ast::UnaryExpr>();
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
      auto *CallExpr = ASTContext.allocate<ast::CallExpr>();
      CallExpr->Range = getRange(Ctx->getStart(), Postfix->getStop());
      CallExpr->Callee = Result;
      if (auto *Args = Call->valueArguments()) {
        for (auto *Arg : Args->valueArgument()) {
          // TODO named arguments and spread
          CallExpr->Arguments.push_back(getExpr(Arg->expression()));
        }
      }
      Result = CallExpr;
    } else if (auto *Index = Postfix->indexingSuffix()) {
      auto *IndexExpr = ASTContext.allocate<ast::IndexExpr>();
      IndexExpr->Range = getRange(Ctx->getStart(), Postfix->getStop());
      IndexExpr->Base = Result;
      IndexExpr->Index = getExpr(Index->expression());
      Result = IndexExpr;
    } else if (auto *Nav = Postfix->navigationSuffix()) {
      auto *Member = ASTContext.allocate<ast::MemberExpr>();
      Member->Range = getRange(Ctx->getStart(), Postfix->getStop());
      Member->Base = Result;
      Member->Member = getText(Nav->IDENTIFIER());
      Member->NullSafe = Nav->memberAccessOperator()->QUEST() != nullptr;
      Result = Member;
    } else {
      // TODO type arguments, e.g. `foo<Int>`
      Diags.report(getRange(Postfix), llvm::SourceMgr::DK_Error,
                   "type arguments are not implemented yet");
    }
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitPrimaryExpr(AstraParser::PrimaryExprContext *Ctx) {
  if (Ctx->IDENTIFIER()) {
    auto *Result = ASTContext.allocate<ast::VarExpr>();
    Result->Range = getRange(Ctx);
    Result->Name = getText(Ctx->IDENTIFIER());
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
    auto *Result = ASTContext.allocate<ast::BoolLiteral>();
    Result->Range = getRange(Ctx);
    Result->Value = getText(Ctx->BOOLEAN_LITERAL()) == "true";
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->NULL_LITERAL()) {
    auto *Result = ASTContext.allocate<ast::NullLiteral>();
    Result->Range = getRange(Ctx);
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->INTEGER_LITERAL()) {
    auto *Result = ASTContext.allocate<ast::IntLiteral>();
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
    // `getAsInteger` produces the minimal bit width (e.g. 8 bits for 0xFF).
    // Zero-extend to 64 bits so the value is not sign-truncated by the
    // signed `APSInt` interpretation.
    Result->Value = llvm::APSInt(Raw.zext(64), /*IsUnsigned=*/false);
    return static_cast<ast::Expr *>(Result);
  }

  // FLOAT_LITERAL or DOUBLE_LITERAL.
  auto *Result = ASTContext.allocate<ast::FloatLiteral>();
  Result->Range = getRange(Ctx);
  auto Text = getText(Ctx->FLOAT_LITERAL() ? Ctx->FLOAT_LITERAL()
                                           : Ctx->DOUBLE_LITERAL());
  llvm::APFloat Value(Ctx->FLOAT_LITERAL() ? llvm::APFloat::IEEEsingle()
                                           : llvm::APFloat::IEEEdouble());
  if (Ctx->FLOAT_LITERAL()) {
    Text = Text.drop_back(); // strip the trailing 'f'/'F' suffix
  }
  if (!basic::convertFloatString(Value, Text,
                                 llvm::APFloat::rmNearestTiesToEven)) {
    // Unreachable for tokens the lexer accepted; degrade instead of crash.
    Diags.report(Result->Range, llvm::SourceMgr::DK_Error,
                 "invalid floating-point literal");
    return static_cast<ast::Expr *>(Result);
  }
  Result->Value = std::move(Value);
  return static_cast<ast::Expr *>(Result);
}

std::any
ASTBuilder::visitCollectionLiteral(AstraParser::CollectionLiteralContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::CollectionExpr>();
  Result->Range = getRange(Ctx);
  for (auto *Element : Ctx->expression()) {
    Result->Elements.push_back(getExpr(Element));
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitThisLiteral(AstraParser::ThisLiteralContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::ThisExpr>();
  Result->Range = getRange(Ctx);
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitIfExpression(AstraParser::IfExpressionContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::IfExpr>();
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
    auto *Result = ASTContext.allocate<ast::ThrowExpr>();
    Result->Range = getRange(Ctx);
    Result->Content = getExpr(Ctx->expression());
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->RETURN()) {
    auto *Result = ASTContext.allocate<ast::ReturnExpr>();
    Result->Range = getRange(Ctx);
    if (Ctx->expression()) {
      Result->Value = getExpr(Ctx->expression());
    }
    return static_cast<ast::Expr *>(Result);
  }
  if (Ctx->CONTINUE()) {
    auto *Result = ASTContext.allocate<ast::ContinueExpr>();
    Result->Range = getRange(Ctx);
    if (Ctx->label()) {
      Result->Pos = std::any_cast<ast::Label *>(visit(Ctx->label()));
    }
    return static_cast<ast::Expr *>(Result);
  }
  // The remaining alternative is BREAK.
  auto *Result = ASTContext.allocate<ast::BreakExpr>();
  Result->Range = getRange(Ctx);
  if (Ctx->label()) {
    Result->Pos = std::any_cast<ast::Label *>(visit(Ctx->label()));
  }
  return static_cast<ast::Expr *>(Result);
}

std::any ASTBuilder::visitLabel(AstraParser::LabelContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::Label>();
  Result->Range = getRange(Ctx);
  Result->Name = getIdentifier(Ctx->IDENTIFIER());
  return Result;
}

} // namespace astra::frontend
