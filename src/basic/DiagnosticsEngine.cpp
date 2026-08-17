#include "astra/basic/DiagnosticsEngine.h"

#include <llvm/ADT/STLExtras.h>

namespace astra::basic {

void DiagnosticsEngine::report(llvm::SMRange Range,
                               llvm::SourceMgr::DiagKind Kind,
                               llvm::StringRef Message) {
  Diagnostics.push_back(
      SrcMgr.GetMessage(Range.Start, Kind, Message, llvm::SMRange(Range)));
  HasErrors = (Kind == llvm::SourceMgr::DK_Error) ? true : HasErrors;
}

void DiagnosticsEngine::report(llvm::SMLoc Loc, llvm::SourceMgr::DiagKind Kind,
                               llvm::StringRef Message) {
  report(llvm::SMRange(Loc, Loc), Kind, Message);
}

bool DiagnosticsEngine::hasErrors() const { return HasErrors; }

void DiagnosticsEngine::print(llvm::raw_ostream &OS) {
  for (const auto &D : Diagnostics) {
    SrcMgr.PrintMessage(OS, D);
  }
  Diagnostics.clear();
  HasErrors = false;
}

} // namespace astra::basic
