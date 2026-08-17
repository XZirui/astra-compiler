#include "ParseHelper.h"

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

TEST_CASE("Empty type argument lists are accepted", "[diag]") {
  test::parseSource("var x = foo<>(); fun f() -> Box<> {}",
                    [](ASTContext &, Program *) {
                      // An empty argument list is allowed on purpose: default
                      // type parameters will fill it in, so there is no
                      // diagnostic.
                    });
}

TEST_CASE("Bare type arguments are a syntax error", "[diag]") {
  test::parseSourceWithDiags(
      "var x = foo<int>;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // Type arguments are only allowed directly before `(`.
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All[0].getMessage().find("no viable alternative") !=
                llvm::StringRef::npos);
      });
}

TEST_CASE("Unterminated string literals are lexer errors", "[diag]") {
  test::parseSourceWithDiags(
      "var s = \"abc;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // The lexer reports the unterminated string; error recovery then
        // makes the parser report the missing expression after `=`. The
        // program is not built.
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 2);
        REQUIRE(All[0].getMessage().find("token recognition") !=
                llvm::StringRef::npos);
        REQUIRE(All[1].getMessage().find("mismatched input") !=
                llvm::StringRef::npos);
      });
}

TEST_CASE("Empty char literals are rejected", "[diag]") {
  test::parseSourceWithDiags(
      "var c = '';",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
      });
}

TEST_CASE("Whitespace between '?' and '.' is a syntax error", "[diag]") {
  test::parseSourceWithDiags(
      "var r = a ? .b;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // `?.` is a single token; a separated `?` cannot start a suffix.
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
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

TEST_CASE("Diagnostics print style", "[diag]") {
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
