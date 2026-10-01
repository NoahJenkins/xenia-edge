/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/portal_slot_state.h"

#include "third_party/catch/single_include/catch2/catch.hpp"

namespace xe::hid {

TEST_CASE("Inserted figures advance to ready without changing identity",
          "[skylanders][slots]") {
  PortalSlotState slots;
  REQUIRE(slots.Snapshot().size() == 16);
  REQUIRE(slots.Insert(0, 42));
  const auto added = *slots.Get(0);
  REQUIRE(added.phase == PortalSlotPhase::kAdded);
  REQUIRE(added.figure == 42);
  REQUIRE(added.generation > 0);
  slots.Advance();
  REQUIRE(slots.Get(0)->phase == PortalSlotPhase::kReady);
  REQUIRE(slots.Get(0)->figure == 42);
  REQUIRE(slots.Get(0)->generation == added.generation);
  const auto ready = slots.Snapshot();
  slots.Advance();
  REQUIRE(slots.Snapshot() == ready);
}

TEST_CASE("Removal immediately invalidates access and then clears the slot",
          "[skylanders][slots]") {
  PortalSlotState slots;
  REQUIRE(slots.Insert(15, 17));
  const auto generation = slots.Get(15)->generation;
  REQUIRE(slots.Remove(15));
  REQUIRE(slots.Get(15)->phase == PortalSlotPhase::kRemoving);
  REQUIRE_FALSE(slots.Get(15)->figure);
  REQUIRE(slots.Get(15)->generation > generation);
  const auto removing_generation = slots.Get(15)->generation;
  slots.Advance();
  REQUIRE(slots.Get(15)->phase == PortalSlotPhase::kEmpty);
  REQUIRE_FALSE(slots.Get(15)->figure);
  REQUIRE(slots.Get(15)->generation > removing_generation);
  REQUIRE_FALSE(slots.Remove(15));
}

TEST_CASE("Replacement exposes removal and empty before the new figure",
          "[skylanders][slots]") {
  PortalSlotState slots;
  REQUIRE(slots.Insert(3, 8));
  slots.Advance();
  const auto old = *slots.Get(3);
  REQUIRE(slots.Replace(3, 9));
  REQUIRE(slots.Get(3)->phase == PortalSlotPhase::kRemoving);
  REQUIRE_FALSE(slots.Get(3)->figure);
  REQUIRE(slots.Get(3)->generation > old.generation);
  const auto replacing = slots.Snapshot();
  REQUIRE_FALSE(slots.Replace(3, 10));
  REQUIRE_FALSE(slots.Insert(3, 10));
  REQUIRE_FALSE(slots.Insert(4, 9));
  REQUIRE(slots.Snapshot() == replacing);
  slots.Advance();
  REQUIRE(slots.Get(3)->phase == PortalSlotPhase::kEmpty);
  REQUIRE_FALSE(slots.Get(3)->figure);
  REQUIRE_FALSE(slots.Insert(3, 10));
  slots.Advance();
  REQUIRE(slots.Get(3)->phase == PortalSlotPhase::kAdded);
  REQUIRE(slots.Get(3)->figure == 9);
  slots.Advance();
  REQUIRE(slots.Get(3)->phase == PortalSlotPhase::kReady);
}

TEST_CASE("Move changes both generations and preserves the handle",
          "[skylanders][slots]") {
  PortalSlotState slots;
  REQUIRE(slots.Insert(1, 11));
  slots.Advance();
  const auto before = slots.Snapshot();
  REQUIRE(slots.Move(1, 14));
  REQUIRE(slots.Get(1)->phase == PortalSlotPhase::kRemoving);
  REQUIRE_FALSE(slots.Get(1)->figure);
  REQUIRE(slots.Get(14)->phase == PortalSlotPhase::kAdded);
  REQUIRE(slots.Get(14)->figure == 11);
  REQUIRE(slots.Get(1)->generation > before[1].generation);
  REQUIRE(slots.Get(14)->generation > before[14].generation);
  slots.Advance();
  REQUIRE(slots.Get(1)->phase == PortalSlotPhase::kEmpty);
  REQUIRE(slots.Get(14)->phase == PortalSlotPhase::kReady);
}

TEST_CASE("Rejected slot operations preserve every snapshot",
          "[skylanders][slots]") {
  PortalSlotState slots;
  REQUIRE(slots.Insert(0, 100));
  REQUIRE(slots.Insert(1, 101));
  const auto before = slots.Snapshot();
  REQUIRE_FALSE(slots.Insert(1, 102));
  REQUIRE_FALSE(slots.Insert(2, 100));
  REQUIRE_FALSE(slots.Replace(1, 100));
  REQUIRE_FALSE(slots.Replace(1, 101));
  REQUIRE_FALSE(slots.Replace(2, 102));
  REQUIRE_FALSE(slots.Move(0, 1));
  REQUIRE_FALSE(slots.Move(0, 0));
  REQUIRE_FALSE(slots.Move(2, 3));
  for (const PortalSlot slot : {16, 255}) {
    REQUIRE_FALSE(slots.Get(slot));
    REQUIRE_FALSE(slots.Insert(slot, 102));
    REQUIRE_FALSE(slots.Remove(slot));
    REQUIRE_FALSE(slots.Replace(slot, 102));
    REQUIRE_FALSE(slots.Move(0, slot));
    REQUIRE_FALSE(slots.Move(slot, 0));
  }
  REQUIRE(slots.Snapshot() == before);
}

}  // namespace xe::hid
