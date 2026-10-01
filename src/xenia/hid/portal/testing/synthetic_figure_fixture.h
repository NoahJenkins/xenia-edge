/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_TESTING_SYNTHETIC_FIGURE_FIXTURE_H_
#define XENIA_HID_PORTAL_TESTING_SYNTHETIC_FIGURE_FIXTURE_H_

#include <array>

#include "xenia/hid/portal/figure_types.h"

namespace xe::hid::testing {

// Test-only structural data, not a playable figure or retail dump. The format
// facts are recorded in 2026-08-24-skylanders-figure-format.md. Crypto, sector
// keys, and gameplay checksums are intentionally not claimed valid.
inline std::array<uint8_t, kFigureSize> MakeSyntheticFigureBytes(
    uint16_t character_id, uint16_t variant_id, std::array<uint8_t, 4> uid) {
  std::array<uint8_t, kFigureSize> bytes{};
  for (size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = static_cast<uint8_t>(i * 13 + 7);
  }
  for (size_t i = 0; i < uid.size(); ++i) {
    bytes[i] = uid[i];
  }
  bytes[4] = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];
  bytes[5] = 0x81;
  bytes[6] = 0x01;
  bytes[7] = 0x0F;
  bytes[0x10] = static_cast<uint8_t>(character_id);
  bytes[0x11] = static_cast<uint8_t>(character_id >> 8);
  bytes[0x1C] = static_cast<uint8_t>(variant_id);
  bytes[0x1D] = static_cast<uint8_t>(variant_id >> 8);
  for (size_t sector = 0; sector < 16; ++sector) {
    const auto offset = sector * 0x40 + 0x36;
    bytes[offset] = sector == 0 ? 0x0F : 0x7F;
    bytes[offset + 1] = 0x0F;
    bytes[offset + 2] = sector == 0 ? 0x0F : 0x08;
    bytes[offset + 3] = 0x69;
  }
  return bytes;
}

}  // namespace xe::hid::testing

#endif  // XENIA_HID_PORTAL_TESTING_SYNTHETIC_FIGURE_FIXTURE_H_
