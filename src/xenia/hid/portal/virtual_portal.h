/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef XENIA_HID_PORTAL_VIRTUAL_PORTAL_H_
#define XENIA_HID_PORTAL_VIRTUAL_PORTAL_H_

#include <array>
#include <map>
#include <optional>

#include "xenia/hid/portal/figure_store.h"
#include "xenia/hid/portal/portal_manager.h"

namespace xe::hid {
// Management and restart state only. Guest reports are closed until the
// Xbox protocol evidence gate is met. PortalManager serializes calls.
class VirtualPortal {
 public:
  VirtualPortal(std::filesystem::path storage_root,
                std::filesystem::path library_root);
  PortalManagerSnapshot Snapshot() const;
  PortalOperationResult Apply(const PortalOperation& operation);

 private:
  std::optional<PortalSessionEntry> EntryFor(
      const std::filesystem::path& path) const;
  PortalOperationResult Save(const std::array<std::optional<PortalSessionEntry>,
                                              kPortalSlotCount>& records);
  PortalOperationResult LoadHandle(const std::filesystem::path& path,
                                   FigureHandle& handle);

  std::filesystem::path library_root_;
  std::filesystem::path session_root_;
  std::unique_ptr<FigureStore> figures_;
  std::unique_ptr<PortalSessionStore> session_;
  PortalSlotState slots_;
  std::array<std::optional<PortalSessionEntry>, kPortalSlotCount> records_{};
  std::map<std::filesystem::path, FigureHandle> handles_;
  std::vector<std::string> errors_;
  bool ready_ = false;
};
}  // namespace xe::hid
#endif  // XENIA_HID_PORTAL_VIRTUAL_PORTAL_H_
