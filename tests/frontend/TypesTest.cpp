#include "ParseHelper.h"

#include "astra/ast/Declaration.h"

#include <llvm/Support/Casting.h>

#include <utility>

using namespace astra::ast;
using namespace astra;

TEST_CASE("Builtin type keywords", "[types]") {
  const std::pair<llvm::StringRef, BuiltinType::Ty> Types[] = {
      {"void", BuiltinType::Void},   {"bool", BuiltinType::Bool},
      {"int", BuiltinType::Int},     {"long", BuiltinType::Long},
      {"float", BuiltinType::Float}, {"double", BuiltinType::Double},
      {"char", BuiltinType::Char},   {"string", BuiltinType::String},
  };
  for (auto [Keyword, Expected] : Types) {
    test::parseSource(
        ("fun f() -> " + Keyword + " {}").str(),
        [Expected](ASTContext &, Program *P) {
          auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
          REQUIRE(Fn->ReturnType->getKind() == NodeKind::BuiltinType);
          auto *Ty = llvm::cast<BuiltinType>(Fn->ReturnType);
          REQUIRE(Ty->Value == Expected);
        });
  }
}

TEST_CASE("User type reference", "[types]") {
  test::parseSource("fun f() -> Foo {}", [](ASTContext &Ctx, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    REQUIRE(Fn->ReturnType->getKind() == NodeKind::TypeRef);
    auto *Ty = llvm::cast<TypeRef>(Fn->ReturnType);
    REQUIRE(Ty->Name == Ctx.getIdentifier("Foo"));
  });
}

TEST_CASE("Array types", "[types]") {
  test::parseSource("fun f() -> int[3] {}", [](ASTContext &, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    REQUIRE(Fn->ReturnType->getKind() == NodeKind::ArrayType);
    auto *Arr = llvm::cast<ArrayType>(Fn->ReturnType);
    REQUIRE(Arr->ElementType->getKind() == NodeKind::BuiltinType);
    REQUIRE(llvm::cast<BuiltinType>(Arr->ElementType)->Value ==
            BuiltinType::Int);
    REQUIRE(Arr->Size->getKind() == NodeKind::IntLiteral);
    auto *Size = llvm::cast<IntLiteral>(Arr->Size);
    REQUIRE(Size->Value.getSExtValue() == 3);
  });

  test::parseSource("fun f() -> int[2][3] {}", [](ASTContext &, Program *P) {
    // The builder wraps left to right: `int[2][3]` -> `ArrayType(int[2], 3)`.
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    auto *Outer = llvm::cast<ArrayType>(Fn->ReturnType);
    REQUIRE(Outer->Size->getKind() == NodeKind::IntLiteral);
    REQUIRE(llvm::cast<IntLiteral>(Outer->Size)->Value.getSExtValue() == 3);
    REQUIRE(Outer->ElementType->getKind() == NodeKind::ArrayType);
    auto *Inner = llvm::cast<ArrayType>(Outer->ElementType);
    REQUIRE(llvm::cast<IntLiteral>(Inner->Size)->Value.getSExtValue() == 2);
    REQUIRE(Inner->ElementType->getKind() == NodeKind::BuiltinType);
    REQUIRE(llvm::cast<BuiltinType>(Inner->ElementType)->Value ==
            BuiltinType::Int);
  });
}

TEST_CASE("Function types", "[types]") {
  test::parseSource(
      "fun f() -> fun (int, bool) -> double {}", [](ASTContext &, Program *P) {
        auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
        REQUIRE(Fn->ReturnType->getKind() == NodeKind::FunctionType);
        auto *Ty = llvm::cast<FunctionType>(Fn->ReturnType);
        REQUIRE(Ty->Parameters.size() == 2);
        REQUIRE(Ty->Parameters[0]->getKind() == NodeKind::BuiltinType);
        REQUIRE(llvm::cast<BuiltinType>(Ty->Parameters[0])->Value ==
                BuiltinType::Int);
        REQUIRE(llvm::cast<BuiltinType>(Ty->Parameters[1])->Value ==
                BuiltinType::Bool);
        REQUIRE(Ty->ReturnType->getKind() == NodeKind::BuiltinType);
        REQUIRE(llvm::cast<BuiltinType>(Ty->ReturnType)->Value ==
                BuiltinType::Double);
      });

  test::parseSource("fun g() -> fun () -> int {}", [](ASTContext &,
                                                      Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    auto *Ty = llvm::cast<FunctionType>(Fn->ReturnType);
    REQUIRE(Ty->Parameters.empty());
    REQUIRE(llvm::cast<BuiltinType>(Ty->ReturnType)->Value == BuiltinType::Int);
  });
}

TEST_CASE("Generic type references", "[types]") {
  test::parseSource("fun f() -> Box<Int> {}", [](ASTContext &Ctx, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    auto *Ty = llvm::cast<TypeRef>(Fn->ReturnType);
    REQUIRE(Ty->getKind() == NodeKind::TypeRef);
    REQUIRE(Ty->Name == Ctx.getIdentifier("Box"));
    REQUIRE(Ty->TypeArgs.size() == 1);
    REQUIRE(Ty->TypeArgs[0]->getKind() == NodeKind::TypeRef);
    REQUIRE(llvm::cast<TypeRef>(Ty->TypeArgs[0])->Name ==
            Ctx.getIdentifier("Int"));
    REQUIRE(Ty->ExplicitTypeArgs);
  });

  test::parseSource("fun h() -> Box<> {}", [](ASTContext &Ctx, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    auto *Ty = llvm::cast<TypeRef>(Fn->ReturnType);
    REQUIRE(Ty->Name == Ctx.getIdentifier("Box"));
    REQUIRE(Ty->TypeArgs.empty());
    // `<>` is empty but explicit: it forces the default type parameters.
    REQUIRE(Ty->ExplicitTypeArgs);
  });

  test::parseSource("fun i() -> Box {}", [](ASTContext &Ctx, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    auto *Ty = llvm::cast<TypeRef>(Fn->ReturnType);
    REQUIRE(Ty->Name == Ctx.getIdentifier("Box"));
    REQUIRE(Ty->TypeArgs.empty());
    REQUIRE(!Ty->ExplicitTypeArgs);
  });

  test::parseSource(
      "fun g() -> Box<Box<Int>> {}", [](ASTContext &Ctx, Program *P) {
        auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
        auto *Outer = llvm::cast<TypeRef>(Fn->ReturnType);
        REQUIRE(Outer->TypeArgs.size() == 1);
        auto *Inner = llvm::cast<TypeRef>(Outer->TypeArgs[0]);
        REQUIRE(Inner->Name == Ctx.getIdentifier("Box"));
        REQUIRE(Inner->TypeArgs.size() == 1);
        REQUIRE(Inner->TypeArgs[0]->getKind() == NodeKind::TypeRef);
        REQUIRE(llvm::cast<TypeRef>(Inner->TypeArgs[0])->Name ==
                Ctx.getIdentifier("Int"));
      });
}

TEST_CASE("Parenthesized types are transparent", "[types]") {
  test::parseSource("fun f() -> (int) {}", [](ASTContext &, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    REQUIRE(Fn->ReturnType->getKind() == NodeKind::BuiltinType);
    REQUIRE(llvm::cast<BuiltinType>(Fn->ReturnType)->Value == BuiltinType::Int);
  });
}
