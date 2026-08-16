#pragma once

#include "Expression.h"
#include "IdentifierInfo.h"
#include "Node.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/SmallVector.h>

namespace astra::ast {
struct Type : ASTNode {
  static bool classof(const ASTNode *Node) {
    auto K = Node->getKind();
    return (K <= NodeKind::FunctionType) && (K >= NodeKind::TypeRef);
  }
};

struct TypeRef : Type {
  IdentifierInfo *Name = nullptr;
  /// Type arguments of a generic reference.
  llvm::SmallVector<Type *, 0> TypeArgs;
  /// Whether the type argument list was written explicitly. Distinguishes
  /// `Box` from `Box<>` (force default type parameters).
  bool ExplicitTypeArgs = false;
  TypeRef() { Kind = NodeKind::TypeRef; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::TypeRef;
  }
};

struct BuiltinType : Type {
  enum Ty { Void, Bool, Int, Long, Float, Double, Char, String } Type = Void;
  BuiltinType() { Kind = NodeKind::BuiltinType; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::BuiltinType;
  }
};

struct ArrayType : Type {
  /// The element type, possibly another `ArrayType` for nested arrays.
  Type *ElementType = nullptr;
  /// The size expression written between the brackets.
  Expr *Size = nullptr;
  ArrayType() { Kind = NodeKind::ArrayType; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ArrayType;
  }
};

struct FunctionType : Type {
  /// The parameter types in order. Empty for a function type written without
  /// a parameter list.
  llvm::SmallVector<Type *, 2> Parameters;
  Type *ReturnType = nullptr;
  FunctionType() { Kind = NodeKind::FunctionType; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::FunctionType;
  }
};
} // namespace astra::ast
