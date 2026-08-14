#pragma once

#include "Declaration.h"

namespace astra::ast {
struct TopLevelObject : ASTNode {
  Declaration *Decl = nullptr;
  TopLevelObject() { Kind = NodeKind::TopLevelObject; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::TopLevelObject;
  }
};

struct Program : ASTNode {
  llvm::SmallVector<TopLevelObject *, 4> Objects;
  Program() { Kind = NodeKind::Program; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::Program;
  }
};
} // namespace astra::ast
