#pragma once

#include "Statement.h"
#include "Type.h"

namespace astra::ast {
/// The visibility of a declaration. Declarations without a modifier default
/// to `Public`. For class members this controls visibility outside the
/// class; for top-level declarations it controls visibility on import.
enum class Visibility { Public, Private, Protected };

struct Declaration : ASTNode {
  /// The visibility modifier, if any.
  Visibility Vis = Visibility::Public;
  static bool classof(const ASTNode *Node) {
    auto K = Node->getKind();
    return (K >= NodeKind::FunctionDecl) && (K <= NodeKind::VarDecl);
  }
};

struct Parameter : ASTNode {
  IdentifierInfo *Name = nullptr;
  Type *Type = nullptr;
  /// The optional default value of the parameter.
  Expr *DefaultValue = nullptr;

  Parameter() { Kind = NodeKind::Parameter; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::Parameter;
  }
};

struct FunctionDecl : Declaration {
  IdentifierInfo *Name = nullptr;
  llvm::SmallVector<Parameter *, 2> Parameters;
  Type *ReturnType = nullptr;
  Block *Body = nullptr;

  FunctionDecl() { Kind = NodeKind::FunctionDecl; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::FunctionDecl;
  }
};

struct VarDecl : Declaration {
  IdentifierInfo *Name = nullptr;
  /// True for `var` declarations. A plain `val` is immutable.
  bool IsMutable = false;
  Type *VarType = nullptr;
  Expr *Value = nullptr;
  VarDecl() { Kind = NodeKind::VarDecl; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::VarDecl;
  }
};

/// A declared type parameter of a class, e.g. `T` in `class Box<T>`, with an
/// optional default type (`T = Int`).
struct TypeParam : ASTNode {
  IdentifierInfo *Name = nullptr;
  /// The default type written after `=`. Null when no default is given.
  Type *DefaultType = nullptr;
  TypeParam() { Kind = NodeKind::TypeParam; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::TypeParam;
  }
};

struct ClassDecl : Declaration {
  IdentifierInfo *Name = nullptr;
  /// The declared type parameters in order, e.g. `T, U = Int` in
  /// `class Box<T, U = Int>`. Empty when no type parameter list is written.
  llvm::SmallVector<TypeParam *, 2> TypeParams;
  /// The class body members (`VarDecl` properties, `FunctionDecl` member
  /// functions and nested `ClassDecl`s) in source order. Empty when no body
  /// is written.
  llvm::SmallVector<Declaration *, 4> Members;
  ClassDecl() { Kind = NodeKind::ClassDecl; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ClassDecl;
  }
};
} // namespace astra::ast
