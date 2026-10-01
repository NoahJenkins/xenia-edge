/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/atomic_file_writer.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <vector>

namespace xe::hid {
namespace {
std::error_code LastError() { return {errno, std::generic_category()}; }
std::error_code Sync(int fd) {
  int result;
  do {
    result = fsync(fd);
  } while (result == -1 && errno == EINTR);
  return result == -1 ? LastError() : std::error_code{};
}
std::error_code SyncFile(int fd) {
#ifdef __APPLE__
  // fsync alone need not flush the drive cache on macOS. Do not silently
  // downgrade a failed full flush to an ordinary fsync.
  int result;
  do {
    result = fcntl(fd, F_FULLFSYNC);
  } while (result == -1 && errno == EINTR);
  return result == -1 ? LastError() : std::error_code{};
#else
  return Sync(fd);
#endif
}
class PosixTransaction final : public AtomicFileTransaction {
 public:
  ~PosixTransaction() override {
    if (file_ != -1) {
      close(file_);
    }
    if (!temporary_.empty() && !retained_) {
      unlink(temporary_.c_str());
    }
    if (directory_ != -1) {
      close(directory_);
    }
  }
  std::error_code Create(const std::filesystem::path& destination) override {
    destination_ = destination;
    auto parent = destination.parent_path();
    if (parent.empty()) {
      parent = ".";
    }
    directory_ = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory_ == -1) {
      return LastError();
    }
    auto pattern = (parent / ".xenia-portal-XXXXXX").string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    file_ = mkstemp(name.data());
    if (file_ == -1) {
      return LastError();
    }
    temporary_ = name.data();
    if (fcntl(file_, F_SETFD, FD_CLOEXEC) == -1) {
      return LastError();
    }
    return {};
  }
  std::error_code Write(std::span<const uint8_t> bytes) override {
    while (!bytes.empty()) {
      const auto count =
          std::min(bytes.size(),
                   static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
      auto written = write(file_, bytes.data(), count);
      if (written == -1) {
        if (errno == EINTR) {
          continue;
        }
        return LastError();
      }
      if (written == 0) {
        return std::make_error_code(std::errc::io_error);
      }
      bytes = bytes.subspan(static_cast<size_t>(written));
    }
    return {};
  }
  std::error_code FlushFile() override { return SyncFile(file_); }
  std::error_code Replace() override {
    if (rename(temporary_.c_str(), destination_.c_str()) == -1) {
      return LastError();
    }
    temporary_.clear();
    return {};
  }
  std::error_code FlushCommit() override {
    if (auto error = Sync(directory_)) {
      return error;
    }
#ifdef __APPLE__
    // Flush the drive again after the directory metadata reaches the device.
    return SyncFile(file_);
#else
    return {};
#endif
  }
  std::filesystem::path RetainTemporary() override {
    retained_ = true;
    return temporary_;
  }

 private:
  int file_ = -1;
  int directory_ = -1;
  bool retained_ = false;
  std::filesystem::path temporary_;
  std::filesystem::path destination_;
};
}  // namespace
std::unique_ptr<AtomicFileTransaction> CreateNativeAtomicFileTransaction() {
  return std::make_unique<PosixTransaction>();
}
}  // namespace xe::hid
