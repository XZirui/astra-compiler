#pragma once

#include "NodeKind.h"
#include <llvm/Support/SMLoc.h>

namespace astra::ast {
/// Base class of every AST node.
/// `Range` points into the `llvm::SourceMgr` buffer that the node was parsed
/// from. That buffer must outlive the AST.
struct ASTNode {
  /// The source range spanned by the node.
  llvm::SMRange Range;
  NodeKind getKind() const { return Kind; }

protected:
  NodeKind Kind;
};
} // namespace astra::ast
