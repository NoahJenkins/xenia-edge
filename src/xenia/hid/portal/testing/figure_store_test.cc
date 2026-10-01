/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/hid/portal/figure_store.h"

#include <algorithm>
#include <fstream>
#include <random>

#include "third_party/catch/single_include/catch2/catch.hpp"
#include "xenia/hid/portal/testing/synthetic_figure_fixture.h"

namespace xe::hid {
namespace {
class StoreFixture {
 public:
  StoreFixture() {
    for (int attempt = 0; attempt < 100; ++attempt) {
      root = std::filesystem::temp_directory_path() /
             ("xenia-figure-store-" + std::to_string(std::random_device{}()));
      if (std::filesystem::create_directory(root)) {
        file = root / std::filesystem::path(u8"figure café.sky");
        Put(bytes);
        return;
      }
    }
    throw std::runtime_error("Cannot create test directory");
  }
  ~StoreFixture() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  void Put(std::span<const uint8_t> data) {
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(data.data()), data.size());
  }
  std::vector<uint8_t> Read() const {
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
  }
  std::filesystem::path root, file;
  std::array<uint8_t, kFigureSize> bytes =
      testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
};
class ControlledWriter : public AtomicFileWriter {
 public:
  AtomicCommitOutcome outcome = AtomicCommitOutcome::kDurable;
  size_t calls = 0;
  AtomicWriteResult Write(const std::filesystem::path& path,
                          std::span<const uint8_t> bytes) override {
    ++calls;
    if (outcome == AtomicCommitOutcome::kNotReplaced) {
      return {outcome,
              AtomicWriteError::kWriteFailed,
              {},
              std::make_error_code(std::errc::io_error)};
    }
    auto result = CreateNativeAtomicFileWriter()->Write(path, bytes);
    if (result.outcome == AtomicCommitOutcome::kDurable &&
        outcome == AtomicCommitOutcome::kReplacedDurabilityUnknown) {
      result.outcome = outcome;
      result.error = AtomicWriteError::kCommitFlushFailed;
      result.system_error = std::make_error_code(std::errc::io_error);
    }
    return result;
  }
};
}  // namespace
TEST_CASE("Uncertain saves block all figure I/O until durable recovery",
          "[skylanders][store]") {
  StoreFixture fixture;
  auto writer = std::make_unique<ControlledWriter>();
  auto* control = writer.get();
  FigureStore store(fixture.root, std::move(writer));
  const auto load = store.LoadManaged(fixture.file);
  REQUIRE(load.handle);
  const auto handle = *load.handle;
  FigureBlock block{};
  block.fill(0xAB);
  control->outcome = AtomicCommitOutcome::kReplacedDurabilityUnknown;
  const auto saved = store.WriteBlock(handle, 8, block);
  REQUIRE(saved.error == FigureIoError::kPersistenceFailed);
  REQUIRE(saved.persistence.outcome ==
          AtomicCommitOutcome::kReplacedDurabilityUnknown);
  REQUIRE(saved.persistence.error == AtomicWriteError::kCommitFlushFailed);
  REQUIRE(fixture.Read()[128] == 0xAB);
  REQUIRE(store.ReadBlock(handle, 8).error == FigureIoError::kUnavailable);
  REQUIRE(store.WriteBlock(handle, 8, block).error ==
          FigureIoError::kUnavailable);
  REQUIRE(control->calls == 1);
  REQUIRE(store.LoadManaged(fixture.file).error ==
          FigureStoreError::kRecoveryRequired);
  REQUIRE(store.LoadReadOnly(fixture.file).error ==
          FigureStoreError::kRecoveryRequired);

  control->outcome = AtomicCommitOutcome::kNotReplaced;
  REQUIRE_FALSE(store.Recover(handle).handle);
  REQUIRE(store.ReadBlock(handle, 8).error == FigureIoError::kUnavailable);
  control->outcome = AtomicCommitOutcome::kReplacedDurabilityUnknown;
  REQUIRE_FALSE(store.Recover(handle).handle);
  control->outcome = AtomicCommitOutcome::kDurable;
  const auto recovered = store.Recover(handle);
  REQUIRE(recovered.handle);
  REQUIRE(*recovered.handle != handle);
  REQUIRE(store.ReadBlock(*recovered.handle, 8).data == block);
  REQUIRE(store.ReadBlock(handle, 8).error == FigureIoError::kUnavailable);
  REQUIRE_FALSE(store.Recover(handle).handle);
  REQUIRE(store.WriteBlock(*recovered.handle, 8, block).error ==
          FigureIoError::kNone);
}
TEST_CASE("Pre-replacement failure preserves disk and readable memory",
          "[skylanders][store]") {
  StoreFixture fixture;
  auto writer = std::make_unique<ControlledWriter>();
  writer->outcome = AtomicCommitOutcome::kNotReplaced;
  FigureStore store(fixture.root, std::move(writer));
  auto load = store.LoadManaged(fixture.file);
  REQUIRE(load.handle);
  auto before = store.ReadBlock(*load.handle, 8);
  FigureBlock block{};
  const auto result = store.WriteBlock(*load.handle, 8, block);
  REQUIRE(result.error == FigureIoError::kPersistenceFailed);
  REQUIRE(result.persistence.outcome == AtomicCommitOutcome::kNotReplaced);
  REQUIRE(fixture.Read() ==
          std::vector<uint8_t>(fixture.bytes.begin(), fixture.bytes.end()));
  REQUIRE(store.ReadBlock(*load.handle, 8).data == before.data);
  REQUIRE(store.ReadBlock(*load.handle, 8).error == FigureIoError::kNone);
}
TEST_CASE("Recovery checks the actual file structure and figure identity",
          "[skylanders][store]") {
  StoreFixture fixture;
  auto writer = std::make_unique<ControlledWriter>();
  auto* control = writer.get();
  control->outcome = AtomicCommitOutcome::kReplacedDurabilityUnknown;
  FigureStore store(fixture.root, std::move(writer));
  auto load = store.LoadManaged(fixture.file);
  REQUIRE(load.handle);
  REQUIRE(store.WriteBlock(*load.handle, 8, FigureBlock{}).error ==
          FigureIoError::kPersistenceFailed);
  control->outcome = AtomicCommitOutcome::kDurable;
  SECTION("truncated") {
    fixture.Put(std::span<const uint8_t>(fixture.bytes).first(100));
    REQUIRE(store.Recover(*load.handle).error ==
            FigureStoreError::kInvalidImage);
  }
  SECTION("different UID") {
    fixture.Put(testing::MakeSyntheticFigureBytes(1, 2, {7, 8, 9, 10}));
    REQUIRE(store.Recover(*load.handle).error ==
            FigureStoreError::kExternalConflict);
  }
  SECTION("different character") {
    fixture.Put(testing::MakeSyntheticFigureBytes(4, 2, {3, 4, 5, 6}));
    REQUIRE(store.Recover(*load.handle).error ==
            FigureStoreError::kExternalConflict);
  }
  REQUIRE(control->calls == 1);
  REQUIRE(store.ReadBlock(*load.handle, 8).error ==
          FigureIoError::kUnavailable);
}
TEST_CASE("Store rejects stale files even with unchanged size and timestamp",
          "[skylanders][store]") {
  StoreFixture fixture;
  auto writer = std::make_unique<ControlledWriter>();
  auto* control = writer.get();
  FigureStore store(fixture.root, std::move(writer));
  const auto load = store.LoadManaged(fixture.file);
  REQUIRE(load.handle);
  const auto timestamp = std::filesystem::last_write_time(fixture.file);
  fixture.bytes[128] ^= 0xFF;
  fixture.Put(fixture.bytes);
  std::filesystem::last_write_time(fixture.file, timestamp);
  REQUIRE(store.WriteBlock(*load.handle, 8, FigureBlock{}).error ==
          FigureIoError::kConflict);
  REQUIRE(control->calls == 0);
  REQUIRE(fixture.Read()[128] == fixture.bytes[128]);
}
TEST_CASE("Store validates loads and rejects invalid access",
          "[skylanders][store]") {
  StoreFixture fixture;
  FigureStore store(fixture.root, CreateNativeAtomicFileWriter());
  SECTION("read-only") {
    const auto load = store.LoadReadOnly(fixture.file);
    REQUIRE(load.handle);
    REQUIRE(store.ReadBlock(*load.handle, 8).error == FigureIoError::kNone);
    REQUIRE(store.WriteBlock(*load.handle, 8, FigureBlock{}).error ==
            FigureIoError::kReadOnly);
    REQUIRE(store.LoadManaged(fixture.file).error ==
            FigureStoreError::kAlreadyLoaded);
  }
  SECTION("bounds") {
    const auto load = store.LoadManaged(fixture.file);
    REQUIRE(load.handle);
    REQUIRE_FALSE(load.validation.IsFullyValid());
    REQUIRE(store.ReadBlock(*load.handle, 64).error ==
            FigureIoError::kInvalidBlock);
    REQUIRE(store.WriteBlock(*load.handle, 64, FigureBlock{}).error ==
            FigureIoError::kInvalidBlock);
    REQUIRE(store.ReadBlock(0, 0).error == FigureIoError::kUnavailable);
  }
  SECTION("short image") {
    fixture.Put(std::span<const uint8_t>(fixture.bytes).first(1023));
    REQUIRE(store.LoadManaged(fixture.file).error ==
            FigureStoreError::kInvalidImage);
  }
  SECTION("long image") {
    auto bytes = std::vector<uint8_t>(1025);
    fixture.Put(bytes);
    REQUIRE(store.LoadManaged(fixture.file).error ==
            FigureStoreError::kInvalidImage);
  }
  SECTION("external managed path") {
    FigureStore external(fixture.root / "elsewhere",
                         CreateNativeAtomicFileWriter());
    REQUIRE(external.LoadManaged(fixture.file).error ==
            FigureStoreError::kOutsideLibrary);
  }
}
TEST_CASE("Recovery uses the replacement candidate identity after uncertainty",
          "[skylanders][store]") {
  StoreFixture fixture;
  auto writer = std::make_unique<ControlledWriter>();
  auto* control = writer.get();
  FigureStore store(fixture.root, std::move(writer));
  const auto loaded = store.LoadManaged(fixture.file);
  REQUIRE(loaded.handle);
  const auto changed = testing::MakeSyntheticFigureBytes(42, 2, {3, 4, 5, 6});
  FigureBlock identifier;
  std::copy_n(changed.begin() + 16, identifier.size(), identifier.begin());
  control->outcome = AtomicCommitOutcome::kReplacedDurabilityUnknown;
  REQUIRE(store.WriteBlock(*loaded.handle, 1, identifier).error ==
          FigureIoError::kPersistenceFailed);
  control->outcome = AtomicCommitOutcome::kDurable;
  const auto recovered = store.Recover(*loaded.handle);
  REQUIRE(recovered.handle);
  REQUIRE(store.ReadBlock(*recovered.handle, 1).data == identifier);
}

TEST_CASE("Filesystem read-only state blocks writes and recovery",
          "[skylanders][store]") {
  StoreFixture fixture;
  auto writer = std::make_unique<ControlledWriter>();
  auto* control = writer.get();
  FigureStore store(fixture.root, std::move(writer));
  const auto loaded = store.LoadManaged(fixture.file);
  REQUIRE(loaded.handle);
  SECTION("permission removed after load") {
    std::filesystem::permissions(fixture.file,
                                 std::filesystem::perms::owner_read);
    const auto result = store.WriteBlock(*loaded.handle, 8, FigureBlock{});
    std::filesystem::permissions(fixture.file,
                                 std::filesystem::perms::owner_all);
    REQUIRE(result.error == FigureIoError::kReadOnly);
    REQUIRE(control->calls == 0);
  }
  SECTION("permission removed after uncertainty") {
    control->outcome = AtomicCommitOutcome::kReplacedDurabilityUnknown;
    REQUIRE(store.WriteBlock(*loaded.handle, 8, FigureBlock{}).error ==
            FigureIoError::kPersistenceFailed);
    std::filesystem::permissions(fixture.file,
                                 std::filesystem::perms::owner_read);
    const auto result = store.Recover(*loaded.handle);
    std::filesystem::permissions(fixture.file,
                                 std::filesystem::perms::owner_all);
    REQUIRE(result.error == FigureStoreError::kReadOnly);
    REQUIRE_FALSE(result.handle);
    REQUIRE(control->calls == 1);
    REQUIRE(store.ReadBlock(*loaded.handle, 8).error ==
            FigureIoError::kUnavailable);
  }
}
}  // namespace xe::hid
