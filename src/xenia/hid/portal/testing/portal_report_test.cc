/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/portal_report.h"

#include "third_party/catch/single_include/catch2/catch.hpp"

namespace xe::hid {

TEST_CASE("Portal report keeps the Xbox XAM size", "[skylanders][portal]") {
  STATIC_REQUIRE(sizeof(PortalReport) == kPortalBufferSize);
  QueuedPortalReport report{};
  REQUIRE(report.size == 0);
  REQUIRE(report.data == PortalReport{});
  report.size = kPortalBufferSize;
  REQUIRE(report.size == 32);
}

}  // namespace xe::hid
