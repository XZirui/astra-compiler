#pragma once

#include <llvm/ADT/StringRef.h>

namespace astra::ast {
class IdentifierInfo {
  friend class ASTContext;

  llvm::StringRef Name;
  bool IsKeyword = false;

  /// Private construct, ensuring that it is created only
  /// by ASTContext via method `getIdentifier`.
  IdentifierInfo(llvm::StringRef Name, bool IsKeyword = false)
      : Name(Name), IsKeyword(IsKeyword) {}

public:
  llvm::StringRef getName() const { return Name; }

  bool operator==(const IdentifierInfo &Other) { return this == &Other; }
  bool operator!=(const IdentifierInfo &Other) { return this != &Other; }

  bool isKeyword() const { return IsKeyword; }
  void setIsKeyword(bool V) { IsKeyword = V; }
};
} // namespace astra::ast
