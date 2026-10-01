/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/portal_slot_state.h"

#include <algorithm>

namespace xe::hid {

PortalSlotState::PortalSlotState() {
  for (size_t i = 0; i < slots_.size(); ++i) {
    slots_[i].slot = static_cast<PortalSlot>(i);
  }
}

bool PortalSlotState::Contains(FigureHandle figure) const {
  return std::any_of(
             slots_.begin(), slots_.end(),
             [figure](const auto& slot) { return slot.figure == figure; }) ||
         std::find(pending_.begin(), pending_.end(), figure) != pending_.end();
}

bool PortalSlotState::IsAvailable(PortalSlot slot) const {
  return slot < slots_.size() &&
         slots_[slot].phase == PortalSlotPhase::kEmpty && !pending_[slot];
}

bool PortalSlotState::Insert(PortalSlot slot, FigureHandle figure) {
  if (!IsAvailable(slot) || Contains(figure)) {
    return false;
  }
  auto& target = slots_[slot];
  target.figure = figure;
  target.phase = PortalSlotPhase::kAdded;
  ++target.generation;
  return true;
}

bool PortalSlotState::Remove(PortalSlot slot) {
  if (slot >= slots_.size() || !slots_[slot].figure) {
    return false;
  }
  auto& target = slots_[slot];
  target.figure.reset();
  target.phase = PortalSlotPhase::kRemoving;
  ++target.generation;
  return true;
}

bool PortalSlotState::Replace(PortalSlot slot, FigureHandle figure) {
  if (slot >= slots_.size() || !slots_[slot].figure || Contains(figure)) {
    return false;
  }
  Remove(slot);
  pending_[slot] = figure;
  return true;
}

bool PortalSlotState::Move(PortalSlot source, PortalSlot destination) {
  if (source >= slots_.size() || !slots_[source].figure ||
      !IsAvailable(destination)) {
    return false;
  }
  const auto figure = *slots_[source].figure;
  Remove(source);
  Insert(destination, figure);
  return true;
}

void PortalSlotState::Advance() {
  for (auto& slot : slots_) {
    switch (slot.phase) {
      case PortalSlotPhase::kRemoving:
        slot.phase = PortalSlotPhase::kEmpty;
        ++slot.generation;
        break;
      case PortalSlotPhase::kEmpty:
        if (pending_[slot.slot]) {
          slot.figure = pending_[slot.slot];
          pending_[slot.slot].reset();
          slot.phase = PortalSlotPhase::kAdded;
          ++slot.generation;
        }
        break;
      case PortalSlotPhase::kAdded:
        slot.phase = PortalSlotPhase::kReady;
        break;
      case PortalSlotPhase::kReady:
        break;
    }
  }
}

std::optional<PortalSlotSnapshot> PortalSlotState::Get(PortalSlot slot) const {
  if (slot >= slots_.size()) {
    return std::nullopt;
  }
  return slots_[slot];
}

std::array<PortalSlotSnapshot, kPortalSlotCount> PortalSlotState::Snapshot()
    const {
  return slots_;
}

}  // namespace xe::hid
