/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/xbox360_portal_protocol.h"

#include <algorithm>
#include <array>
#include <vector>

#include "third_party/catch/single_include/catch2/catch.hpp"
#include "xenia/hid/portal/testing/confirmed_protocol_replays.h"

namespace xe::hid {
namespace {

class FakeClock final : public PortalClock {
 public:
  uint64_t NowTicks() const override { return ticks; }
  uint64_t ticks = 100;
};

class FakeFigureIo final : public ProtocolFigureIo {
 public:
  FigureBlockReadResult ReadBlock(PortalSlot, uint8_t) const override {
    ++reads;
    return {FigureIoError::kUnavailable, {}};
  }
  FigureBlockWriteResult WriteBlock(
      PortalSlot, uint8_t,
      std::span<const uint8_t, kFigureBlockSize>) override {
    ++writes;
    return {FigureIoError::kUnavailable};
  }
  mutable size_t reads = 0;
  size_t writes = 0;
};

struct Fixture {
  FakeClock clock;
  FakeFigureIo io;
  PortalSlotState slots;
  Xbox360PortalProtocol protocol{io, slots, clock};
};

}  // namespace

TEST_CASE("Confirmed portal reports replay byte for byte",
          "[skylanders][protocol]") {
  REQUIRE_FALSE(testing::GetConfirmedProtocolReplays().empty());
  for (const auto& replay : testing::GetConfirmedProtocolReplays()) {
    INFO(replay.name);
    INFO(replay.evidence_reference);
    Fixture fixture;
    REQUIRE(fixture.protocol.SubmitHostReport(replay.host_report) ==
            ProtocolError::kNone);
    const auto reply = fixture.protocol.PopGuestReport();
    REQUIRE(reply);
    REQUIRE(reply->size == replay.expected_guest_report.size());
    REQUIRE(std::equal(reply->data.begin(), reply->data.end(),
                       replay.expected_guest_report.begin(),
                       replay.expected_guest_report.end()));
    REQUIRE_FALSE(fixture.protocol.PopGuestReport());
    REQUIRE(fixture.io.reads == 0);
    REQUIRE(fixture.io.writes == 0);
  }
}

TEST_CASE("Malformed report does not mutate slot state or pending replies",
          "[skylanders][protocol]") {
  Fixture fixture;
  REQUIRE(fixture.slots.Insert(2, 123));
  const auto before = fixture.slots.Snapshot();
  const std::vector<std::vector<uint8_t>> invalid{{},
                                                  {0x0B},
                                                  {0x0B, 0x14},
                                                  {0x00, 0x14, 0x52},
                                                  {0x0B, 0x00, 0x52},
                                                  {0x0B, 0x14, 0x41},
                                                  {0x0B, 0x14, 0x4D},
                                                  std::vector<uint8_t>(33, 0)};
  for (const auto& report : invalid) {
    REQUIRE(fixture.protocol.SubmitHostReport(report) ==
            ProtocolError::kMalformedReport);
    REQUIRE(fixture.slots.Snapshot() == before);
    REQUIRE_FALSE(fixture.protocol.PopGuestReport());
  }
  REQUIRE(fixture.io.reads == 0);
  REQUIRE(fixture.io.writes == 0);
}

TEST_CASE("Unsupported commands produce no guessed replies",
          "[skylanders][protocol]") {
  Fixture fixture;
  for (const uint8_t command :
       std::array<uint8_t, 9>{'Q', 'W', 'S', 'J', 'C', 'L', 'V', 0, 255}) {
    PortalReport report{0x0B, 0x14, command};
    REQUIRE(fixture.protocol.SubmitHostReport(report) ==
            ProtocolError::kUnsupportedCommand);
    REQUIRE_FALSE(fixture.protocol.PopGuestReport());
  }
  REQUIRE(fixture.io.reads == 0);
  REQUIRE(fixture.io.writes == 0);
}

TEST_CASE("Command replies are ordered with zero padding",
          "[skylanders][protocol]") {
  Fixture fixture;
  const std::array<uint8_t, 3> identify{0x0B, 0x14, 0x52};
  REQUIRE(fixture.protocol.SubmitHostReport(identify) == ProtocolError::kNone);
  REQUIRE(fixture.protocol.SubmitHostReport(testing::kVersionHost) ==
          ProtocolError::kNone);
  REQUIRE(fixture.protocol.PopGuestReport()->data == testing::kIdentifyGuest);
  REQUIRE(fixture.protocol.PopGuestReport()->data == testing::kVersionGuest);
  REQUIRE_FALSE(fixture.protocol.PopGuestReport());
}

TEST_CASE("Reset clears reports but preserves loaded figures",
          "[skylanders][protocol]") {
  Fixture fixture;
  REQUIRE(fixture.slots.Insert(1, 77));
  const auto before = fixture.slots.Snapshot();
  REQUIRE(fixture.protocol.SubmitHostReport(testing::kActivateHost) ==
          ProtocolError::kNone);
  fixture.protocol.Reset();
  REQUIRE_FALSE(fixture.protocol.PopGuestReport());
  REQUIRE(fixture.slots.Snapshot() == before);
  REQUIRE(fixture.protocol.SubmitHostReport(testing::kIdentifyHost) ==
          ProtocolError::kNone);
  REQUIRE(fixture.protocol.PopGuestReport()->data == testing::kIdentifyGuest);
}

TEST_CASE("Full reply queue rejects input without dropping earlier replies",
          "[skylanders][protocol]") {
  Fixture fixture;
  for (size_t i = 0; i < Xbox360PortalProtocol::kMaxPendingReports; ++i) {
    REQUIRE(fixture.protocol.SubmitHostReport(testing::kIdentifyHost) ==
            ProtocolError::kNone);
  }
  REQUIRE(fixture.protocol.SubmitHostReport(testing::kVersionHost) ==
          ProtocolError::kQueueFull);
  for (size_t i = 0; i < Xbox360PortalProtocol::kMaxPendingReports; ++i) {
    const auto report = fixture.protocol.PopGuestReport();
    REQUIRE(report);
    REQUIRE(report->data == testing::kIdentifyGuest);
  }
  REQUIRE_FALSE(fixture.protocol.PopGuestReport());
  REQUIRE(fixture.protocol.SubmitHostReport(testing::kVersionHost) ==
          ProtocolError::kNone);
  REQUIRE(fixture.protocol.PopGuestReport()->data == testing::kVersionGuest);
}

}  // namespace xe::hid
