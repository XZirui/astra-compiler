#include "ParseHelper.h"

#include "astra/ast/ASTDumper.h"
#include "astra/ast/Program.h"
#include <catch2/catch_test_macros.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>

using namespace astra;
using namespace astra::ast;

TEST_CASE("Dump a simple function", "[dumper]") {
  test::parseSource("fun main() -> void {}", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- FunctionDecl 'main' [public]
      |- ReturnType
      | `- BuiltinType 'Void'
      `- Body
        `- Block
)");
  });
}

TEST_CASE("Dump declarations, statements and operators", "[dumper]") {
  test::parseSource(R"(fun main() -> void {
    var x: int = 1;
    x = x + 2;
})",
                    [](ASTContext &, Program *P) {
                      llvm::SmallString<256> Buf;
                      llvm::raw_svector_ostream OS(Buf);
                      dump(P, OS);
                      CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- FunctionDecl 'main' [public]
      |- ReturnType
      | `- BuiltinType 'Void'
      `- Body
        `- Block
          |- DeclStatement
          | `- Declaration
          |   `- VarDecl 'x' [public] [mutable]
          |     |- VarType
          |     | `- BuiltinType 'Int'
          |     `- Value
          |       `- IntLiteral '1'
          `- AssignmentStmt '='
            |- LHS
            | `- VarExpr 'x'
            `- RHS
              `- BinaryExpr '+'
                |- LHS
                | `- VarExpr 'x'
                `- RHS
                  `- IntLiteral '2'
)");
                    });
}

TEST_CASE("Dump calls with type arguments", "[dumper]") {
  test::parseSource("var r = foo<Int>(1);", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- VarDecl 'r' [public] [mutable]
      `- Value
        `- CallExpr '<>'
          |- Callee
          | `- VarExpr 'foo'
          |- TypeRef 'Int'
          `- IntLiteral '1'
)");
  });

  test::parseSource("var r = foo<>();", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    // The empty list is explicit: it forces the default type parameters.
    CHECK(OS.str().find("CallExpr '<>'") != llvm::StringRef::npos);
  });

  test::parseSource("var r = foo();", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    // No type argument list was written: no marker.
    CHECK(OS.str().find("CallExpr '<>'") == llvm::StringRef::npos);
  });
}

TEST_CASE("Dump explicit empty type argument lists", "[dumper]") {
  test::parseSource("fun f() -> Box<> {}", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("TypeRef 'Box' '<>'") != llvm::StringRef::npos);
  });

  test::parseSource("fun g() -> Box {}", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("TypeRef 'Box' '<>'") == llvm::StringRef::npos);
  });
}

TEST_CASE("Dump string and char literals", "[dumper]") {
  test::parseSource(R"(var s = "hi";)", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("StringLiteral 'hi'") != llvm::StringRef::npos);
  });

  test::parseSource(R"(var s = "a\nb";)", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    // The newline is escaped so the tree stays on one line.
    CHECK(OS.str().find("StringLiteral 'a\\nb'") != llvm::StringRef::npos);
  });

  test::parseSource("var c = 'a';", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("CharLiteral 'a'") != llvm::StringRef::npos);
  });

  test::parseSource(R"(var c = '\n';)", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("CharLiteral 10") != llvm::StringRef::npos);
  });

  test::parseSource(R"(var s = "\u0001";)", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    // Control characters dump as hex escapes so the tree stays on one line
    // per node.
    CHECK(OS.str().find("\\x01") != llvm::StringRef::npos);
  });

  test::parseSource(
      "fun f() -> char {} fun g() -> string {}", [](ASTContext &, Program *P) {
        llvm::SmallString<128> Buf;
        llvm::raw_svector_ostream OS(Buf);
        dump(P, OS);
        CHECK(OS.str().find("BuiltinType 'Char'") != llvm::StringRef::npos);
        CHECK(OS.str().find("BuiltinType 'String'") != llvm::StringRef::npos);
      });
}

TEST_CASE("Dump comparison chains", "[dumper]") {
  test::parseSource("var r = a < b < c;", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("ComparisonChainExpr '<' '<'") !=
          llvm::StringRef::npos);
  });
}

TEST_CASE("Dump a class declaration", "[dumper]") {
  test::parseSource("class Box<T = int> { var v = 0; fun f() -> void {} }",
                    [](ASTContext &, Program *P) {
                      llvm::SmallString<256> Buf;
                      llvm::raw_svector_ostream OS(Buf);
                      dump(P, OS);
                      CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- ClassDecl 'Box' [public]
      |- TypeParam 'T'
      | `- DefaultType
      |   `- BuiltinType 'Int'
      |- VarDecl 'v' [public] [mutable]
      | `- Value
      |   `- IntLiteral '0'
      `- FunctionDecl 'f' [public]
        |- ReturnType
        | `- BuiltinType 'Void'
        `- Body
          `- Block
)");
                    });

  test::parseSource("class Point {}", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str().find("ClassDecl 'Point'") != llvm::StringRef::npos);
  });
}

TEST_CASE("Dump visibility modifiers", "[dumper]") {
  test::parseSource(
      "class Foo { public var x: int; private val y = 1; protected fun f() -> "
      "void {} } private class Bar {}",
      [](ASTContext &, Program *P) {
        llvm::SmallString<256> Buf;
        llvm::raw_svector_ostream OS(Buf);
        dump(P, OS);
        CHECK(OS.str().find("VarDecl 'x' [public] [mutable]") !=
              llvm::StringRef::npos);
        CHECK(OS.str().find("VarDecl 'y' [private]") != llvm::StringRef::npos);
        CHECK(OS.str().find("FunctionDecl 'f' [protected]") !=
              llvm::StringRef::npos);
        CHECK(OS.str().find("ClassDecl 'Bar' [private]") !=
              llvm::StringRef::npos);
      });
}

TEST_CASE("Dump nullsafe member access and elvis", "[dumper]") {
  test::parseSource("var r = a?.b ?: c;", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- VarDecl 'r' [public] [mutable]
      `- Value
        `- BinaryExpr '?:'
          |- LHS
          | `- MemberExpr 'b' [nullsafe]
          |   `- Base
          |     `- VarExpr 'a'
          `- RHS
            `- VarExpr 'c'
)");
  });
}

TEST_CASE("Dump a try statement", "[dumper]") {
  test::parseSource(
      "fun f() -> void { try {} catch (e: Exception) {} finally {} }",
      [](ASTContext &, Program *P) {
        llvm::SmallString<256> Buf;
        llvm::raw_svector_ostream OS(Buf);
        dump(P, OS);
        CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- FunctionDecl 'f' [public]
      |- ReturnType
      | `- BuiltinType 'Void'
      `- Body
        `- Block
          `- TryStmt
            |- Body
            | `- Block
            |- CatchClause 'e'
            | |- Param
            | | `- Parameter 'e'
            | |   `- Type
            | |     `- TypeRef 'Exception'
            | `- Body
            |   `- Block
            `- Finally
              `- Block
)");
      });

  test::parseSource(
      "fun f() -> void { try {} catch (e: A) {} catch (g: B) {} }",
      [](ASTContext &, Program *P) {
        llvm::SmallString<128> Buf;
        llvm::raw_svector_ostream OS(Buf);
        dump(P, OS);
        CHECK(OS.str().find("CatchClause 'e'") != llvm::StringRef::npos);
        CHECK(OS.str().find("CatchClause 'g'") != llvm::StringRef::npos);
        // No `finally` block was written: no slot appears.
        CHECK(OS.str().find("Finally") == llvm::StringRef::npos);
      });
}
