#include "astra/frontend/Parse.h"

#include "astra/frontend/ASTBuilder.h"
#include "astra/frontend/SourceMgrCharStream.h"
#include "astra/lexer/AstraLexer.h"
#include "astra/parser/AstraParser.h"

namespace astra::frontend {

namespace {

/// Forwards ANTLR lexer/parser errors to the `DiagnosticsEngine`, translating
/// the line/column positions into `SMLoc`s into the source buffer.
class ErrorListener : public antlr4::BaseErrorListener {
  basic::DiagnosticsEngine &Diags;
  llvm::SourceMgr &SrcMgr;
  unsigned FileID;

public:
  ErrorListener(basic::DiagnosticsEngine &Diags, llvm::SourceMgr &SrcMgr,
                unsigned FileID)
      : Diags(Diags), SrcMgr(SrcMgr), FileID(FileID) {}

  void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t Line,
                   size_t CharPos, const std::string &Msg,
                   std::exception_ptr) override {
    // ANTLR columns are 0-based; LLVM's are 1-based.
    auto Loc = SrcMgr.FindLocForLineAndColumn(FileID, Line, CharPos + 1);
    Diags.report(Loc, llvm::SourceMgr::DK_Error, Msg);
  }
};

} // namespace

ast::Program *parse(ast::ASTContext &Ctx, llvm::SourceMgr &SrcMgr,
                    unsigned FileID, basic::DiagnosticsEngine &Diags) {
  SourceMgrCharStream Stream(SrcMgr, FileID);
  lexer::AstraLexer Lexer(&Stream);
  ErrorListener Listener(Diags, SrcMgr, FileID);
  Lexer.removeErrorListeners();
  Lexer.addErrorListener(&Listener);

  antlr4::CommonTokenStream Tokens(&Lexer);
  parser::AstraParser Parser(&Tokens);
  Parser.removeErrorListeners();
  Parser.addErrorListener(&Listener);

  auto *FileCtx = Parser.file();
  if (Diags.hasErrors()) {
    // Error recovery may have polluted the parse tree. Don't build on it.
    return nullptr;
  }

  ASTBuilder Builder(Ctx, FileID, Diags);
  return Builder.build(FileCtx);
}

} // namespace astra::frontend
