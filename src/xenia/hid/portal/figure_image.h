/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_FIGURE_IMAGE_H_
#define XENIA_HID_PORTAL_FIGURE_IMAGE_H_

#include <array>
#include <optional>
#include <span>

#include "xenia/hid/portal/figure_types.h"

namespace xe::hid {

class FigureImage {
 public:
  static std::optional<FigureImage> Parse(std::span<const uint8_t> bytes,
                                          FigureValidationReport& report);
  std::span<const uint8_t, kFigureSize> bytes() const { return bytes_; }
  std::optional<FigureBlock> ReadBlock(uint8_t block) const;
  bool ReplaceBlock(uint8_t block,
                    std::span<const uint8_t, kFigureBlockSize> data);
  FigureIdentity identity() const;
  FigureValidationReport Validate() const;

 private:
  FigureImage() = default;
  std::array<uint8_t, kFigureSize> bytes_{};
};

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_FIGURE_IMAGE_H_
