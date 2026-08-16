#pragma once

#include <llvm/ADT/SmallVector.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

namespace astra::basic {

/// Collects diagnostics and prints them LLVM-style (clang format). The
/// diagnostic carrier (`llvm::SMDiagnostic`), construction
/// (`SourceMgr::GetMessage`, which snapshots line/column/source line) and
/// printing (`SourceMgr::PrintMessage`) are all LLVM facilities; this engine
/// only collects, counts and forwards.
///
/// Note: `SMDiagnostic` keeps a pointer to the `SourceMgr`, so the engine
/// must not outlive it (the engine's own reference enforces the same rule).
class DiagnosticsEngine {
  llvm::SourceMgr &SourceMgr;
  llvm::SmallVector<llvm::SMDiagnostic, 4> Diagnostics;
  bool HasErrors = false;

public:
  explicit DiagnosticsEngine(llvm::SourceMgr &SrcMgr) : SourceMgr(SrcMgr) {}

  /// Report a diagnostic covering `Range`. The range is highlighted with
  /// tildes when printed.
  void report(llvm::SMRange Range, llvm::SourceMgr::DiagKind Kind,
              llvm::StringRef Message);

  /// Report a single-point diagnostic.
  void report(llvm::SMLoc Loc, llvm::SourceMgr::DiagKind Kind,
              llvm::StringRef Message);

  bool hasErrors() const;

  llvm::ArrayRef<llvm::SMDiagnostic> getDiagnostics() const {
    return Diagnostics;
  }

  /// Print all diagnostics in clang style (`file:line:col: kind: message`
  /// plus the source line with a caret/tilde marker), then clear the engine.
  void print(llvm::raw_ostream &OS);
};

} // namespace astra::basic
