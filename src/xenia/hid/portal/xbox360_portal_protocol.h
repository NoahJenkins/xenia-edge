/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_XBOX360_PORTAL_PROTOCOL_H_
#define XENIA_HID_PORTAL_XBOX360_PORTAL_PROTOCOL_H_

#include <array>
#include <optional>
#include <span>

#include "xenia/hid/portal/portal_report.h"
#include "xenia/hid/portal/portal_slot_state.h"
#include "xenia/hid/portal/protocol_figure_io.h"

namespace xe::hid {

class PortalClock {
 public:
  virtual ~PortalClock() = default;
  virtual uint64_t NowTicks() const = 0;
};

// Partial Traptanium software profile. Only independently corroborated R/A/M
// replies are enabled. There are no periodic status reports or figure commands
// yet, and this core is not connected to the emulator's XAM path.
// The caller serializes access.
class Xbox360PortalProtocol {
 public:
  static constexpr size_t kMaxPendingReports = 64;

  Xbox360PortalProtocol(ProtocolFigureIo& figure_io, PortalSlotState& slots,
                        PortalClock& clock);
  ProtocolError SubmitHostReport(std::span<const uint8_t> report);
  std::optional<QueuedPortalReport> PopGuestReport();
  void Reset();

 private:
  std::array<QueuedPortalReport, kMaxPendingReports> reports_{};
  size_t head_ = 0;
  size_t count_ = 0;
};

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_XBOX360_PORTAL_PROTOCOL_H_
