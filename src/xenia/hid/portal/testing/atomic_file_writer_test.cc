/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/atomic_file_writer.h"

#include <array>
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <random>

#include "third_party/catch/single_include/catch2/catch.hpp"

namespace xe::hid {
namespace {
class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    for (int attempt = 0; attempt < 100; ++attempt) {
      path = std::filesystem::temp_directory_path() /
             ("xenia-portal-test-" + std::to_string(std::random_device{}()));
      if (std::filesystem::create_directory(path)) {
        return;
      }
    }
    throw std::runtime_error("Cannot create test directory");
  }
  ~TemporaryDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);
  }
  std::filesystem::path path;
};
std::string Read(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
const std::array<uint8_t, 3> kCandidate{'n', 'e', 'w'};

// Inject a failed stage around real filesystem operations. Successful stages,
// including replacement, still run against a private directory.
class FailingTransaction : public AtomicFileTransaction {
 public:
  explicit FailingTransaction(AtomicWriteError stage)
      : native_(CreateNativeAtomicFileTransaction()), stage_(stage) {}
  std::error_code Create(const std::filesystem::path& path) override {
    return stage_ == AtomicWriteError::kCreateTemporaryFailed
               ? Failure()
               : native_->Create(path);
  }
  std::error_code Write(std::span<const uint8_t> bytes) override {
    if (stage_ == AtomicWriteError::kWriteFailed) {
      native_->Write(bytes.first(1));
      return Failure();
    }
    return native_->Write(bytes);
  }
  std::error_code FlushFile() override {
    return stage_ == AtomicWriteError::kFlushFailed ? Failure()
                                                    : native_->FlushFile();
  }
  std::error_code Replace() override {
    return stage_ == AtomicWriteError::kReplaceFailed ? Failure()
                                                      : native_->Replace();
  }
  std::error_code FlushCommit() override {
    return stage_ == AtomicWriteError::kCommitFlushFailed
               ? Failure()
               : native_->FlushCommit();
  }
  std::filesystem::path RetainTemporary() override {
    return native_->RetainTemporary();
  }

 private:
  static std::error_code Failure() {
    return std::make_error_code(std::errc::io_error);
  }
  std::unique_ptr<AtomicFileTransaction> native_;
  AtomicWriteError stage_;
};
}  // namespace

TEST_CASE("Atomic saves replace complete bytes in Unicode paths",
          "[skylanders][atomic-write]") {
  TemporaryDirectory temp;
  const auto destination =
      temp.path / std::filesystem::path(u8"portal café.sky");
  auto writer = CreateNativeAtomicFileWriter();
  auto result = writer->Write(destination, kCandidate);
  REQUIRE(result.outcome == AtomicCommitOutcome::kDurable);
  REQUIRE(result.error == AtomicWriteError::kNone);
  REQUIRE_FALSE(result.system_error);
  REQUIRE(result.recovery_path.empty());
  REQUIRE(Read(destination) == "new");
  const std::array<uint8_t, 1> shorter{'x'};
  REQUIRE(writer->Write(destination, shorter).outcome ==
          AtomicCommitOutcome::kDurable);
  REQUIRE(Read(destination) == "x");
  REQUIRE(std::distance(std::filesystem::directory_iterator(temp.path), {}) ==
          1);
}

TEST_CASE("Save failures report whether replacement happened",
          "[skylanders][atomic-write]") {
  const auto stage = GENERATE(
      AtomicWriteError::kCreateTemporaryFailed, AtomicWriteError::kWriteFailed,
      AtomicWriteError::kFlushFailed, AtomicWriteError::kReplaceFailed,
      AtomicWriteError::kCommitFlushFailed);
  TemporaryDirectory temp;
  const auto destination = temp.path / "figure.sky";
  {
    std::ofstream file(destination);
    file << "old";
  }
  auto writer = CreateAtomicFileWriter(
      [stage] { return std::make_unique<FailingTransaction>(stage); });
  const auto result = writer->Write(destination, kCandidate);
  REQUIRE(result.error == stage);
  REQUIRE(result.system_error == std::make_error_code(std::errc::io_error));
  if (stage == AtomicWriteError::kCommitFlushFailed) {
    REQUIRE(result.outcome == AtomicCommitOutcome::kReplacedDurabilityUnknown);
    REQUIRE(Read(destination) == "new");
    REQUIRE(result.recovery_path.empty());
  } else {
    REQUIRE(result.outcome == AtomicCommitOutcome::kNotReplaced);
    REQUIRE(Read(destination) == "old");
    if (stage == AtomicWriteError::kFlushFailed ||
        stage == AtomicWriteError::kReplaceFailed) {
      REQUIRE_FALSE(result.recovery_path.empty());
      REQUIRE(Read(result.recovery_path) == "new");
    } else {
      REQUIRE(result.recovery_path.empty());
      REQUIRE(std::distance(std::filesystem::directory_iterator(temp.path),
                            {}) == 1);
    }
  }
}

TEST_CASE("Atomic writer rejects a missing parent without creating directories",
          "[skylanders][atomic-write]") {
  TemporaryDirectory temp;
  auto result = CreateNativeAtomicFileWriter()->Write(
      temp.path / "missing" / "figure.sky", kCandidate);
  REQUIRE(result.outcome == AtomicCommitOutcome::kNotReplaced);
  REQUIRE(result.error == AtomicWriteError::kCreateTemporaryFailed);
  REQUIRE(result.system_error);
  REQUIRE_FALSE(std::filesystem::exists(temp.path / "missing"));
}
TEST_CASE("Native replacement failure retains a complete candidate",
          "[skylanders][atomic-write]") {
  TemporaryDirectory temp;
  const auto destination = temp.path / "existing-directory";
  REQUIRE(std::filesystem::create_directory(destination));
  const auto result =
      CreateNativeAtomicFileWriter()->Write(destination, kCandidate);
  REQUIRE(result.outcome == AtomicCommitOutcome::kNotReplaced);
  REQUIRE(result.error == AtomicWriteError::kReplaceFailed);
  REQUIRE(result.system_error);
  REQUIRE(std::filesystem::is_directory(destination));
  REQUIRE(Read(result.recovery_path) == "new");
}
#ifndef _WIN32
TEST_CASE("Read-only parent rejects a save without touching old bytes",
          "[skylanders][atomic-write]") {
  if (geteuid() == 0) {
    WARN(
        "Root bypasses POSIX permission checks; read-only test not applicable");
    return;
  }
  TemporaryDirectory temp;
  const auto destination = temp.path / "figure.sky";
  {
    std::ofstream file(destination);
    file << "old";
  }
  std::filesystem::permissions(
      temp.path,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  const auto result =
      CreateNativeAtomicFileWriter()->Write(destination, kCandidate);
  std::filesystem::permissions(temp.path, std::filesystem::perms::owner_all);
  REQUIRE(result.outcome == AtomicCommitOutcome::kNotReplaced);
  REQUIRE(result.error == AtomicWriteError::kCreateTemporaryFailed);
  REQUIRE(result.system_error == std::errc::permission_denied);
  REQUIRE(Read(destination) == "old");
}
#endif
}  // namespace xe::hid
