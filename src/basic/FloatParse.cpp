#include "astra/basic/FloatParse.h"

#include <llvm/Support/Error.h>

namespace astra::basic {

llvm::APFloat::opStatus convertFloatString(llvm::APFloat &Value,
                                           llvm::StringRef Text,
                                           llvm::APFloat::roundingMode RM) {
  auto Parsed = Value.convertFromString(Text, RM);
  if (!Parsed) {
    llvm::consumeError(Parsed.takeError());
    // `opInvalidOp` doubles as the parse-failure marker: a successful
    // conversion never returns it.
    return llvm::APFloat::opInvalidOp;
  }
  return *Parsed;
}

} // namespace astra::basic
