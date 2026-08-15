#pragma once

#include "astra/ast/ASTContext.h"
#include "astra/ast/Program.h"
#include "astra/frontend/Parse.h"

#include <catch2/catch_test_macros.hpp>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>

namespace astra::test {

/// Parse `Src` as an astra program and invoke `Fn(Ctx, Program)` while the
/// SourceMgr/ASTContext/Program are alive. The SourceMgr is declared first so
/// it is destroyed last: AST node `Range`s point into its buffer. The
/// callback form keeps the lifetimes structurally impossible to misuse.
template <typename FnTy> void parseSource(llvm::StringRef Src, FnTy &&Fn) {
  llvm::SourceMgr SrcMgr;
  unsigned FileID = SrcMgr.AddNewSourceBuffer(
      llvm::MemoryBuffer::getMemBufferCopy(Src, "<test>"), llvm::SMLoc());

  ast::ASTContext Ctx(SrcMgr);
  ast::Program *Program = frontend::parse(Ctx, SrcMgr, FileID);
  REQUIRE(Program != nullptr);

  Fn(Ctx, Program);
}

} // namespace astra::test
