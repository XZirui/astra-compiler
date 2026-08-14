#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/IdentifierInfo.h"
#include "astra/ast/Program.h"
#include "astra/ast/Statement.h"
#include "astra/parser/AstraParser.h"
#include "astra/parser/AstraParserBaseVisitor.h"
#include "llvm/Support/SMLoc.h"

using namespace astra::parser;

namespace astra::frontend {
class ASTBuilder : public AstraParserBaseVisitor {
  ast::ASTContext &ASTContext;
  llvm::SourceMgr &SourceMgr;
  unsigned CurrentFile;

public:
  ASTBuilder(ast::ASTContext &ASTContext, unsigned FileID)
      : ASTContext(ASTContext), SourceMgr(ASTContext.getSourceMgr()),
        CurrentFile(FileID) {}

  ast::Program *build(AstraParser::FileContext *Ctx) {
    return std::any_cast<ast::Program *>(visitFile(Ctx));
  }

protected:
  /// Get llvm::SMLoc from a `Token`.
  /// The SMLoc points to the first char of Token.
  /// Note: Token mustn't be `nullptr`.
  llvm::SMLoc getLoc(antlr4::Token *Token) {
    auto Offset = Token->getStartIndex();
    auto *BufferStart =
        SourceMgr.getMemoryBuffer(CurrentFile)->getBufferStart();
    return llvm::SMLoc::getFromPointer(BufferStart + Offset);
  }

  /// Return a SMRange that points to [Start, Stop + 1).
  llvm::SMRange getRange(antlr4::Token *Start, antlr4::Token *Stop) {
    auto StopIndex = Stop->getStopIndex();
    auto *StopChar = SourceMgr.getMemoryBuffer(CurrentFile)->getBufferStart() +
                     StopIndex + 1;
    return llvm::SMRange(getLoc(Start), llvm::SMLoc::getFromPointer(StopChar));
  }

  llvm::SMRange getRange(antlr4::ParserRuleContext *Ctx) {
    return getRange(Ctx->getStart(), Ctx->getStop());
  }

  /// Return a StringRef that points to [StartIndex, StopIndex].
  /// Note that the StringRef contains StopIndex.
  llvm::StringRef getText(size_t StartIndex, size_t StopIndex) {
    if (StopIndex < StartIndex) {
      return llvm::StringRef();
    }
    const auto *MB = SourceMgr.getMemoryBuffer(CurrentFile);
    const char *BufferStart = MB->getBufferStart();
    return llvm::StringRef(BufferStart + StartIndex,
                           StopIndex - StartIndex + 1);
  }

  llvm::StringRef getText(antlr4::ParserRuleContext *Ctx) {
    auto StartIndex = Ctx->getStart()->getStartIndex();
    auto StopIndex = Ctx->getStop()->getStopIndex(); // contains the end char.
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
};
} // namespace astra::frontend
