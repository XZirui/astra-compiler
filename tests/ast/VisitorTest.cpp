#include "astra/ast/ASTVisitor.h"

#include <catch2/catch_test_macros.hpp>

using namespace astra;
using namespace astra::ast;

namespace {

/// Counts every node that reaches the end of the fallback chain.
struct Probe : ASTVisitor<Probe> {
  int Count = 0;
  void visitASTNode(ASTNode *) { ++Count; }
};

/// Overrides only the expression level of the fallback chain.
struct ExprProbe : ASTVisitor<ExprProbe> {
  int Exprs = 0;
  void visitExpr(Expr *) { ++Exprs; }
};

/// Overrides both a concrete method and the expression level.
struct OverrideProbe : ASTVisitor<OverrideProbe> {
  int Exprs = 0;
  int BinaryExprs = 0;
  void visitExpr(Expr *) { ++Exprs; }
  void visitBinaryExpr(BinaryExpr *) { ++BinaryExprs; }
};

} // namespace

TEST_CASE("Every node kind reaches the visitor", "[visitor]") {
  // The default visitor does not recurse into children — traversal is up to
  // the concrete visitor — so visit one representative node of each
  // base-class branch directly to exercise the dispatch and the fallback
  // chain.
  Program P;
  TopLevelObject TLO;
  FunctionDecl Fn;
  Parameter Param;
  TypeRef TR;
  BuiltinType BT;
  Block Blk;
  ExprStmt ES;
  BinaryExpr Bin;
  BoolLiteral B;

  Probe Pr;
  Pr.visit(&P);
  Pr.visit(&TLO);
  Pr.visit(&Fn);
  Pr.visit(&Param);
  Pr.visit(&TR);
  Pr.visit(&BT);
  Pr.visit(&Blk);
  Pr.visit(&ES);
  Pr.visit(&Bin);
  Pr.visit(&B);
  REQUIRE(Pr.Count == 10);
}

TEST_CASE("The fallback chain reaches a higher-level override", "[visitor]") {
  BoolLiteral B;
  BinaryExpr Bin;
  ExprProbe Pr;
  Pr.visit(&B);
  Pr.visit(&Bin);
  // Neither kind has a concrete override, so both fall back to `visitExpr`.
  REQUIRE(Pr.Exprs == 2);
}

TEST_CASE("A concrete method takes precedence over the fallback", "[visitor]") {
  BinaryExpr Bin;
  BoolLiteral B;
  OverrideProbe Pr;
  Pr.visit(&Bin);
  REQUIRE(Pr.BinaryExprs == 1);
  REQUIRE(Pr.Exprs == 0);
  Pr.visit(&B);
  REQUIRE(Pr.Exprs == 1);
}
