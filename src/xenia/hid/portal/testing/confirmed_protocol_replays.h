/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_TESTING_CONFIRMED_PROTOCOL_REPLAYS_H_
#define XENIA_HID_PORTAL_TESTING_CONFIRMED_PROTOCOL_REPLAYS_H_

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace xe::hid::testing {

struct ConfirmedProtocolReplay {
  std::string_view name;
  std::span<const uint8_t> host_report;
  std::span<const uint8_t> expected_guest_report;
  std::string_view evidence_reference;
};

// Intentionally empty. The correlated framing facts in the protocol report do
// not yet include a redistributable, end-to-end Xbox request and response pair.
// Add a replay only when evidence_reference identifies a Confirmed row with the
// exact reports and provenance.
inline constexpr std::array<ConfirmedProtocolReplay, 0>
    kConfirmedProtocolReplays{};

inline constexpr std::span<const ConfirmedProtocolReplay>
GetConfirmedProtocolReplays() {
  return kConfirmedProtocolReplays;
}

}  // namespace xe::hid::testing

#endif  // XENIA_HID_PORTAL_TESTING_CONFIRMED_PROTOCOL_REPLAYS_H_
