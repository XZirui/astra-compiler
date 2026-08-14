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
  IdentifierInfo *Name;
  TypeRef() { Kind = NodeKind::TypeRef; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::TypeRef;
  }
};

struct BuiltinType : Type {
  enum Ty { Void, Bool, Int, Long, Float, Double } Type = Void;
  BuiltinType() { Kind = NodeKind::BuiltinType; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::BuiltinType;
  }
};

struct ArrayType : Type {
  Type *ElementType = nullptr;
  Expr *Size = nullptr;
  ArrayType() { Kind = NodeKind::ArrayType; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::ArrayType;
  }
};

struct FunctionType : Type {
  llvm::SmallVector<Type *, 2> Parameters;
  Type *ReturnType = nullptr;
  FunctionType() { Kind = NodeKind::FunctionType; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::FunctionType;
  }
};
} // namespace astra::ast
