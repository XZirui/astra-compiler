#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/StringRef.h>

namespace astra::basic {

/// Parse `Text` as a floating-point value into `Value`, following rounding
/// mode `RM`. Returns false on failure.
///
/// This lives in `astra-basic` (compiled with -fno-rtti) because
/// `APFloat::convertFromString` returns `llvm::Expected<opStatus>`: the
/// `llvm::Error` machinery must not be instantiated in RTTI compilation
/// units, or linking fails with a missing `typeinfo for llvm::ErrorInfoBase`.
bool convertFloatString(llvm::APFloat &Value, llvm::StringRef Text,
                        llvm::APFloat::roundingMode RM);

} // namespace astra::basic
