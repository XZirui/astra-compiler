#pragma once

namespace astra::ast {
/// The kind of every AST node.
/// The order matters. The base classes `Expr`, `Statement`, `Declaration` and
/// `Type` implement `classof` as a contiguous range test over `NodeKind`, so
/// related kinds must stay grouped when new nodes are added.
enum class NodeKind {
  Program,
  TopLevelObject,
  FunctionDecl,
  ClassDecl,
  VarDecl,
  Parameter,
  TypeRef,
  BuiltinType,
  ArrayType,
  FunctionType,
  Block,
  DeclStatement,
  ExprStmt,
  AssignmentStmt,
  IfStmt,
  ForStmt,
  ForEachStmt,
  WhileStmt,
  TryStmt,
  DoWhileStmt,
  NullLiteral,
  BoolLiteral,
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  CharLiteral,
  VarExpr,
  UnaryExpr,
  BinaryExpr,
  ComparisonChainExpr,
  IfExpr,
  ThrowExpr,
  ReturnExpr,
  ContinueExpr,
  BreakExpr,
  CallExpr,
  IndexExpr,
  MemberExpr,
  IsExpr,
  AsExpr,
  ThisExpr,
  CollectionExpr,
  Label,
  // `TypeParam` and `CatchClause` belong to no base-class range (`Expr`/
  // `Statement`/`Declaration`/`Type`), so they live at the end of the enum.
  TypeParam,
  CatchClause,
};
} // namespace astra::ast