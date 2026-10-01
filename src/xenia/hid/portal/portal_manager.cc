/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/portal_manager.h"

#include "xenia/hid/portal/portal.h"
#include "xenia/hid/portal/virtual_portal.h"
#ifdef XE_PLATFORM_WIN32
#include "xenia/hid/portal/hardware_portal.h"
#endif

namespace xe::hid {
PortalBackendKind ParsePortalBackend(const std::string& value) {
  if (value == "physical") {
    return PortalBackendKind::kPhysical;
  }
  if (value == "virtual") {
    return PortalBackendKind::kVirtual;
  }
  return PortalBackendKind::kDisabled;
}
PortalBackendKind DefaultPortalBackend() {
#ifdef XE_PLATFORM_WIN32
  return PortalBackendKind::kPhysical;
#else
  return PortalBackendKind::kDisabled;
#endif
}
PortalManager::PortalManager(std::filesystem::path storage_root,
                             PortalBackendKind requested_backend,
                             std::filesystem::path library_root)
    : backend_(requested_backend) {
  if (backend_ == PortalBackendKind::kVirtual) {
    virtual_ = std::make_unique<VirtualPortal>(std::move(storage_root),
                                               std::move(library_root));
  } else if (backend_ == PortalBackendKind::kPhysical) {
#ifdef XE_PLATFORM_WIN32
    physical_ = std::make_unique<HardwarePortal>();
#else
    backend_ = PortalBackendKind::kDisabled;
#endif
  }
}
PortalManager::~PortalManager() = default;
bool PortalManager::IsConnected() const {
  std::lock_guard guard(mutex_);
  return physical_ && physical_->IsConnected();
}
X_STATUS PortalManager::Read(std::span<uint8_t> data, uint32_t& bytes_read,
                             uint16_t& state) {
  std::lock_guard guard(mutex_);
  if (!physical_) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  return physical_->Read(data, bytes_read, state);
}
X_STATUS PortalManager::Write(std::span<uint8_t> data) {
  std::lock_guard guard(mutex_);
  if (!physical_) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  return physical_->Write(data);
}
void PortalManager::OnDeviceArrival() {
  std::lock_guard guard(mutex_);
  if (physical_) {
    physical_->OnDeviceArrival();
  }
}
void PortalManager::OnDeviceRemoval() {
  std::lock_guard guard(mutex_);
  if (physical_) {
    physical_->OnDeviceRemoval();
  }
}
PortalManagerSnapshot PortalManager::Snapshot() const {
  std::lock_guard guard(mutex_);
  if (virtual_) {
    return virtual_->Snapshot();
  }
  PortalManagerSnapshot result;
  result.backend = backend_;
  result.connected = physical_ && physical_->IsConnected();
  return result;
}
PortalOperationResult PortalManager::Apply(const PortalOperation& operation) {
  std::lock_guard guard(mutex_);
  if (!virtual_) {
    return {false, "Virtual portal is not selected"};
  }
  return virtual_->Apply(operation);
}
}  // namespace xe::hid
