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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xe::hid {

constexpr size_t kFigureSize = 1024;
constexpr size_t kFigureBlockSize = 16;
constexpr size_t kFigureBlockCount = kFigureSize / kFigureBlockSize;

using PortalSlot = uint8_t;
using FigureHandle = uint64_t;
using FigureBlock = std::array<uint8_t, kFigureBlockSize>;

struct FigureIdentity {
  uint16_t character_id = 0;
  uint16_t variant_id = 0;
};

enum class FigureValidationCode {
  kWrongSize,
  kInvalidBcc,
  kInvalidTagConfiguration,
  kInvalidSectorTrailer,
  kInvalidChecksum,
  kDecryptFailure,
  kUnknownIdentity,
  kUnverifiedCrypto,
  kUnverifiedChecksums,
};

enum class FigureIssueSeverity { kWarning, kError };

struct FigureValidationIssue {
  FigureValidationCode code;
  FigureIssueSeverity severity;
  size_t offset;
  std::string message;
};

struct FigureValidationReport {
  std::vector<FigureValidationIssue> issues;
  bool structure_checked = false;

  // Safe for structural inspection only, not proof of a playable figure or
  // permission to overwrite an imported file.
  bool IsSafeToLoad() const;
  bool IsFullyValid() const;
};

enum class FigureIoError {
  kNone,
  kInvalidSlot,
  kInvalidBlock,
  kUnavailable,
  kReadOnly,
  kConflict,
  kPersistenceFailed,
};

struct FigureBlockReadResult {
  FigureIoError error = FigureIoError::kNone;
  FigureBlock data{};
};

struct FigureBlockWriteResult {
  FigureIoError error = FigureIoError::kNone;
};

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_FIGURE_TYPES_H_
