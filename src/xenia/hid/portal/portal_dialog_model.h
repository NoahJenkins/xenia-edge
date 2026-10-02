/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef XENIA_HID_PORTAL_PORTAL_DIALOG_MODEL_H_
#define XENIA_HID_PORTAL_PORTAL_DIALOG_MODEL_H_

#include "xenia/hid/portal/portal_manager.h"

namespace xe::hid {
enum class PortalLibraryFilter { kAll, kAvailable, kNeedsAttention };

// Selection and action rules shared by all input methods. No GUI dependency.
class PortalDialogModel {
 public:
  PortalSlot selected_slot() const { return selected_slot_; }
  bool SelectSlot(PortalSlot slot);
  void MoveSlotSelection(int direction);
  void SelectFigure(std::filesystem::path path) {
    selected_path_ = std::move(path);
  }
  const PortalLibraryEntry* SelectedFigure(
      const PortalLibrarySnapshot& library) const;
  static std::vector<size_t> Filter(const PortalLibrarySnapshot& library,
                                    const std::string& search,
                                    PortalLibraryFilter filter);
  std::vector<PortalOperationKind> Actions(
      const PortalManagerSnapshot& portal,
      const PortalLibrarySnapshot& library) const;
  PortalOperation Operation(PortalOperationKind kind,
                            const PortalManagerSnapshot& portal) const;
  static std::string Status(const PortalLibraryEntry& entry);
  static std::string Message(const PortalOperationResult& result);

 private:
  PortalSlot selected_slot_ = 0;
  std::filesystem::path selected_path_;
};
}  // namespace xe::hid
#endif  // XENIA_HID_PORTAL_PORTAL_DIALOG_MODEL_H_
