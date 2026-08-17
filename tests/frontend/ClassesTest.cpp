#include "ParseHelper.h"

#include "astra/ast/Declaration.h"
#include "astra/ast/Expression.h"
#include "astra/ast/Statement.h"
#include "astra/ast/Type.h"
#include "astra/basic/DiagnosticsEngine.h"

using namespace astra::ast;
using namespace astra;

TEST_CASE("Empty class declarations", "[classes]") {
  test::parseSource("class Foo", [](ASTContext &Ctx, Program *P) {
    auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
    REQUIRE(Cls->Name == Ctx.getIdentifier("Foo"));
    REQUIRE(Cls->TypeParams.empty());
    REQUIRE(Cls->Members.empty());
  });

  test::parseSource("class Foo {}", [](ASTContext &Ctx, Program *P) {
    auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
    REQUIRE(Cls->Name == Ctx.getIdentifier("Foo"));
    REQUIRE(Cls->TypeParams.empty());
    REQUIRE(Cls->Members.empty());
  });
}

TEST_CASE("Class type parameters", "[classes]") {
  test::parseSource("class Box<T, U>", [](ASTContext &Ctx, Program *P) {
    auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
    REQUIRE(Cls->TypeParams.size() == 2);
    REQUIRE(Cls->TypeParams[0]->Name == Ctx.getIdentifier("T"));
    REQUIRE(Cls->TypeParams[1]->Name == Ctx.getIdentifier("U"));
    REQUIRE(Cls->TypeParams[0]->DefaultType == nullptr);
    REQUIRE(Cls->TypeParams[1]->DefaultType == nullptr);
  });

  test::parseSource("class Box<T = int>", [](ASTContext &Ctx, Program *P) {
    auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
    REQUIRE(Cls->TypeParams.size() == 1);
    auto *Param = Cls->TypeParams[0];
    REQUIRE(Param->Name == Ctx.getIdentifier("T"));
    REQUIRE(Param->DefaultType != nullptr);
    REQUIRE(Param->DefaultType->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(Param->DefaultType)->Type ==
            BuiltinType::Int);
  });

  test::parseSource("class Box<T, U = string>", [](ASTContext &Ctx,
                                                   Program *P) {
    auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
    REQUIRE(Cls->TypeParams.size() == 2);
    REQUIRE(Cls->TypeParams[0]->DefaultType == nullptr);
    auto *Default = Cls->TypeParams[1]->DefaultType;
    REQUIRE(Default != nullptr);
    REQUIRE(Default->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(Default)->Type == BuiltinType::String);
  });
}

TEST_CASE("Class body members", "[classes]") {
  test::parseSource(
      "class Foo { var x: int; val y = 1; fun f() -> void {} }",
      [](ASTContext &Ctx, Program *P) {
        auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
        REQUIRE(Cls->Members.size() == 3);

        auto *Prop = static_cast<VarDecl *>(Cls->Members[0]);
        REQUIRE(Prop->Name == Ctx.getIdentifier("x"));
        REQUIRE(Prop->IsMutable);
        // No modifier defaults to `Public`.
        REQUIRE(Prop->Vis == Visibility::Public);
        REQUIRE(Prop->VarType->getKind() == NodeKind::BuiltinType);
        REQUIRE(static_cast<BuiltinType *>(Prop->VarType)->Type ==
                BuiltinType::Int);

        auto *Val = static_cast<VarDecl *>(Cls->Members[1]);
        REQUIRE(Val->Name == Ctx.getIdentifier("y"));
        REQUIRE(!Val->IsMutable);
        REQUIRE(Val->Value->getKind() == NodeKind::IntLiteral);
        REQUIRE(static_cast<IntLiteral *>(Val->Value)->Value.getSExtValue() ==
                1);

        auto *Fn = static_cast<FunctionDecl *>(Cls->Members[2]);
        REQUIRE(Fn->Name == Ctx.getIdentifier("f"));
        REQUIRE(static_cast<BuiltinType *>(Fn->ReturnType)->Type ==
                BuiltinType::Void);
      });
}

TEST_CASE("Class mixed with other top level objects", "[classes]") {
  test::parseSource(
      "fun f() -> void {} class Foo {} var x = 1;",
      [](ASTContext &Ctx, Program *P) {
        REQUIRE(P->Objects.size() == 3);
        REQUIRE(P->Objects[0]->Decl->getKind() == NodeKind::FunctionDecl);
        REQUIRE(P->Objects[1]->Decl->getKind() == NodeKind::ClassDecl);
        REQUIRE(static_cast<ClassDecl *>(P->Objects[1]->Decl)->Name ==
                Ctx.getIdentifier("Foo"));
        REQUIRE(P->Objects[2]->Decl->getKind() == NodeKind::VarDecl);
      });
}

TEST_CASE("Class declaration inside a function body", "[classes]") {
  // Local classes are accepted by the parser for now; a semantic pass could
  // reject them later.
  test::parseSource(
      "fun f() -> void { class Foo {} }", [](ASTContext &Ctx, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        REQUIRE(Fn->Body->Statements.size() == 1);
        auto *Stmt = Fn->Body->Statements[0];
        REQUIRE(Stmt->getKind() == NodeKind::DeclStatement);
        auto *Cls = static_cast<ClassDecl *>(
            static_cast<DeclStatement *>(Stmt)->Declaration);
        REQUIRE(Cls->Name == Ctx.getIdentifier("Foo"));
        REQUIRE(Cls->Members.empty());
      });
}

TEST_CASE("Class member visibility modifiers", "[classes]") {
  test::parseSource(
      "class Foo { public var x: int; private val y = 1; protected fun f() -> "
      "void {} var z: int; }",
      [](ASTContext &, Program *P) {
        auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
        REQUIRE(Cls->Members.size() == 4);
        REQUIRE(static_cast<VarDecl *>(Cls->Members[0])->Vis ==
                Visibility::Public);
        REQUIRE(static_cast<VarDecl *>(Cls->Members[1])->Vis ==
                Visibility::Private);
        REQUIRE(static_cast<FunctionDecl *>(Cls->Members[2])->Vis ==
                Visibility::Protected);
        // No modifier defaults to `Public`.
        REQUIRE(static_cast<VarDecl *>(Cls->Members[3])->Vis ==
                Visibility::Public);
      });
}

TEST_CASE("Top-level visibility modifiers", "[classes]") {
  test::parseSource(
      "public fun f() -> void {} private class Foo {} protected var x = 1;",
      [](ASTContext &, Program *P) {
        REQUIRE(P->Objects.size() == 3);
        REQUIRE(P->Objects[0]->Decl->Vis == Visibility::Public);
        REQUIRE(P->Objects[1]->Decl->Vis == Visibility::Private);
        REQUIRE(P->Objects[2]->Decl->Vis == Visibility::Protected);
      });
}

TEST_CASE("Nested class", "[classes]") {
  test::parseSource("class Foo { class Bar {} }",
                    [](ASTContext &Ctx, Program *P) {
                      auto *Cls = static_cast<ClassDecl *>(P->Objects[0]->Decl);
                      REQUIRE(Cls->Members.size() == 1);
                      auto *Inner = static_cast<ClassDecl *>(Cls->Members[0]);
                      REQUIRE(Inner->Name == Ctx.getIdentifier("Bar"));
                      REQUIRE(Inner->Members.empty());
                    });
}

TEST_CASE("Class body member without semicolon is rejected", "[classes]") {
  test::parseSourceWithDiags(
      "class Foo { var x: int }",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
      });
}
