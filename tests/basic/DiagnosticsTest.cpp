#include "ParseHelper.h"

#include "astra/ast/Declaration.h"
#include "astra/ast/Expression.h"
#include "astra/basic/DiagnosticsEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>

using namespace astra;
using namespace astra::ast;

TEST_CASE("Syntax errors are reported with source locations", "[diag]") {
  // `fun f() {` is missing the `-> type block` part of a function decl.
  test::parseSourceWithDiags("fun f() {", [](ASTContext &, Program *P,
                                             basic::DiagnosticsEngine &Diags) {
    REQUIRE(P == nullptr);
    REQUIRE(Diags.hasErrors());
    const auto &All = Diags.getDiagnostics();
    REQUIRE(All.size() == 1);
    REQUIRE(All[0].getKind() == llvm::SourceMgr::DK_Error);
    REQUIRE(All[0].getLineNo() == 1);
    REQUIRE(All[0].getColumnNo() > 0);
    REQUIRE(All[0].getMessage().find("mismatched input") !=
            llvm::StringRef::npos);
  });
}

TEST_CASE("Lexer errors are reported", "[diag]") {
  test::parseSourceWithDiags("var x = $;", [](ASTContext &, Program *P,
                                              basic::DiagnosticsEngine &Diags) {
    REQUIRE(P == nullptr);
    REQUIRE(Diags.hasErrors());
    const auto &All = Diags.getDiagnostics();
    // The lexer reports the illegal character and skips it.
    // Error recovery then makes the parser report the missing
    // expression after `=`.
    REQUIRE(All.size() == 2);
    REQUIRE(All[0].getMessage().find("token recognition") !=
            llvm::StringRef::npos);
    REQUIRE(All[1].getMessage().find("mismatched input") !=
            llvm::StringRef::npos);
  });
}

TEST_CASE("Unimplemented constructs report a diagnostic instead of crashing",
          "[diag]") {
  test::parseSourceWithDiags(
      "var x = foo<int>;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // No syntax error: the parse tree is valid, so
        // the builder runs and reports the gap.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("not implemented") !=
                llvm::StringRef::npos);

        auto *Var = static_cast<VarDecl *>(P->Objects[0]->Decl);
        REQUIRE(Var->Value->getKind() == NodeKind::VarExpr);
      });
}

TEST_CASE("Multiple errors are collected", "[diag]") {
  test::parseSourceWithDiags(
      "var a = ; var b = ;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
        REQUIRE(Diags.getDiagnostics().size() == 2);
      });
}

TEST_CASE("Diagnostics print in clang style", "[diag]") {
  test::parseSourceWithDiags("fun f() {", [](ASTContext &, Program *,
                                             basic::DiagnosticsEngine &Diags) {
    llvm::SmallString<256> Buf;
    llvm::raw_svector_ostream OS(Buf);
    Diags.print(OS);
    // PrintMessage format: `<file>:<line>:<col>: error: <message>`.
    REQUIRE(OS.str().find("<test>") != llvm::StringRef::npos);
    REQUIRE(OS.str().find("error:") != llvm::StringRef::npos);
    REQUIRE(OS.str().find("mismatched input") != llvm::StringRef::npos);
  });
}
