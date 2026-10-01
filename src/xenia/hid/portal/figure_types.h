/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_FIGURE_TYPES_H_
#define XENIA_HID_PORTAL_FIGURE_TYPES_H_

#include <cstddef>
#include <cstdint>

namespace xe::hid {

constexpr size_t kFigureSize = 1024;
constexpr size_t kFigureBlockSize = 16;
constexpr size_t kFigureBlockCount = kFigureSize / kFigureBlockSize;

using PortalSlot = uint8_t;
using FigureHandle = uint64_t;

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_FIGURE_TYPES_H_
