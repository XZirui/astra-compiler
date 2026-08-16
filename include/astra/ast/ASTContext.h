#pragma once

#include "IdentifierInfo.h"
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/SourceMgr.h>

#include <cstring>
#include <new>

namespace astra::ast {
/// The memory arena that owns all AST nodes and identifiers of one parse.
/// Nothing is freed individually. The caller manages the context's lifetime,
/// and it must outlive the AST built from it.
class ASTContext : public llvm::RefCountedBase<ASTContext> {
  /// The arena backing every allocation the context hands out.
  mutable llvm::BumpPtrAllocator BumpAlloc;
  llvm::SourceMgr &SourceMgr;
  /// The interning table of identifiers, keyed by their spelling.
  llvm::StringMap<IdentifierInfo *> Identifiers;

public:
  /// Construct a context for `SrcMgr`. The context is owned by the caller,
  /// which is also responsible for destroying it.
  explicit ASTContext(llvm::SourceMgr &SrcMgr) : SourceMgr(SrcMgr) {}

  /// Get the interned `IdentifierInfo` for `Name`, creating it on first use.
  /// @param Name The identifier text. It must point to stable memory, e.g.
  ///             the source buffer or a string literal.
  /// @param IsKeyword Whether the identifier is a keyword. Defaults to false.
  IdentifierInfo *getIdentifier(llvm::StringRef Name, bool IsKeyword = false) {
    auto It = Identifiers.find(Name);
    if (It != Identifiers.end()) {
      return It->second;
    }

    void *Mem = allocate(sizeof(IdentifierInfo), alignof(IdentifierInfo));
    auto *Info = new (Mem) IdentifierInfo(Name, IsKeyword);
    Identifiers[Name] = Info;
    return Info;
  }

  llvm::SourceMgr &getSourceMgr() { return SourceMgr; }

  llvm::BumpPtrAllocator &getAllocator() const { return BumpAlloc; }

  /// Allocate `Size` bytes of uninitialized storage from the arena, aligned
  /// to at least `Align` bytes. Individual allocations are never freed.
  void *allocate(size_t Size, unsigned Align = 8) const {
    return BumpAlloc.Allocate(Size, Align);
  }

  /// Allocate storage for `Num` objects of type `T` and default-construct
  /// them (placement new). The objects are never destroyed individually. All
  /// memory is reclaimed when the context dies.
  template <typename T> T *allocate(size_t Num = 1) const {
    void *Mem = allocate(Num * sizeof(T), alignof(T));
    auto *Result = static_cast<T *>(Mem);
    for (size_t I = 0; I < Num; ++I) {
      new (Result + I) T();
    }
    return Result;
  }

  /// Copy `Str` into the arena and return a `StringRef` to the copy.
  /// The copy is never freed; it lives as long as the context.
  llvm::StringRef allocateCopy(llvm::StringRef Str) const {
    if (Str.empty()) {
      return llvm::StringRef();
    }
    void *Mem = allocate(Str.size());
    std::memcpy(Mem, Str.data(), Str.size());
    return llvm::StringRef(static_cast<char *>(Mem), Str.size());
  }
};
} // namespace astra::ast
