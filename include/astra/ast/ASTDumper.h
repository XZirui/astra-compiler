#pragma once

#include "Expression.h"
#include "Type.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/raw_ostream.h>

namespace astra::ast {
struct Program;
enum class Visibility;

/// Dumps an `ast::Program` to a `llvm::raw_ostream` as a clang AST-dump
/// style tree. Each node is printed as `KindName 'attribute'` (e.g.
/// `FunctionDecl 'main'`), children are indented two spaces per level, with
/// `|-`/`` `- `` connectors. Single-field children (e.g. `ReturnType`,
/// `Condition`) get a label line with the field name above their subtree.
class ASTDumper {
  llvm::raw_ostream &OS;

  /// A child slot: the label printed above the child (empty for children of
  /// list members) and the child node itself.
  struct Child {
    llvm::StringRef Label;
    const ASTNode *Node;
  };

  /// Append a labeled child, skipping null nodes.
  static void pushChild(llvm::SmallVectorImpl<Child> &Children,
                        llvm::StringRef Label, const ASTNode *Node);

  /// Append every node of a `SmallVector` child list, unlabeled.
  template <typename T>
  static void appendChildren(llvm::SmallVectorImpl<Child> &Children,
                             const llvm::SmallVectorImpl<T *> &Nodes) {
    for (auto *Node : Nodes) {
      Children.push_back({llvm::StringRef(), Node});
    }
  }

  /// Return the source spelling of `Operator` as printed in the dump.
  static llvm::StringRef getOpSymbol(Op Operator);
  /// Return the spelling of a builtin type kind as printed in the dump.
  static llvm::StringRef getBuiltinTypeName(BuiltinType::Ty Value);
  /// Return the spelling of a visibility as printed in the dump.
  static llvm::StringRef getVisibilityName(Visibility Vis);

  /// Print the node's header line, e.g. `BinaryExpr '+'`.
  void dumpHeader(const ASTNode *Node);

  /// Collect the child slots of `Node` in dump order.
  void collectChildren(const ASTNode *Node,
                       llvm::SmallVectorImpl<Child> &Children);

  /// Print a list of child slots under `Prefix`.
  void dumpChildren(const llvm::SmallVectorImpl<Child> &Children,
                    llvm::StringRef Prefix);

  /// Print one node (header + subtree) under `Prefix`. `IsLast` selects the
  /// `-` or `` `- `` connector.
  void dumpNode(const ASTNode *Node, llvm::StringRef Prefix, bool IsLast);

public:
  explicit ASTDumper(llvm::raw_ostream &OS) : OS(OS) {}

  void dump(const Program *P);
};

/// Dump `Program` to `OS`.
void dump(const Program *P, llvm::raw_ostream &OS);
} // namespace astra::ast
