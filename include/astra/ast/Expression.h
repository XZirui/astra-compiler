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
    return (K <= NodeKind::CharLiteral) && (K >= NodeKind::NullLiteral);
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

struct StringLiteral : LiteralConstant {
  /// The decoded content of the literal, without the surrounding quotes.
  /// The bytes live in the ASTContext arena and stay valid for the lifetime
  /// of the context.
  llvm::StringRef Value;

  StringLiteral() { Kind = NodeKind::StringLiteral; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::StringLiteral;
  }
};

struct CharLiteral : LiteralConstant {
  /// The decoded value of the literal as a Unicode code point (or the raw
  /// byte for non-escaped source characters).
  uint32_t Value = 0;

  CharLiteral() { Kind = NodeKind::CharLiteral; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::CharLiteral;
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

/// A chain of comparison operators, e.g. `a < b <= c`. This has the math
/// semantics `a < b && b <= c` (each middle operand is evaluated once); the
/// expansion is left to semantic analysis / codegen. All operators must
/// point in the same direction; mixed directions are a builder diagnostic.
struct ComparisonChainExpr : Expr {
  llvm::SmallVector<Expr *, 2> Operands;
  /// One `Op` per gap, `Operands.size() - 1` entries in total.
  /// Only Lt / Le / Gt / Ge are valid.
  llvm::SmallVector<Op, 1> Operators;

  ComparisonChainExpr() { Kind = NodeKind::ComparisonChainExpr; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ComparisonChainExpr;
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
  IdentifierInfo *Name = nullptr;
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
  /// Type arguments written before the argument list, e.g. `<Int>` in
  /// `foo<Int>(1)`.
  llvm::SmallVector<Type *, 1> TypeArgs;
  /// Whether a type argument list was written explicitly. Distinguishes
  /// `foo()` (type inference) from `foo<>()` (force default type parameters).
  bool ExplicitTypeArgs = false;

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
