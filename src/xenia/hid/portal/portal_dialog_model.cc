/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/portal_dialog_model.h"

#include <algorithm>
#include <cctype>

namespace xe::hid {
namespace {
std::string Fold(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}
bool Available(const PortalLibraryEntry& entry) {
  return entry.validation.IsSafeToLoad() && !entry.changed &&
         !entry.recovery_required && !entry.mounted_slot;
}
}  // namespace
bool PortalDialogModel::SelectSlot(PortalSlot slot) {
  if (slot >= kPortalSlotCount) {
    return false;
  }
  selected_slot_ = slot;
  return true;
}
void PortalDialogModel::MoveSlotSelection(int direction) {
  const int step = direction > 0 ? 1 : direction < 0 ? -1 : 0;
  selected_slot_ = static_cast<PortalSlot>(
      (selected_slot_ + kPortalSlotCount + step) % kPortalSlotCount);
}
const PortalLibraryEntry* PortalDialogModel::SelectedFigure(
    const PortalLibrarySnapshot& library) const {
  const auto found = std::find_if(
      library.entries.begin(), library.entries.end(),
      [&](const auto& entry) { return entry.path == selected_path_; });
  return found == library.entries.end() ? nullptr : &*found;
}
std::vector<size_t> PortalDialogModel::Filter(
    const PortalLibrarySnapshot& library, const std::string& search,
    PortalLibraryFilter filter) {
  std::vector<size_t> result;
  const auto needle = Fold(search);
  for (size_t i = 0; i < library.entries.size(); ++i) {
    const auto& entry = library.entries[i];
    const bool attention = !entry.validation.IsSafeToLoad() || entry.changed ||
                           entry.recovery_required;
    if ((filter == PortalLibraryFilter::kAvailable && !Available(entry)) ||
        (filter == PortalLibraryFilter::kNeedsAttention && !attention)) {
      continue;
    }
    if (Fold(entry.name).find(needle) != std::string::npos) {
      result.push_back(i);
    }
  }
  return result;
}
std::vector<PortalOperationKind> PortalDialogModel::Actions(
    const PortalManagerSnapshot& portal,
    const PortalLibrarySnapshot& library) const {
  std::vector<PortalOperationKind> result;
  if (portal.backend != PortalBackendKind::kVirtual ||
      !portal.management_ready || !library.ready) {
    return result;
  }
  const auto* figure = SelectedFigure(library);
  const bool occupied = portal.slots[selected_slot_].figure.has_value();
  if (!occupied && figure && Available(*figure)) {
    result.push_back(PortalOperationKind::kAdd);
  }
  if (occupied) {
    result.push_back(PortalOperationKind::kRemove);
  }
  if (occupied && figure && Available(*figure)) {
    result.push_back(PortalOperationKind::kReplace);
  }
  if (occupied && std::any_of(portal.slots.begin(), portal.slots.end(),
                              [](const auto& slot) { return !slot.figure; })) {
    result.push_back(PortalOperationKind::kMove);
  }
  result.push_back(PortalOperationKind::kImport);
  if (occupied) {
    result.push_back(PortalOperationKind::kExport);
  }
  if (figure && figure->recovery_required) {
    result.push_back(PortalOperationKind::kRecover);
  }
  return result;
}
PortalOperation PortalDialogModel::Operation(
    PortalOperationKind kind, const PortalManagerSnapshot& portal) const {
  PortalOperation operation{kind};
  operation.source_slot = selected_slot_;
  operation.path = selected_path_;
  operation.expected_generation = portal.slots[selected_slot_].generation;
  return operation;
}
std::string PortalDialogModel::Status(const PortalLibraryEntry& entry) {
  if (entry.recovery_required) {
    return "Recovery required";
  }
  if (entry.changed) {
    return "Changed outside Xenia";
  }
  if (!entry.validation.IsSafeToLoad()) {
    return "Invalid figure";
  }
  if (entry.mounted_slot) {
    return "On slot " + std::to_string(*entry.mounted_slot + 1);
  }
  if (entry.read_only) {
    return "Read only";
  }
  return "Available";
}
std::string PortalDialogModel::Message(const PortalOperationResult& result) {
  if (result.success) {
    return "Saved.";
  }
  if (result.persistence.outcome ==
      AtomicCommitOutcome::kReplacedDurabilityUnknown) {
    return "The file was replaced, but its save could not be confirmed. Figure "
           "access stays blocked until recovery.";
  }
  switch (result.figure_error) {
    case FigureStoreError::kInvalidImage:
      return "The file is not a structurally valid 1,024-byte figure. It was "
             "not imported.";
    case FigureStoreError::kReadOnly:
      return "The figure is read only. Choose a writable library folder.";
    case FigureStoreError::kAlreadyExists:
      return "That file already exists in the library. Choose a different "
             "name.";
    case FigureStoreError::kAlreadyLoaded:
      return "That figure is already loaded.";
    case FigureStoreError::kOverwriteNotConfirmed:
      return "Confirm replacement before exporting to an existing file.";
    case FigureStoreError::kExternalConflict:
      return "The figure changed outside Xenia or the destination is in use. "
             "Refresh the library.";
    case FigureStoreError::kRecoveryRequired:
      return "The save needs recovery. Select the figure and choose Recover.";
    case FigureStoreError::kPersistenceFailed:
      return "The save failed. The operation was not acknowledged; review the "
             "library and session errors.";
    case FigureStoreError::kOutsideLibrary:
      return "Choose a file inside the managed library, or import it first.";
    case FigureStoreError::kIoFailed:
      return "Cannot read or write that file. Check the folder and "
             "permissions.";
    case FigureStoreError::kUnavailable:
      return "The figure is unavailable. Refresh the library before trying "
             "again.";
    case FigureStoreError::kNone:
      break;
  }
  return result.error.empty() ? "The operation did not complete."
                              : result.error;
}
}  // namespace xe::hid
