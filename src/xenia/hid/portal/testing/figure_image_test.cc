/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/figure_image.h"

#include <algorithm>
#include <array>
#include <vector>

#include "third_party/catch/single_include/catch2/catch.hpp"
#include "xenia/hid/portal/testing/synthetic_figure_fixture.h"

namespace xe::hid {
namespace {

bool HasIssue(const FigureValidationReport& report, FigureValidationCode code,
              FigureIssueSeverity severity, size_t offset) {
  return std::any_of(
      report.issues.begin(), report.issues.end(), [=](const auto& issue) {
        return issue.code == code && issue.severity == severity &&
               issue.offset == offset;
      });
}

auto MakeBytes() {
  return testing::MakeSyntheticFigureBytes(0xABCD, 0x1234,
                                           {0x12, 0x34, 0x56, 0x78});
}

}  // namespace

TEST_CASE("Figure parsing requires exactly 1024 bytes",
          "[skylanders][figure]") {
  for (const size_t size : {0, 1, 1023, 1025, 2048}) {
    const std::vector<uint8_t> source(size, 0x5A);
    FigureValidationReport report;
    REQUIRE_FALSE(FigureImage::Parse(source, report));
    REQUIRE_FALSE(report.IsSafeToLoad());
    REQUIRE_FALSE(report.IsFullyValid());
    REQUIRE(HasIssue(report, FigureValidationCode::kWrongSize,
                     FigureIssueSeverity::kError, 0));
    REQUIRE(source == std::vector<uint8_t>(size, 0x5A));
  }
}

TEST_CASE("Figure identity and all raw bytes survive parsing",
          "[skylanders][figure]") {
  const auto source = MakeBytes();
  FigureValidationReport report;
  const auto image = FigureImage::Parse(source, report);
  REQUIRE(image);
  REQUIRE(image->identity().character_id == 0xABCD);
  REQUIRE(image->identity().variant_id == 0x1234);
  REQUIRE(std::equal(source.begin(), source.end(), image->bytes().begin()));
  for (uint8_t block = 0; block < 64; ++block) {
    const auto data = image->ReadBlock(block);
    REQUIRE(data);
    REQUIRE(
        std::equal(data->begin(), data->end(), source.begin() + block * 16));
  }
}

TEST_CASE("Structural success does not claim crypto or identity verification",
          "[skylanders][figure]") {
  FigureValidationReport report;
  REQUIRE_FALSE(report.IsSafeToLoad());
  REQUIRE_FALSE(report.IsFullyValid());
  REQUIRE(FigureImage::Parse(MakeBytes(), report));
  REQUIRE(report.IsSafeToLoad());
  REQUIRE_FALSE(report.IsFullyValid());
  REQUIRE(HasIssue(report, FigureValidationCode::kUnverifiedCrypto,
                   FigureIssueSeverity::kWarning, 0));
  REQUIRE(HasIssue(report, FigureValidationCode::kUnverifiedChecksums,
                   FigureIssueSeverity::kWarning, 0x1E));
  REQUIRE(HasIssue(report, FigureValidationCode::kUnknownIdentity,
                   FigureIssueSeverity::kWarning, 0x10));
}

TEST_CASE("Bad identity check byte is rejected without repair",
          "[skylanders][figure]") {
  auto source = MakeBytes();
  source[4] ^= 1;
  const auto before = source;
  FigureValidationReport report;
  REQUIRE_FALSE(FigureImage::Parse(source, report));
  REQUIRE(HasIssue(report, FigureValidationCode::kInvalidBcc,
                   FigureIssueSeverity::kError, 4));
  REQUIRE(source == before);
  REQUIRE_FALSE(report.IsSafeToLoad());
}

TEST_CASE("Unrecognized tag configuration is rejected without repair",
          "[skylanders][figure]") {
  for (const size_t offset : {5, 6, 7}) {
    auto source = MakeBytes();
    source[offset] ^= 1;
    const auto before = source;
    FigureValidationReport report;
    REQUIRE_FALSE(FigureImage::Parse(source, report));
    REQUIRE(HasIssue(report, FigureValidationCode::kInvalidTagConfiguration,
                     FigureIssueSeverity::kError, offset));
    REQUIRE(source == before);
  }
}

TEST_CASE("Each sector access field is checked without changing input",
          "[skylanders][figure]") {
  for (size_t sector = 0; sector < 16; ++sector) {
    for (size_t byte = 0; byte < 4; ++byte) {
      auto source = MakeBytes();
      const size_t offset = sector * 0x40 + 0x36;
      source[offset + byte] ^= 1;
      const auto before = source;
      FigureValidationReport report;
      REQUIRE_FALSE(FigureImage::Parse(source, report));
      REQUIRE(HasIssue(report, FigureValidationCode::kInvalidSectorTrailer,
                       FigureIssueSeverity::kError, offset));
      REQUIRE(source == before);
    }
  }
}

TEST_CASE("Block replacement changes only the requested raw block",
          "[skylanders][figure]") {
  const auto source = MakeBytes();
  FigureValidationReport report;
  auto image = FigureImage::Parse(source, report);
  REQUIRE(image);
  std::array<uint8_t, 16> replacement{};
  replacement.fill(0xA5);
  REQUIRE(image->ReplaceBlock(63, replacement));
  REQUIRE(image->ReadBlock(63) == replacement);
  REQUIRE(std::equal(source.begin(), source.begin() + 1008,
                     image->bytes().begin()));
  const auto before =
      std::vector<uint8_t>(image->bytes().begin(), image->bytes().end());
  for (const uint8_t block : {64, 255}) {
    REQUIRE_FALSE(image->ReadBlock(block));
    REQUIRE_FALSE(image->ReplaceBlock(block, replacement));
    REQUIRE(std::equal(before.begin(), before.end(), image->bytes().begin()));
  }
  // Game writes are exact; validation reports new damage instead of repairing
  // it.
  REQUIRE_FALSE(image->Validate().IsSafeToLoad());
}

TEST_CASE("Parser clears old diagnostics on each attempt",
          "[skylanders][figure]") {
  FigureValidationReport report;
  REQUIRE_FALSE(FigureImage::Parse({}, report));
  REQUIRE(FigureImage::Parse(MakeBytes(), report));
  REQUIRE(report.IsSafeToLoad());
  REQUIRE_FALSE(HasIssue(report, FigureValidationCode::kWrongSize,
                         FigureIssueSeverity::kError, 0));
}

}  // namespace xe::hid
