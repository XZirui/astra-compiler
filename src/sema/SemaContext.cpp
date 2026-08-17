#include "astra/sema/SemaContext.h"
#include "astra/sema/SemaBuilder.h"
#include "astra/sema/Type.h"

#include "astra/ast/Program.h"

#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

using astra::ast::NodeKind;

namespace astra::sema {

SemaContext::SemaContext(ast::ASTContext &C, basic::DiagnosticsEngine &D)
    : Ctx(C), Diags(D) {}

void SemaContext::run(ast::Program *P) {
  // Collect all top-level declarations first so that forward references are
  // legal, then let the builder resolve them in source order.
  CurScope = pushScope(ScopeKind::TopLevel);
  for (auto *O : P->Objects)
    registerDecl(O->Decl);
  SemaBuilder(*this).visit(P);
}

Scope *SemaContext::pushScope(ScopeKind Kind, ast::ClassDecl *Class) {
  auto *S = Ctx.allocate<Scope>();
  S->Parent = CurScope;
  S->Kind = Kind;
  S->Class = Class;
  CurScope = S;
  return S;
}

void SemaContext::popScope() { CurScope = CurScope->Parent; }

bool SemaContext::bind(Namespace NS, ast::IdentifierInfo *Name,
                       ast::ASTNode *Node) {
  auto *Map = &CurScope->map(NS);
  if (auto *Prev = Map->lookup(Name)) {
    Diags.reportf(Node->Range, llvm::SourceMgr::DK_Error,
                  "redeclaration of '{0}'", Name->getName());
    Diags.report(Prev->Range, llvm::SourceMgr::DK_Note,
                 "previous declaration is here");
    return false;
  }
  (*Map)[Name] = Node;
  return true;
}

ast::ASTNode *SemaContext::lookup(Namespace NS, ast::IdentifierInfo *Name) {
  for (auto *S = CurScope; S; S = S->Parent) {
    if (auto *Found = S->map(NS).lookup(Name))
      return Found;
  }
  return nullptr;
}

/// Whether `Found` can be referenced from the current context. Public
/// declarations are always accessible; a non-public declaration is gated only
/// when it is a class member, and it is then accessible from any scope nested
/// in the declaring class's scope (which covers nested classes accessing
/// outer-class members). Top-level declarations are always accessible — there
/// is no import mechanism yet.
///
/// Note: with the current grammar a simple name can never resolve to a class
/// member from outside the class (member scopes are not on the lookup chain
/// there), so a gated declaration is only ever found inside its own class or
/// at the top level — both scopes are on the current chain, and the scan
/// below always succeeds. The error path becomes live once member access is
/// resolved in the type-checking phase.
bool SemaContext::isAccessible(ast::ASTNode *Found) {
  if (!llvm::isa<ast::FunctionDecl>(Found) && !llvm::isa<ast::VarDecl>(Found) &&
      !llvm::isa<ast::ClassDecl>(Found))
    return true; // Parameters, type parameters and loop variables are never
                 // gated.
  auto *D = llvm::cast<ast::Declaration>(Found);
  if (D->Vis == ast::Visibility::Public)
    return true;
  for (auto *S = CurScope; S; S = S->Parent) {
    if (S->Kind != ScopeKind::Class && S->Kind != ScopeKind::TopLevel)
      continue;
    for (const auto *Map : {&S->Values, &S->Functions, &S->Types})
      for (const auto &Entry : *Map)
        if (Entry.second == Found)
          return true;
  }
  llvm::StringRef Name = [&]() {
    switch (D->getKind()) {
    case NodeKind::FunctionDecl:
      return llvm::cast<ast::FunctionDecl>(D)->Name->getName();
    case NodeKind::VarDecl:
      return llvm::cast<ast::VarDecl>(D)->Name->getName();
    case NodeKind::ClassDecl:
      return llvm::cast<ast::ClassDecl>(D)->Name->getName();
    default:
      llvm_unreachable("not a declaration");
    }
  }();
  Diags.reportf(Found->Range, llvm::SourceMgr::DK_Error,
                "cannot access '{0}': it is {1}", Name,
                D->Vis == ast::Visibility::Private ? "private" : "protected");
  return false;
}

/// Bind the name of a declaration into the current scope. Class declarations
/// additionally create their class scope and register their members (see
/// `collectClass`).
void SemaContext::registerDecl(ast::Declaration *D) {
  switch (D->getKind()) {
  case NodeKind::FunctionDecl: {
    auto *F = llvm::cast<ast::FunctionDecl>(D);
    bind(Namespace::Functions, F->Name, F);
    break;
  }
  case NodeKind::VarDecl: {
    auto *V = llvm::cast<ast::VarDecl>(D);
    bind(Namespace::Values, V->Name, V);
    break;
  }
  case NodeKind::ClassDecl:
    collectClass(llvm::cast<ast::ClassDecl>(D));
    break;
  default:
    llvm_unreachable("unexpected declaration kind");
  }
}

/// Bind the class name into the enclosing scope, then create the class scope
/// with the type parameters and all member names registered (recursing into
/// nested classes). This makes every name inside the class body resolvable,
/// including forward references to later members, before any type or body is
/// analyzed.
void SemaContext::collectClass(ast::ClassDecl *C) {
  bind(Namespace::Types, C->Name, C);
  C->ClassScope = pushScope(ScopeKind::Class, C);
  for (auto *TP : C->TypeParams)
    bind(Namespace::Types, TP->Name, TP);
  for (auto *M : C->Members)
    registerDecl(M);
  popScope();
}

BuiltinType *SemaContext::getBuiltinType(ast::BuiltinType::Ty Value) {
  auto &Slot = BuiltinCache[Value];
  if (!Slot) {
    Slot = Ctx.allocate<BuiltinType>();
    Slot->Value = Value;
  }
  return Slot;
}

Type *SemaContext::getErrorType() {
  if (!ErrorTy)
    ErrorTy = Ctx.allocate<ErrorType>();
  return ErrorTy;
}

TypeParamType *SemaContext::getParamType(ast::TypeParam *P) {
  if (P->ResolvedType)
    return llvm::cast<TypeParamType>(P->ResolvedType);
  auto *T = Ctx.allocate<TypeParamType>();
  T->Param = P;
  P->ResolvedType = T;
  return T;
}

} // namespace astra::sema
