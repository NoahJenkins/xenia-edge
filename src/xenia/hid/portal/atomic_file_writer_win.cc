/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/atomic_file_writer.h"

#include "xenia/base/platform_win.h"

#include <algorithm>
#include <atomic>
#include <limits>

namespace xe::hid {
namespace {
std::error_code LastError() {
  return {static_cast<int>(GetLastError()), std::system_category()};
}
class WindowsTransaction final : public AtomicFileTransaction {
 public:
  ~WindowsTransaction() override {
    if (file_ != INVALID_HANDLE_VALUE) {
      CloseHandle(file_);
    }
    if (!temporary_.empty() && !retained_) {
      DeleteFileW(temporary_.c_str());
    }
  }
  std::error_code Create(const std::filesystem::path& destination) override {
    destination_ = destination;
    static std::atomic<uint64_t> sequence{0};
    for (int attempt = 0; attempt < 100; ++attempt) {
      temporary_ = destination.parent_path() /
                   (L".xenia-portal-" + std::to_wstring(GetCurrentProcessId()) +
                    L"-" + std::to_wstring(sequence.fetch_add(1)) + L".tmp");
      file_ =
          CreateFileW(temporary_.c_str(), GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (file_ != INVALID_HANDLE_VALUE) {
        return {};
      }
      auto error = LastError();
      // We do not own an existing file and must not remove it on destruction.
      temporary_.clear();
      if (error.value() != ERROR_FILE_EXISTS &&
          error.value() != ERROR_ALREADY_EXISTS) {
        return error;
      }
    }
    return std::make_error_code(std::errc::file_exists);
  }
  std::error_code Write(std::span<const uint8_t> bytes) override {
    while (!bytes.empty()) {
      DWORD written = 0;
      const auto count = static_cast<DWORD>(
          std::min(bytes.size(),
                   static_cast<size_t>(std::numeric_limits<DWORD>::max())));
      if (!WriteFile(file_, bytes.data(), count, &written, nullptr)) {
        return LastError();
      }
      if (!written) {
        return std::make_error_code(std::errc::io_error);
      }
      bytes = bytes.subspan(written);
    }
    return {};
  }
  std::error_code FlushFile() override {
    return FlushFileBuffers(file_) ? std::error_code{} : LastError();
  }
  std::error_code Replace() override {
    if (!MoveFileExW(temporary_.c_str(), destination_.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      return LastError();
    }
    temporary_.clear();
    return {};
  }
  std::error_code PublishNew() override {
    if (!MoveFileExW(temporary_.c_str(), destination_.c_str(),
                     MOVEFILE_WRITE_THROUGH)) {
      return LastError();
    }
    temporary_.clear();
    return {};
  }
  // MoveFileExW requests write-through for the namespace change. Confirm file
  // buffers once more after replacement; any failure is an uncertain commit.
  std::error_code FlushCommit() override { return FlushFile(); }
  std::filesystem::path RetainTemporary() override {
    retained_ = true;
    return temporary_;
  }

 private:
  HANDLE file_ = INVALID_HANDLE_VALUE;
  bool retained_ = false;
  std::filesystem::path temporary_;
  std::filesystem::path destination_;
};
}  // namespace
std::unique_ptr<AtomicFileTransaction> CreateNativeAtomicFileTransaction() {
  return std::make_unique<WindowsTransaction>();
}
}  // namespace xe::hid
