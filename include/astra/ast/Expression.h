#pragma once

#include "IdentifierInfo.h"
#include "Node.h"

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallVector.h>

namespace astra::ast {
struct Type;

/// Base class of all expressions.
/// The kinds of all expressions form the contiguous range
/// `NullLiteral`..`CollectionExpr`, which `classof` relies on.
struct Expr : ASTNode {
  /// Whether the expression value is a compile-time constant.
  /// Only literal constants set this for now.
  bool IsConst = false;
  static bool classof(const ASTNode *Node) {
    auto K = Node->getKind();
    return (K <= NodeKind::CollectionExpr) && (K >= NodeKind::NullLiteral);
  }
};

struct LiteralConstant : Expr {
  LiteralConstant() { IsConst = true; }
  static bool classof(const ASTNode *Node) {
    auto K = Node->getKind();
    return (K <= NodeKind::FloatLiteral) && (K >= NodeKind::NullLiteral);
  }
};

struct BoolLiteral : LiteralConstant {
  bool Value = false;

  BoolLiteral() { Kind = NodeKind::BoolLiteral; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::BoolLiteral;
  }
};

struct NullLiteral : LiteralConstant {
  NullLiteral() { Kind = NodeKind::NullLiteral; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::NullLiteral;
  }
};

struct IntLiteral : LiteralConstant {
  llvm::APSInt Value;

  IntLiteral() { Kind = NodeKind::IntLiteral; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::IntLiteral;
  }
};

struct FloatLiteral : LiteralConstant {
  llvm::APFloat Value{0.};

  FloatLiteral() { Kind = NodeKind::FloatLiteral; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::FloatLiteral;
  }
};

struct VarExpr : Expr {
  llvm::StringRef Name;

  VarExpr() { Kind = NodeKind::VarExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::VarExpr;
  }
};

/// The operators of `UnaryExpr`, `BinaryExpr` and `AssignmentStmt`.
/// `ASTDumper::getOpSymbol` maps each one to its source spelling.
enum Op {
  Add,
  Sub,
  Mult,
  Div,
  Mod,
  Not,
  Eq,
  Neq,
  LShift,
  RShift,
  BitAnd,
  BitXor,
  BitOr,
  BitNot,
  Lt,
  Gt,
  Le,
  Ge,
  Disj,
  Conj,
  In,
  Is,
  As,
  AsQuest,
  Elvis,
  Assignment
};

struct UnaryExpr : Expr {
  Op Operator = Add;
  Expr *Operand = nullptr;

  UnaryExpr() { Kind = NodeKind::UnaryExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::UnaryExpr;
  }
};

struct BinaryExpr : Expr {
  Op Operator = Add;
  Expr *LHS = nullptr;
  Expr *RHS = nullptr;

  BinaryExpr() { Kind = NodeKind::BinaryExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::BinaryExpr;
  }
};

struct IfExpr : Expr {
  Expr *Condition = nullptr;
  Expr *Then = nullptr;
  Expr *Else = nullptr;

  IfExpr() { Kind = NodeKind::IfExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::IfExpr;
  }
};

struct ThrowExpr : Expr {
  Expr *Content = nullptr;

  ThrowExpr() { Kind = NodeKind::ThrowExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ThrowExpr;
  }
};

struct Label : ASTNode {
  IdentifierInfo *Name;
  // TODO attach to statement
  Label() { Kind = NodeKind::Label; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::Label;
  }
};

struct ReturnExpr : Expr {
  Expr *Value = nullptr;

  ReturnExpr() { Kind = NodeKind::ReturnExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ReturnExpr;
  }
};

struct ContinueExpr : Expr {
  Label *Pos = nullptr;

  ContinueExpr() { Kind = NodeKind::ContinueExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ContinueExpr;
  }
};

struct BreakExpr : Expr {
  Label *Pos = nullptr;

  BreakExpr() { Kind = NodeKind::BreakExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::BreakExpr;
  }
};

struct CallExpr : Expr {
  Expr *Callee = nullptr;
  llvm::SmallVector<Expr *> Arguments;

  CallExpr() { Kind = NodeKind::CallExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::CallExpr;
  }
};

struct IndexExpr : Expr {
  Expr *Base = nullptr;
  Expr *Index = nullptr;

  IndexExpr() { Kind = NodeKind::IndexExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::IndexExpr;
  }
};

struct MemberExpr : Expr {
  Expr *Base = nullptr;
  /// The name of the accessed member.
  llvm::StringRef Member;
  /// Whether the access uses the `?.` operator.
  bool NullSafe = false;

  MemberExpr() { Kind = NodeKind::MemberExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::MemberExpr;
  }
};

struct IsExpr : Expr {
  Expr *Operand = nullptr;
  Type *CheckType = nullptr;

  IsExpr() { Kind = NodeKind::IsExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::IsExpr;
  }
};

struct AsExpr : Expr {
  Expr *Operand = nullptr;
  Type *TargetType = nullptr;
  /// Whether the cast uses `as?` instead of `as`.
  bool NullSafe = false;

  AsExpr() { Kind = NodeKind::AsExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::AsExpr;
  }
};

struct ThisExpr : Expr {
  ThisExpr() { Kind = NodeKind::ThisExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ThisExpr;
  }
};

struct CollectionExpr : Expr {
  llvm::SmallVector<Expr *> Elements;

  CollectionExpr() { Kind = NodeKind::CollectionExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::CollectionExpr;
  }
};
} // namespace astra::ast
