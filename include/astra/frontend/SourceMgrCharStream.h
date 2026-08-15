#pragma once

#include "CharStream.h"
#include <algorithm>
#include <llvm/Support/SourceMgr.h>

// The included C headers define the macro `EOF`. ANTLR uses the same name for
// its end-of-input sentinel, so undefine the macro and let `EOF` denote
// `IntStream::EOF`.
#undef EOF

namespace astra::frontend {

/// Adapt a buffer of `llvm::SourceMgr` to the ANTLR `CharStream` interface.
/// The whole buffer is held in memory, so `consume` and `seek` only move an
/// index into it. The underlying buffer must outlive the lexer.
class SourceMgrCharStream : public antlr4::CharStream {
  const llvm::SourceMgr &SourceMgr;
  /// The file content. Points into the buffer owned by `SourceMgr`.
  llvm::StringRef Buffer;
  /// The current read position inside `Buffer`.
  size_t CurrentIndex = 0;
  /// The `FileID` of the adapted buffer.
  unsigned FileID;

public:
  SourceMgrCharStream(const llvm::SourceMgr &SrcMgr, unsigned FileID)
      : SourceMgr(SrcMgr), FileID(FileID) {
    const auto *MB = SourceMgr.getMemoryBuffer(FileID);
    if (MB) {
      Buffer = MB->getBuffer();
    } else {
      // The file is invalid. Fall back to an empty buffer so the lexer does
      // not crash. TODO report a proper error instead.
      Buffer = llvm::StringRef("");

      assert(false && "SourceMgr is empty.");
    }
  }

  virtual void consume() override {
    if (CurrentIndex < Buffer.size()) {
      ++CurrentIndex;
    }
  }

  virtual size_t LA(ssize_t I) override {
    if (I == 0) {
      return 0;
    }

    auto Pos = static_cast<ssize_t>(CurrentIndex) + I - 1;
    if (Pos < 0 || Pos >= static_cast<ssize_t>(Buffer.size())) {
      // IntStream::EOF, not macro `EOF`
      return EOF;
    }
    return static_cast<unsigned char>(Buffer[Pos]);
  }

  virtual size_t index() override { return CurrentIndex; }

  virtual size_t size() override { return Buffer.size(); }

  virtual void seek(size_t Index) override {
    CurrentIndex = std::min(Index, Buffer.size());
  }

  /// mark/release do nothing. The whole buffer is already in memory, so
  /// there is no state to save or restore.
  virtual ssize_t mark() override { return CurrentIndex; }
  virtual void release(ssize_t /* marker */) override {}

  virtual std::string getText(const antlr4::misc::Interval &Interval) override {
    // Clamp the requested interval to the buffer bounds.
    auto Start = static_cast<size_t>(Interval.a);
    auto Length = static_cast<size_t>(Interval.b - Interval.a + 1);
    if (Start >= Buffer.size()) {
      return "";
    }

    if (Start + Length > Buffer.size()) {
      Length = Buffer.size() - Start;
    }
    return Buffer.substr(Start, Length).str();
  }

  virtual std::string toString() const override { return Buffer.str(); }

  virtual std::string getSourceName() const override {
    auto FileName =
        SourceMgr.getMemoryBuffer(FileID)->getBufferIdentifier().str();
    if (FileName.empty()) {
      return "<input>";
    }
    return FileName;
  }
};

} // namespace astra::frontend
