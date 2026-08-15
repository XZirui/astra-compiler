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
    `- FunctionDecl 'main'
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
    `- FunctionDecl 'main'
      |- ReturnType
      | `- BuiltinType 'Void'
      `- Body
        `- Block
          |- DeclStatement
          | `- Declaration
          |   `- VarDecl 'x' [mutable]
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

TEST_CASE("Dump nullsafe member access and elvis", "[dumper]") {
  test::parseSource("var r = a?.b ?: c;", [](ASTContext &, Program *P) {
    llvm::SmallString<128> Buf;
    llvm::raw_svector_ostream OS(Buf);
    dump(P, OS);
    CHECK(OS.str() == R"(Program
`- TopLevelObject
  `- Decl
    `- VarDecl 'r' [mutable]
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
