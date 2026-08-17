#include "ParseHelper.h"

#include "astra/ast/Declaration.h"
#include "astra/ast/Expression.h"
#include "astra/ast/Statement.h"
#include "astra/ast/Type.h"

#include <llvm/Support/Casting.h>

using namespace astra::ast;
using namespace astra;

TEST_CASE("Function declaration with parameters", "[declarations]") {
  test::parseSource("fun add(a: int, b: int) -> int { return a + b; }",
                    [](ASTContext &Ctx, Program *P) {
                      auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
                      REQUIRE(Fn->Name == Ctx.getIdentifier("add"));

                      REQUIRE(Fn->Parameters.size() == 2);
                      auto *A = Fn->Parameters[0];
                      REQUIRE(A->Name == Ctx.getIdentifier("a"));
                      REQUIRE(A->ParamType->getKind() == NodeKind::BuiltinType);
                      REQUIRE(llvm::cast<BuiltinType>(A->ParamType)->Value ==
                              BuiltinType::Int);
                      REQUIRE(A->DefaultValue == nullptr);
                      auto *B = Fn->Parameters[1];
                      REQUIRE(B->Name == Ctx.getIdentifier("b"));

                      REQUIRE(Fn->ReturnType->getKind() ==
                              NodeKind::BuiltinType);
                      REQUIRE(llvm::cast<BuiltinType>(Fn->ReturnType)->Value ==
                              BuiltinType::Int);

                      REQUIRE(Fn->Body != nullptr);
                      REQUIRE(Fn->Body->Statements.size() == 1);
                    });
}

TEST_CASE("Function with no parameters and empty body", "[declarations]") {
  test::parseSource("fun f() -> void {}", [](ASTContext &, Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    REQUIRE(Fn->Parameters.empty());
    REQUIRE(llvm::cast<BuiltinType>(Fn->ReturnType)->Value ==
            BuiltinType::Void);
    REQUIRE(Fn->Body->Statements.empty());
  });
}

TEST_CASE("Parameter default values", "[declarations]") {
  test::parseSource("fun f(a: int = 1, b: int) -> void {}", [](ASTContext &Ctx,
                                                               Program *P) {
    auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
    auto *A = Fn->Parameters[0];
    REQUIRE(A->Name == Ctx.getIdentifier("a"));
    REQUIRE(A->DefaultValue != nullptr);
    REQUIRE(A->DefaultValue->getKind() == NodeKind::IntLiteral);
    REQUIRE(llvm::cast<IntLiteral>(A->DefaultValue)->Value.getSExtValue() == 1);
    auto *B = Fn->Parameters[1];
    REQUIRE(B->Name == Ctx.getIdentifier("b"));
    REQUIRE(B->DefaultValue == nullptr);
  });
}

TEST_CASE("Variable declarations", "[declarations]") {
  test::parseSource(
      "var x: int = 5; val y = 3; var z: int;", [](ASTContext &, Program *P) {
        auto *X = llvm::cast<VarDecl>(P->Objects[0]->Decl);
        REQUIRE(X->IsMutable);
        REQUIRE(X->VarType->getKind() == NodeKind::BuiltinType);
        REQUIRE(llvm::cast<BuiltinType>(X->VarType)->Value == BuiltinType::Int);
        REQUIRE(llvm::cast<IntLiteral>(X->Value)->Value.getSExtValue() == 5);

        auto *Y = llvm::cast<VarDecl>(P->Objects[1]->Decl);
        REQUIRE(!Y->IsMutable);
        REQUIRE(Y->VarType == nullptr);
        REQUIRE(llvm::cast<IntLiteral>(Y->Value)->Value.getSExtValue() == 3);

        auto *Z = llvm::cast<VarDecl>(P->Objects[2]->Decl);
        REQUIRE(Z->IsMutable);
        REQUIRE(Z->VarType->getKind() == NodeKind::BuiltinType);
        REQUIRE(Z->Value == nullptr);
      });
}

TEST_CASE("Declarations inside a block are wrapped", "[declarations]") {
  test::parseSource(
      "fun f() -> void { var x: int; }", [](ASTContext &, Program *P) {
        auto *Fn = llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
        REQUIRE(Fn->Body->Statements.size() == 1);
        auto *Stmt = Fn->Body->Statements[0];
        REQUIRE(Stmt->getKind() == NodeKind::DeclStatement);
        auto *Decl = llvm::cast<DeclStatement>(Stmt)->Decl;
        REQUIRE(Decl->getKind() == NodeKind::VarDecl);
        REQUIRE(llvm::cast<VarDecl>(Decl)->Name->getName() == "x");
      });
}

TEST_CASE("Identifiers are interned", "[declarations]") {
  test::parseSource("fun foo() -> void {} fun foo() -> void {}",
                    [](ASTContext &Ctx, Program *P) {
                      auto *First =
                          llvm::cast<FunctionDecl>(P->Objects[0]->Decl);
                      auto *Second =
                          llvm::cast<FunctionDecl>(P->Objects[1]->Decl);
                      REQUIRE(First->Name == Ctx.getIdentifier("foo"));
                      REQUIRE(Second->Name == Ctx.getIdentifier("foo"));
                      REQUIRE(First->Name == Second->Name);
                    });
}
