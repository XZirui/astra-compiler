#include "astra/frontend/Parse.h"

#include "astra/frontend/ASTBuilder.h"
#include "astra/frontend/SourceMgrCharStream.h"
#include "astra/lexer/AstraLexer.h"
#include "astra/parser/AstraParser.h"

namespace astra::frontend {

ast::Program *parse(ast::ASTContext &Ctx, llvm::SourceMgr &SrcMgr,
                    unsigned FileID) {
  SourceMgrCharStream Stream(SrcMgr, FileID);
  lexer::AstraLexer Lexer(&Stream);
  antlr4::CommonTokenStream Tokens(&Lexer);
  parser::AstraParser Parser(&Tokens);
  auto *FileCtx = Parser.file();
  if (Parser.getNumberOfSyntaxErrors() > 0) {
    // Error recovery may have polluted the parse tree. Don't build on it.
    return nullptr;
  }

  ASTBuilder Builder(Ctx, FileID);
  return Builder.build(FileCtx);
}

} // namespace astra::frontend
