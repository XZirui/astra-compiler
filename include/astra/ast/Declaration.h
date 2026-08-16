#pragma once

#include "Statement.h"
#include "Type.h"

namespace astra::ast {
struct Declaration : ASTNode {
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

struct ClassDecl : Declaration {
  // TODO
};
} // namespace astra::ast
