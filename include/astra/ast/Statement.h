#pragma once

#include "Expression.h"

namespace astra::ast {
struct Declaration;
struct Parameter;

struct Statement : ASTNode {
  static bool classof(const ASTNode *Node) {
    auto K = Node->getKind();
    return (K <= NodeKind::DoWhileStmt) && (K >= NodeKind::Block);
  }
};

struct Block : Statement {
  llvm::SmallVector<Statement *> Statements;

  Block() { Kind = NodeKind::Block; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::Block;
  }
};

struct DeclStatement : Statement {
  Declaration *Decl = nullptr;
  DeclStatement() { Kind = NodeKind::DeclStatement; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::DeclStatement;
  }
};

struct ExprStmt : Statement {
  Expr *Expression = nullptr;
  ExprStmt() { Kind = NodeKind::ExprStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ExprStmt;
  }
};

struct AssignmentStmt : Statement {
  Expr *LHS = nullptr;
  /// The assignment operator. Compound assignments such as `+=` fold into
  /// the plain operator (`ast::Op::Add`) for now, so codegen cannot
  /// distinguish them.
  Op Operator = Assignment;
  Expr *RHS = nullptr;
  AssignmentStmt() { Kind = NodeKind::AssignmentStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::AssignmentStmt;
  }
};

struct IfStmt : Statement {
  Expr *Condition = nullptr;
  Statement *Then = nullptr;
  /// Either a nested `IfStmt` for `else if` or a plain `Block`.
  Statement *Else = nullptr;
  IfStmt() { Kind = NodeKind::IfStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::IfStmt;
  }
};

struct ForStmt : Statement {
  /// The declarations of the init part. Empty when the `for` has no init
  /// declaration.
  llvm::SmallVector<Declaration *> InitStmts;
  Expr *Condition = nullptr;
  Statement *Update = nullptr;
  Block *Body = nullptr;
  ForStmt() { Kind = NodeKind::ForStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ForStmt;
  }
};

struct ForEachStmt : Statement {
  /// The name of the loop variable.
  llvm::StringRef VarName;
  Expr *Scope = nullptr;
  Block *Body = nullptr;
  ForEachStmt() { Kind = NodeKind::ForEachStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ForEachStmt;
  }
};

struct WhileStmt : Statement {
  Expr *Condition = nullptr;
  Block *Body = nullptr;
  WhileStmt() { Kind = NodeKind::WhileStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::WhileStmt;
  }
};

/// One `catch` clause of a `TryStmt`. The parameter names the caught
/// exception; it has no default value.
struct CatchClause : ASTNode {
  Parameter *Param = nullptr;
  Block *Body = nullptr;
  CatchClause() { Kind = NodeKind::CatchClause; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::CatchClause;
  }
};

/// A `try` statement. The grammar requires at least one catch clause or a
/// `finally` block, so `CatchClauses` and `Finally` are never both empty.
struct TryStmt : Statement {
  Block *Body = nullptr;
  llvm::SmallVector<CatchClause *> CatchClauses;
  Block *Finally = nullptr;
  TryStmt() { Kind = NodeKind::TryStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::TryStmt;
  }
};

struct DoWhileStmt : Statement {
  Block *Body = nullptr;
  Expr *Condition = nullptr;
  DoWhileStmt() { Kind = NodeKind::DoWhileStmt; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::DoWhileStmt;
  }
};
} // namespace astra::ast
