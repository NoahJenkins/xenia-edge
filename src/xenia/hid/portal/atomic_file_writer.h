/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_ATOMIC_FILE_WRITER_H_
#define XENIA_HID_PORTAL_ATOMIC_FILE_WRITER_H_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <system_error>

namespace xe::hid {
enum class AtomicCommitOutcome {
  kNotReplaced,
  kDurable,
  kReplacedDurabilityUnknown,
};
enum class AtomicWriteError {
  kNone,
  kCreateTemporaryFailed,
  kWriteFailed,
  kFlushFailed,
  kReplaceFailed,
  kCommitFlushFailed,
};
struct AtomicWriteResult {
  // A default result must never acknowledge a save.
  AtomicCommitOutcome outcome = AtomicCommitOutcome::kNotReplaced;
  AtomicWriteError error = AtomicWriteError::kNone;
  std::filesystem::path recovery_path;
  std::error_code system_error;
};
class AtomicFileWriter {
 public:
  virtual ~AtomicFileWriter() = default;
  // The destination parent must already exist. Callers serialize writes.
  virtual AtomicWriteResult Write(const std::filesystem::path& destination,
                                  std::span<const uint8_t> bytes) = 0;
};

// Platform transaction boundary. Each object is used for exactly one save.
// Destruction closes resources and removes incomplete temporary files.
// Replace returning an error means no replacement occurred. FlushCommit runs
// only after a successful Replace; its failure cannot undo replacement.
class AtomicFileTransaction {
 public:
  virtual ~AtomicFileTransaction() = default;
  virtual std::error_code Create(const std::filesystem::path& destination) = 0;
  virtual std::error_code Write(std::span<const uint8_t> bytes) = 0;
  virtual std::error_code FlushFile() = 0;
  virtual std::error_code Replace() = 0;
  virtual std::error_code FlushCommit() = 0;
  // Only call after a complete Write, before replacement.
  virtual std::filesystem::path RetainTemporary() = 0;
};
using AtomicTransactionFactory =
    std::function<std::unique_ptr<AtomicFileTransaction>()>;
std::unique_ptr<AtomicFileTransaction> CreateNativeAtomicFileTransaction();
std::unique_ptr<AtomicFileWriter> CreateAtomicFileWriter(
    AtomicTransactionFactory factory);
std::unique_ptr<AtomicFileWriter> CreateNativeAtomicFileWriter();
}  // namespace xe::hid
#endif  // XENIA_HID_PORTAL_ATOMIC_FILE_WRITER_H_
