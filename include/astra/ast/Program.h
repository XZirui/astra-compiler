#pragma once

#include "Declaration.h"

namespace astra::ast {
struct TopLevelObject : ASTNode {
  /// The wrapped top-level declaration. Future top-level kinds, such as
  /// class declarations, will be added to this node.
  Declaration *Decl = nullptr;
  TopLevelObject() { Kind = NodeKind::TopLevelObject; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::TopLevelObject;
  }
};

struct Program : ASTNode {
  /// The top-level objects of the file, in source order.
  llvm::SmallVector<TopLevelObject *, 4> Objects;
  Program() { Kind = NodeKind::Program; }
  static bool classof(const ASTNode *Node) {
    return Node->getKind() == NodeKind::Program;
  }
};
} // namespace astra::ast
