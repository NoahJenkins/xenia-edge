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
#include <cctype>
#include <fstream>
#include <set>

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
  result.management_ready = ready_;
  result.errors = errors_;
  for (size_t i = 0; i < records_.size(); ++i) {
    if (records_[i]) {
      result.figure_paths[i] = library_root_ / records_[i]->relative_path;
    }
  }
  return result;
}

PortalLibrarySnapshot VirtualPortal::ListLibrary() const {
  PortalLibrarySnapshot result;
  result.root = library_root_;
  result.errors = errors_;
  if (!session_) {
    return result;
  }
  auto session = session_->Load();
  result.ready = ready_ && !session.fatal;
  result.errors.insert(result.errors.end(), session.errors.begin(),
                       session.errors.end());
  std::set<std::filesystem::path> pending;
  for (const auto& item : session.session.pending_saves) {
    pending.insert(item.relative_path);
  }
  auto add = [&](const std::filesystem::path& path) {
    PortalLibraryEntry entry;
    entry.path = path;
    const auto relative = path.lexically_relative(library_root_);
    entry.name = PortalSessionStore::Utf8(relative);
    entry.recovery_required = pending.contains(relative);
    std::error_code error;
    const auto canonical = std::filesystem::canonical(path, error);
    const bool contained =
        !error && PortalSessionStore::SafeRelativePath(
                      canonical.lexically_relative(library_root_));
    const auto status = contained ? std::filesystem::status(canonical, error)
                                  : std::filesystem::file_status{};
    const auto writable = std::filesystem::perms::owner_write |
                          std::filesystem::perms::group_write |
                          std::filesystem::perms::others_write;
    entry.read_only = !error && (status.permissions() & writable) ==
                                    std::filesystem::perms::none;
    if (contained && !error && std::filesystem::is_regular_file(status) &&
        std::filesystem::file_size(canonical, error) == kFigureSize && !error) {
      std::array<uint8_t, kFigureSize> bytes{};
      std::ifstream input(canonical, std::ios::binary);
      input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
      if (input && input.gcount() == kFigureSize &&
          input.peek() == std::char_traits<char>::eof()) {
        auto image = FigureImage::Parse(bytes, entry.validation);
        if (image) {
          entry.identity = image->identity();
        }
      }
    }
    if (!entry.validation.structure_checked) {
      entry.validation.issues.push_back(
          {FigureValidationCode::kWrongSize, FigureIssueSeverity::kError, 0,
           "Cannot read a complete 1,024-byte figure"});
    }
    for (size_t i = 0; i < records_.size(); ++i) {
      if (records_[i] && records_[i]->relative_path == relative) {
        entry.mounted_slot = static_cast<PortalSlot>(i);
      }
    }
    const auto loaded = handles_.find(canonical);
    if (loaded != handles_.end() && !entry.recovery_required) {
      entry.changed =
          figures_->ReadBlock(loaded->second, 0).error != FigureIoError::kNone;
    }
    result.entries.push_back(std::move(entry));
  };
  // Keep recovery entries visible even when a large library hits its limit.
  for (const auto& relative : pending) {
    add(library_root_ / relative);
  }
  std::error_code error;
  std::filesystem::recursive_directory_iterator it(
      library_root_, std::filesystem::directory_options::none, error),
      end;
  size_t examined = 0;
  while (!error && it != end && examined++ < 4096 &&
         result.entries.size() < 1024) {
    const auto path = it->path();
    const auto status = it->symlink_status(error);
    if (error) {
      break;
    }
    if (it.depth() >= 4) {
      it.disable_recursion_pending();
    }
    if (!std::filesystem::is_symlink(status) &&
        std::filesystem::is_regular_file(status)) {
      auto extension = PortalSessionStore::Utf8(path.extension());
      std::transform(
          extension.begin(), extension.end(), extension.begin(),
          [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (extension == ".sky" || extension == ".bin" || extension == ".dump" ||
          extension == ".dmp" ||
          pending.contains(path.lexically_relative(library_root_))) {
        if (pending.contains(path.lexically_relative(library_root_))) {
          it.increment(error);
          continue;
        }
        add(path);
      }
    }
    it.increment(error);
  }
  if (error) {
    result.errors.push_back("Cannot read the whole figure library");
  }
  if (it != end) {
    result.errors.push_back(
        "Library scan limit reached; use a smaller library folder");
  }
  std::sort(result.entries.begin(), result.entries.end(),
            [](const auto& a, const auto& b) { return a.name < b.name; });
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
    const auto before = session_->Load();
    std::error_code path_error;
    const auto resolved =
        std::filesystem::weakly_canonical(operation.path, path_error);
    const auto relative = resolved.lexically_relative(library_root_);
    const auto pending_at_path = [&](const SessionLoadResult& state) {
      return std::any_of(
          state.session.pending_saves.begin(),
          state.session.pending_saves.end(),
          [&](const auto& item) { return item.relative_path == relative; });
    };
    const bool was_pending =
        !path_error && !before.fatal && pending_at_path(before);
    auto recovered = figures_->Recover(operation.path);
    if (was_pending && recovered.error == FigureStoreError::kUnavailable) {
      const auto after = session_->Load();
      if (!after.fatal && !pending_at_path(after)) {
        return {true, {}};
      }
    }
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
  if (operation.expected_generation &&
      slots_.Get(operation.source_slot)->generation !=
          *operation.expected_generation) {
    return Failure("The selected slot changed; select it again");
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
