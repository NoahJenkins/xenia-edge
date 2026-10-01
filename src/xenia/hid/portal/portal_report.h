/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_PORTAL_REPORT_H_
#define XENIA_HID_PORTAL_PORTAL_REPORT_H_

#include <array>
#include <cstdint>

#include "xenia/hid/portal/portal.h"

namespace xe::hid {

using PortalReport = std::array<uint8_t, kPortalBufferSize>;

struct QueuedPortalReport {
  PortalReport data{};
  uint8_t size = 0;
};

enum class ProtocolError {
  kNone,
  kMalformedReport,
  kUnsupportedCommand,
  kInvalidSlot,
  kInvalidBlock,
  kFigureUnavailable,
  kPersistenceFailed,
};

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_PORTAL_REPORT_H_
