#pragma once

#include "astra/ast/ASTVisitor.h"
#include "astra/sema/SemaContext.h"

namespace astra::sema {
/// The traversal engine of the first semantic-analysis phase. Every
/// `visitXxx` method of the AST visitor pattern lives here; state and
/// services stay in `SemaContext` (which this class is a friend of). One
/// instance per `SemaContext::run` call.
class SemaBuilder : public ast::ASTVisitor<SemaBuilder> {
public:
  explicit SemaBuilder(SemaContext &C) : Ctx(C) {}

  // --- declarations ---
  void visitProgram(ast::Program *P);
  void visitTopLevelObject(ast::TopLevelObject *T);
  void visitFunctionDecl(ast::FunctionDecl *F);
  void visitVarDecl(ast::VarDecl *V);
  void visitClassDecl(ast::ClassDecl *C);

  // --- statements ---
  void visitBlock(ast::Block *B);
  void visitDeclStatement(ast::DeclStatement *DS);
  void visitExprStmt(ast::ExprStmt *ES);
  void visitAssignmentStmt(ast::AssignmentStmt *A);
  void visitIfStmt(ast::IfStmt *I);
  void visitForStmt(ast::ForStmt *F);
  void visitForEachStmt(ast::ForEachStmt *FE);
  void visitWhileStmt(ast::WhileStmt *W);
  void visitDoWhileStmt(ast::DoWhileStmt *DW);
  void visitTryStmt(ast::TryStmt *T);
  void visitCatchClause(ast::CatchClause *CC);

  // --- expressions ---
  void visitVarExpr(ast::VarExpr *V);
  void visitCallExpr(ast::CallExpr *C);
  void visitMemberExpr(ast::MemberExpr *M);
  void visitIndexExpr(ast::IndexExpr *IX);
  void visitUnaryExpr(ast::UnaryExpr *U);
  void visitBinaryExpr(ast::BinaryExpr *B);
  void visitComparisonChainExpr(ast::ComparisonChainExpr *CC);
  void visitIfExpr(ast::IfExpr *IE);
  void visitThrowExpr(ast::ThrowExpr *T);
  void visitReturnExpr(ast::ReturnExpr *R);
  void visitIsExpr(ast::IsExpr *IE);
  void visitAsExpr(ast::AsExpr *AE);
  void visitThisExpr(ast::ThisExpr *T);
  void visitCollectionExpr(ast::CollectionExpr *CE);

  // --- types ---
  void visitBuiltinType(ast::BuiltinType *B);
  void visitTypeRef(ast::TypeRef *T);
  void visitArrayType(ast::ArrayType *A);
  void visitFunctionType(ast::FunctionType *F);

private:
  Type *resolveClassArgs(ast::ClassDecl *C, ast::TypeRef *T);

  SemaContext &Ctx;
};
} // namespace astra::sema
