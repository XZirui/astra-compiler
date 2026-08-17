#pragma once

#include "Program.h"
#include <llvm/Support/ErrorHandling.h>
#include <type_traits>

namespace astra::ast {
/// CRTP base of every AST visitor. `Ptr` maps a node type to the pointer type
/// the visitor uses (`T *` for `ASTVisitor`, `const T *` for
/// `ASTConstVisitor`); `Derived` is the concrete visitor; `RetTy` is the
/// return type of every visit method (defaults to `void`).
///
/// `visit` dispatches on `NodeKind`; every concrete node's visit method falls
/// back to its base class's method through the `AllNodes.def` chain, so a
/// visitor may override any level (e.g. overriding `visitExpr` covers every
/// expression kind not overridden individually).
template <template <typename> class Ptr, typename Derived,
          typename RetTy = void>
class ASTVisitorBase {
public:
#define PTR(CLASS) typename Ptr<CLASS>::type
#define DISPATCH(NAME, CLASS)                                                  \
  return static_cast<Derived *>(this)->visit##NAME(static_cast<PTR(CLASS)>(D));

  /// Visit any node, dispatching on its kind.
  RetTy visit(PTR(ASTNode) D) {
    switch (D->getKind()) {
#define NODE(NAME, BASE, ...)                                                  \
  case NodeKind::NAME:                                                         \
    DISPATCH(NAME, NAME)
#include "AllNodes.def"
#undef NODE
    default:
      llvm_unreachable("unknown node kind in ASTVisitorBase::visit");
    }
  }

  /// Default implementations: fall back to the direct base class. Nodes
  /// without a concrete visitor method end up at the base-class chain.
#define NODE(NAME, BASE, ...)                                                  \
  RetTy visit##NAME(PTR(NAME) D) { DISPATCH(BASE, BASE); }
#include "AllNodes.def"
#undef NODE

  // Manual endpoints of the fallback chain: the abstract base classes have no
  // `NodeKind`, so the .def cannot generate methods for them.
  RetTy visitASTNode(PTR(ASTNode)) {
    if constexpr (std::is_void_v<RetTy>)
      return;
    else
      return RetTy();
  }
  RetTy visitDeclaration(PTR(Declaration) D) { DISPATCH(ASTNode, ASTNode); }
  RetTy visitStatement(PTR(Statement) D) { DISPATCH(ASTNode, ASTNode); }
  RetTy visitExpr(PTR(Expr) D) { DISPATCH(ASTNode, ASTNode); }
  RetTy visitType(PTR(Type) D) { DISPATCH(ASTNode, ASTNode); }

#undef PTR
#undef DISPATCH
};

template <typename T> struct MakePtr {
  using type = std::add_pointer_t<T>;
};
template <typename T> struct MakeConstPtr {
  using type = std::add_pointer_t<std::add_const_t<T>>;
};

/// Non-const visitor: visit methods receive plain `T *`.
template <typename Derived, typename RetTy = void>
class ASTVisitor : public ASTVisitorBase<MakePtr, Derived, RetTy> {};

/// Const visitor: visit methods receive `const T *`.
template <typename Derived, typename RetTy = void>
class ASTConstVisitor : public ASTVisitorBase<MakeConstPtr, Derived, RetTy> {};

} // namespace astra::ast
