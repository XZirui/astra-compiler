#include "astra/ast/ASTDumper.h"
#include "astra/frontend/Parse.h"
#include "astra/sema/Analyze.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

// Temporary driver that exercises the frontend pipeline end to end. It parses
// a source file and dumps the resulting AST to stdout.

static llvm::cl::opt<std::string>
    InputFilename(llvm::cl::Positional,
                  llvm::cl::desc("<input .as source file>"),
                  llvm::cl::init(""));

// Dumping the AST is the only useful output for now, so it defaults to on.
static llvm::cl::opt<bool>
    DumpAST("dump-ast", llvm::cl::desc("Dump the parsed AST to stdout"),
            llvm::cl::init(true));

int main(int argc, char **argv) {
  llvm::cl::ParseCommandLineOptions(argc, argv,
                                    "astra - the astra compiler driver");

  // The SourceMgr is declared first so it is destroyed last: AST node
  // `Range`s point into its buffer and must stay valid.
  llvm::SourceMgr SrcMgr;

  auto FileOrErr = llvm::MemoryBuffer::getFile(InputFilename);
  if (!FileOrErr) {
    SrcMgr.PrintMessage(llvm::SMLoc(), llvm::SourceMgr::DK_Error,
                        "cannot open '" + InputFilename +
                            "': " + FileOrErr.getError().message());
    return 1;
  }
  unsigned FileID =
      SrcMgr.AddNewSourceBuffer(std::move(*FileOrErr), llvm::SMLoc());

  astra::ast::ASTContext Ctx(SrcMgr);
  astra::basic::DiagnosticsEngine Diags(SrcMgr);
  auto *Program = astra::frontend::parse(Ctx, SrcMgr, FileID, Diags);
  if (!Program || Diags.hasErrors()) {
    // The builder reports semantic-ish diagnostics (e.g. a spaced `>>`) on
    // an otherwise parseable program; never exit cleanly on an error.
    Diags.print(llvm::errs());
    return 1;
  }

  // Semantic analysis phase 1: symbol tables, name resolution and canonical
  // type resolution. The results are attached to the AST nodes.
  astra::sema::analyze(Ctx, Program, Diags);
  if (Diags.hasErrors()) {
    Diags.print(llvm::errs());
    return 1;
  }

  if (DumpAST) {
    astra::ast::dump(Program, llvm::outs());
  }
  return 0;
}
