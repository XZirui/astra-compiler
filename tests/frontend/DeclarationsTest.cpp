#include "ParseHelper.h"

#include "astra/ast/Declaration.h"
#include "astra/ast/Expression.h"
#include "astra/ast/Statement.h"
#include "astra/ast/Type.h"

using namespace astra::ast;
using namespace astra;

TEST_CASE("Function declaration with parameters", "[declarations]") {
  test::parseSource(
      "fun add(a: int, b: int) -> int { return a + b; }",
      [](ASTContext &Ctx, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        REQUIRE(Fn->Name == Ctx.getIdentifier("add"));

        REQUIRE(Fn->Parameters.size() == 2);
        auto *A = Fn->Parameters[0];
        REQUIRE(A->Name == Ctx.getIdentifier("a"));
        REQUIRE(A->Type->getKind() == NodeKind::BuiltinType);
        REQUIRE(static_cast<BuiltinType *>(A->Type)->Type == BuiltinType::Int);
        REQUIRE(A->DefaultValue == nullptr);
        auto *B = Fn->Parameters[1];
        REQUIRE(B->Name == Ctx.getIdentifier("b"));

        REQUIRE(Fn->ReturnType->getKind() == NodeKind::BuiltinType);
        REQUIRE(static_cast<BuiltinType *>(Fn->ReturnType)->Type ==
                BuiltinType::Int);

        REQUIRE(Fn->Body != nullptr);
        REQUIRE(Fn->Body->Statements.size() == 1);
      });
}

TEST_CASE("Function with no parameters and empty body", "[declarations]") {
  test::parseSource("fun f() -> void {}", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    REQUIRE(Fn->Parameters.empty());
    REQUIRE(static_cast<BuiltinType *>(Fn->ReturnType)->Type ==
            BuiltinType::Void);
    REQUIRE(Fn->Body->Statements.empty());
  });
}

TEST_CASE("Parameter default values", "[declarations]") {
  test::parseSource("fun f(a: int = 1, b: int) -> void {}", [](ASTContext &Ctx,
                                                               Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *A = Fn->Parameters[0];
    REQUIRE(A->Name == Ctx.getIdentifier("a"));
    REQUIRE(A->DefaultValue != nullptr);
    REQUIRE(A->DefaultValue->getKind() == NodeKind::IntLiteral);
    REQUIRE(static_cast<IntLiteral *>(A->DefaultValue)->Value.getSExtValue() ==
            1);
    auto *B = Fn->Parameters[1];
    REQUIRE(B->Name == Ctx.getIdentifier("b"));
    REQUIRE(B->DefaultValue == nullptr);
  });
}

TEST_CASE("Variable declarations", "[declarations]") {
  test::parseSource("var x: int = 5; val y = 3; var z: int;", [](ASTContext &,
                                                                 Program *P) {
    auto *X = static_cast<VarDecl *>(P->Objects[0]->Decl);
    REQUIRE(X->IsMutable);
    REQUIRE(X->VarType->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(X->VarType)->Type == BuiltinType::Int);
    REQUIRE(static_cast<IntLiteral *>(X->Value)->Value.getSExtValue() == 5);

    auto *Y = static_cast<VarDecl *>(P->Objects[1]->Decl);
    REQUIRE(!Y->IsMutable);
    REQUIRE(Y->VarType == nullptr);
    REQUIRE(static_cast<IntLiteral *>(Y->Value)->Value.getSExtValue() == 3);

    auto *Z = static_cast<VarDecl *>(P->Objects[2]->Decl);
    REQUIRE(Z->IsMutable);
    REQUIRE(Z->VarType->getKind() == NodeKind::BuiltinType);
    REQUIRE(Z->Value == nullptr);
  });
}

TEST_CASE("Declarations inside a block are wrapped", "[declarations]") {
  test::parseSource(
      "fun f() -> void { var x: int; }", [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        REQUIRE(Fn->Body->Statements.size() == 1);
        auto *Stmt = Fn->Body->Statements[0];
        REQUIRE(Stmt->getKind() == NodeKind::DeclStatement);
        auto *Decl = static_cast<DeclStatement *>(Stmt)->Declaration;
        REQUIRE(Decl->getKind() == NodeKind::VarDecl);
        REQUIRE(static_cast<VarDecl *>(Decl)->Name->getName() == "x");
      });
}

TEST_CASE("Identifiers are interned", "[declarations]") {
  test::parseSource("fun foo() -> void {} fun foo() -> void {}",
                    [](ASTContext &Ctx, Program *P) {
                      auto *First =
                          static_cast<FunctionDecl *>(P->Objects[0]->Decl);
                      auto *Second =
                          static_cast<FunctionDecl *>(P->Objects[1]->Decl);
                      REQUIRE(First->Name == Ctx.getIdentifier("foo"));
                      REQUIRE(Second->Name == Ctx.getIdentifier("foo"));
                      REQUIRE(First->Name == Second->Name);
                    });
}
