/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/portal_manager.h"

#include <fstream>
#include <random>

#include "third_party/catch/single_include/catch2/catch.hpp"
#include "xenia/base/xxhash.h"
#include "xenia/hid/portal/portal_session_store.h"
#include "xenia/hid/portal/testing/synthetic_figure_fixture.h"

namespace xe::hid {
namespace {
struct Fixture {
  Fixture() {
    for (int i = 0; i < 100; ++i) {
      root = std::filesystem::temp_directory_path() /
             ("xenia-portal-manager-" + std::to_string(std::random_device{}()));
      if (std::filesystem::create_directory(root)) {
        std::filesystem::create_directories(root / "skylanders" / "figures");
        return;
      }
    }
    throw std::runtime_error("Cannot create manager test directory");
  }
  ~Fixture() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  std::filesystem::path Figure(std::string name) {
    auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
    auto path = root / "skylanders" / "figures" / name;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return path;
  }
  std::filesystem::path root;
};
}  // namespace

TEST_CASE("Portal backend selection fails closed", "[skylanders][manager]") {
  Fixture fixture;
  REQUIRE(ParsePortalBackend("virtual") == PortalBackendKind::kVirtual);
  REQUIRE(ParsePortalBackend("unknown") == PortalBackendKind::kDisabled);
#ifdef XE_PLATFORM_WIN32
  REQUIRE(DefaultPortalBackend() == PortalBackendKind::kPhysical);
#else
  REQUIRE(DefaultPortalBackend() == PortalBackendKind::kDisabled);
  PortalManager unavailable(fixture.root, PortalBackendKind::kPhysical);
  REQUIRE(unavailable.Snapshot().backend == PortalBackendKind::kDisabled);
#endif
  PortalManager disabled(fixture.root, PortalBackendKind::kDisabled);
  std::array<uint8_t, 32> report{};
  uint32_t read = 99;
  uint16_t state = 99;
  REQUIRE_FALSE(disabled.IsConnected());
  REQUIRE(disabled.Read(report, read, state) == X_ERROR_DEVICE_NOT_CONNECTED);
  REQUIRE(disabled.Write(report) == X_ERROR_DEVICE_NOT_CONNECTED);
  REQUIRE(disabled.Snapshot().backend == PortalBackendKind::kDisabled);
  REQUIRE(disabled.Snapshot().slots[15].slot == 15);
  PortalManager virtual_portal(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE_FALSE(virtual_portal.IsConnected());
  REQUIRE(virtual_portal.Snapshot().backend == PortalBackendKind::kVirtual);
  REQUIRE(virtual_portal.Read(report, read, state) ==
          X_ERROR_DEVICE_NOT_CONNECTED);
  REQUIRE(virtual_portal.Write(report) == X_ERROR_DEVICE_NOT_CONNECTED);
  std::array<uint8_t, 33> too_large{};
  REQUIRE(virtual_portal.Write(too_large) == X_ERROR_DEVICE_NOT_CONNECTED);
}

TEST_CASE("Portal manager restores safe slots and persists changes",
          "[skylanders][manager]") {
  Fixture fixture;
  auto figure = fixture.Figure("first.sky");
  auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
  PortalSessionStore session(fixture.root / "skylanders" / "figures",
                             CreateNativeAtomicFileWriter(),
                             fixture.root / "skylanders");
  PortalSession initial;
  initial.entries.push_back(
      {2, figure.filename(), XXH3_64bits(bytes.data(), bytes.size())});
  REQUIRE(session.Save(initial).success());
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  auto slots = manager.Snapshot().slots;
  REQUIRE(slots[2].figure.has_value());
  REQUIRE(slots[2].phase == PortalSlotPhase::kReady);
  REQUIRE(manager.Apply({PortalOperationKind::kMove, 2, 4}).success);
  REQUIRE(manager.Snapshot().slots[4].figure.has_value());
  REQUIRE(manager.Apply({PortalOperationKind::kRemove, 4}).success);
  REQUIRE_FALSE(manager.Snapshot().slots[4].figure);
  REQUIRE(session.Load().session.entries.empty());
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 0, 0, figure}).success);
  PortalManager restarted(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE(restarted.Snapshot().slots[0].figure.has_value());
  REQUIRE_FALSE(restarted.Snapshot().slots[2].figure);
}

TEST_CASE(
    "Portal manager skips unsafe restore and does not erase pending saves",
    "[skylanders][manager]") {
  Fixture fixture;
  auto figure = fixture.Figure("safe.sky");
  PortalSessionStore session(fixture.root / "skylanders" / "figures",
                             CreateNativeAtomicFileWriter(),
                             fixture.root / "skylanders");
  PortalSession initial;
  initial.entries.push_back({1, "missing.sky", 123});
  initial.pending_saves.push_back(
      {"unknown.sky", std::string(64, '0'), std::string(64, '1')});
  REQUIRE(session.Save(initial).success());
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE_FALSE(manager.Snapshot().slots[1].figure);
  REQUIRE_FALSE(manager.Snapshot().errors.empty());
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 2, 0, figure}).success);
  auto loaded = session.Load();
  REQUIRE(loaded.session.pending_saves.size() == 1);
  REQUIRE(loaded.persisted_entries.size() == 2);
  REQUIRE(loaded.session.entries.size() == 1);
  REQUIRE(loaded.session.entries[0].slot == 2);
}

TEST_CASE("Portal manager blocks a pending figure and a malformed session",
          "[skylanders][manager]") {
  Fixture fixture;
  auto figure = fixture.Figure("pending.sky");
  PortalSessionStore session(fixture.root / "skylanders" / "figures",
                             CreateNativeAtomicFileWriter(),
                             fixture.root / "skylanders");
  PortalSession pending;
  pending.pending_saves.push_back(
      {figure.filename(), std::string(64, '0'), std::string(64, '1')});
  REQUIRE(session.Save(pending).success());
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  auto blocked = manager.Apply({PortalOperationKind::kAdd, 0, 0, figure});
  REQUIRE_FALSE(blocked.success);
  REQUIRE(blocked.figure_error == FigureStoreError::kRecoveryRequired);
  REQUIRE_FALSE(manager.Snapshot().slots[0].figure);
  {
    std::ofstream invalid(session.manifest_path(), std::ios::trunc);
    invalid << "not = [valid";
  }
  PortalManager restarted(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE_FALSE(restarted.Snapshot().errors.empty());
  REQUIRE_FALSE(
      restarted.Apply({PortalOperationKind::kAdd, 0, 0, figure}).success);
}

TEST_CASE("Portal manager imports and exports without changing source",
          "[skylanders][manager]") {
  Fixture fixture;
  auto source = fixture.Figure("source.sky");
  auto copy = fixture.root / "skylanders" / "figures" / "copy.sky";
  auto exported = fixture.root / "export.sky";
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  PortalOperation import{PortalOperationKind::kImport};
  import.path = source;
  import.destination = copy;
  REQUIRE(manager.Apply(import).success);
  REQUIRE(std::filesystem::exists(source));
  REQUIRE(std::filesystem::exists(copy));
  auto duplicate = manager.Apply(import);
  REQUIRE_FALSE(duplicate.success);
  REQUIRE(duplicate.figure_error == FigureStoreError::kAlreadyExists);
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 1, 0, copy}).success);
  PortalOperation export_op{PortalOperationKind::kExport};
  export_op.source_slot = 1;
  export_op.destination = exported;
  REQUIRE(manager.Apply(export_op).success);
  REQUIRE(std::filesystem::file_size(exported) == kFigureSize);
  auto confirmation = manager.Apply(export_op);
  REQUIRE_FALSE(confirmation.success);
  REQUIRE(confirmation.figure_error ==
          FigureStoreError::kOverwriteNotConfirmed);
  export_op.overwrite_confirmed = true;
  REQUIRE(manager.Apply(export_op).success);
}

TEST_CASE("Portal manager keeps slots unchanged when session save fails",
          "[skylanders][manager]") {
  Fixture fixture;
  auto first = fixture.Figure("first.sky");
  auto second = fixture.Figure("second.sky");
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 0, 0, first}).success);
  const auto before = manager.Snapshot().slots;
  const auto manifest = fixture.root / "skylanders" / "portal-session.toml";
  REQUIRE(std::filesystem::remove(manifest));
  REQUIRE(std::filesystem::create_directory(manifest));
  REQUIRE_FALSE(
      manager.Apply({PortalOperationKind::kReplace, 0, 0, second}).success);
  REQUIRE(manager.Snapshot().slots == before);
  REQUIRE_FALSE(manager.Apply({PortalOperationKind::kRemove, 0}).success);
  REQUIRE(manager.Snapshot().slots == before);
}

TEST_CASE("Portal manager persists replacement and rejects duplicate slots",
          "[skylanders][manager]") {
  Fixture fixture;
  auto first = fixture.Figure("first.sky");
  auto second = fixture.Figure("second.sky");
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 0, 0, first}).success);
  REQUIRE_FALSE(
      manager.Apply({PortalOperationKind::kAdd, 1, 0, first}).success);
  REQUIRE_FALSE(manager.Apply({PortalOperationKind::kMove, 0, 16}).success);
  REQUIRE(manager.Apply({PortalOperationKind::kReplace, 0, 0, second}).success);
  REQUIRE(manager.Snapshot().slots[0].phase == PortalSlotPhase::kReady);
  PortalManager restarted(fixture.root, PortalBackendKind::kVirtual);
  REQUIRE(restarted.Snapshot().slots[0].figure.has_value());
  PortalSessionStore session(fixture.root / "skylanders" / "figures",
                             CreateNativeAtomicFileWriter(),
                             fixture.root / "skylanders");
  REQUIRE(session.Load().session.entries[0].relative_path == second.filename());
}
TEST_CASE(
    "Library inspection reports invalid files and detects stale slot actions",
    "[skylanders][manager][library]") {
  Fixture fixture;
  const auto figure = fixture.Figure("valid.sky");
  {
    std::ofstream invalid(figure.parent_path() / "invalid.sky");
    invalid << "invalid";
  }
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  auto library = manager.ListLibrary();
  REQUIRE(library.ready);
  REQUIRE(library.entries.size() == 2);
  REQUIRE_FALSE(library.entries[0].validation.IsSafeToLoad());
  REQUIRE(library.entries[1].validation.IsSafeToLoad());
  REQUIRE(library.entries[1].identity.has_value());
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 0, 0, figure}).success);
  PortalOperation stale{PortalOperationKind::kRemove};
  stale.expected_generation = manager.Snapshot().slots[0].generation;
  REQUIRE(manager.Apply({PortalOperationKind::kRemove, 0}).success);
  REQUIRE(manager.Apply({PortalOperationKind::kAdd, 0, 0, figure}).success);
  REQUIRE_FALSE(manager.Apply(stale).success);
  REQUIRE(manager.Snapshot().slots[0].figure.has_value());
  library = manager.ListLibrary();
  REQUIRE(library.entries[1].mounted_slot == 0);
  {
    std::ofstream changed(figure, std::ios::app);
    changed << "changed";
  }
  REQUIRE(manager.ListLibrary().entries[1].changed);
}
TEST_CASE("Backend changes are limited to stopped titles",
          "[skylanders][manager]") {
  Fixture fixture;
  PortalManager manager(fixture.root, PortalBackendKind::kDisabled);
  REQUIRE_FALSE(
      manager.SelectBackend(PortalBackendKind::kVirtual, true).success);
  REQUIRE(manager.Snapshot().backend == PortalBackendKind::kDisabled);
  REQUIRE(manager.SelectBackend(PortalBackendKind::kVirtual, false).success);
  REQUIRE(manager.Snapshot().management_ready);
  REQUIRE_FALSE(manager.IsConnected());
  REQUIRE_FALSE(
      manager.SelectBackend(PortalBackendKind::kDisabled, true).success);
  REQUIRE(manager.SelectBackend(PortalBackendKind::kDisabled, false).success);
}
TEST_CASE(
    "Library inspection confines pending paths and clears missing imports",
    "[skylanders][manager][library]") {
  Fixture fixture;
  const auto outside = fixture.root / "outside.sky";
  const auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
  {
    std::ofstream output(outside, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }
  const auto library = fixture.root / "skylanders" / "figures";
  std::filesystem::create_directories(library);
  std::error_code link_error;
  std::filesystem::create_symlink(outside, library / "linked.sky", link_error);
  PortalSessionStore session(library, CreateNativeAtomicFileWriter(),
                             fixture.root / "skylanders");
  PortalSession state;
  state.pending_saves.push_back(
      {"missing.sky", std::string(64, '0'), std::string(64, '1')});
  if (!link_error) {
    state.pending_saves.push_back(
        {"linked.sky", std::string(64, '0'), std::string(64, '1')});
  }
  REQUIRE(session.Save(state).success());
  PortalManager manager(fixture.root, PortalBackendKind::kVirtual);
  const auto snapshot = manager.ListLibrary();
  REQUIRE(snapshot.entries.size() == (link_error ? 1 : 2));
  for (const auto& entry : snapshot.entries) {
    REQUIRE(entry.recovery_required);
    REQUIRE_FALSE(entry.identity);
    REQUIRE_FALSE(entry.validation.IsSafeToLoad());
  }
  REQUIRE(
      manager
          .Apply({PortalOperationKind::kRecover, 0, 0, library / "missing.sky"})
          .success);
  REQUIRE_FALSE(
      manager
          .Apply({PortalOperationKind::kRecover, 0, 0, library / "missing.sky"})
          .success);
  REQUIRE(session.Load().session.pending_saves.size() == (link_error ? 0 : 1));
  if (!link_error) {
    REQUIRE_FALSE(manager
                      .Apply({PortalOperationKind::kRecover, 0, 0,
                              library / "linked.sky"})
                      .success);
  }
}
}  // namespace xe::hid
