/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_FIGURE_STORE_H_
#define XENIA_HID_PORTAL_FIGURE_STORE_H_

#include <map>
#include <optional>

#include "xenia/hid/portal/atomic_file_writer.h"
#include "xenia/hid/portal/figure_image.h"
#include "xenia/hid/portal/portal_session_store.h"

namespace xe::hid {
enum class FigureStoreError {
  kNone,
  kInvalidImage,
  kIoFailed,
  kOutsideLibrary,
  kAlreadyLoaded,
  kAlreadyExists,
  kOverwriteNotConfirmed,
  kExternalConflict,
  kRecoveryRequired,
  kPersistenceFailed,
  kUnavailable,
  kReadOnly,
};
struct FigureLoadResult {
  FigureStoreError error = FigureStoreError::kNone;
  std::optional<FigureHandle> handle;
  FigureValidationReport validation;
  AtomicWriteResult persistence;
};
struct FigureStoreWriteResult {
  FigureIoError error = FigureIoError::kNone;
  AtomicWriteResult persistence;
};
struct FigureStoreResult {
  FigureStoreError error = FigureStoreError::kNone;
  AtomicWriteResult persistence;
};

// Raw figure storage, not a creator or crypto repair layer. Loading preserves
// validation warnings. Callers must serialize all operations and explicitly
// select managed use; external files are read-only. No runtime integration yet.
class FigureStore {
 public:
  FigureStore(std::filesystem::path library_root,
              std::unique_ptr<AtomicFileWriter> writer,
              std::unique_ptr<AtomicFileWriter> session_writer =
                  CreateNativeAtomicFileWriter(),
              std::filesystem::path session_root = {});
  FigureLoadResult LoadManaged(const std::filesystem::path& path);
  FigureLoadResult LoadReadOnly(const std::filesystem::path& path);
  FigureLoadResult Import(const std::filesystem::path& source,
                          const std::filesystem::path& destination);
  FigureStoreResult Export(FigureHandle handle,
                           const std::filesystem::path& destination,
                           bool overwrite_confirmed);
  FigureBlockReadResult ReadBlock(FigureHandle handle, uint8_t block) const;
  FigureStoreWriteResult WriteBlock(
      FigureHandle handle, uint8_t block,
      std::span<const uint8_t, kFigureBlockSize> bytes);
  // Explicit recovery reopens, validates, checks identity, and durably saves
  // the actual file. Only success issues a fresh handle. Old handles stay dead.
  FigureLoadResult Recover(FigureHandle handle);
  FigureLoadResult Recover(const std::filesystem::path& path);

 private:
  struct Snapshot {
    std::array<uint8_t, kFigureSize> bytes{};
    std::filesystem::file_time_type modified;
    uint64_t fingerprint = 0;
    bool read_only = false;
  };
  struct Entry {
    std::filesystem::path path;
    FigureImage image;
    FigureValidationReport validation;
    Snapshot snapshot;
    bool read_only = false;
    bool available = true;
    bool recovery_required = false;
  };
  static FigureStoreError ReadSnapshot(const std::filesystem::path& path,
                                       Snapshot& snapshot);
  FigureLoadResult Load(const std::filesystem::path& path, bool managed);
  FigureLoadResult Add(Entry entry);
  FigureLoadResult RecoverPath(const std::filesystem::path& path,
                               const FigureImage* expected);
  static bool SameVersion(const Snapshot& a, const Snapshot& b);
  static bool SameIdentity(const FigureImage& a, const FigureImage& b);
  bool InLibrary(const std::filesystem::path& path) const;
  std::filesystem::path library_root_;
  std::unique_ptr<AtomicFileWriter> writer_;
  PortalSessionStore session_store_;
  std::map<FigureHandle, Entry> entries_;
  FigureHandle next_handle_ = 1;
};
}  // namespace xe::hid
#endif  // XENIA_HID_PORTAL_FIGURE_STORE_H_
