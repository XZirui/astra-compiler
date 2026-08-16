#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/IdentifierInfo.h"
#include "astra/ast/Program.h"
#include "astra/ast/Statement.h"
#include "astra/basic/DiagnosticsEngine.h"
#include "astra/parser/AstraParser.h"
#include "astra/parser/AstraParserBaseVisitor.h"
#include "llvm/Support/SMLoc.h"

using namespace astra::parser;

namespace astra::frontend {
/// Build an `astra::ast` tree from the ANTLR parse tree of a file.
/// Every node is allocated in `ASTContext`. Helper methods such as `getExpr`
/// unwrap the `std::any` that `visit` returns. The parse tree must stay alive
/// while the builder runs.
class ASTBuilder : public AstraParserBaseVisitor {
  /// The arena that owns every node this builder creates.
  ast::ASTContext &ASTContext;
  /// The source manager whose buffer the AST ranges point into.
  llvm::SourceMgr &SourceMgr;
  /// The `FileID` of the buffer currently being parsed.
  unsigned CurrentFile;
  /// Collects diagnostics for errors the builder detects (e.g. unimplemented
  /// constructs); the builder degrades gracefully instead of asserting.
  basic::DiagnosticsEngine &Diags;

public:
  ASTBuilder(ast::ASTContext &ASTContext, unsigned FileID,
             basic::DiagnosticsEngine &Diags)
      : ASTContext(ASTContext), SourceMgr(ASTContext.getSourceMgr()),
        CurrentFile(FileID), Diags(Diags) {}

  /// Visit the file parse tree and return the resulting `Program`.
  ast::Program *build(AstraParser::FileContext *Ctx) {
    return std::any_cast<ast::Program *>(visitFile(Ctx));
  }

protected:
  /// Get the `llvm::SMLoc` that points at the first character of `Token`.
  /// Note: `Token` must not be nullptr.
  llvm::SMLoc getLoc(antlr4::Token *Token) {
    auto Offset = Token->getStartIndex();
    auto *BufferStart =
        SourceMgr.getMemoryBuffer(CurrentFile)->getBufferStart();
    return llvm::SMLoc::getFromPointer(BufferStart + Offset);
  }

  /// Return a SMRange that points to [Start, Stop + 1).
  llvm::SMRange getRange(antlr4::Token *Start, antlr4::Token *Stop) {
    auto *MB = SourceMgr.getMemoryBuffer(CurrentFile);
    size_t Size = MB->getBufferSize();
    // The EOF token reports a stop index one past the last byte (`SIZE_MAX`
    // for an empty buffer); clamp it so `+1` never wraps past the buffer.
    size_t StopIndex = Stop->getStopIndex();
    if (StopIndex >= Size) {
      StopIndex = Size;
    }
    auto *StopChar = MB->getBufferStart() + StopIndex + 1;
    return llvm::SMRange(getLoc(Start), llvm::SMLoc::getFromPointer(StopChar));
  }

  llvm::SMRange getRange(antlr4::ParserRuleContext *Ctx) {
    return getRange(Ctx->getStart(), Ctx->getStop());
  }

  /// Return a `StringRef` over [StartIndex, StopIndex], both ends inclusive.
  llvm::StringRef getText(size_t StartIndex, size_t StopIndex) {
    const auto *MB = SourceMgr.getMemoryBuffer(CurrentFile);
    size_t Size = MB->getBufferSize();
    // The EOF token reports a stop index one past the last byte (`SIZE_MAX`
    // for an empty buffer); clamp it so the length never goes out of bounds.
    if (StartIndex >= Size || StopIndex < StartIndex) {
      return llvm::StringRef();
    }
    if (StopIndex >= Size) {
      StopIndex = Size - 1; // Size > 0 because StartIndex < Size.
    }
    return llvm::StringRef(MB->getBufferStart() + StartIndex,
                           StopIndex - StartIndex + 1);
  }

  llvm::StringRef getText(antlr4::ParserRuleContext *Ctx) {
    auto StartIndex = Ctx->getStart()->getStartIndex();
    // The stop index is inclusive.
    auto StopIndex = Ctx->getStop()->getStopIndex();
    return getText(StartIndex, StopIndex);
  }

  llvm::StringRef getText(antlr4::Token *Tok) {
    auto StartIndex = Tok->getStartIndex();
    auto StopIndex = Tok->getStopIndex();
    return getText(StartIndex, StopIndex);
  }

  llvm::StringRef getText(antlr4::tree::TerminalNode *Node) {
    return getText(Node->getSymbol());
  }

  /// Fold `Subs[0] Ops[0] Subs[1] Ops[1] ...` into a left-associative chain of
  /// `BinaryExpr`, e.g. `a + b - c` -> `(a + b) - c`. The range of each
  /// `BinaryExpr` spans from the first operand to the end of its right
  /// operand. `GetOp` maps the I-th operator to the corresponding `ast::Op`.
  template <typename SubCtx, typename OpCtx, typename OpFn>
  ast::Expr *foldLeftAssoc(const std::vector<SubCtx *> &Subs,
                           const std::vector<OpCtx *> &Ops, OpFn &&GetOp) {
    auto *Result = getExpr(Subs.front());
    for (size_t I = 1; I < Subs.size(); ++I) {
      auto *Binary = ASTContext.allocate<ast::BinaryExpr>();
      Binary->Range = getRange(Subs.front()->getStart(), Subs[I]->getStop());
      Binary->Operator = GetOp(Ops[I - 1]);
      Binary->LHS = Result;
      Binary->RHS = getExpr(Subs[I]);
      Result = Binary;
    }
    return static_cast<ast::Expr *>(Result);
  }

  /// Intern the text of `Node` as an `IdentifierInfo` in the `ASTContext`.
  ast::IdentifierInfo *getIdentifier(antlr4::tree::TerminalNode *Node) {
    auto Text = getText(Node);
    return ASTContext.getIdentifier(Text);
  }

  ast::Declaration *getDecl(antlr4::tree::ParseTree *Tree);
  ast::Type *getType(antlr4::tree::ParseTree *Tree);
  ast::Statement *getStmt(antlr4::tree::ParseTree *Tree);
  ast::Block *getBlock(antlr4::tree::ParseTree *Tree);
  ast::Expr *getExpr(antlr4::tree::ParseTree *Tree);

public:
  std::any visitFile(AstraParser::FileContext *Ctx) override;
  std::any
  visitTopLevelObject(AstraParser::TopLevelObjectContext *Ctx) override;

  // --- Declarations ---

  std::any visitDeclaration(AstraParser::DeclarationContext *Ctx) override;
  std::any visitFunctionDecl(AstraParser::FunctionDeclContext *Ctx) override;
  std::any visitParameter(AstraParser::ParameterContext *Ctx) override;
  std::any visitVariableDecl(AstraParser::VariableDeclContext *Ctx) override;

  // --- Types ---

  std::any visitType(AstraParser::TypeContext *Ctx) override;
  std::any visitParenType(AstraParser::ParenTypeContext *Ctx) override;
  std::any visitTypeRef(AstraParser::TypeRefContext *Ctx) override;
  std::any visitFunctionType(AstraParser::FunctionTypeContext *Ctx) override;
  std::any visitBuiltinType(AstraParser::BuiltinTypeContext *Ctx) override;

  // --- Statements ---

  std::any visitBlock(AstraParser::BlockContext *Ctx) override;
  std::any visitStatement(AstraParser::StatementContext *Ctx) override;
  std::any visitDeclStatement(AstraParser::DeclStatementContext *Ctx) override;
  std::any visitAssignment(AstraParser::AssignmentContext *Ctx) override;
  std::any
  visitControlStatement(AstraParser::ControlStatementContext *Ctx) override;
  std::any visitForStmt(AstraParser::ForStmtContext *Ctx) override;
  std::any visitForUpdate(AstraParser::ForUpdateContext *Ctx) override;
  std::any visitForEachStmt(AstraParser::ForEachStmtContext *Ctx) override;
  std::any visitWhileStmt(AstraParser::WhileStmtContext *Ctx) override;
  std::any visitDoWhileStmt(AstraParser::DoWhileStmtContext *Ctx) override;
  std::any visitIfStmt(AstraParser::IfStmtContext *Ctx) override;
  std::any visitExprStmt(AstraParser::ExprStmtContext *Ctx) override;

  // --- Expressions ---

  std::any visitExpression(AstraParser::ExpressionContext *Ctx) override;
  std::any visitDisjunction(AstraParser::DisjunctionContext *Ctx) override;
  std::any visitConjunction(AstraParser::ConjunctionContext *Ctx) override;
  std::any visitEquality(AstraParser::EqualityContext *Ctx) override;
  std::any visitComparison(AstraParser::ComparisonContext *Ctx) override;
  std::any visitInfixExpr(AstraParser::InfixExprContext *Ctx) override;
  std::any visitElvisExpr(AstraParser::ElvisExprContext *Ctx) override;
  std::any visitBitwiseOr(AstraParser::BitwiseOrContext *Ctx) override;
  std::any visitBitwiseXor(AstraParser::BitwiseXorContext *Ctx) override;
  std::any visitBitwiseAnd(AstraParser::BitwiseAndContext *Ctx) override;
  std::any visitBitwiseShift(AstraParser::BitwiseShiftContext *Ctx) override;
  std::any visitAddition(AstraParser::AdditionContext *Ctx) override;
  std::any
  visitMultiplication(AstraParser::MultiplicationContext *Ctx) override;
  std::any visitAsExpr(AstraParser::AsExprContext *Ctx) override;
  std::any
  visitPrefixUnaryExpr(AstraParser::PrefixUnaryExprContext *Ctx) override;
  std::any
  visitPostfixUnaryExpr(AstraParser::PostfixUnaryExprContext *Ctx) override;
  std::any visitPrimaryExpr(AstraParser::PrimaryExprContext *Ctx) override;
  std::any visitParenExpr(AstraParser::ParenExprContext *Ctx) override;
  std::any
  visitLiteralConstant(AstraParser::LiteralConstantContext *Ctx) override;
  std::any
  visitCollectionLiteral(AstraParser::CollectionLiteralContext *Ctx) override;
  std::any visitThisLiteral(AstraParser::ThisLiteralContext *Ctx) override;
  std::any visitIfExpression(AstraParser::IfExpressionContext *Ctx) override;
  std::any
  visitJumpExpression(AstraParser::JumpExpressionContext *Ctx) override;
  std::any visitLabel(AstraParser::LabelContext *Ctx) override;
};
} // namespace astra::frontend
