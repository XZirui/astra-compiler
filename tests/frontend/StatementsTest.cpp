#include "ParseHelper.h"

#include "astra/ast/Declaration.h"

using namespace astra::ast;
using namespace astra;

/// Return the first statement of `f`'s body.
static Statement *firstStmt(Program *P) {
  auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
  REQUIRE(!Fn->Body->Statements.empty());
  return Fn->Body->Statements[0];
}

TEST_CASE("If statement", "[statements]") {
  test::parseSource(
      "fun f() -> void { if (a) { x(); } }", [](ASTContext &, Program *P) {
        auto *If = static_cast<IfStmt *>(firstStmt(P));
        REQUIRE(If->Condition->getKind() == NodeKind::VarExpr);
        REQUIRE(If->Then->getKind() == NodeKind::Block);
        REQUIRE(static_cast<Block *>(If->Then)->Statements.size() == 1);
        REQUIRE(If->Else == nullptr);
      });

  test::parseSource("fun f() -> void { if (a) { x(); } else { y(); } }",
                    [](ASTContext &, Program *P) {
                      auto *If = static_cast<IfStmt *>(firstStmt(P));
                      REQUIRE(If->Else != nullptr);
                      REQUIRE(If->Else->getKind() == NodeKind::Block);
                    });

  test::parseSource(
      "fun f() -> void { if (a) { x(); } else if (b) { y(); } else { z(); } }",
      [](ASTContext &, Program *P) {
        auto *If = static_cast<IfStmt *>(firstStmt(P));
        REQUIRE(If->Else != nullptr);
        REQUIRE(If->Else->getKind() == NodeKind::IfStmt);
        auto *ElseIf = static_cast<IfStmt *>(If->Else);
        REQUIRE(ElseIf->Condition->getKind() == NodeKind::VarExpr);
        REQUIRE(ElseIf->Else != nullptr);
        REQUIRE(ElseIf->Else->getKind() == NodeKind::Block);
      });
}

TEST_CASE("For statement", "[statements]") {
  test::parseSource(
      "fun f() -> void { for (var i = 0; i < 10; i = i + 1) { } }",
      [](ASTContext &, Program *P) {
        auto *For = static_cast<ForStmt *>(firstStmt(P));
        REQUIRE(For->InitStmts.size() == 1);
        REQUIRE(For->InitStmts[0]->getKind() == NodeKind::VarDecl);

        REQUIRE(For->Condition->getKind() == NodeKind::BinaryExpr);
        auto *Cond = static_cast<BinaryExpr *>(For->Condition);
        REQUIRE(Cond->Operator == Op::Lt);

        REQUIRE(For->Update != nullptr);
        REQUIRE(For->Update->getKind() == NodeKind::AssignmentStmt);
        auto *Update = static_cast<AssignmentStmt *>(For->Update);
        REQUIRE(Update->Operator == Op::Assignment);
        REQUIRE(Update->LHS->getKind() == NodeKind::VarExpr);
        REQUIRE(Update->RHS->getKind() == NodeKind::BinaryExpr);
        REQUIRE(static_cast<BinaryExpr *>(Update->RHS)->Operator == Op::Add);

        REQUIRE(For->Body != nullptr);
        REQUIRE(For->Body->Statements.empty());
      });

  test::parseSource("fun f() -> void { for (;;) { } }",
                    [](ASTContext &, Program *P) {
                      auto *For = static_cast<ForStmt *>(firstStmt(P));
                      REQUIRE(For->InitStmts.empty());
                      REQUIRE(For->Condition == nullptr);
                      REQUIRE(For->Update == nullptr);
                      REQUIRE(For->Body != nullptr);
                    });
}

TEST_CASE("For-each statement", "[statements]") {
  test::parseSource(
      "fun f() -> void { for (v in list) { } }", [](ASTContext &, Program *P) {
        auto *ForEach = static_cast<ForEachStmt *>(firstStmt(P));
        REQUIRE(ForEach->VarName == "v");
        REQUIRE(ForEach->Scope->getKind() == NodeKind::VarExpr);
        REQUIRE(static_cast<VarExpr *>(ForEach->Scope)->Name == "list");
        REQUIRE(ForEach->Body != nullptr);
      });
}

TEST_CASE("While and do-while statements", "[statements]") {
  test::parseSource("fun f() -> void { while (c) { } }",
                    [](ASTContext &, Program *P) {
                      auto *While = static_cast<WhileStmt *>(firstStmt(P));
                      REQUIRE(While->Condition->getKind() == NodeKind::VarExpr);
                      REQUIRE(While->Body != nullptr);
                    });

  test::parseSource(
      "fun f() -> void { do { } while (c) }", [](ASTContext &, Program *P) {
        auto *DoWhile = static_cast<DoWhileStmt *>(firstStmt(P));
        REQUIRE(DoWhile->Body != nullptr);
        REQUIRE(DoWhile->Condition->getKind() == NodeKind::VarExpr);
      });
}

TEST_CASE("Assignment statements", "[statements]") {
  test::parseSource("fun f() -> void { x = 1; }", [](ASTContext &, Program *P) {
    auto *Stmt = static_cast<AssignmentStmt *>(firstStmt(P));
    REQUIRE(Stmt->Operator == Op::Assignment);
    REQUIRE(Stmt->LHS->getKind() == NodeKind::VarExpr);
    REQUIRE(Stmt->RHS->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource("fun f() -> void { x += 1; }",
                    [](ASTContext &, Program *P) {
                      // Compound assignments are folded into the plain
                      // operator.
                      auto *Stmt = static_cast<AssignmentStmt *>(firstStmt(P));
                      REQUIRE(Stmt->Operator == Op::Add);
                    });

  test::parseSource(
      "fun f() -> void { x <<= 1; x >>= 2; x &= 3; x |= 4; x ^= 5; }",
      [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        const Op Expected[] = {Op::LShift, Op::RShift, Op::BitAnd, Op::BitOr,
                               Op::BitXor};
        for (size_t I = 0; I < 5; ++I) {
          auto *Stmt = static_cast<AssignmentStmt *>(Fn->Body->Statements[I]);
          REQUIRE(Stmt->Operator == Expected[I]);
        }
      });
}

TEST_CASE("Statement order is preserved", "[statements]") {
  test::parseSource(
      "fun f() -> void { a(); b(); }", [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        REQUIRE(Fn->Body->Statements.size() == 2);
        auto *First = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
        auto *Second = static_cast<ExprStmt *>(Fn->Body->Statements[1]);
        REQUIRE(First->Expression->getKind() == NodeKind::CallExpr);
        REQUIRE(Second->Expression->getKind() == NodeKind::CallExpr);
        auto *FirstCall = static_cast<CallExpr *>(First->Expression);
        auto *SecondCall = static_cast<CallExpr *>(Second->Expression);
        REQUIRE(FirstCall->Callee->getKind() == NodeKind::VarExpr);
        REQUIRE(static_cast<VarExpr *>(FirstCall->Callee)->Name == "a");
        REQUIRE(SecondCall->Callee->getKind() == NodeKind::VarExpr);
        REQUIRE(static_cast<VarExpr *>(SecondCall->Callee)->Name == "b");
      });
}
