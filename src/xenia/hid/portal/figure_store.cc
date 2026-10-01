/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/figure_store.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "xenia/base/xxhash.h"

namespace xe::hid {
namespace {
FigureLoadResult Error(FigureStoreError error) {
  FigureLoadResult result;
  result.error = error;
  return result;
}
constexpr auto kWritePermissions = std::filesystem::perms::owner_write |
                                   std::filesystem::perms::group_write |
                                   std::filesystem::perms::others_write;
}  // namespace
FigureStore::FigureStore(std::filesystem::path library_root,
                         std::unique_ptr<AtomicFileWriter> writer)
    : writer_(std::move(writer)) {
  std::error_code error;
  library_root_ = std::filesystem::canonical(library_root, error);
  if (error) {
    library_root_.clear();
  }
}
bool FigureStore::InLibrary(const std::filesystem::path& path) const {
  if (library_root_.empty()) {
    return false;
  }
  auto file = path.begin();
  for (const auto& component : library_root_) {
    if (file == path.end() || *file++ != component) {
      return false;
    }
  }
  return file != path.end();
}
FigureStoreError FigureStore::ReadSnapshot(const std::filesystem::path& path,
                                           Snapshot& snapshot) {
  std::error_code error;
  auto status = std::filesystem::status(path, error);
  if (error || !std::filesystem::is_regular_file(status)) {
    return FigureStoreError::kIoFailed;
  }
  auto size = std::filesystem::file_size(path, error);
  if (error) {
    return FigureStoreError::kIoFailed;
  }
  if (size != kFigureSize) {
    return FigureStoreError::kInvalidImage;
  }
  const auto before = std::filesystem::last_write_time(path, error);
  if (error) {
    return FigureStoreError::kIoFailed;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return FigureStoreError::kIoFailed;
  }
  input.read(reinterpret_cast<char*>(snapshot.bytes.data()),
             snapshot.bytes.size());
  if (input.gcount() != kFigureSize || input.bad()) {
    return FigureStoreError::kIoFailed;
  }
  if (input.peek() != std::char_traits<char>::eof() || input.bad()) {
    return FigureStoreError::kExternalConflict;
  }
  snapshot.modified = std::filesystem::last_write_time(path, error);
  if (error) {
    return FigureStoreError::kIoFailed;
  }
  if (before != snapshot.modified) {
    return FigureStoreError::kExternalConflict;
  }
  snapshot.fingerprint =
      XXH3_64bits(snapshot.bytes.data(), snapshot.bytes.size());
  snapshot.read_only = (status.permissions() & kWritePermissions) ==
                       std::filesystem::perms::none;
  return FigureStoreError::kNone;
}
bool FigureStore::SameVersion(const Snapshot& a, const Snapshot& b) {
  return a.modified == b.modified && a.fingerprint == b.fingerprint;
}
bool FigureStore::SameIdentity(const FigureImage& a, const FigureImage& b) {
  return a.identity().character_id == b.identity().character_id &&
         a.identity().variant_id == b.identity().variant_id &&
         std::equal(a.bytes().begin(), a.bytes().begin() + 4,
                    b.bytes().begin());
}
FigureLoadResult FigureStore::Add(Entry entry) {
  if (next_handle_ == 0) {
    return Error(FigureStoreError::kUnavailable);
  }
  FigureLoadResult result;
  result.validation = entry.validation;
  result.handle = next_handle_++;
  entries_.emplace(*result.handle, std::move(entry));
  return result;
}
FigureLoadResult FigureStore::LoadManaged(const std::filesystem::path& path) {
  return Load(path, true);
}
FigureLoadResult FigureStore::LoadReadOnly(const std::filesystem::path& path) {
  return Load(path, false);
}
FigureLoadResult FigureStore::Load(const std::filesystem::path& path,
                                   bool managed) {
  std::error_code error;
  auto canonical = std::filesystem::canonical(path, error);
  if (error) {
    return Error(FigureStoreError::kIoFailed);
  }
  if (managed && !InLibrary(canonical)) {
    return Error(FigureStoreError::kOutsideLibrary);
  }
  for (const auto& [handle, entry] : entries_) {
    const bool same = canonical == entry.path ||
                      std::filesystem::equivalent(canonical, entry.path, error);
    if (same && entry.recovery_required) {
      return Error(FigureStoreError::kRecoveryRequired);
    }
    if (same && entry.available) {
      return Error(FigureStoreError::kAlreadyLoaded);
    }
  }
  Snapshot snapshot;
  auto status = ReadSnapshot(canonical, snapshot);
  if (status != FigureStoreError::kNone) {
    return Error(status);
  }
  FigureValidationReport validation;
  auto image = FigureImage::Parse(snapshot.bytes, validation);
  if (!image || !validation.IsSafeToLoad()) {
    auto result = Error(FigureStoreError::kInvalidImage);
    result.validation = std::move(validation);
    return result;
  }
  return Add({canonical, *image, validation, snapshot,
              !managed || snapshot.read_only});
}
FigureBlockReadResult FigureStore::ReadBlock(FigureHandle handle,
                                             uint8_t block) const {
  auto entry = entries_.find(handle);
  if (entry == entries_.end() || !entry->second.available) {
    return {FigureIoError::kUnavailable};
  }
  auto data = entry->second.image.ReadBlock(block);
  if (!data) {
    return {FigureIoError::kInvalidBlock};
  }
  return {FigureIoError::kNone, *data};
}
FigureStoreWriteResult FigureStore::WriteBlock(
    FigureHandle handle, uint8_t block,
    std::span<const uint8_t, kFigureBlockSize> bytes) {
  auto found = entries_.find(handle);
  if (found == entries_.end() || !found->second.available) {
    return {FigureIoError::kUnavailable};
  }
  auto& entry = found->second;
  if (block >= kFigureBlockCount) {
    return {FigureIoError::kInvalidBlock};
  }
  if (entry.read_only) {
    return {FigureIoError::kReadOnly};
  }
  Snapshot current;
  if (ReadSnapshot(entry.path, current) != FigureStoreError::kNone ||
      !SameVersion(current, entry.snapshot)) {
    return {FigureIoError::kConflict};
  }
  if (current.read_only) {
    return {FigureIoError::kReadOnly};
  }
  auto candidate = entry.image;
  candidate.ReplaceBlock(block, bytes);
  auto persistence = writer_->Write(entry.path, candidate.bytes());
  if (persistence.outcome == AtomicCommitOutcome::kNotReplaced) {
    return {FigureIoError::kPersistenceFailed, persistence};
  }
  entry.image = candidate;
  entry.validation = candidate.Validate();
  if (persistence.outcome != AtomicCommitOutcome::kDurable ||
      persistence.error != AtomicWriteError::kNone ||
      ReadSnapshot(entry.path, current) != FigureStoreError::kNone ||
      !std::equal(current.bytes.begin(), current.bytes.end(),
                  candidate.bytes().begin())) {
    entry.available = false;
    entry.recovery_required = true;
    return {FigureIoError::kPersistenceFailed, persistence};
  }
  entry.snapshot = current;
  return {FigureIoError::kNone, persistence};
}
FigureLoadResult FigureStore::Recover(FigureHandle handle) {
  auto found = entries_.find(handle);
  if (found == entries_.end() || !found->second.recovery_required) {
    return Error(FigureStoreError::kUnavailable);
  }
  auto& entry = found->second;
  Snapshot current;
  auto status = ReadSnapshot(entry.path, current);
  if (status != FigureStoreError::kNone) {
    return Error(status);
  }
  FigureValidationReport validation;
  auto image = FigureImage::Parse(current.bytes, validation);
  if (!image || !validation.IsSafeToLoad()) {
    auto result = Error(FigureStoreError::kInvalidImage);
    result.validation = validation;
    return result;
  }
  if (!SameIdentity(*image, entry.image)) {
    return Error(FigureStoreError::kExternalConflict);
  }
  if (current.read_only) {
    return Error(FigureStoreError::kReadOnly);
  }
  // Recheck immediately before saving. Concurrent external writers are not
  // supported; a later manager must serialize access to this store.
  Snapshot check;
  if (ReadSnapshot(entry.path, check) != FigureStoreError::kNone ||
      !SameVersion(current, check)) {
    return Error(FigureStoreError::kExternalConflict);
  }
  const auto persistence = writer_->Write(entry.path, image->bytes());
  if (persistence.outcome != AtomicCommitOutcome::kNotReplaced) {
    entry.image = *image;
    entry.validation = validation;
  }
  if (persistence.outcome != AtomicCommitOutcome::kDurable ||
      persistence.error != AtomicWriteError::kNone ||
      ReadSnapshot(entry.path, check) != FigureStoreError::kNone ||
      check.bytes != current.bytes) {
    auto result = Error(FigureStoreError::kPersistenceFailed);
    result.persistence = persistence;
    return result;
  }
  auto result = Add({entry.path, *image, validation, check, false});
  result.persistence = persistence;
  if (result.handle) {
    entry.recovery_required = false;
  }
  return result;
}
}  // namespace xe::hid
