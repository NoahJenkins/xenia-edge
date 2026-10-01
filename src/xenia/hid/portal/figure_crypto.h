/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_FIGURE_CRYPTO_H_
#define XENIA_HID_PORTAL_FIGURE_CRYPTO_H_

#include <cstdint>
#include <span>

namespace xe::hid {

// CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xFFFF, no reflection or
// xorout. Figure identifier input is bytes [0, 0x1E), stored little-endian at
// 0x1E.
uint16_t ComputeFigureCrc16(std::span<const uint8_t> bytes);

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_FIGURE_CRYPTO_H_
