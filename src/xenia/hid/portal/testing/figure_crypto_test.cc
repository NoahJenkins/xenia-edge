/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/figure_crypto.h"

#include <array>

#include "third_party/catch/single_include/catch2/catch.hpp"

namespace xe::hid {

TEST_CASE("Identifier CRC matches independently computed vectors",
          "[skylanders][crypto]") {
  // Independently computed with Python binascii.crc_hqx(input, 0xFFFF),
  // recorded in the figure-format report. Expected values never call production
  // helpers.
  REQUIRE(ComputeFigureCrc16({}) == 0xFFFF);
  const std::array<uint8_t, 9> standard{'1', '2', '3', '4', '5',
                                        '6', '7', '8', '9'};
  REQUIRE(ComputeFigureCrc16(standard) == 0x29B1);
  const std::array<uint8_t, 30> zero_header{};
  REQUIRE(ComputeFigureCrc16(zero_header) == 0x2A45);
  std::array<uint8_t, 30> increasing{};
  for (size_t i = 0; i < increasing.size(); ++i) {
    increasing[i] = static_cast<uint8_t>(i);
  }
  REQUIRE(ComputeFigureCrc16(increasing) == 0x3554);
}

TEST_CASE("Identifier CRC detects every single-bit header change",
          "[skylanders][crypto]") {
  for (size_t bit = 0; bit < 30 * 8; ++bit) {
    std::array<uint8_t, 30> header{};
    header[bit / 8] = static_cast<uint8_t>(1u << (bit % 8));
    REQUIRE(ComputeFigureCrc16(header) != 0x2A45);
  }
}

}  // namespace xe::hid
