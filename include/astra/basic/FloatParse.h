#pragma once

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/StringRef.h>

namespace astra::basic {

/// Parse `Text` as a floating-point value into `Value`, following rounding
/// mode `RM`. Returns the conversion status; `opInvalidOp` means `Text` was
/// not a valid floating-point literal. `opOverflow`/`opUnderflow` signal a
/// value out of range (the value is still set, to infinity or zero).
///
/// This lives in `astra-basic` (compiled with -fno-rtti) because
/// `APFloat::convertFromString` returns `llvm::Expected<opStatus>`: the
/// `llvm::Error` machinery must not be instantiated in RTTI compilation
/// units, or linking fails with a missing `typeinfo for llvm::ErrorInfoBase`.
llvm::APFloat::opStatus convertFloatString(llvm::APFloat &Value,
                                           llvm::StringRef Text,
                                           llvm::APFloat::roundingMode RM);

} // namespace astra::basic
