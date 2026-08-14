#pragma once

#include "NodeKind.h"
#include <llvm/Support/SMLoc.h>

namespace astra::ast {
struct ASTNode {
  llvm::SMRange Range;
  NodeKind getKind() const { return Kind; }

protected:
  NodeKind Kind;
};
} // namespace astra::ast
