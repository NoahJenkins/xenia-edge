/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/virtual_portal.h"

#include <algorithm>
#include <fstream>

#include "xenia/base/xxhash.h"

namespace xe::hid {
namespace {
PortalOperationResult Failure(const char* message) { return {false, message}; }
}  // namespace

VirtualPortal::VirtualPortal(std::filesystem::path storage_root,
                             std::filesystem::path library_root)
    : session_root_(std::move(storage_root) / "skylanders") {
  if (session_root_.parent_path().empty()) {
    errors_.push_back("Cannot resolve portal storage root");
    return;
  }
  if (library_root.empty()) {
    library_root = session_root_ / "figures";
  }
  std::error_code error;
  std::filesystem::create_directories(session_root_, error);
  if (!error) {
    std::filesystem::create_directories(library_root, error);
  }
  if (!error) {
    library_root_ = std::filesystem::canonical(library_root, error);
  }
  if (error) {
    errors_.push_back("Cannot open figure library");
    return;
  }
  session_ = std::make_unique<PortalSessionStore>(
      library_root_, CreateNativeAtomicFileWriter(), session_root_);
  figures_ = std::make_unique<FigureStore>(
      library_root_, CreateNativeAtomicFileWriter(),
      CreateNativeAtomicFileWriter(), session_root_);
  auto loaded = session_->Load();
  errors_ = std::move(loaded.errors);
  if (loaded.fatal) {
    return;
  }
  ready_ = true;
  for (const auto& entry : loaded.session.entries) {
    const auto path = library_root_ / entry.relative_path;
    auto figure = figures_->LoadManaged(path);
    if (figure.error != FigureStoreError::kNone || !figure.handle) {
      errors_.push_back("Cannot restore session figure");
      continue;
    }
    records_[entry.slot] = entry;
    std::error_code path_error;
    const auto canonical = std::filesystem::canonical(path, path_error);
    if (path_error) {
      records_[entry.slot].reset();
      errors_.push_back("Cannot resolve restored figure path");
      continue;
    }
    handles_[canonical] = *figure.handle;
    slots_.Insert(entry.slot, *figure.handle);
  }
  slots_.Advance();
}

PortalManagerSnapshot VirtualPortal::Snapshot() const {
  PortalManagerSnapshot result;
  result.backend = PortalBackendKind::kVirtual;
  result.slots = slots_.Snapshot();
  result.errors = errors_;
  return result;
}

std::optional<PortalSessionEntry> VirtualPortal::EntryFor(
    const std::filesystem::path& path) const {
  std::error_code error;
  const auto canonical = std::filesystem::canonical(path, error);
  if (error) {
    return std::nullopt;
  }
  const auto relative = canonical.lexically_relative(library_root_);
  if (!PortalSessionStore::SafeRelativePath(relative) ||
      relative == canonical || relative.empty()) {
    return std::nullopt;
  }
  if (std::filesystem::file_size(canonical, error) != kFigureSize || error) {
    return std::nullopt;
  }
  std::array<uint8_t, kFigureSize> bytes{};
  std::ifstream input(canonical, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  if (input.gcount() != kFigureSize || input.bad() ||
      input.peek() != std::char_traits<char>::eof()) {
    return std::nullopt;
  }
  return PortalSessionEntry{0, relative,
                            XXH3_64bits(bytes.data(), bytes.size())};
}

PortalOperationResult VirtualPortal::LoadHandle(
    const std::filesystem::path& path, FigureHandle& handle) {
  std::error_code error;
  const auto canonical = std::filesystem::canonical(path, error);
  if (error) {
    return Failure("Cannot resolve figure path");
  }
  auto found = handles_.find(canonical);
  if (found != handles_.end()) {
    if (figures_->ReadBlock(found->second, 0).error != FigureIoError::kNone) {
      return Failure("Figure changed or requires recovery");
    }
    handle = found->second;
    return {true, {}};
  }
  auto loaded = figures_->LoadManaged(canonical);
  if (loaded.error != FigureStoreError::kNone || !loaded.handle) {
    return {false, "Cannot load managed figure", loaded.error,
            loaded.persistence};
  }
  handle = *loaded.handle;
  handles_[canonical] = handle;
  return {true, {}};
}

PortalOperationResult VirtualPortal::Save(
    const std::array<std::optional<PortalSessionEntry>, kPortalSlotCount>&
        records) {
  auto loaded = session_->Load();
  if (loaded.fatal) {
    ready_ = false;
    errors_.push_back("Session file requires recovery");
    return Failure("Session file requires recovery");
  }
  PortalSession next = loaded.session;
  next.entries = loaded.persisted_entries;
  for (size_t i = 0; i < records.size(); ++i) {
    const auto& old = records_[i];
    const auto& updated = records[i];
    if ((!old && !updated) ||
        (old && updated && old->relative_path == updated->relative_path &&
         old->fingerprint == updated->fingerprint)) {
      continue;
    }
    next.entries.erase(
        std::remove_if(next.entries.begin(), next.entries.end(),
                       [&](const auto& entry) {
                         return entry.slot == i ||
                                (updated &&
                                 entry.relative_path == updated->relative_path);
                       }),
        next.entries.end());
    if (updated) {
      next.entries.push_back(*updated);
    }
  }
  auto saved = session_->Save(next);
  if (!saved.success()) {
    ready_ = false;
    errors_.push_back("Cannot durably save portal session");
    return {false, "Cannot durably save portal session",
            FigureStoreError::kPersistenceFailed, saved.persistence};
  }
  return {true, {}};
}

PortalOperationResult VirtualPortal::Apply(const PortalOperation& operation) {
  if (!ready_) {
    return Failure("Portal session requires recovery");
  }
  if (operation.kind == PortalOperationKind::kImport) {
    auto imported = figures_->Import(operation.path, operation.destination);
    if (imported.error != FigureStoreError::kNone || !imported.handle) {
      return {false, "Cannot import figure", imported.error,
              imported.persistence};
    }
    std::error_code error;
    const auto canonical =
        std::filesystem::canonical(operation.destination, error);
    if (error) {
      return Failure("Cannot resolve imported figure path");
    }
    handles_[canonical] = *imported.handle;
    return {true, {}};
  }
  if (operation.kind == PortalOperationKind::kRecover) {
    auto recovered = figures_->Recover(operation.path);
    if (recovered.error != FigureStoreError::kNone || !recovered.handle) {
      return {false, "Figure recovery did not complete", recovered.error,
              recovered.persistence};
    }
    std::error_code error;
    const auto canonical = std::filesystem::canonical(operation.path, error);
    if (error) {
      return Failure("Cannot resolve recovered figure path");
    }
    handles_[canonical] = *recovered.handle;
    return {true, {}};
  }
  if (operation.source_slot >= kPortalSlotCount) {
    return Failure("Invalid portal slot");
  }
  if (operation.kind == PortalOperationKind::kExport) {
    const auto slot = slots_.Get(operation.source_slot);
    if (!slot || !slot->figure) {
      return Failure("Portal slot is empty");
    }
    auto exported = figures_->Export(*slot->figure, operation.destination,
                                     operation.overwrite_confirmed);
    return exported.error == FigureStoreError::kNone
               ? PortalOperationResult{true, {}}
               : PortalOperationResult{false, "Cannot export figure",
                                       exported.error, exported.persistence};
  }
  auto next_slots = slots_;
  auto next_records = records_;
  if (operation.kind == PortalOperationKind::kRemove) {
    if (!next_slots.Remove(operation.source_slot)) {
      return Failure("Portal slot is empty");
    }
    next_records[operation.source_slot].reset();
  } else if (operation.kind == PortalOperationKind::kMove) {
    if (operation.destination_slot >= kPortalSlotCount ||
        !next_slots.Move(operation.source_slot, operation.destination_slot)) {
      return Failure("Cannot move figure to that slot");
    }
    next_records[operation.destination_slot] =
        next_records[operation.source_slot];
    next_records[operation.destination_slot]->slot = operation.destination_slot;
    next_records[operation.source_slot].reset();
  } else if (operation.kind == PortalOperationKind::kAdd ||
             operation.kind == PortalOperationKind::kReplace) {
    auto entry = EntryFor(operation.path);
    if (!entry) {
      return Failure("Figure path is outside the managed library");
    }
    for (const auto& existing : records_) {
      if (existing && existing->relative_path == entry->relative_path) {
        return Failure("Figure is already on the portal");
      }
    }
    FigureHandle handle = 0;
    auto loaded = LoadHandle(operation.path, handle);
    if (!loaded.success) {
      return loaded;
    }
    // The manifest fingerprint must describe the same version held by the
    // store, even if an external process changes the file during Add.
    entry = EntryFor(operation.path);
    if (!entry ||
        figures_->ReadBlock(handle, 0).error != FigureIoError::kNone) {
      return Failure("Figure changed while placing it on the portal");
    }
    const bool changed =
        operation.kind == PortalOperationKind::kAdd
            ? next_slots.Insert(operation.source_slot, handle)
            : next_slots.Replace(operation.source_slot, handle);
    if (!changed) {
      return Failure("Cannot place figure in that slot");
    }
    entry->slot = operation.source_slot;
    next_records[operation.source_slot] = *entry;
  } else {
    return Failure("Unsupported portal operation");
  }
  auto saved = Save(next_records);
  if (!saved.success) {
    return saved;
  }
  slots_ = std::move(next_slots);
  records_ = std::move(next_records);
  // Internal UI state is settled without asserting any Xbox status timing.
  slots_.Advance();
  slots_.Advance();
  slots_.Advance();
  return {true, {}};
}
}  // namespace xe::hid
