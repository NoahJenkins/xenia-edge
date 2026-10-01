/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_PROTOCOL_FIGURE_IO_H_
#define XENIA_HID_PORTAL_PROTOCOL_FIGURE_IO_H_

#include <span>

#include "xenia/hid/portal/figure_types.h"

namespace xe::hid {

class ProtocolFigureIo {
 public:
  virtual ~ProtocolFigureIo() = default;
  virtual FigureBlockReadResult ReadBlock(PortalSlot slot,
                                         uint8_t block) const = 0;
  virtual FigureBlockWriteResult WriteBlock(
      PortalSlot slot, uint8_t block,
      std::span<const uint8_t, kFigureBlockSize> data) = 0;
};

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_PROTOCOL_FIGURE_IO_H_
