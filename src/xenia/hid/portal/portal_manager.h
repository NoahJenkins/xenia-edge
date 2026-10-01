/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef XENIA_HID_PORTAL_PORTAL_MANAGER_H_
#define XENIA_HID_PORTAL_PORTAL_MANAGER_H_

#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "xenia/hid/portal/figure_store.h"
#include "xenia/hid/portal/portal.h"
#include "xenia/hid/portal/portal_slot_state.h"
#include "xenia/xbox.h"

namespace xe::hid {
class VirtualPortal;

enum class PortalBackendKind { kDisabled, kPhysical, kVirtual };
PortalBackendKind ParsePortalBackend(const std::string& value);
PortalBackendKind DefaultPortalBackend();

enum class PortalOperationKind {
  kAdd,
  kRemove,
  kReplace,
  kMove,
  kImport,
  kExport,
  kRecover,
};
struct PortalOperation {
  PortalOperationKind kind;
  PortalSlot source_slot = 0;
  PortalSlot destination_slot = 0;
  std::filesystem::path path;
  std::filesystem::path destination;
  bool overwrite_confirmed = false;
};
struct PortalOperationResult {
  bool success = false;
  std::string error;
  FigureStoreError figure_error = FigureStoreError::kNone;
  AtomicWriteResult persistence;
};
struct PortalManagerSnapshot {
  PortalBackendKind backend = PortalBackendKind::kDisabled;
  // Guest access remains false for virtual mode until Xbox wire behavior is
  // verified. Slots are still available to the management interface.
  bool connected = false;
  std::array<PortalSlotSnapshot, kPortalSlotCount> slots =
      PortalSlotState{}.Snapshot();
  std::vector<std::string> errors;
};

class PortalManager {
 public:
  PortalManager(std::filesystem::path storage_root,
                PortalBackendKind requested_backend,
                std::filesystem::path library_root = {});
  ~PortalManager();
  bool IsConnected() const;
  X_STATUS Read(std::span<uint8_t> data, uint32_t& bytes_read, uint16_t& state);
  X_STATUS Write(std::span<uint8_t> data);
  void OnDeviceArrival();
  void OnDeviceRemoval();
  PortalManagerSnapshot Snapshot() const;
  PortalOperationResult Apply(const PortalOperation& operation);

 private:
  mutable std::mutex mutex_;
  PortalBackendKind backend_ = PortalBackendKind::kDisabled;
  std::unique_ptr<Portal> physical_;
  std::unique_ptr<VirtualPortal> virtual_;
};
}  // namespace xe::hid
#endif  // XENIA_HID_PORTAL_PORTAL_MANAGER_H_
