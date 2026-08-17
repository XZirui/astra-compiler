#include "astra/sema/SemaBuilder.h"
#include "astra/sema/Type.h"

#include "astra/ast/Program.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

#include <cstddef>

namespace astra::sema {

void SemaBuilder::visitProgram(ast::Program *P) {
  for (auto *O : P->Objects)
    visit(O);
}

void SemaBuilder::visitTopLevelObject(ast::TopLevelObject *T) {
  visit(T->Decl);
}

void SemaBuilder::visitFunctionDecl(ast::FunctionDecl *F) {
  if (F->ReturnType)
    visit(F->ReturnType);
  Ctx.pushScope(ScopeKind::Function);
  // All parameters are bound before their types are resolved so that default
  // values can reference earlier parameters.
  for (auto *P : F->Parameters)
    Ctx.bind(Namespace::Values, P->Name, P);
  for (auto *P : F->Parameters) {
    if (P->ParamType)
      visit(P->ParamType);
    if (P->DefaultValue)
      visit(P->DefaultValue);
  }
  if (F->Body)
    visit(F->Body);
  Ctx.popScope();
}

void SemaBuilder::visitVarDecl(ast::VarDecl *V) {
  if (V->VarType)
    visit(V->VarType);
  if (V->Value)
    visit(V->Value);
}

void SemaBuilder::visitClassDecl(ast::ClassDecl *C) {
  Scope *SavedScope = Ctx.CurScope;
  Ctx.CurScope = C->ClassScope;
  ast::ClassDecl *SavedClass = Ctx.CurClass;
  Ctx.CurClass = C;
  for (auto *TP : C->TypeParams)
    if (TP->DefaultType)
      visit(TP->DefaultType);
  for (auto *M : C->Members)
    visit(M);
  Ctx.CurClass = SavedClass;
  Ctx.CurScope = SavedScope;
}

void SemaBuilder::visitBlock(ast::Block *B) {
  Ctx.pushScope(ScopeKind::Block);
  for (auto *St : B->Statements)
    visit(St);
  Ctx.popScope();
}

void SemaBuilder::visitDeclStatement(ast::DeclStatement *DS) {
  // In incremental scopes the declaration must be bound before its
  // initializer is analyzed so that later references see it.
  Ctx.registerDecl(DS->Decl);
  visit(DS->Decl);
}

void SemaBuilder::visitExprStmt(ast::ExprStmt *ES) { visit(ES->Expression); }

void SemaBuilder::visitAssignmentStmt(ast::AssignmentStmt *A) {
  visit(A->LHS);
  visit(A->RHS);
}

void SemaBuilder::visitIfStmt(ast::IfStmt *I) {
  visit(I->Condition);
  visit(I->Then);
  if (I->Else)
    visit(I->Else);
}

void SemaBuilder::visitForStmt(ast::ForStmt *F) {
  Ctx.pushScope(ScopeKind::For);
  for (auto *D : F->InitStmts)
    Ctx.registerDecl(D);
  for (auto *D : F->InitStmts)
    visit(D);
  if (F->Condition)
    visit(F->Condition);
  if (F->Update)
    visit(F->Update);
  if (F->Body)
    visit(F->Body);
  Ctx.popScope();
}

void SemaBuilder::visitForEachStmt(ast::ForEachStmt *FE) {
  visit(FE->Scope);
  Ctx.pushScope(ScopeKind::ForEach);
  Ctx.bind(Namespace::Values, Ctx.getASTContext().getIdentifier(FE->VarName),
           FE);
  if (FE->Body)
    visit(FE->Body);
  Ctx.popScope();
}

void SemaBuilder::visitWhileStmt(ast::WhileStmt *W) {
  visit(W->Condition);
  visit(W->Body);
}

void SemaBuilder::visitDoWhileStmt(ast::DoWhileStmt *DW) {
  visit(DW->Body);
  visit(DW->Condition);
}

void SemaBuilder::visitTryStmt(ast::TryStmt *T) {
  visit(T->Body);
  for (auto *CC : T->CatchClauses)
    visit(CC);
  if (T->Finally)
    visit(T->Finally);
}

void SemaBuilder::visitCatchClause(ast::CatchClause *CC) {
  Ctx.pushScope(ScopeKind::Catch);
  Ctx.bind(Namespace::Values, CC->Param->Name, CC->Param);
  if (CC->Param->ParamType)
    visit(CC->Param->ParamType);
  visit(CC->Body);
  Ctx.popScope();
}

void SemaBuilder::visitVarExpr(ast::VarExpr *V) {
  auto *Found = Ctx.lookup(Namespace::Values, V->Name);
  if (!Found) {
    Ctx.Diags.reportf(V->Range, llvm::SourceMgr::DK_Error,
                      "unresolved reference '{0}'", V->Name->getName());
    return;
  }
  if (Ctx.isAccessible(Found))
    V->Decl = Found;
}

void SemaBuilder::visitCallExpr(ast::CallExpr *C) {
  if (auto *V = llvm::dyn_cast<ast::VarExpr>(C->Callee)) {
    // A local variable shadows a function of the same name; a plain
    // function name resolves through the functions namespace.
    auto *Found = Ctx.lookup(Namespace::Values, V->Name);
    if (!Found)
      Found = Ctx.lookup(Namespace::Functions, V->Name);
    if (Found) {
      if (Ctx.isAccessible(Found))
        V->Decl = Found;
    } else {
      Ctx.Diags.reportf(V->Range, llvm::SourceMgr::DK_Error,
                        "unresolved reference '{0}'", V->Name->getName());
    }
  } else {
    visit(C->Callee);
  }
  for (auto *A : C->Arguments)
    visit(A);
  for (auto *TA : C->TypeArgs)
    visit(TA);
}

void SemaBuilder::visitMemberExpr(ast::MemberExpr *M) {
  // Only the base expression is resolved here; resolving the member name
  // requires the type of the base and is deferred to the type-checking
  // phase.
  visit(M->Base);
}

void SemaBuilder::visitIndexExpr(ast::IndexExpr *IX) {
  visit(IX->Base);
  visit(IX->Index);
}

void SemaBuilder::visitUnaryExpr(ast::UnaryExpr *U) { visit(U->Operand); }

void SemaBuilder::visitBinaryExpr(ast::BinaryExpr *B) {
  visit(B->LHS);
  visit(B->RHS);
}

void SemaBuilder::visitComparisonChainExpr(ast::ComparisonChainExpr *CC) {
  for (auto *Operand : CC->Operands)
    visit(Operand);
}

void SemaBuilder::visitIfExpr(ast::IfExpr *IE) {
  visit(IE->Condition);
  visit(IE->Then);
  if (IE->Else)
    visit(IE->Else);
}

void SemaBuilder::visitThrowExpr(ast::ThrowExpr *T) { visit(T->Content); }

void SemaBuilder::visitReturnExpr(ast::ReturnExpr *R) {
  if (R->Value)
    visit(R->Value);
}

void SemaBuilder::visitIsExpr(ast::IsExpr *IE) {
  visit(IE->Operand);
  visit(IE->CheckType);
}

void SemaBuilder::visitAsExpr(ast::AsExpr *AE) {
  visit(AE->Operand);
  visit(AE->TargetType);
}

void SemaBuilder::visitThisExpr(ast::ThisExpr *T) {
  if (!Ctx.CurClass) {
    Ctx.Diags.report(T->Range, llvm::SourceMgr::DK_Error,
                     "'this' is not allowed outside a class member");
    return;
  }
  T->EnclosingClass = Ctx.CurClass;
}

void SemaBuilder::visitCollectionExpr(ast::CollectionExpr *CE) {
  for (auto *Element : CE->Elements)
    visit(Element);
}

void SemaBuilder::visitBuiltinType(ast::BuiltinType *B) {
  if (B->ResolvedType)
    return; // Already resolved (default types are shared between uses).
  B->ResolvedType = Ctx.getBuiltinType(B->Value);
}

void SemaBuilder::visitTypeRef(ast::TypeRef *T) {
  if (T->ResolvedType)
    return; // Already resolved (default types are shared between uses).
  auto *Found = Ctx.lookup(Namespace::Types, T->Name);
  if (!Found) {
    Ctx.Diags.reportf(T->Range, llvm::SourceMgr::DK_Error,
                      "unresolved reference '{0}'", T->Name->getName());
    T->ResolvedType = Ctx.getErrorType();
    return;
  }
  if (auto *TP = llvm::dyn_cast<ast::TypeParam>(Found)) {
    if (!T->TypeArgs.empty() || T->ExplicitTypeArgs) {
      Ctx.Diags.reportf(T->Range, llvm::SourceMgr::DK_Error,
                        "type parameter '{0}' cannot be instantiated",
                        TP->Name->getName());
      T->ResolvedType = Ctx.getErrorType();
      return;
    }
    T->ResolvedType = Ctx.getParamType(TP);
    return;
  }
  if (auto *Cls = llvm::dyn_cast<ast::ClassDecl>(Found)) {
    T->ResolvedType = resolveClassArgs(Cls, T);
    return;
  }
  llvm_unreachable("unexpected entry in the types namespace");
}

void SemaBuilder::visitArrayType(ast::ArrayType *A) {
  if (A->ResolvedType)
    return; // Already resolved (default types are shared between uses).
  visit(A->ElementType);
  if (A->Size)
    visit(A->Size);
  auto *R = Ctx.getASTContext().allocate<sema::ArrayType>();
  R->Element = A->ElementType->ResolvedType;
  R->Size = A->Size;
  A->ResolvedType = R;
}

void SemaBuilder::visitFunctionType(ast::FunctionType *F) {
  if (F->ResolvedType)
    return; // Already resolved (default types are shared between uses).
  auto *R = Ctx.getASTContext().allocate<sema::FunctionType>();
  auto *Params = Ctx.getASTContext().allocate<Type *>(
      F->Parameters.empty() ? 1 : F->Parameters.size());
  for (size_t I = 0; I < F->Parameters.size(); ++I) {
    visit(F->Parameters[I]);
    Params[I] = F->Parameters[I]->ResolvedType;
  }
  visit(F->ReturnType);
  R->Params = llvm::ArrayRef<Type *>(Params, F->Parameters.size());
  R->Return = F->ReturnType->ResolvedType;
  F->ResolvedType = R;
}

/// Build the resolved type of a class reference, validating the type
/// argument count and filling in defaults. The result always has one
/// `Type` per declared type parameter.
Type *SemaBuilder::resolveClassArgs(ast::ClassDecl *C, ast::TypeRef *T) {
  size_t N = C->TypeParams.size();
  size_t K = T->TypeArgs.size();
  // Resolve every written argument first, so that names inside them are
  // diagnosed even when the argument count is wrong. The array is sized by
  // the larger of the two counts for that reason.
  size_t ArgCount = K > N ? K : N;
  auto *Args =
      Ctx.getASTContext().allocate<Type *>(ArgCount == 0 ? 1 : ArgCount);
  for (size_t I = 0; I < K; ++I) {
    visit(T->TypeArgs[I]);
    Args[I] = T->TypeArgs[I]->ResolvedType;
  }
  if (K > N) {
    Ctx.Diags.reportf(T->Range, llvm::SourceMgr::DK_Error,
                      "wrong number of type arguments for '{0}' (expected "
                      "{1}, got {2})",
                      C->Name->getName(), N, K);
    return Ctx.getErrorType();
  }
  // Fill the remaining parameters with their defaults, resolved inside the
  // class scope so that earlier type parameters are visible.
  Scope *SavedScope = Ctx.CurScope;
  Ctx.CurScope = C->ClassScope;
  for (size_t I = K; I < N; ++I) {
    auto *TP = C->TypeParams[I];
    if (!TP->DefaultType) {
      Ctx.Diags.reportf(T->Range, llvm::SourceMgr::DK_Error,
                        "type parameter '{0}' of '{1}' has no default type",
                        TP->Name->getName(), C->Name->getName());
      Ctx.CurScope = SavedScope;
      return Ctx.getErrorType();
    }
    if (llvm::is_contained(Ctx.DefaultChain, C)) {
      Ctx.Diags.reportf(T->Range, llvm::SourceMgr::DK_Error,
                        "cyclic type parameter default involving '{0}'",
                        C->Name->getName());
      Ctx.CurScope = SavedScope;
      return Ctx.getErrorType();
    }
    Ctx.DefaultChain.push_back(C);
    visit(TP->DefaultType);
    Ctx.DefaultChain.pop_back();
    // A failed default (e.g. a cyclic chain detected deeper) must make this
    // class type fail too, instead of being wrapped into a nested class
    // type, so the error propagates to every reference on the chain.
    if (TP->DefaultType->ResolvedType->Kind == TypeKind::Error) {
      Ctx.CurScope = SavedScope;
      return Ctx.getErrorType();
    }
    Args[I] = TP->DefaultType->ResolvedType;
  }
  Ctx.CurScope = SavedScope;
  auto *R = Ctx.getASTContext().allocate<sema::ClassType>();
  R->Decl = C;
  R->TypeArgs = llvm::ArrayRef<Type *>(Args, N);
  return R;
}

} // namespace astra::sema
