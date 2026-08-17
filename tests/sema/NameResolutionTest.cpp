#include "astra/sema/Analyze.h"

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

/// The expression of the first statement of a function body.
Expr *firstExpr(FunctionDecl *F) {
  auto *Stmt = F->Body->Statements[0];
  return llvm::cast<ExprStmt>(Stmt)->Expression;
}

/// The `VarExpr` of the first `return X;` statement of a function body.
VarExpr *returnedVar(FunctionDecl *F) {
  auto *Ret = llvm::cast<ReturnExpr>(firstExpr(F));
  return llvm::cast<VarExpr>(Ret->Value);
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

/// Assert a redeclaration pair: an error at `ErrorLine` and a note pointing
/// at the previous declaration at `NoteLine`.
void checkRedeclaration(basic::DiagnosticsEngine &Diags, int ErrorLine,
                        int NoteLine, llvm::StringRef Name) {
  REQUIRE(Diags.hasErrors());
  auto D = Diags.getDiagnostics();
  REQUIRE(D.size() == 2);
  REQUIRE(D[0].getKind() == llvm::SourceMgr::DK_Error);
  REQUIRE(D[0].getLineNo() == ErrorLine);
  REQUIRE(D[0].getMessage().find("redeclaration of '") !=
          llvm::StringRef::npos);
  REQUIRE(D[0].getMessage().find(Name) != llvm::StringRef::npos);
  REQUIRE(D[1].getKind() == llvm::SourceMgr::DK_Note);
  REQUIRE(D[1].getLineNo() == NoteLine);
  REQUIRE(D[1].getMessage() == "previous declaration is here");
}

} // namespace

TEST_CASE("A reference to a top-level variable resolves", "[sema]") {
  parseAndAnalyze(
      "var x: int; fun f() -> int { return x; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *Var = P->Objects[0]->Decl;
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        REQUIRE(returnedVar(F)->Decl == Var);
      });
}

TEST_CASE("A function call resolves to the function declaration", "[sema]") {
  parseAndAnalyze(
      "fun f() -> int { return 1; } fun g() -> int { return f(); }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
        auto *G = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Call =
            llvm::cast<CallExpr>(llvm::cast<ReturnExpr>(firstExpr(G))->Value);
        REQUIRE(llvm::cast<VarExpr>(Call->Callee)->Decl == F);
      });
}

TEST_CASE("A parameter reference resolves to the parameter", "[sema]") {
  parseAndAnalyze(
      "fun f(x: int) -> int { return x; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        REQUIRE(returnedVar(F)->Decl == F->Parameters[0]);
      });
}

TEST_CASE("A local variable shadows a parameter", "[sema]") {
  parseAndAnalyze(
      "fun f(x: int) -> int { var x = 2; return x; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        auto *Local = llvm::cast<DeclStatement>(F->Body->Statements[0])->Decl;
        auto *Ret = llvm::cast<ReturnExpr>(
            llvm::cast<ExprStmt>(F->Body->Statements[1])->Expression);
        // The parameter is declared in the function scope, the local in the
        // block scope; shadowing is legal across the two.
        REQUIRE(llvm::cast<VarExpr>(Ret->Value)->Decl == Local);
      });
}

TEST_CASE("Top-level forward references resolve", "[sema]") {
  parseAndAnalyze(
      "fun a() -> int { return b(); } fun b() -> int { return 1; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *B = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *A = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
        auto *Call =
            llvm::cast<CallExpr>(llvm::cast<ReturnExpr>(firstExpr(A))->Value);
        REQUIRE(llvm::cast<VarExpr>(Call->Callee)->Decl == B);
      });
  parseAndAnalyze("val a = b; val b = 1;", [](ASTContext &, Program *P,
                                              basic::DiagnosticsEngine &Diags) {
    REQUIRE(!Diags.hasErrors());
    auto *B = llvm::cast<VarDecl>(P->Objects[1]->Decl);
    auto *A = llvm::cast<VarDecl>(P->Objects[0]->Decl);
    REQUIRE(llvm::cast<VarExpr>(A->Value)->Decl == B);
  });
}

TEST_CASE("The loop variable of a for-each is bound in the loop body",
          "[sema]") {
  parseAndAnalyze(
      "var xs = 1; fun f() -> int { var s = 0; for (i in xs) { s = i; } "
      "return s; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *Xs = P->Objects[0]->Decl;
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *SDecl = llvm::cast<DeclStatement>(F->Body->Statements[0])->Decl;
        auto *For = llvm::cast<ForEachStmt>(F->Body->Statements[1]);
        REQUIRE(llvm::cast<VarExpr>(For->Scope)->Decl == Xs);
        auto *Assign = llvm::cast<AssignmentStmt>(For->Body->Statements[0]);
        REQUIRE(llvm::cast<VarExpr>(Assign->LHS)->Decl == SDecl);
        REQUIRE(llvm::cast<VarExpr>(Assign->RHS)->Decl == For);
      });
}

TEST_CASE("A catch parameter is bound in its clause", "[sema]") {
  parseAndAnalyze(
      "fun f() -> int { try { return 1; } catch (e: int) { return e; } }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = firstFunction(P);
        auto *Try = llvm::cast<TryStmt>(F->Body->Statements[0]);
        auto *CC = Try->CatchClauses[0];
        auto *Ret = llvm::cast<ReturnExpr>(
            llvm::cast<ExprStmt>(CC->Body->Statements[0])->Expression);
        REQUIRE(llvm::cast<VarExpr>(Ret->Value)->Decl == CC->Param);
      });
}

TEST_CASE("Class members reference each other", "[sema]") {
  parseAndAnalyze(
      "class C { var p: int = 1; fun f() -> int { return p; } fun g() -> "
      "int { return f(); } }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *C = llvm::cast<ClassDecl>(P->Objects[0]->Decl);
        auto *Prop = C->Members[0];
        auto *F = llvm::cast<FunctionDecl>(C->Members[1]);
        auto *G = llvm::cast<FunctionDecl>(C->Members[2]);
        REQUIRE(returnedVar(F)->Decl == Prop);
        auto *Call =
            llvm::cast<CallExpr>(llvm::cast<ReturnExpr>(firstExpr(G))->Value);
        REQUIRE(llvm::cast<VarExpr>(Call->Callee)->Decl == F);
      });
}

TEST_CASE("A nested class can access outer-class members", "[sema]") {
  parseAndAnalyze(
      "class C { fun o() -> int { return 1; } class N { fun f() -> int { "
      "return o(); } } }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *C = llvm::cast<ClassDecl>(P->Objects[0]->Decl);
        auto *O = C->Members[0];
        auto *N = llvm::cast<ClassDecl>(C->Members[1]);
        auto *F = llvm::cast<FunctionDecl>(N->Members[0]);
        auto *Call =
            llvm::cast<CallExpr>(llvm::cast<ReturnExpr>(firstExpr(F))->Value);
        REQUIRE(llvm::cast<VarExpr>(Call->Callee)->Decl == O);
      });
}

TEST_CASE("this resolves to the enclosing class", "[sema]") {
  parseAndAnalyze(
      "class C { var p: int = 1; fun f() -> int { return this.p; } }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *C = llvm::cast<ClassDecl>(P->Objects[0]->Decl);
        auto *F = llvm::cast<FunctionDecl>(C->Members[1]);
        auto *Member =
            llvm::cast<MemberExpr>(llvm::cast<ReturnExpr>(firstExpr(F))->Value);
        REQUIRE(llvm::cast<ThisExpr>(Member->Base)->EnclosingClass == C);
      });
}

TEST_CASE("Class, function and variable namespaces coexist", "[sema]") {
  parseAndAnalyze("fun f() -> int { return 1; } var f = 2;",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    REQUIRE(!Diags.hasErrors());
                  });
  parseAndAnalyze("class C { var x: int; fun x() -> int { return 1; } }",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    REQUIRE(!Diags.hasErrors());
                  });
  parseAndAnalyze("class C {} fun C() -> int { return 1; }",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    REQUIRE(!Diags.hasErrors());
                  });
}

TEST_CASE("Redeclaration of a top-level variable", "[sema]") {
  parseAndAnalyze("var x = 1;\nvar x = 2;",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkRedeclaration(Diags, 2, 1, "x");
                  });
}

TEST_CASE("Redeclaration of a top-level function", "[sema]") {
  parseAndAnalyze("fun f() -> int { return 1; }\nfun f() -> int { return 2; }",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkRedeclaration(Diags, 2, 1, "f");
                  });
}

TEST_CASE("Redeclaration of a parameter", "[sema]") {
  parseAndAnalyze("fun f(a: int, a: int) -> int { return a; }",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkRedeclaration(Diags, 1, 1, "a");
                  });
}

TEST_CASE("Redeclaration of class members", "[sema]") {
  parseAndAnalyze(
      "class C { fun f() -> int { return 1; } fun f() -> int { return 2; } }",
      [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
        checkRedeclaration(Diags, 1, 1, "f");
      });
  parseAndAnalyze("class C { var p: int; var p: int; }",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkRedeclaration(Diags, 1, 1, "p");
                  });
}

TEST_CASE("Redeclaration inside a block", "[sema]") {
  parseAndAnalyze("fun f() -> int { var x = 1;\nvar x = 2; return x; }",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkRedeclaration(Diags, 2, 1, "x");
                  });
}

TEST_CASE("An unresolved name reference reports an error", "[sema]") {
  parseAndAnalyze(
      "fun f() -> int { return nope; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *F = firstFunction(P);
        checkError(Diags, 1, 1, "unresolved reference 'nope'");
        REQUIRE(returnedVar(F)->Decl == nullptr);
      });
}

TEST_CASE("A function name is not usable as a value", "[sema]") {
  parseAndAnalyze("fun f() -> int { return 1; } val g = f;",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkError(Diags, 1, 1, "unresolved reference 'f'");
                  });
}

TEST_CASE("Non-public top-level declarations stay accessible in the file",
          "[sema]") {
  // There is no import mechanism yet, so visibility modifiers on top-level
  // declarations do not restrict references within the same file.
  parseAndAnalyze(
      "private fun f() -> int { return 1; } fun g() -> int { return f(); }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *F = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
        auto *G = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        auto *Call =
            llvm::cast<CallExpr>(llvm::cast<ReturnExpr>(firstExpr(G))->Value);
        REQUIRE(llvm::cast<VarExpr>(Call->Callee)->Decl == F);
      });
  parseAndAnalyze(
      "private var x = 1; fun g() -> int { return x; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(!Diags.hasErrors());
        auto *X = P->Objects[0]->Decl;
        auto *G = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        REQUIRE(returnedVar(G)->Decl == X);
      });
  parseAndAnalyze("private class C {} var c: C;",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    REQUIRE(!Diags.hasErrors());
                  });
}

TEST_CASE("this is rejected outside a class member", "[sema]") {
  parseAndAnalyze(
      "fun f() -> int { return this.x; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *F = firstFunction(P);
        checkError(Diags, 1, 1, "'this' is not allowed outside a class member");
        auto *Member =
            llvm::cast<MemberExpr>(llvm::cast<ReturnExpr>(firstExpr(F))->Value);
        REQUIRE(llvm::cast<ThisExpr>(Member->Base)->EnclosingClass == nullptr);
      });
}

TEST_CASE("A loop variable is not visible outside its loop", "[sema]") {
  parseAndAnalyze(
      "var xs = 1; fun f() -> int { for (i in xs) { } return i; }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        auto *F = llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
        checkError(Diags, 1, 1, "unresolved reference 'i'");
        auto *Ret = llvm::cast<ReturnExpr>(
            llvm::cast<ExprStmt>(F->Body->Statements[1])->Expression);
        REQUIRE(llvm::cast<VarExpr>(Ret->Value)->Decl == nullptr);
      });
}

TEST_CASE("Redeclaration of a type parameter", "[sema]") {
  parseAndAnalyze("class Box<T, T> {}",
                  [](ASTContext &, Program *, basic::DiagnosticsEngine &Diags) {
                    checkRedeclaration(Diags, 1, 1, "T");
                  });
}
