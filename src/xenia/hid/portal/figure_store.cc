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
std::string HeaderHex(const FigureImage& image) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const uint8_t byte : image.bytes().first(32)) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}
auto FindPending(PortalSession& session,
                 const std::filesystem::path& relative) {
  return std::find_if(
      session.pending_saves.begin(), session.pending_saves.end(),
      [&](const auto& item) { return item.relative_path == relative; });
}
}  // namespace
FigureStore::FigureStore(std::filesystem::path library_root,
                         std::unique_ptr<AtomicFileWriter> writer,
                         std::unique_ptr<AtomicFileWriter> session_writer,
                         std::filesystem::path session_root)
    : writer_(std::move(writer)),
      session_store_(library_root, std::move(session_writer),
                     std::move(session_root)) {
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
FigureLoadResult FigureStore::Import(const std::filesystem::path& source,
                                     const std::filesystem::path& destination) {
  std::error_code error;
  const auto parent =
      std::filesystem::canonical(destination.parent_path(), error);
  if (error || !InLibrary(parent / destination.filename()) ||
      destination.filename().empty()) {
    return Error(FigureStoreError::kOutsideLibrary);
  }
  const auto target = parent / destination.filename();
  if (std::filesystem::exists(target, error)) {
    return Error(FigureStoreError::kAlreadyExists);
  }
  if (error) {
    return Error(FigureStoreError::kIoFailed);
  }
  Snapshot source_snapshot;
  auto status = ReadSnapshot(source, source_snapshot);
  if (status != FigureStoreError::kNone) {
    return Error(status);
  }
  FigureValidationReport report;
  auto image = FigureImage::Parse(source_snapshot.bytes, report);
  if (!image || !report.IsSafeToLoad()) {
    auto result = Error(FigureStoreError::kInvalidImage);
    result.validation = report;
    return result;
  }
  auto state = session_store_.Load();
  if (state.fatal) {
    return Error(FigureStoreError::kRecoveryRequired);
  }
  state.session.entries = state.persisted_entries;
  const auto relative = target.lexically_relative(library_root_);
  if (FindPending(state.session, relative) !=
      state.session.pending_saves.end()) {
    return Error(FigureStoreError::kRecoveryRequired);
  }
  state.session.pending_saves.push_back(
      {relative, std::string(64, '0'), HeaderHex(*image)});
  auto marked = session_store_.Save(state.session);
  if (!marked.success()) {
    auto result = Error(FigureStoreError::kPersistenceFailed);
    result.persistence = marked.persistence;
    return result;
  }
  const auto persistence = writer_->WriteNew(target, image->bytes());
  if (persistence.outcome == AtomicCommitOutcome::kDurable &&
      persistence.error == AtomicWriteError::kNone) {
    Snapshot check;
    if (ReadSnapshot(target, check) != FigureStoreError::kNone ||
        check.bytes != source_snapshot.bytes) {
      auto result = Error(FigureStoreError::kRecoveryRequired);
      result.persistence = persistence;
      return result;
    }
  } else if (persistence.outcome != AtomicCommitOutcome::kNotReplaced) {
    auto result = Error(FigureStoreError::kRecoveryRequired);
    result.persistence = persistence;
    return result;
  }
  state.session.pending_saves.erase(FindPending(state.session, relative));
  auto cleared = session_store_.Save(state.session);
  if (!cleared.success()) {
    auto result = Error(FigureStoreError::kRecoveryRequired);
    result.persistence = cleared.persistence;
    return result;
  }
  if (persistence.outcome == AtomicCommitOutcome::kNotReplaced) {
    auto result = Error(std::filesystem::exists(target)
                            ? FigureStoreError::kAlreadyExists
                            : FigureStoreError::kPersistenceFailed);
    result.persistence = persistence;
    return result;
  }
  auto result = LoadManaged(target);
  result.persistence = persistence;
  return result;
}
FigureStoreResult FigureStore::Export(FigureHandle handle,
                                      const std::filesystem::path& destination,
                                      bool overwrite_confirmed) {
  auto found = entries_.find(handle);
  if (found == entries_.end() || !found->second.available) {
    return {FigureStoreError::kUnavailable};
  }
  const auto& entry = found->second;
  auto state = session_store_.Load();
  if (state.fatal || (InLibrary(entry.path) &&
                      FindPending(state.session, entry.path.lexically_relative(
                                                     library_root_)) !=
                          state.session.pending_saves.end())) {
    return {FigureStoreError::kRecoveryRequired};
  }
  Snapshot current;
  if (ReadSnapshot(entry.path, current) != FigureStoreError::kNone ||
      !SameVersion(current, entry.snapshot)) {
    return {FigureStoreError::kExternalConflict};
  }
  std::error_code error;
  const auto parent =
      std::filesystem::canonical(destination.parent_path(), error);
  if (error || destination.filename().empty()) {
    return {FigureStoreError::kIoFailed};
  }
  const auto target = parent / destination.filename();
  if (target == entry.path) {
    return {FigureStoreError::kExternalConflict};
  }
  for (const auto& [other_handle, other] : entries_) {
    if (target == other.path && other_handle != handle) {
      return {FigureStoreError::kExternalConflict};
    }
  }
  const bool exists = std::filesystem::exists(target, error);
  if (error) {
    return {FigureStoreError::kIoFailed};
  }
  if (exists && (std::filesystem::is_symlink(target, error) ||
                 std::filesystem::is_directory(target, error))) {
    return {FigureStoreError::kExternalConflict};
  }
  if (error) {
    return {FigureStoreError::kIoFailed};
  }
  if (exists && !overwrite_confirmed) {
    return {FigureStoreError::kOverwriteNotConfirmed};
  }
  const auto persistence = exists
                               ? writer_->Write(target, entry.image.bytes())
                               : writer_->WriteNew(target, entry.image.bytes());
  if (persistence.outcome != AtomicCommitOutcome::kDurable ||
      persistence.error != AtomicWriteError::kNone) {
    return {FigureStoreError::kPersistenceFailed, persistence};
  }
  return {FigureStoreError::kNone, persistence};
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
  auto state = session_store_.Load();
  if (state.fatal) {
    return Error(FigureStoreError::kRecoveryRequired);
  }
  if (InLibrary(canonical) &&
      FindPending(state.session, canonical.lexically_relative(library_root_)) !=
          state.session.pending_saves.end()) {
    return Error(FigureStoreError::kRecoveryRequired);
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
  auto state = session_store_.Load();
  if (state.fatal ||
      (InLibrary(entry->second.path) &&
       FindPending(state.session,
                   entry->second.path.lexically_relative(library_root_)) !=
           state.session.pending_saves.end())) {
    return {FigureIoError::kUnavailable};
  }
  Snapshot current;
  if (ReadSnapshot(entry->second.path, current) != FigureStoreError::kNone ||
      !SameVersion(current, entry->second.snapshot)) {
    return {FigureIoError::kConflict};
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
  auto state = session_store_.Load();
  if (state.fatal) {
    return {FigureIoError::kUnavailable};
  }
  state.session.entries = state.persisted_entries;
  const auto relative = entry.path.lexically_relative(library_root_);
  if (FindPending(state.session, relative) !=
      state.session.pending_saves.end()) {
    return {FigureIoError::kUnavailable};
  }
  state.session.pending_saves.push_back(
      {relative, HeaderHex(entry.image), HeaderHex(candidate)});
  auto marked = session_store_.Save(state.session);
  if (!marked.success()) {
    entry.available = false;
    entry.recovery_required = true;
    return {FigureIoError::kPersistenceFailed, marked.persistence};
  }
  auto persistence = writer_->Write(entry.path, candidate.bytes());
  if (persistence.outcome == AtomicCommitOutcome::kNotReplaced) {
    state.session.pending_saves.erase(FindPending(state.session, relative));
    auto cleared = session_store_.Save(state.session);
    if (!cleared.success()) {
      entry.available = false;
      entry.recovery_required = true;
    }
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
  state.session.pending_saves.erase(FindPending(state.session, relative));
  for (auto& saved : state.session.entries) {
    if (saved.relative_path == relative) {
      saved.fingerprint = current.fingerprint;
    }
  }
  auto cleared = session_store_.Save(state.session);
  if (!cleared.success()) {
    entry.available = false;
    entry.recovery_required = true;
    return {FigureIoError::kPersistenceFailed, cleared.persistence};
  }
  entry.snapshot = current;
  return {FigureIoError::kNone, persistence};
}
FigureLoadResult FigureStore::Recover(FigureHandle handle) {
  auto found = entries_.find(handle);
  if (found == entries_.end() || !found->second.recovery_required) {
    return Error(FigureStoreError::kUnavailable);
  }
  auto result = RecoverPath(found->second.path, &found->second.image);
  if (result.handle) {
    found->second.recovery_required = false;
  }
  return result;
}
FigureLoadResult FigureStore::Recover(const std::filesystem::path& path) {
  std::error_code error;
  auto canonical = std::filesystem::canonical(path, error);
  if (error) {
    // A create-only import may have failed before publishing its file. Its
    // durable pending record still needs an explicit, durable clear.
    const auto parent = std::filesystem::canonical(path.parent_path(), error);
    if (error || path.filename().empty()) {
      return Error(FigureStoreError::kIoFailed);
    }
    const auto missing = parent / path.filename();
    if (!InLibrary(missing)) {
      return Error(FigureStoreError::kOutsideLibrary);
    }
    auto state = session_store_.Load();
    if (state.fatal) {
      return Error(FigureStoreError::kRecoveryRequired);
    }
    const auto relative = missing.lexically_relative(library_root_);
    auto pending = FindPending(state.session, relative);
    if (pending == state.session.pending_saves.end()) {
      return Error(FigureStoreError::kUnavailable);
    }
    const bool exists = std::filesystem::exists(missing, error);
    if (error || exists) {
      return Error(FigureStoreError::kIoFailed);
    }
    state.session.entries = state.persisted_entries;
    state.session.pending_saves.erase(pending);
    auto cleared = session_store_.Save(state.session);
    if (!cleared.success()) {
      auto result = Error(FigureStoreError::kPersistenceFailed);
      result.persistence = cleared.persistence;
      return result;
    }
    return Error(FigureStoreError::kUnavailable);
  }
  if (!InLibrary(canonical)) {
    return Error(FigureStoreError::kOutsideLibrary);
  }
  auto state = session_store_.Load();
  if (state.fatal) {
    return Error(FigureStoreError::kRecoveryRequired);
  }
  if (FindPending(state.session, canonical.lexically_relative(library_root_)) ==
      state.session.pending_saves.end()) {
    return Error(FigureStoreError::kUnavailable);
  }
  return RecoverPath(canonical, nullptr);
}
FigureLoadResult FigureStore::RecoverPath(const std::filesystem::path& path,
                                          const FigureImage* expected) {
  auto state = session_store_.Load();
  if (state.fatal) {
    return Error(FigureStoreError::kRecoveryRequired);
  }
  state.session.entries = state.persisted_entries;
  const auto relative = path.lexically_relative(library_root_);
  auto pending = FindPending(state.session, relative);
  if (!expected && pending == state.session.pending_saves.end()) {
    return Error(FigureStoreError::kUnavailable);
  }
  Snapshot current;
  auto status = ReadSnapshot(path, current);
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
  const auto header = HeaderHex(*image);
  if ((expected && !SameIdentity(*image, *expected)) ||
      (pending != state.session.pending_saves.end() &&
       header != pending->old_header && header != pending->candidate_header)) {
    return Error(FigureStoreError::kExternalConflict);
  }
  if (current.read_only) {
    return Error(FigureStoreError::kReadOnly);
  }
  // Recheck immediately before saving. Concurrent external writers are not
  // supported; a later manager must serialize access to this store.
  Snapshot check;
  if (ReadSnapshot(path, check) != FigureStoreError::kNone ||
      !SameVersion(current, check)) {
    return Error(FigureStoreError::kExternalConflict);
  }
  const auto persistence = writer_->Write(path, image->bytes());
  if (persistence.outcome != AtomicCommitOutcome::kDurable ||
      persistence.error != AtomicWriteError::kNone ||
      ReadSnapshot(path, check) != FigureStoreError::kNone ||
      check.bytes != current.bytes) {
    auto result = Error(FigureStoreError::kPersistenceFailed);
    result.persistence = persistence;
    return result;
  }
  if (pending != state.session.pending_saves.end()) {
    state.session.pending_saves.erase(pending);
    for (auto& saved : state.session.entries) {
      if (saved.relative_path == relative) {
        saved.fingerprint = check.fingerprint;
      }
    }
    auto cleared = session_store_.Save(state.session);
    if (!cleared.success()) {
      auto result = Error(FigureStoreError::kPersistenceFailed);
      result.persistence = cleared.persistence;
      return result;
    }
  }
  auto result = Add({path, *image, validation, check, false});
  result.persistence = persistence;
  return result;
}
}  // namespace xe::hid
