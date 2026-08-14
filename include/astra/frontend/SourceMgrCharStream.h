#pragma once

#include "CharStream.h"
#include <llvm/Support/SourceMgr.h>

#undef EOF

namespace astra::frontend {

class SourceMgrCharStream : public antlr4::CharStream {
  const llvm::SourceMgr &SourceMgr;
  llvm::StringRef Buffer;
  size_t CurrentIndex = 0;
  unsigned FileID;

public:
  SourceMgrCharStream(const llvm::SourceMgr &SrcMgr, unsigned FileID)
      : SourceMgr(SrcMgr), FileID(FileID) {
    const auto *MB = SourceMgr.getMemoryBuffer(FileID);
    if (MB) {
      Buffer = MB->getBuffer();
    } else {
      // File is invalid.
      // Create an empty buffer, to avoid crashing.
      Buffer = llvm::StringRef("");

      // TODO report error
      assert(false && "SourceMgr is enpty.");
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

  /// mark/release do nothing, we have entire buffer.
  virtual ssize_t mark() override { return CurrentIndex; }
  virtual void release(ssize_t /* marker */) override {}

  virtual std::string getText(const antlr4::misc::Interval &Interval) override {
    // Ensure the interval is valid.
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
