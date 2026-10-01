/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_HID_PORTAL_PORTAL_SLOT_STATE_H_
#define XENIA_HID_PORTAL_PORTAL_SLOT_STATE_H_

#include <array>
#include <optional>

#include "xenia/hid/portal/figure_types.h"

namespace xe::hid {

constexpr size_t kPortalSlotCount = 16;

// These are internal phases, not Xbox wire values.
enum class PortalSlotPhase { kEmpty, kRemoving, kAdded, kReady };

struct PortalSlotSnapshot {
  PortalSlot slot;
  PortalSlotPhase phase;
  uint64_t generation;
  std::optional<FigureHandle> figure;

  bool operator==(const PortalSlotSnapshot&) const = default;
};

// The caller serializes access. No protocol timing or wire layout lives here.
class PortalSlotState {
 public:
  PortalSlotState();

  bool Insert(PortalSlot slot, FigureHandle figure);
  bool Remove(PortalSlot slot);
  bool Replace(PortalSlot slot, FigureHandle figure);
  bool Move(PortalSlot source, PortalSlot destination);
  void Advance();

  std::optional<PortalSlotSnapshot> Get(PortalSlot slot) const;
  std::array<PortalSlotSnapshot, kPortalSlotCount> Snapshot() const;

 private:
  bool Contains(FigureHandle figure) const;
  bool IsAvailable(PortalSlot slot) const;

  std::array<PortalSlotSnapshot, kPortalSlotCount> slots_{};
  std::array<std::optional<FigureHandle>, kPortalSlotCount> pending_{};
};

}  // namespace xe::hid

#endif  // XENIA_HID_PORTAL_PORTAL_SLOT_STATE_H_
