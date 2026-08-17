#include "astra/sema/Type.h"

#include "astra/ast/Expression.h"

#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

namespace astra::sema {

namespace {

/// Whether two size expressions denote the same size. Integer literals are
/// compared by value (different source positions of `3` are equal); anything
/// else is compared by identity for now.
bool sameSize(const ast::Expr *A, const ast::Expr *B) {
  if (A == B)
    return true;
  if (!A || !B)
    return false;
  if (llvm::isa<ast::IntLiteral>(A) && llvm::isa<ast::IntLiteral>(B))
    return llvm::cast<const ast::IntLiteral>(A)->Value ==
           llvm::cast<const ast::IntLiteral>(B)->Value;
  return false;
}

} // namespace

bool sameType(const Type *A, const Type *B) {
  if (A == B)
    return true;
  if (!A || !B)
    return false;
  if (A->Kind != B->Kind)
    return false;
  switch (A->Kind) {
  case TypeKind::Builtin:
    return llvm::cast<const BuiltinType>(A)->Value ==
           llvm::cast<const BuiltinType>(B)->Value;
  case TypeKind::Class: {
    auto *CA = llvm::cast<const ClassType>(A);
    auto *CB = llvm::cast<const ClassType>(B);
    if (CA->Decl != CB->Decl || CA->TypeArgs.size() != CB->TypeArgs.size())
      return false;
    for (size_t I = 0; I < CA->TypeArgs.size(); ++I)
      if (!sameType(CA->TypeArgs[I], CB->TypeArgs[I]))
        return false;
    return true;
  }
  case TypeKind::TypeParam:
    return llvm::cast<const TypeParamType>(A)->Param ==
           llvm::cast<const TypeParamType>(B)->Param;
  case TypeKind::Array: {
    auto *AA = llvm::cast<const ArrayType>(A);
    auto *AB = llvm::cast<const ArrayType>(B);
    return sameType(AA->Element, AB->Element) && sameSize(AA->Size, AB->Size);
  }
  case TypeKind::Function: {
    auto *FA = llvm::cast<const FunctionType>(A);
    auto *FB = llvm::cast<const FunctionType>(B);
    if (!sameType(FA->Return, FB->Return) ||
        FA->Params.size() != FB->Params.size())
      return false;
    for (size_t I = 0; I < FA->Params.size(); ++I)
      if (!sameType(FA->Params[I], FB->Params[I]))
        return false;
    return true;
  }
  case TypeKind::Error:
    return true;
  }
  llvm_unreachable("unhandled type kind");
}

} // namespace astra::sema
