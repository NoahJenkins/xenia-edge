/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/portal_dialog_model.h"

#include "third_party/catch/single_include/catch2/catch.hpp"

namespace xe::hid {
namespace {
PortalLibraryEntry Valid(const char* name) {
  PortalLibraryEntry entry;
  entry.name = name;
  entry.path = name;
  entry.validation.structure_checked = true;
  return entry;
}
}  // namespace
TEST_CASE("Portal dialog filters files and excludes unavailable actions",
          "[skylanders][dialog-model]") {
  PortalLibrarySnapshot library;
  library.ready = true;
  library.entries = {Valid("Alpha.sky"), Valid("BETA.sky"),
                     Valid("pending.sky"), Valid("invalid.sky")};
  library.entries[1].read_only = true;
  library.entries[2].recovery_required = true;
  library.entries[3].validation.structure_checked = false;
  REQUIRE(
      PortalDialogModel::Filter(library, "aLpHa", PortalLibraryFilter::kAll) ==
      std::vector<size_t>{0});
  REQUIRE(
      PortalDialogModel::Filter(library, "", PortalLibraryFilter::kAvailable) ==
      std::vector<size_t>{0, 1});
  REQUIRE(PortalDialogModel::Filter(library, "",
                                    PortalLibraryFilter::kNeedsAttention) ==
          std::vector<size_t>{2, 3});
  PortalManagerSnapshot portal;
  portal.backend = PortalBackendKind::kVirtual;
  portal.management_ready = true;
  PortalDialogModel model;
  model.SelectFigure("Alpha.sky");
  REQUIRE(model.Actions(portal, library) ==
          std::vector<PortalOperationKind>{PortalOperationKind::kAdd,
                                           PortalOperationKind::kImport});
  portal.slots[0].figure = 1;
  portal.slots[0].generation = 9;
  REQUIRE(model.Actions(portal, library) ==
          std::vector<PortalOperationKind>{
              PortalOperationKind::kRemove, PortalOperationKind::kReplace,
              PortalOperationKind::kMove, PortalOperationKind::kImport,
              PortalOperationKind::kExport});
  auto operation = model.Operation(PortalOperationKind::kExport, portal);
  REQUIRE(operation.expected_generation == 9);
  REQUIRE_FALSE(operation.overwrite_confirmed);
  model.SelectFigure("pending.sky");
  REQUIRE(model.Actions(portal, library).back() ==
          PortalOperationKind::kRecover);
  library.ready = false;
  REQUIRE(model.Actions(portal, library).empty());
}
TEST_CASE(
    "Portal dialog slot selection stays bounded and reports save outcomes",
    "[skylanders][dialog-model]") {
  PortalDialogModel model;
  model.MoveSlotSelection(-1);
  REQUIRE(model.selected_slot() == 15);
  model.MoveSlotSelection(1);
  REQUIRE(model.selected_slot() == 0);
  REQUIRE_FALSE(model.SelectSlot(16));
  REQUIRE(model.selected_slot() == 0);
  auto entry = Valid("figure.sky");
  REQUIRE(PortalDialogModel::Status(entry) == "Available");
  entry.read_only = true;
  REQUIRE(PortalDialogModel::Status(entry) == "Read only");
  entry.changed = true;
  REQUIRE(PortalDialogModel::Status(entry) == "Changed outside Xenia");
  entry.recovery_required = true;
  REQUIRE(PortalDialogModel::Status(entry) == "Recovery required");
  PortalOperationResult uncertain;
  uncertain.persistence.outcome =
      AtomicCommitOutcome::kReplacedDurabilityUnknown;
  REQUIRE(PortalDialogModel::Message(uncertain).find("blocked") !=
          std::string::npos);
  REQUIRE(
      PortalDialogModel::Message({false, {}, FigureStoreError::kInvalidImage})
          .find("not imported") != std::string::npos);
}
}  // namespace xe::hid
