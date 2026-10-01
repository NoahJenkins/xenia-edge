/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/atomic_file_writer.h"

#include <utility>

namespace xe::hid {
namespace {
class TransactionalFileWriter final : public AtomicFileWriter {
 public:
  explicit TransactionalFileWriter(AtomicTransactionFactory factory)
      : factory_(std::move(factory)) {}
  AtomicWriteResult Write(const std::filesystem::path& destination,
                          std::span<const uint8_t> bytes) override {
    return WriteImpl(destination, bytes, false);
  }
  AtomicWriteResult WriteNew(const std::filesystem::path& destination,
                             std::span<const uint8_t> bytes) override {
    return WriteImpl(destination, bytes, true);
  }

 private:
  AtomicWriteResult WriteImpl(const std::filesystem::path& destination,
                              std::span<const uint8_t> bytes, bool new_only) {
    auto transaction = factory_();
    AtomicWriteResult result;
    const auto fail = [&](AtomicWriteError stage, std::error_code error,
                          bool complete = false) {
      result.error = stage;
      result.system_error = error;
      if (complete) {
        result.recovery_path = transaction->RetainTemporary();
      }
      return result;
    };
    if (auto error = transaction->Create(destination)) {
      return fail(AtomicWriteError::kCreateTemporaryFailed, error);
    }
    if (auto error = transaction->Write(bytes)) {
      return fail(AtomicWriteError::kWriteFailed, error);
    }
    if (auto error = transaction->FlushFile()) {
      return fail(AtomicWriteError::kFlushFailed, error, true);
    }
    if (auto error =
            new_only ? transaction->PublishNew() : transaction->Replace()) {
      const bool useful_candidate =
          !(new_only && error == std::errc::file_exists);
      return fail(AtomicWriteError::kReplaceFailed, error, useful_candidate);
    }
    result.outcome = AtomicCommitOutcome::kReplacedDurabilityUnknown;
    if (auto error = transaction->FlushCommit()) {
      return fail(AtomicWriteError::kCommitFlushFailed, error);
    }
    result.outcome = AtomicCommitOutcome::kDurable;
    return result;
  }

  AtomicTransactionFactory factory_;
};
}  // namespace
std::unique_ptr<AtomicFileWriter> CreateAtomicFileWriter(
    AtomicTransactionFactory factory) {
  return std::make_unique<TransactionalFileWriter>(std::move(factory));
}
std::unique_ptr<AtomicFileWriter> CreateNativeAtomicFileWriter() {
  return CreateAtomicFileWriter(CreateNativeAtomicFileTransaction);
}
}  // namespace xe::hid
