#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/Program.h"
#include "astra/basic/DiagnosticsEngine.h"

#include <llvm/Support/SourceMgr.h>

namespace astra::frontend {

/// Parse the source buffer identified by `FileID` in `SrcMgr` into a
/// `Program`, running the whole pipeline (CharStream -> Lexer -> Parser ->
/// ASTBuilder). Lexer/parser errors are reported to `Diags`; returns nullptr
/// if any syntax error was reported (error recovery may have polluted the
/// parse tree). The returned AST is allocated in `Ctx`. `SrcMgr` must outlive
/// the AST, whose node ranges point into its buffer.
ast::Program *parse(ast::ASTContext &Ctx, llvm::SourceMgr &SrcMgr,
                    unsigned FileID, basic::DiagnosticsEngine &Diags);

} // namespace astra::frontend
