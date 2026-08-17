#include "astra/sema/Analyze.h"
#include "astra/sema/Type.h"

#include "ParseHelper.h"
#include "astra/ast/Declaration.h"
#include "astra/ast/Expression.h"
#include "astra/ast/Program.h"
#include "astra/ast/Statement.h"
#include "astra/basic/DiagnosticsEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>

using namespace astra;
using namespace astra::ast;

namespace {

/// Parse `Src` and run semantic analysis on it, then invoke `Fn` with the
/// context, program and diagnostics. The source must parse cleanly so that
/// every diagnostic comes from the semantic analysis.
template <typename FnTy> void parseAndAnalyze(llvm::StringRef Src, FnTy &&Fn) {
  test::parseSourceWithDiags(
      Src, [&](ASTContext &Ctx, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P != nullptr);
        REQUIRE(!Diags.hasErrors());
        sema::analyze(Ctx, P, Diags);
        Fn(Ctx, P, Diags);
      });
}

FunctionDecl *firstFunction(Program *P) {
  return llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
}

/// Assert `Diags` has exactly `Count` diagnostics, the first one an error at
/// `Line` whose message contains `Message`.
void checkError(basic::DiagnosticsEngine &Diags, size_t Count, int Line,
                llvm::StringRef Message) {
  REQUIRE(Diags.hasErrors());
  auto D = Diags.getDiagnostics();
  REQUIRE(D.size() == Count);
  REQUIRE(D[0].getKind() == llvm::SourceMgr::DK_Error);
  REQUIRE(D[0].getLineNo() == Line);
  REQUIRE(D[0].getMessage().find(Message) != llvm::StringRef::npos);
}

} // namespace

TEST_CASE("A class reference resolves to a class type", "[sema]") {
  parseAndAnalyze(
      "class Box {} fun f() -> Box { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *Box = P->Objects[0]->Decl;
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Resolved =
            llvm::cast<sema::ClassType>(F->ReturnType->ResolvedType);
        REQUIRE(Resolved->Kind == sema::TypeKind::Class);
        REQUIRE(Resolved->Decl == Box);
        REQUIRE(Resolved->TypeArgs.empty());
      });
}

TEST_CASE("Type arguments are filled with defaults", "[sema]") {
  parseAndAnalyze(
      "class Box<T, U = string> {} fun f() -> Box<int> { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Resolved =
            llvm::cast<sema::ClassType>(F->ReturnType->ResolvedType);
        REQUIRE(Resolved->TypeArgs.size() == 2);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->TypeArgs[0])->Value ==
                ast::BuiltinType::Int);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->TypeArgs[1])->Value ==
                ast::BuiltinType::String);
      });
}

TEST_CASE("An empty type argument list forces the defaults", "[sema]") {
  parseAndAnalyze(
      "class Box<T = int, U = string> {} fun f() -> Box<> { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Resolved =
            llvm::cast<sema::ClassType>(F->ReturnType->ResolvedType);
        REQUIRE(Resolved->TypeArgs.size() == 2);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->TypeArgs[0])->Value ==
                ast::BuiltinType::Int);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->TypeArgs[1])->Value ==
                ast::BuiltinType::String);
      });
}

TEST_CASE("A bare class reference uses the defaults", "[sema]") {
  parseAndAnalyze(
      "class Box<T = int> {} var b: Box;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *B = llvm::cast<VarDecl>(P->Objects[1]->Decl);
        auto *Resolved = llvm::cast<sema::ClassType>(B->VarType->ResolvedType);
        REQUIRE(Resolved->TypeArgs.size() == 1);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->TypeArgs[0])->Value ==
                ast::BuiltinType::Int);
      });
}

TEST_CASE("A type parameter reference resolves to a type-parameter type",
          "[sema]") {
  parseAndAnalyze(
      "class Box<T> { var x: T; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *Box = llvm::cast<ClassDecl>(P->Objects[0]->Decl);
        auto *X = llvm::cast<VarDecl>(Box->Members[0]);
        auto *Resolved =
            llvm::cast<sema::TypeParamType>(X->VarType->ResolvedType);
        REQUIRE(Resolved->Param == Box->TypeParams[0]);
      });
}

TEST_CASE("Nested generic references resolve recursively", "[sema]") {
  parseAndAnalyze(
      "class Box<T> {} var b: Box<Box<int>>;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *B = llvm::cast<VarDecl>(P->Objects[1]->Decl);
        auto *Outer = llvm::cast<sema::ClassType>(B->VarType->ResolvedType);
        auto *Inner = llvm::cast<sema::ClassType>(Outer->TypeArgs[0]);
        REQUIRE(Inner->TypeArgs.size() == 1);
        REQUIRE(llvm::cast<sema::BuiltinType>(Inner->TypeArgs[0])->Value ==
                ast::BuiltinType::Int);
      });
}

TEST_CASE("Type parameters are visible in the class scope", "[sema]") {
  parseAndAnalyze(
      "class Box<T> { var x: Box<T>; var y: T; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *Box = llvm::cast<ClassDecl>(P->Objects[0]->Decl);
        auto *X = llvm::cast<VarDecl>(Box->Members[0]);
        auto *Y = llvm::cast<VarDecl>(Box->Members[1]);
        auto *XType = llvm::cast<sema::ClassType>(X->VarType->ResolvedType);
        auto *XArg = llvm::cast<sema::TypeParamType>(XType->TypeArgs[0]);
        auto *YType = llvm::cast<sema::TypeParamType>(Y->VarType->ResolvedType);
        // Both references denote the same parameter, so they share the cached
        // `TypeParamType` instance.
        REQUIRE(XArg == YType);
      });
}

TEST_CASE("Builtin types are cached by identity", "[sema]") {
  parseAndAnalyze(
      "fun f(a: int) -> int { return a; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        REQUIRE(F->Parameters[0]->ParamType->ResolvedType ==
                F->ReturnType->ResolvedType);
        REQUIRE(
            llvm::cast<sema::BuiltinType>(F->ReturnType->ResolvedType)->Value ==
            ast::BuiltinType::Int);
      });
}

TEST_CASE("A class named like a builtin does not collide", "[sema]") {
  parseAndAnalyze(
      "class String {} fun f() -> String { return null; } fun g() -> string "
      "{ return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *String = P->Objects[0]->Decl;
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *G = llvm::cast<FunctionDecl>(P->Objects[2]->Decl);
        auto *RF = llvm::cast<sema::ClassType>(F->ReturnType->ResolvedType);
        REQUIRE(RF->Decl == String);
        auto *RG = llvm::cast<sema::BuiltinType>(G->ReturnType->ResolvedType);
        REQUIRE(RG->Value == ast::BuiltinType::String);
      });
}

TEST_CASE("Array types carry their size expression", "[sema]") {
  parseAndAnalyze(
      "fun f() -> int[3] { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        auto *Resolved =
            llvm::cast<sema::ArrayType>(F->ReturnType->ResolvedType);
        REQUIRE(Resolved->Kind == sema::TypeKind::Array);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->Element)->Value ==
                ast::BuiltinType::Int);
        REQUIRE(Resolved->Size != nullptr);
        REQUIRE(Resolved->Size->getKind() == NodeKind::IntLiteral);
      });
  parseAndAnalyze(
      "var n = 2; fun g() -> int[n] { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *N = P->Objects[0]->Decl;
        auto *G = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Resolved =
            llvm::cast<sema::ArrayType>(G->ReturnType->ResolvedType);
        auto *Size = llvm::cast<VarExpr>(Resolved->Size);
        REQUIRE(Size->Decl == N);
      });
}

TEST_CASE("Function types resolve their parameters and return type", "[sema]") {
  parseAndAnalyze(
      "fun g(x: fun (int, string) -> bool) -> void {}",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *G = firstFunction(P);
        auto *Resolved = llvm::cast<sema::FunctionType>(
            G->Parameters[0]->ParamType->ResolvedType);
        REQUIRE(Resolved->Params.size() == 2);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->Params[0])->Value ==
                ast::BuiltinType::Int);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->Params[1])->Value ==
                ast::BuiltinType::String);
        REQUIRE(llvm::cast<sema::BuiltinType>(Resolved->Return)->Value ==
                ast::BuiltinType::Bool);
      });
}

TEST_CASE("Structurally equal class types compare equal", "[sema]") {
  parseAndAnalyze(
      "class Box<T> {} fun f(a: Box<int>) -> Box<int> { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Param = llvm::cast<sema::ClassType>(
            F->Parameters[0]->ParamType->ResolvedType);
        auto *Return = llvm::cast<sema::ClassType>(F->ReturnType->ResolvedType);
        // Every resolution allocates a fresh instance...
        REQUIRE(Param != Return);
        // ...but structurally they are the same type.
        REQUIRE(sema::sameType(Param, Return));
      });
}

TEST_CASE("Array sizes take part in type equality", "[sema]") {
  parseAndAnalyze(
      "fun f(a: int[3]) -> int[3] { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        auto *Param = llvm::cast<sema::ArrayType>(
            F->Parameters[0]->ParamType->ResolvedType);
        auto *Return = llvm::cast<sema::ArrayType>(F->ReturnType->ResolvedType);
        // Different source positions, but the same literal size.
        REQUIRE(sema::sameType(Param, Return));
      });
  parseAndAnalyze(
      "fun f(a: int[3]) -> int[4] { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        auto *Param = llvm::cast<sema::ArrayType>(
            F->Parameters[0]->ParamType->ResolvedType);
        auto *Return = llvm::cast<sema::ArrayType>(F->ReturnType->ResolvedType);
        REQUIRE(!sema::sameType(Param, Return));
      });
}

TEST_CASE("Too many type arguments", "[sema]") {
  parseAndAnalyze(
      "class Box<T, U> {} fun f() -> Box<int, string, bool> { return null; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        checkError(
            Diags, 1, 1,
            "wrong number of type arguments for 'Box' (expected 2, got 3)");
        REQUIRE(F->ReturnType->ResolvedType->Kind == sema::TypeKind::Error);
      });
}

TEST_CASE("Names inside an over-counted type argument list are still resolved",
          "[sema]") {
  parseAndAnalyze(
      "class Box<T, U> {} var b: Box<int, string, nope>;",
      [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
        // The count error and the unresolved reference inside the extra
        // argument are both reported.
        auto D = Diags.getDiagnostics();
        REQUIRE(D.size() == 2);
        REQUIRE(D[0].getKind() == llvm::SourceMgr::DK_Error);
        REQUIRE(D[1].getKind() == llvm::SourceMgr::DK_Error);
        auto Contains = [&](llvm::StringRef Msg) {
          return D[0].getMessage().find(Msg) != llvm::StringRef::npos ||
                 D[1].getMessage().find(Msg) != llvm::StringRef::npos;
        };
        REQUIRE(Contains("wrong number of type arguments for 'Box'"));
        REQUIRE(Contains("unresolved reference 'nope'"));
      });
}

TEST_CASE("A missing default type argument", "[sema]") {
  parseAndAnalyze(
      "class Box<T, U = string> {} var b: Box;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *B = llvm::cast<VarDecl>(P->Objects[1]->Decl);
        checkError(Diags, 1, 1,
                   "type parameter 'T' of 'Box' has no default type");
        REQUIRE(B->VarType->ResolvedType->Kind == sema::TypeKind::Error);
      });
}

TEST_CASE("A type parameter cannot be instantiated", "[sema]") {
  parseAndAnalyze(
      "class Box<T> { var x: T<int>; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *Box = llvm::cast<ClassDecl>(P->Objects[0]->Decl);
        auto *X = llvm::cast<VarDecl>(Box->Members[0]);
        checkError(Diags, 1, 1, "type parameter 'T' cannot be instantiated");
        REQUIRE(X->VarType->ResolvedType->Kind == sema::TypeKind::Error);
      });
}

TEST_CASE("Unresolved types degrade to the error type", "[sema]") {
  parseAndAnalyze(
      "var a: Foo; var b: Foo;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto A = Diags.getDiagnostics();
        REQUIRE(A.size() == 2);
        for (auto &D : A) {
          REQUIRE(D.getKind() == llvm::SourceMgr::DK_Error);
          REQUIRE(D.getMessage().find("unresolved reference 'Foo'") !=
                  llvm::StringRef::npos);
        }
        auto *A1 = llvm::cast<VarDecl>(P->Objects[0]->Decl);
        auto *B1 = llvm::cast<VarDecl>(P->Objects[1]->Decl);
        REQUIRE(A1->VarType->ResolvedType->Kind == sema::TypeKind::Error);
        // Both failures share the cached error sentinel.
        REQUIRE(A1->VarType->ResolvedType == B1->VarType->ResolvedType);
      });
}

TEST_CASE("Cyclic type parameter defaults", "[sema]") {
  parseAndAnalyze(
      "class A<T = B> {} class B<U = A> {} var a: A;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *A = llvm::cast<VarDecl>(P->Objects[2]->Decl);
        // The cycle A -> B -> A is detected when resolving B's default (B is
        // already on the resolution chain); the error type then stops the
        // cascade, so exactly one diagnostic is reported.
        checkError(Diags, 1, 1, "cyclic type parameter default involving 'B'");
        REQUIRE(A->VarType->ResolvedType->Kind == sema::TypeKind::Error);
      });
}
