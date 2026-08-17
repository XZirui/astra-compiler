#pragma once

namespace astra::ast {
/// The kind of every AST node, generated from `AllNodes.def`.
/// The order matters: the base classes `Expr`, `Statement`, `Declaration` and
/// `Type` implement `classof` as a contiguous range test over `NodeKind`, so
/// related kinds must stay grouped when new nodes are added — see the
/// ordering notes at the top of `AllNodes.def`.
enum class NodeKind {
#define NODE(NAME, BASE, ...) NAME,
#include "AllNodes.def"
#undef NODE
};
} // namespace astra::ast
