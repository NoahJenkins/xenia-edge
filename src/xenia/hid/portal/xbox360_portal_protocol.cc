/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/xbox360_portal_protocol.h"

#include <algorithm>

namespace xe::hid {
namespace {

struct CommandReply {
  uint8_t command;
  size_t minimum_size;
  bool echo_argument;
  PortalReport data;
};

// Traptanium software-reference contracts P1-P3, not hardware captures:
// docs/researchReports/2026-10-01-portal-public-evidence.md.
constexpr std::array<CommandReply, 3> kTraptaniumReplies{{
    {0x52, 3, false, {0x0B, 0x14, 0x52, 0x02, 0x1B}},
    {0x41, 4, true, {0x0B, 0x14, 0x41, 0x00, 0xFF, 0x77}},
    {0x4D, 4, true, {0x0B, 0x14, 0x4D, 0x00, 0x00, 0x19}},
}};

}  // namespace

Xbox360PortalProtocol::Xbox360PortalProtocol(ProtocolFigureIo&,
                                             PortalSlotState&, PortalClock&) {
  // Figure I/O and timing remain gated. Keep the planned injection boundary,
  // without retaining unused references or inventing activation side effects.
}

ProtocolError Xbox360PortalProtocol::SubmitHostReport(
    std::span<const uint8_t> report) {
  if (report.size() < 3 || report.size() > kPortalBufferSize ||
      report[0] != 0x0B || report[1] != 0x14) {
    return ProtocolError::kMalformedReport;
  }
  const auto command = std::find_if(
      kTraptaniumReplies.begin(), kTraptaniumReplies.end(),
      [&report](const auto& entry) { return entry.command == report[2]; });
  if (command == kTraptaniumReplies.end()) {
    return ProtocolError::kUnsupportedCommand;
  }
  if (report.size() < command->minimum_size) {
    return ProtocolError::kMalformedReport;
  }
  if (count_ == reports_.size()) {
    return ProtocolError::kQueueFull;
  }
  auto& reply = reports_[(head_ + count_) % reports_.size()];
  reply = {command->data, kPortalBufferSize};
  if (command->echo_argument) {
    reply.data[3] = report[3];
  }
  ++count_;
  return ProtocolError::kNone;
}

std::optional<QueuedPortalReport> Xbox360PortalProtocol::PopGuestReport() {
  if (!count_) {
    return std::nullopt;
  }
  const auto reply = reports_[head_];
  head_ = (head_ + 1) % reports_.size();
  --count_;
  return reply;
}

void Xbox360PortalProtocol::Reset() {
  head_ = 0;
  count_ = 0;
}

}  // namespace xe::hid
