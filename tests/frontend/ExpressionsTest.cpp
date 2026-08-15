#include "ParseHelper.h"

#include "astra/ast/Declaration.h"

using namespace astra::ast;
using namespace astra;

/// The initializer expression of the I-th top-level variable declaration.
static Expr *initExpr(Program *P, size_t I = 0) {
  auto *Var = static_cast<VarDecl *>(P->Objects[I]->Decl);
  REQUIRE(Var->Value != nullptr);
  return Var->Value;
}

TEST_CASE("Integer literals in different radixes", "[expressions]") {
  test::parseSource(
      "var a = 42; var b = 0xFF; var c = 0b1010; var d = 0o17;",
      [](ASTContext &, Program *P) {
        REQUIRE(
            static_cast<IntLiteral *>(initExpr(P, 0))->Value.getSExtValue() ==
            42);
        REQUIRE(
            static_cast<IntLiteral *>(initExpr(P, 1))->Value.getSExtValue() ==
            255);
        REQUIRE(
            static_cast<IntLiteral *>(initExpr(P, 2))->Value.getSExtValue() ==
            10);
        REQUIRE(
            static_cast<IntLiteral *>(initExpr(P, 3))->Value.getSExtValue() ==
            15);
      });
}

TEST_CASE("Boolean and null literals", "[expressions]") {
  test::parseSource(
      "var t = true; var f = false; var n = null;",
      [](ASTContext &, Program *P) {
        REQUIRE(initExpr(P, 0)->getKind() == NodeKind::BoolLiteral);
        REQUIRE(static_cast<BoolLiteral *>(initExpr(P, 0))->Value);
        REQUIRE(initExpr(P, 1)->getKind() == NodeKind::BoolLiteral);
        REQUIRE(!static_cast<BoolLiteral *>(initExpr(P, 1))->Value);
        REQUIRE(initExpr(P, 2)->getKind() == NodeKind::NullLiteral);
      });
}

TEST_CASE("Float literals", "[expressions]") {
  test::parseSource("var d = 1.5; var f = 1.5f;", [](ASTContext &, Program *P) {
    auto *Double = static_cast<FloatLiteral *>(initExpr(P, 0));
    REQUIRE(&Double->Value.getSemantics() == &llvm::APFloat::IEEEdouble());
    auto *Float = static_cast<FloatLiteral *>(initExpr(P, 1));
    REQUIRE(&Float->Value.getSemantics() == &llvm::APFloat::IEEEsingle());
  });
}

TEST_CASE("Operator precedence", "[expressions]") {
  test::parseSource("var r = 1 + 2 * 3;", [](ASTContext &, Program *P) {
    // `*` binds tighter: `1 + (2 * 3)`.
    auto *Add = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Add->Operator == Op::Add);
    REQUIRE(Add->LHS->getKind() == NodeKind::IntLiteral);
    REQUIRE(Add->RHS->getKind() == NodeKind::BinaryExpr);
    REQUIRE(static_cast<BinaryExpr *>(Add->RHS)->Operator == Op::Mult);
  });

  test::parseSource("var r = a & b | c;", [](ASTContext &, Program *P) {
    // `&` binds tighter: `(a & b) | c`.
    auto *Or = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Or->Operator == Op::BitOr);
    REQUIRE(Or->LHS->getKind() == NodeKind::BinaryExpr);
    REQUIRE(static_cast<BinaryExpr *>(Or->LHS)->Operator == Op::BitAnd);
  });

  test::parseSource("var r = a || b && c;", [](ASTContext &, Program *P) {
    // `&&` binds tighter: `a || (b && c)`.
    auto *Or = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Or->Operator == Op::Disj);
    REQUIRE(Or->RHS->getKind() == NodeKind::BinaryExpr);
    REQUIRE(static_cast<BinaryExpr *>(Or->RHS)->Operator == Op::Conj);
  });
}

TEST_CASE("Left associativity", "[expressions]") {
  test::parseSource("var r = 1 - 2 - 3;", [](ASTContext &, Program *P) {
    // `(1 - 2) - 3`.
    auto *Outer = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Outer->Operator == Op::Sub);
    REQUIRE(Outer->LHS->getKind() == NodeKind::BinaryExpr);
    REQUIRE(static_cast<BinaryExpr *>(Outer->LHS)->Operator == Op::Sub);
  });
}

TEST_CASE("Comparison, shift and equality operators", "[expressions]") {
  test::parseSource("var r = 1 << 3;", [](ASTContext &, Program *P) {
    auto *Shift = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Shift->Operator == Op::LShift);
  });

  test::parseSource("var r = a <= b;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::Le);
  });

  test::parseSource("var r = a == b;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::Eq);
  });
}

TEST_CASE("Elvis, in and is operators", "[expressions]") {
  test::parseSource("var r = a ?: b;", [](ASTContext &, Program *P) {
    auto *Elvis = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Elvis->Operator == Op::Elvis);
  });

  test::parseSource("var r = a in b;", [](ASTContext &, Program *P) {
    auto *In = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(In->Operator == Op::In);
  });

  test::parseSource("var r = x is int;", [](ASTContext &, Program *P) {
    auto *Is = static_cast<IsExpr *>(initExpr(P));
    REQUIRE(Is->Operand->getKind() == NodeKind::VarExpr);
    REQUIRE(Is->CheckType->getKind() == NodeKind::BuiltinType);
    REQUIRE(static_cast<BuiltinType *>(Is->CheckType)->Type ==
            BuiltinType::Int);
  });

  test::parseSource("var r = x in xs is int;", [](ASTContext &, Program *P) {
    // `in` and `is` are left-folded: `(x in xs) is int`.
    auto *Is = static_cast<IsExpr *>(initExpr(P));
    REQUIRE(Is->Operand->getKind() == NodeKind::BinaryExpr);
    REQUIRE(static_cast<BinaryExpr *>(Is->Operand)->Operator == Op::In);
    REQUIRE(static_cast<BuiltinType *>(Is->CheckType)->Type ==
            BuiltinType::Int);
  });
}

TEST_CASE("Prefix unary operators", "[expressions]") {
  test::parseSource("var r = !a;", [](ASTContext &, Program *P) {
    auto *Not = static_cast<UnaryExpr *>(initExpr(P));
    REQUIRE(Not->Operator == Op::Not);
    REQUIRE(Not->Operand->getKind() == NodeKind::VarExpr);
  });

  test::parseSource("var r = -a;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<UnaryExpr *>(initExpr(P))->Operator == Op::Sub);
  });

  test::parseSource("var r = +a;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<UnaryExpr *>(initExpr(P))->Operator == Op::Add);
  });

  test::parseSource("var r = ~a;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<UnaryExpr *>(initExpr(P))->Operator == Op::BitNot);
  });

  test::parseSource("var r = !-a;", [](ASTContext &, Program *P) {
    // The rightmost prefix binds closest: `!( -a )`.
    auto *Not = static_cast<UnaryExpr *>(initExpr(P));
    REQUIRE(Not->Operator == Op::Not);
    REQUIRE(Not->Operand->getKind() == NodeKind::UnaryExpr);
    REQUIRE(static_cast<UnaryExpr *>(Not->Operand)->Operator == Op::Sub);
  });
}

TEST_CASE("Postfix expressions", "[expressions]") {
  test::parseSource("var r = f(1, 2);", [](ASTContext &, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->Callee->getKind() == NodeKind::VarExpr);
    REQUIRE(Call->Arguments.size() == 2);
    REQUIRE(Call->Arguments[0]->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource("var r = a[0];", [](ASTContext &, Program *P) {
    auto *Index = static_cast<IndexExpr *>(initExpr(P));
    REQUIRE(Index->Base->getKind() == NodeKind::VarExpr);
    REQUIRE(Index->Index->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource("var r = a.b;", [](ASTContext &, Program *P) {
    auto *Member = static_cast<MemberExpr *>(initExpr(P));
    REQUIRE(Member->Base->getKind() == NodeKind::VarExpr);
    REQUIRE(Member->Member == "b");
    REQUIRE(!Member->NullSafe);
  });

  test::parseSource("var r = a?.b;", [](ASTContext &, Program *P) {
    auto *Member = static_cast<MemberExpr *>(initExpr(P));
    REQUIRE(Member->Member == "b");
    REQUIRE(Member->NullSafe);
  });

  test::parseSource("var r = a.b(1).c;", [](ASTContext &, Program *P) {
    // Suffixes chain left to right: `(a.b)(1).c`.
    auto *Outer = static_cast<MemberExpr *>(initExpr(P));
    REQUIRE(Outer->Member == "c");
    REQUIRE(Outer->Base->getKind() == NodeKind::CallExpr);
    auto *Call = static_cast<CallExpr *>(Outer->Base);
    REQUIRE(Call->Arguments.size() == 1);
    REQUIRE(Call->Callee->getKind() == NodeKind::MemberExpr);
    REQUIRE(static_cast<MemberExpr *>(Call->Callee)->Member == "b");
  });
}

TEST_CASE("This and collection literals", "[expressions]") {
  test::parseSource("var t = this;", [](ASTContext &, Program *P) {
    REQUIRE(initExpr(P)->getKind() == NodeKind::ThisExpr);
  });

  test::parseSource("var xs = [1, 2, 3];", [](ASTContext &, Program *P) {
    auto *Coll = static_cast<CollectionExpr *>(initExpr(P));
    REQUIRE(Coll->Elements.size() == 3);
    REQUIRE(Coll->Elements[0]->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource("var ys = [];", [](ASTContext &, Program *P) {
    auto *Coll = static_cast<CollectionExpr *>(initExpr(P));
    REQUIRE(Coll->Elements.empty());
  });
}

TEST_CASE("If expressions", "[expressions]") {
  test::parseSource("var r = if (c) 1 else 2;", [](ASTContext &, Program *P) {
    auto *If = static_cast<IfExpr *>(initExpr(P));
    REQUIRE(If->Condition->getKind() == NodeKind::VarExpr);
    REQUIRE(If->Then->getKind() == NodeKind::IntLiteral);
    REQUIRE(If->Else != nullptr);
    REQUIRE(If->Else->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource("var r = if (c) 1;", [](ASTContext &, Program *P) {
    auto *If = static_cast<IfExpr *>(initExpr(P));
    REQUIRE(If->Else == nullptr);
  });
}

TEST_CASE("Jump expressions", "[expressions]") {
  test::parseSource("fun f() -> void { return; }", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *Stmt = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
    REQUIRE(Stmt->Expression->getKind() == NodeKind::ReturnExpr);
    REQUIRE(static_cast<ReturnExpr *>(Stmt->Expression)->Value == nullptr);
  });

  test::parseSource("fun f() -> int { return 1; }", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *Stmt = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
    auto *Ret = static_cast<ReturnExpr *>(Stmt->Expression);
    REQUIRE(Ret->Value != nullptr);
    REQUIRE(Ret->Value->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource("fun f() -> void { throw e; }", [](ASTContext &, Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *Stmt = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
    REQUIRE(Stmt->Expression->getKind() == NodeKind::ThrowExpr);
    REQUIRE(static_cast<ThrowExpr *>(Stmt->Expression)->Content->getKind() ==
            NodeKind::VarExpr);
  });

  test::parseSource("fun f() -> void { break; continue; }", [](ASTContext &,
                                                             Program *P) {
    auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
    auto *Break = static_cast<ExprStmt *>(Fn->Body->Statements[0])->Expression;
    auto *Continue =
        static_cast<ExprStmt *>(Fn->Body->Statements[1])->Expression;
    REQUIRE(Break->getKind() == NodeKind::BreakExpr);
    REQUIRE(static_cast<BreakExpr *>(Break)->Pos == nullptr);
    REQUIRE(Continue->getKind() == NodeKind::ContinueExpr);
    REQUIRE(static_cast<ContinueExpr *>(Continue)->Pos == nullptr);
  });

  test::parseSource(
      "fun f() -> void { break @lbl; }", [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        auto *Stmt = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
        auto *Break = static_cast<BreakExpr *>(Stmt->Expression);
        REQUIRE(Break->Pos != nullptr);
        REQUIRE(Break->Pos->Name->getName() == "lbl");
      });
}

TEST_CASE("Parentheses are transparent", "[expressions]") {
  test::parseSource("var r = (1 + 2);", [](ASTContext &, Program *P) {
    // No extra node for the parentheses.
    REQUIRE(initExpr(P)->getKind() == NodeKind::BinaryExpr);
  });

  test::parseSource("var r = x;", [](ASTContext &, Program *P) {
    auto *Var = static_cast<VarExpr *>(initExpr(P));
    REQUIRE(Var->Name == "x");
  });
}
