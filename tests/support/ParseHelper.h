#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/Program.h"
#include "astra/basic/DiagnosticsEngine.h"
#include "astra/frontend/Parse.h"

#include <catch2/catch_test_macros.hpp>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

namespace astra::test {

/// Parse `Src` as an astra program and invoke `Fn(Ctx, Program, Diags)` while
/// the SourceMgr/ASTContext/Program/Diags are alive. The SourceMgr is
/// declared first so it is destroyed last: AST node `Range`s point into its
/// buffer. The callback form keeps the lifetimes structurally impossible to
/// misuse.
template <typename FnTy> void parseSourceWithDiags(llvm::StringRef Src, FnTy &&Fn) {
  llvm::SourceMgr SrcMgr;
  unsigned FileID = SrcMgr.AddNewSourceBuffer(
      llvm::MemoryBuffer::getMemBufferCopy(Src, "<test>"), llvm::SMLoc());

  ast::ASTContext Ctx(SrcMgr);
  basic::DiagnosticsEngine Diags(SrcMgr);
  ast::Program *Program = frontend::parse(Ctx, SrcMgr, FileID, Diags);
  Fn(Ctx, Program, Diags);
}

/// Like `parseSourceWithDiags`, but requires a successful parse; used by
/// tests that only exercise valid programs.
template <typename FnTy> void parseSource(llvm::StringRef Src, FnTy &&Fn) {
  parseSourceWithDiags(Src, [&](ast::ASTContext &Ctx, ast::Program *Program,
                                basic::DiagnosticsEngine &Diags) {
    REQUIRE(Program != nullptr);
    REQUIRE(!Diags.hasErrors());
    Fn(Ctx, Program);
  });
}

} // namespace astra::test
