#include "astra/frontend/ASTBuilder.h"

#include "astra/ast/Program.h"
#include "astra/parser/AstraParser.h"

namespace astra::frontend {

static antlr4::tree::ParseTree *available() { return nullptr; }

template <typename... Args>
static antlr4::tree::ParseTree *available(antlr4::tree::ParseTree *First,
                                          Args... Rest) {
  if (First) {
    return First;
  }
  return available(Rest...);
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
    Result->Value = getExpr(Ctx->type());
  }

  Result->IsMutable = (Ctx->VAR() == nullptr);
  return static_cast<ast::Declaration *>(Result);
}

std::any ASTBuilder::visitType(AstraParser::TypeContext *Ctx) {
  if (Ctx->functionType()) {
    return visit(Ctx->functionType());
  }

  auto *Result =
      getType(available(Ctx->parenType(), Ctx->typeRef(), Ctx->builtinType()));

  // Array type
  if (const auto &Expressions = Ctx->expression(); !Expressions.empty()) {
    auto N = Expressions.size();
    for (size_t I = 0; I < N; ++I) {
      auto *ArrayTy = ASTContext.allocate<ast::ArrayType>();

      // From the first char to "]" char.
      ArrayTy->Range = getRange(Ctx->getStart(), Ctx->RBRACKET(I)->getSymbol());
      ArrayTy->ElementType = Result;
      Result = ArrayTy;
    }
  }
  return Result;
}

std::any ASTBuilder::visitParenType(AstraParser::ParenTypeContext *Ctx) {
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
  const auto &ParamList = Ctx->paramTypeList()->type();
  for (auto *Param : ParamList) {
    Result->Parameters.push_back(getType(Param));
  }

  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitBuiltinType(AstraParser::BuiltinTypeContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::BuiltinType>();
  Result->Range = getRange(Ctx);

  auto BuiltinTy = static_cast<antlr4::tree::TerminalNode *>(
                       available(Ctx->VOID(), Ctx->BOOL(), Ctx->INT(),
                                 Ctx->LONG(), Ctx->FLOAT(), Ctx->DOUBLE()))
                       ->getSymbol()
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
    // Shouldn't be here!
    assert(false && "Unknown builtin type.");
  }
  return static_cast<ast::Type *>(Result);
}

std::any ASTBuilder::visitBlock(AstraParser::BlockContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::Block>();
  Result->Range = getRange(Ctx);

  const auto &Statements = Ctx->statement();
  for (auto *Statement : Statements) {
    Result->Statements.push_back(std::any_cast<ast::Block *>(visit(Statement)));
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
  auto Op = static_cast<antlr4::tree::TerminalNode *>(
                available(OpTree->ASSIGNMENT(), OpTree->ADD_ASSIGNMENT(),
                          OpTree->SUB_ASSIGNMENT(), OpTree->MULT_ASSIGNMENT(),
                          OpTree->DIV_ASSIGNMENT(), OpTree->MOD_ASSIGNMENT()))
                ->getSymbol()
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
    // Shouldn't be here!
    assert(false && "Unknown assignment operator.");
  }

  return Result;
}

std::any
ASTBuilder::visitControlStatement(AstraParser::ControlStatementContext *Ctx) {
  return visit(available(Ctx->forStmt(), Ctx->forEachStmt(), Ctx->whileStmt(),
                         Ctx->doWhileStmt(), Ctx->ifStmt()));
}

std::any ASTBuilder::visitForStmt(AstraParser::ForStmtContext *Ctx) {
  auto *Result = ASTContext.allocate<ast::ForStmt>();
  Result->Range = getRange(Ctx);

  const auto &InitStmts = Ctx->variableDecls()->variableDecl();
  for (auto *Stmt : InitStmts) {
    Result->InitStmts.push_back(getDecl(Stmt));
  }
  if (Ctx->expression()) {
    Result->Condition = getExpr(Ctx->expression());
  }
  if (Ctx->forUpdate()) {
    Result->Update = getStmt(Ctx->forUpdate());
  }
  Result->Body = getBlock(Ctx->block());

  return Result;
}

std::any ASTBuilder::visitForUpdate(AstraParser::ForUpdateContext *Ctx) {}

std::any ASTBuilder::visitForEachStmt(AstraParser::ForEachStmtContext *Ctx) {}

std::any ASTBuilder::visitWhileStmt(AstraParser::WhileStmtContext *Ctx) {}

std::any ASTBuilder::visitDoWhileStmt(AstraParser::DoWhileStmtContext *Ctx) {}

std::any ASTBuilder::visitIfStmt(AstraParser::IfStmtContext *Ctx) {}

std::any ASTBuilder::visitExprStmt(AstraParser::ExprStmtContext *Ctx) {}

} // namespace astra::frontend
