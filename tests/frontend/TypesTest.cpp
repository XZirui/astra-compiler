#include "ParseHelper.h"

#include "astra/ast/Declaration.h"

#include <utility>

using namespace astra::ast;
using namespace astra;

TEST_CASE("Builtin type keywords", "[types]") {
  const std::pair<llvm::StringRef, BuiltinType::Ty> Types[] = {
      {"void", BuiltinType::Void},   {"bool", BuiltinType::Bool},
      {"int", BuiltinType::Int},     {"long", BuiltinType::Long},
      {"float", BuiltinType::Float}, {"double", BuiltinType::Double},
  };
  for (auto [Keyword, Expected] : Types) {
    test::parseSource(
        ("fun f() -> " + Keyword + " {}").str(),
        [Expected](ASTContext &, Program *P) {
          auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
          REQUIRE(Fn->ReturnType->getKind() == NodeKind::BuiltinType);
          auto *Ty = static_cast<BuiltinType *>(Fn->ReturnType);
          REQUIRE(Ty->Type == Expected);
        });
  }
}

TEST_CASE("User type reference", "[types]") {
  test::parseSource("fun f() -> Foo {}", [](ASTContext &Ctx, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    REQUIRE(Fn->ReturnType->getKind() == NodeKind::TypeRef);
    auto *Ty = static_cast<TypeRef *>(Fn->ReturnType);
    REQUIRE(Ty->Name == Ctx.getIdentifier("Foo"));
  });
}

TEST_CASE("Array types", "[types]") {
  test::parseSource("fun f() -> int[3] {}", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    REQUIRE(Fn->ReturnType->getKind() == NodeKind::ArrayType);
    auto *Arr = static_cast<ArrayType *>(Fn->ReturnType);
    REQUIRE(Arr->ElementType->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(Arr->ElementType)->Type ==
            BuiltinType::Int);
    REQUIRE(Arr->Size->getKind() == NodeKind::IntLiteral);
    auto *Size = static_cast<IntLiteral *>(Arr->Size);
    REQUIRE(Size->Value.getSExtValue() == 3);
  });

  test::parseSource("fun f() -> int[2][3] {}", [](ASTContext &, Program *P) {
    // The builder wraps left to right: `int[2][3]` -> `ArrayType(int[2], 3)`.
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *Outer = static_cast<ArrayType *>(Fn->ReturnType);
    REQUIRE(Outer->Size->getKind() == NodeKind::IntLiteral);
    REQUIRE(static_cast<IntLiteral *>(Outer->Size)->Value.getSExtValue() == 3);
    REQUIRE(Outer->ElementType->getKind() == NodeKind::ArrayType);
    auto *Inner = static_cast<ArrayType *>(Outer->ElementType);
    REQUIRE(static_cast<IntLiteral *>(Inner->Size)->Value.getSExtValue() == 2);
    REQUIRE(Inner->ElementType->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(Inner->ElementType)->Type ==
            BuiltinType::Int);
  });
}

TEST_CASE("Function types", "[types]") {
  test::parseSource(
      "fun f() -> fun (int, bool) -> double {}", [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        REQUIRE(Fn->ReturnType->getKind() == NodeKind::FunctionType);
        auto *Ty = static_cast<FunctionType *>(Fn->ReturnType);
        REQUIRE(Ty->Parameters.size() == 2);
        REQUIRE(Ty->Parameters[0]->getKind() == NodeKind::BuiltinType);
        REQUIRE(static_cast<BuiltinType *>(Ty->Parameters[0])->Type ==
                BuiltinType::Int);
        REQUIRE(static_cast<BuiltinType *>(Ty->Parameters[1])->Type ==
                BuiltinType::Bool);
        REQUIRE(Ty->ReturnType->getKind() == NodeKind::BuiltinType);
        REQUIRE(static_cast<BuiltinType *>(Ty->ReturnType)->Type ==
                BuiltinType::Double);
      });

  test::parseSource("fun g() -> fun () -> int {}", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *Ty = static_cast<FunctionType *>(Fn->ReturnType);
    REQUIRE(Ty->Parameters.empty());
    REQUIRE(static_cast<BuiltinType *>(Ty->ReturnType)->Type ==
            BuiltinType::Int);
  });
}

TEST_CASE("Parenthesized types are transparent", "[types]") {
  test::parseSource("fun f() -> (int) {}", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    REQUIRE(Fn->ReturnType->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(Fn->ReturnType)->Type ==
            BuiltinType::Int);
  });
}
