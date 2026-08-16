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

TEST_CASE("Integer literal range", "[expressions]") {
  test::parseSource(
      "var a = 9223372036854775807; var b = 0x7FFFFFFFFFFFFFFF;",
      [](ASTContext &, Program *P) {
        // `LONG_MAX` is the largest accepted literal.
        REQUIRE(
            static_cast<IntLiteral *>(initExpr(P, 0))->Value.getSExtValue() ==
            9223372036854775807LL);
        REQUIRE(
            static_cast<IntLiteral *>(initExpr(P, 1))->Value.getSExtValue() ==
            9223372036854775807LL);
      });

  test::parseSourceWithDiags(
      "var c = 9223372036854775808;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // One past `LONG_MAX` does not fit in a signed 64-bit `long`.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("too large") != llvm::StringRef::npos);
      });

  test::parseSourceWithDiags(
      "var d = 0x10000000000000000;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // 65 bits: `zext(64)` would assert in Debug or silently truncate in
        // Release; it must be reported instead.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("too large") != llvm::StringRef::npos);
      });
}

TEST_CASE("Non-decimal literals with an 'f' suffix", "[expressions]") {
  test::parseSourceWithDiags(
      "var r = 0b1f;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // The `f` suffix is only valid on decimal literals: `0b1f` lexes as
        // `0b1` followed by the identifier `f`.
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
      });

  test::parseSource("var r = 0xFFf;", [](ASTContext &, Program *P) {
    // `f` is a hex digit, so the whole token is the integer 0xFFF.
    REQUIRE(static_cast<IntLiteral *>(initExpr(P))->Value.getSExtValue() ==
            4095);
  });
}

TEST_CASE("Leading zeros are rejected in double literals", "[expressions]") {
  test::parseSourceWithDiags(
      "var r = 01.5;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // Like `01`, a leading zero makes `01.5` two tokens (`0` + `1.5`)
        // and a syntax error.
        REQUIRE(P == nullptr);
        REQUIRE(Diags.hasErrors());
      });

  test::parseSource("var r = 0.5;", [](ASTContext &, Program *P) {
    // A bare zero before the dot is fine.
    auto *Lit = static_cast<FloatLiteral *>(initExpr(P));
    REQUIRE(&Lit->Value.getSemantics() == &llvm::APFloat::IEEEdouble());
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

  test::parseSourceWithDiags(
      "var d = 1e400;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // Overflow: the value saturates to +inf and the range is reported.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("out of range") !=
                llvm::StringRef::npos);
        auto *Lit = static_cast<FloatLiteral *>(initExpr(P));
        REQUIRE(Lit->Value.isInfinity());
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

  test::parseSource("var r = a >> b;", [](ASTContext &, Program *P) {
    // `>>` lexes as two `GT` tokens and is combined in the parser.
    REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::RShift);
  });

  test::parseSource("var r = a <= b;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::Le);
  });

  test::parseSource("var r = a == b;", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::Eq);
  });
}

TEST_CASE("Right shift requires adjacent '>' tokens", "[expressions]") {
  test::parseSource("var r = a >> b;", [](ASTContext &, Program *P) {
    // No diagnostic: the two `>` tokens are physically adjacent.
    REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::RShift);
  });

  test::parseSourceWithDiags(
      "var r = a > > b;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // Whitespace between the `>` tokens is not a right shift.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("adjacent") != llvm::StringRef::npos);
        // The AST is still built with RShift; semantic analysis rejects it.
        REQUIRE(static_cast<BinaryExpr *>(initExpr(P))->Operator == Op::RShift);
      });

  test::parseSourceWithDiags(
      "var r = a > /* comment */ > b;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // A comment counts as whitespace, so this is not a right shift.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("adjacent") != llvm::StringRef::npos);
      });

  test::parseSourceWithDiags(
      "var r = x > a > > b;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // No bypass: this parses as `x > (a >> b)` and the shift operator
        // still triggers the diagnostic.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("adjacent") != llvm::StringRef::npos);
      });

  test::parseSource("var r = a < b >> c;", [](ASTContext &, Program *P) {
    // `a < (b >> c)`: the shift binds tighter and stays a single operator.
    auto *Lt = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Lt->Operator == Op::Lt);
    auto *Shift = static_cast<BinaryExpr *>(Lt->RHS);
    REQUIRE(Shift->Operator == Op::RShift);
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
  test::parseSource(
      "fun f() -> void { return; }", [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        auto *Stmt = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
        REQUIRE(Stmt->Expression->getKind() == NodeKind::ReturnExpr);
        REQUIRE(static_cast<ReturnExpr *>(Stmt->Expression)->Value == nullptr);
      });

  test::parseSource(
      "fun f() -> int { return 1; }", [](ASTContext &, Program *P) {
        auto *Fn = static_cast<FunctionDecl *>(P->Objects[0]->Decl);
        auto *Stmt = static_cast<ExprStmt *>(Fn->Body->Statements[0]);
        auto *Ret = static_cast<ReturnExpr *>(Stmt->Expression);
        REQUIRE(Ret->Value != nullptr);
        REQUIRE(Ret->Value->getKind() == NodeKind::IntLiteral);
      });

  test::parseSource("fun f() -> void { throw e; }", [](ASTContext &,
                                                       Program *P) {
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

TEST_CASE("Type arguments on calls", "[expressions]") {
  test::parseSource("var r = foo<Int>();", [](ASTContext &Ctx, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->Callee->getKind() == NodeKind::VarExpr);
    REQUIRE(Call->Arguments.empty());
    REQUIRE(Call->TypeArgs.size() == 1);
    auto *Arg = Call->TypeArgs[0];
    REQUIRE(Arg->getKind() == NodeKind::TypeRef);
    REQUIRE(static_cast<TypeRef *>(Arg)->Name == Ctx.getIdentifier("Int"));
    REQUIRE(Call->ExplicitTypeArgs);
  });

  test::parseSource("var r = foo();", [](ASTContext &, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->TypeArgs.empty());
    // Without `<>` the call may use type inference.
    REQUIRE(!Call->ExplicitTypeArgs);
  });

  test::parseSource("var r = foo<>();", [](ASTContext &, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    // `<>` is empty but explicit: it forces the default type parameters.
    REQUIRE(Call->TypeArgs.empty());
    REQUIRE(Call->ExplicitTypeArgs);
  });

  test::parseSource(
      "var r = foo<int, Bar>();", [](ASTContext &Ctx, Program *P) {
        auto *Call = static_cast<CallExpr *>(initExpr(P));
        REQUIRE(Call->TypeArgs.size() == 2);
        REQUIRE(Call->TypeArgs[0]->getKind() == NodeKind::BuiltinType);
        REQUIRE(static_cast<BuiltinType *>(Call->TypeArgs[0])->Type ==
                BuiltinType::Int);
        REQUIRE(Call->TypeArgs[1]->getKind() == NodeKind::TypeRef);
        REQUIRE(static_cast<TypeRef *>(Call->TypeArgs[1])->Name ==
                Ctx.getIdentifier("Bar"));
      });

  test::parseSource("var r = a.b<Int>(1);", [](ASTContext &, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->Callee->getKind() == NodeKind::MemberExpr);
    REQUIRE(static_cast<MemberExpr *>(Call->Callee)->Member == "b");
    REQUIRE(Call->TypeArgs.size() == 1);
    REQUIRE(Call->Arguments.size() == 1);
    REQUIRE(Call->Arguments[0]->getKind() == NodeKind::IntLiteral);
  });

  test::parseSource(
      "var r = foo<fun (int) -> double>();", [](ASTContext &, Program *P) {
        auto *Call = static_cast<CallExpr *>(initExpr(P));
        REQUIRE(Call->TypeArgs.size() == 1);
        auto *Ty = static_cast<FunctionType *>(Call->TypeArgs[0]);
        REQUIRE(Ty->getKind() == NodeKind::FunctionType);
        REQUIRE(Ty->Parameters.size() == 1);
        REQUIRE(static_cast<BuiltinType *>(Ty->Parameters[0])->Type ==
                BuiltinType::Int);
        REQUIRE(static_cast<BuiltinType *>(Ty->ReturnType)->Type ==
                BuiltinType::Double);
      });

  test::parseSource("var r = foo<Int[3]>();", [](ASTContext &, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->TypeArgs.size() == 1);
    auto *Arr = static_cast<ArrayType *>(Call->TypeArgs[0]);
    REQUIRE(Arr->getKind() == NodeKind::ArrayType);
    REQUIRE(static_cast<IntLiteral *>(Arr->Size)->Value.getSExtValue() == 3);
  });

  test::parseSource(
      "var r = foo<Array<Int>>();", [](ASTContext &Ctx, Program *P) {
        auto *Call = static_cast<CallExpr *>(initExpr(P));
        REQUIRE(Call->TypeArgs.size() == 1);
        auto *Outer = static_cast<TypeRef *>(Call->TypeArgs[0]);
        REQUIRE(Outer->getKind() == NodeKind::TypeRef);
        REQUIRE(Outer->Name == Ctx.getIdentifier("Array"));
        REQUIRE(Outer->TypeArgs.size() == 1);
        REQUIRE(Outer->TypeArgs[0]->getKind() == NodeKind::TypeRef);
        REQUIRE(static_cast<TypeRef *>(Outer->TypeArgs[0])->Name ==
                Ctx.getIdentifier("Int"));
      });
}

TEST_CASE("Dangling '>' before '(' is rejected", "[expressions]") {
  // `foo<Int>>(x)` cannot be a generic call: the type argument `Int` is not
  // itself generic, so the second `>` has no type argument list to close.
  // Per the Kotlin rule (a `>` directly before `(` is read as type
  // arguments), the comparison reading `foo < (Int >> (x))` is rejected.
  test::parseSourceWithDiags(
      "var x = foo<Int>>(x);",
      [](ASTContext &Ctx, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("expected '('") !=
                llvm::StringRef::npos);

        // The AST degrades to a best-effort call with the type arguments.
        auto *Call = static_cast<CallExpr *>(initExpr(P));
        REQUIRE(Call->Callee->getKind() == NodeKind::VarExpr);
        REQUIRE(Call->TypeArgs.size() == 1);
        REQUIRE(static_cast<TypeRef *>(Call->TypeArgs[0])->Name ==
                Ctx.getIdentifier("Int"));
        REQUIRE(Call->ExplicitTypeArgs);
        REQUIRE(Call->Arguments.size() == 1);
      });

  // The same token stream as `foo<Int>>(x)`: `a < b >> (x)`.
  test::parseSourceWithDiags(
      "var x = a < b >> (x);",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("expected '('") !=
                llvm::StringRef::npos);
      });

  // Spacing between the two `>` does not change the token stream.
  test::parseSourceWithDiags(
      "var x = a < b > > (x);",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("expected '('") !=
                llvm::StringRef::npos);
      });
}

TEST_CASE("'>' before '(' is a generic call", "[expressions]") {
  // A single `>` directly before `(` takes the preferred generic-call
  // reading (Kotlin rule), not a comparison.
  test::parseSource("var x = a < b > (c);", [](ASTContext &Ctx, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->Callee->getKind() == NodeKind::VarExpr);
    REQUIRE(Call->TypeArgs.size() == 1);
    REQUIRE(static_cast<TypeRef *>(Call->TypeArgs[0])->Name ==
            Ctx.getIdentifier("b"));
    REQUIRE(Call->Arguments.size() == 1);
    REQUIRE(static_cast<VarExpr *>(Call->Arguments[0])->Name == "c");
  });
}

TEST_CASE("Generic call and shift regressions", "[expressions]") {
  test::parseSource("var x = foo<Int>(x);", [](ASTContext &Ctx, Program *P) {
    auto *Call = static_cast<CallExpr *>(initExpr(P));
    REQUIRE(Call->TypeArgs.size() == 1);
    REQUIRE(static_cast<TypeRef *>(Call->TypeArgs[0])->Name ==
            Ctx.getIdentifier("Int"));
    REQUIRE(Call->Arguments.size() == 1);
  });

  test::parseSource(
      "var x = foo<Bar<Baz>>(x);", [](ASTContext &Ctx, Program *P) {
        auto *Call = static_cast<CallExpr *>(initExpr(P));
        REQUIRE(Call->TypeArgs.size() == 1);
        auto *Outer = static_cast<TypeRef *>(Call->TypeArgs[0]);
        REQUIRE(Outer->Name == Ctx.getIdentifier("Bar"));
        REQUIRE(Outer->TypeArgs.size() == 1);
        REQUIRE(static_cast<TypeRef *>(Outer->TypeArgs[0])->Name ==
                Ctx.getIdentifier("Baz"));
        REQUIRE(Call->Arguments.size() == 1);
      });

  // `a >> (x)` without a preceding `<` stays a right shift.
  test::parseSource("var x = a >> (x);", [](ASTContext &, Program *P) {
    auto *Bin = static_cast<BinaryExpr *>(initExpr(P));
    REQUIRE(Bin->Operator == Op::RShift);
  });
}

TEST_CASE("String literals", "[expressions]") {
  test::parseSource(R"(var s = "hello";)", [](ASTContext &, Program *P) {
    auto *Lit = static_cast<StringLiteral *>(initExpr(P));
    REQUIRE(Lit->getKind() == NodeKind::StringLiteral);
    REQUIRE(Lit->IsConst);
    REQUIRE(Lit->Value == "hello");
  });

  test::parseSource(R"(var s = "";)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<StringLiteral *>(initExpr(P))->Value == "");
  });

  test::parseSource(R"(var s = "a\tb\n\r\'\"\\\$";)",
                    [](ASTContext &, Program *P) {
                      auto *Lit = static_cast<StringLiteral *>(initExpr(P));
                      REQUIRE(Lit->Value == "a\tb\n\r'\"\\$");
                    });

  test::parseSource(R"(var s = "A";)", [](ASTContext &, Program *P) {
    auto *Lit = static_cast<StringLiteral *>(initExpr(P));
    REQUIRE(Lit->Value == "A");
  });

  test::parseSource(R"(var s = "é";)", [](ASTContext &, Program *P) {
    // é encodes as two UTF-8 bytes.
    auto *Lit = static_cast<StringLiteral *>(initExpr(P));
    REQUIRE(Lit->Value == "\xC3\xA9");
  });

  test::parseSource(R"(var s = "\u0041";)", [](ASTContext &, Program *P) {
    auto *Lit = static_cast<StringLiteral *>(initExpr(P));
    REQUIRE(Lit->Value == "A");
  });

  test::parseSource(R"(var s = "\u00E9";)", [](ASTContext &, Program *P) {
    // é decodes to the two-byte UTF-8 encoding of é.
    auto *Lit = static_cast<StringLiteral *>(initExpr(P));
    REQUIRE(Lit->Value == "\xC3\xA9");
  });
}

TEST_CASE("Char literals", "[expressions]") {
  test::parseSource("var c = 'a';", [](ASTContext &, Program *P) {
    auto *Lit = static_cast<CharLiteral *>(initExpr(P));
    REQUIRE(Lit->getKind() == NodeKind::CharLiteral);
    REQUIRE(Lit->IsConst);
    REQUIRE(Lit->Value == 97);
  });

  test::parseSource(R"(var c = '\n';)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<CharLiteral *>(initExpr(P))->Value == 10);
  });

  test::parseSource(R"(var c = '\\';)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<CharLiteral *>(initExpr(P))->Value == 92);
  });

  test::parseSource(R"(var c = '\'';)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<CharLiteral *>(initExpr(P))->Value == 39);
  });

  test::parseSource(R"(var c = '"';)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<CharLiteral *>(initExpr(P))->Value == 34);
  });

  test::parseSource(R"(var c = '\u0041';)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<CharLiteral *>(initExpr(P))->Value == 65);
  });

  test::parseSource(R"(var c = '\u2603';)", [](ASTContext &, Program *P) {
    REQUIRE(static_cast<CharLiteral *>(initExpr(P))->Value == 9731);
  });
}

TEST_CASE("Invalid string escapes", "[expressions]") {
  test::parseSourceWithDiags(
      R"(var s = "\x";)",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // No syntax error: the builder degrades gracefully and keeps the
        // raw text in the node.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("invalid escape sequence") !=
                llvm::StringRef::npos);
        auto *Var = static_cast<VarDecl *>(P->Objects[0]->Decl);
        REQUIRE(static_cast<StringLiteral *>(Var->Value)->Value == "\\x");
      });
}

TEST_CASE("Comparison chains", "[expressions]") {
  test::parseSource("var r = a < b < c;", [](ASTContext &, Program *P) {
    auto *Chain = static_cast<ComparisonChainExpr *>(initExpr(P));
    REQUIRE(Chain->getKind() == NodeKind::ComparisonChainExpr);
    REQUIRE(Chain->Operands.size() == 3);
    REQUIRE(Chain->Operators.size() == 2);
    REQUIRE(Chain->Operators[0] == Op::Lt);
    REQUIRE(Chain->Operators[1] == Op::Lt);
  });

  test::parseSource("var r = a < b <= c;", [](ASTContext &, Program *P) {
    auto *Chain = static_cast<ComparisonChainExpr *>(initExpr(P));
    REQUIRE(Chain->Operators[0] == Op::Lt);
    REQUIRE(Chain->Operators[1] == Op::Le);
  });

  test::parseSource("var r = a > b >= c;", [](ASTContext &, Program *P) {
    auto *Chain = static_cast<ComparisonChainExpr *>(initExpr(P));
    REQUIRE(Chain->Operators[0] == Op::Gt);
    REQUIRE(Chain->Operators[1] == Op::Ge);
  });

  test::parseSourceWithDiags(
      "var r = a < b > c;",
      [](ASTContext &, Program *P, basic::DiagnosticsEngine &Diags) {
        // Mixed directions are a builder diagnostic, not a syntax error.
        REQUIRE(P != nullptr);
        REQUIRE(Diags.hasErrors());
        const auto &All = Diags.getDiagnostics();
        REQUIRE(All.size() == 1);
        REQUIRE(All[0].getMessage().find("mixed operators") !=
                llvm::StringRef::npos);
        auto *Chain = static_cast<ComparisonChainExpr *>(initExpr(P));
        REQUIRE(Chain->Operators.size() == 2);
        REQUIRE(Chain->Operators[0] == Op::Lt);
        REQUIRE(Chain->Operators[1] == Op::Gt);
      });
}
