/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/figure_crypto.h"

namespace xe::hid {

uint16_t ComputeFigureCrc16(std::span<const uint8_t> bytes) {
  // Algorithm parameters and independent vectors:
  // docs/researchReports/2026-08-24-skylanders-figure-format.md.
  uint16_t crc = 0xFFFF;
  for (const auto byte : bytes) {
    crc ^= static_cast<uint16_t>(byte) << 8;
    for (unsigned bit = 0; bit < 8; ++bit) {
      const bool high = (crc & 0x8000) != 0;
      crc = static_cast<uint16_t>(crc << 1);
      if (high) {
        crc ^= 0x1021;
      }
    }
  }
  return crc;
}

}  // namespace xe::hid
