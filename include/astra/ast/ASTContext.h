#pragma once

#include "IdentifierInfo.h"
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/SourceMgr.h>

namespace astra::ast {
class ASTContext : public llvm::RefCountedBase<ASTContext> {
  mutable llvm::BumpPtrAllocator BumpAlloc;
  llvm::SourceMgr &SourceMgr;
  llvm::StringMap<IdentifierInfo *> Idenfitifers;

public:
  /// Need a valid constructor, whose lifecycle is managed by the caller.
  explicit ASTContext(llvm::SourceMgr &SrcMgr) : SourceMgr(SrcMgr) {}

  /// Get or create an IdentifierInfo.
  /// @param Name The string of idenfitifer, must points to stable memory.
  /// @param IsKeyword (optional) Whether this idenfitifer is a keyword.
  IdentifierInfo *getIdentifier(llvm::StringRef Name, bool IsKeyword = false) {
    auto It = Idenfitifers.find(Name);
    if (It != Idenfitifers.end()) {
      return It->second;
    }

    void *Mem = allocate(sizeof(IdentifierInfo), alignof(IdentifierInfo));
    auto *Info = new (Mem) IdentifierInfo(Name, IsKeyword);
    Idenfitifers[Name] = Info;
    return Info;
  }

  llvm::SourceMgr &getSourceMgr() { return SourceMgr; }

  llvm::BumpPtrAllocator &getAllocator() const { return BumpAlloc; }

  void *allocate(size_t Size, unsigned Align = 8) const {
    return BumpAlloc.Allocate(Size, Align);
  }

  template <typename T> T *allocate(size_t Num = 1) const {
    return static_cast<T *>(allocate(Num * sizeof(T), alignof(T)));
  }
};
} // namespace astra::ast
