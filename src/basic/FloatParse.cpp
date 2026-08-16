#include "astra/basic/FloatParse.h"

#include <llvm/Support/Error.h>

namespace astra::basic {

bool convertFloatString(llvm::APFloat &Value, llvm::StringRef Text,
                        llvm::APFloat::roundingMode RM) {
  auto Parsed = Value.convertFromString(Text, RM);
  if (!Parsed) {
    llvm::consumeError(Parsed.takeError());
    return false;
  }
  return true;
}

} // namespace astra::basic
