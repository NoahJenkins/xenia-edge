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

// Independently constructed synthetic software-reference exchanges, not USB
// captures. All omitted bytes are zero padding. Each exchange starts with a
// fresh Traptanium-profile protocol instance, no figures, and no pending reply.
// Sources and limits:
// docs/researchReports/2026-10-01-portal-public-evidence.md.
inline constexpr std::array<uint8_t, 32> kIdentifyHost{0x0B, 0x14, 0x52};
inline constexpr std::array<uint8_t, 32> kIdentifyGuest{0x0B, 0x14, 0x52, 0x02,
                                                        0x1B};
inline constexpr std::array<uint8_t, 32> kActivateHost{0x0B, 0x14, 0x41, 0x01};
inline constexpr std::array<uint8_t, 32> kActivateGuest{0x0B, 0x14, 0x41,
                                                        0x01, 0xFF, 0x77};
inline constexpr std::array<uint8_t, 32> kDeactivateHost{0x0B, 0x14, 0x41,
                                                         0x00};
inline constexpr std::array<uint8_t, 32> kDeactivateGuest{0x0B, 0x14, 0x41,
                                                          0x00, 0xFF, 0x77};
inline constexpr std::array<uint8_t, 32> kVersionHost{0x0B, 0x14, 0x4D, 0x01};
inline constexpr std::array<uint8_t, 32> kVersionGuest{0x0B, 0x14, 0x4D,
                                                       0x01, 0x00, 0x19};

inline constexpr std::array<ConfirmedProtocolReplay, 4>
    kConfirmedProtocolReplays{{
        {"identify", kIdentifyHost, kIdentifyGuest,
         "docs/researchReports/2026-10-01-portal-public-evidence.md#p1"},
        {"activate", kActivateHost, kActivateGuest,
         "docs/researchReports/2026-10-01-portal-public-evidence.md#p2"},
        {"deactivate", kDeactivateHost, kDeactivateGuest,
         "docs/researchReports/2026-10-01-portal-public-evidence.md#p2"},
        {"version-without-audio", kVersionHost, kVersionGuest,
         "docs/researchReports/2026-10-01-portal-public-evidence.md#p3"},
    }};

inline constexpr std::span<const ConfirmedProtocolReplay>
GetConfirmedProtocolReplays() {
  return kConfirmedProtocolReplays;
}

}  // namespace xe::hid::testing

#endif  // XENIA_HID_PORTAL_TESTING_CONFIRMED_PROTOCOL_REPLAYS_H_
