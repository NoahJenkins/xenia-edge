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

namespace xe::hid {

bool FigureValidationReport::IsSafeToLoad() const {
  return structure_checked &&
         std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
           return issue.severity == FigureIssueSeverity::kError;
         });
}

bool FigureValidationReport::IsFullyValid() const {
  return IsSafeToLoad() && issues.empty();
}

std::optional<FigureImage> FigureImage::Parse(std::span<const uint8_t> bytes,
                                              FigureValidationReport& report) {
  report = {};
  if (bytes.size() != kFigureSize) {
    report.issues.push_back(
        {FigureValidationCode::kWrongSize, FigureIssueSeverity::kError, 0,
         "A raw figure image must contain exactly 1024 bytes."});
    return std::nullopt;
  }
  FigureImage image;
  std::copy(bytes.begin(), bytes.end(), image.bytes_.begin());
  report = image.Validate();
  if (!report.IsSafeToLoad()) {
    return std::nullopt;
  }
  return image;
}

std::optional<FigureBlock> FigureImage::ReadBlock(uint8_t block) const {
  if (block >= kFigureBlockCount) {
    return std::nullopt;
  }
  FigureBlock data;
  std::copy_n(bytes_.begin() + block * kFigureBlockSize, kFigureBlockSize,
              data.begin());
  return data;
}

bool FigureImage::ReplaceBlock(
    uint8_t block, std::span<const uint8_t, kFigureBlockSize> data) {
  if (block >= kFigureBlockCount) {
    return false;
  }
  std::copy(data.begin(), data.end(),
            bytes_.begin() + block * kFigureBlockSize);
  return true;
}

FigureIdentity FigureImage::identity() const {
  return {static_cast<uint16_t>(bytes_[0x10] | (uint16_t{bytes_[0x11]} << 8)),
          static_cast<uint16_t>(bytes_[0x1C] | (uint16_t{bytes_[0x1D]} << 8))};
}

FigureValidationReport FigureImage::Validate() const {
  FigureValidationReport report;
  report.structure_checked = true;
  // Confirmed structural facts, not a full validity check:
  // docs/researchReports/2026-08-24-skylanders-figure-format.md.
  const uint8_t bcc = bytes_[0] ^ bytes_[1] ^ bytes_[2] ^ bytes_[3];
  if (bytes_[4] != bcc) {
    report.issues.push_back({FigureValidationCode::kInvalidBcc,
                             FigureIssueSeverity::kError, 4,
                             "The tag identifier check byte does not match."});
  }
  constexpr std::array<uint8_t, 3> kTagConfiguration{0x81, 0x01, 0x0F};
  for (size_t i = 0; i < kTagConfiguration.size(); ++i) {
    if (bytes_[5 + i] != kTagConfiguration[i]) {
      report.issues.push_back({FigureValidationCode::kInvalidTagConfiguration,
                               FigureIssueSeverity::kError, 5 + i,
                               "The tag configuration is not supported."});
    }
  }
  constexpr std::array<uint8_t, 4> kFirstAccess{0x0F, 0x0F, 0x0F, 0x69};
  constexpr std::array<uint8_t, 4> kOtherAccess{0x7F, 0x0F, 0x08, 0x69};
  for (size_t sector = 0; sector < 16; ++sector) {
    const size_t offset = sector * 0x40 + 0x36;
    const auto& access = sector == 0 ? kFirstAccess : kOtherAccess;
    if (!std::equal(access.begin(), access.end(), bytes_.begin() + offset)) {
      report.issues.push_back({FigureValidationCode::kInvalidSectorTrailer,
                               FigureIssueSeverity::kError, offset,
                               "The sector access fields are not supported."});
    }
  }
  report.issues.push_back(
      {FigureValidationCode::kUnverifiedCrypto, FigureIssueSeverity::kWarning,
       0, "Sector keys and encrypted data have not been verified."});
  report.issues.push_back({FigureValidationCode::kUnverifiedChecksums,
                           FigureIssueSeverity::kWarning, 0x1E,
                           "Figure checksums have not been verified."});
  report.issues.push_back(
      {FigureValidationCode::kUnknownIdentity, FigureIssueSeverity::kWarning,
       0x10, "The character and variant have not been verified."});
  return report;
}

}  // namespace xe::hid
