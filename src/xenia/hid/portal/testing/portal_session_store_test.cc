/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#include "xenia/hid/portal/portal_session_store.h"

#include <fstream>
#include <random>

#include "third_party/catch/single_include/catch2/catch.hpp"
#include "xenia/base/xxhash.h"
#include "xenia/hid/portal/testing/synthetic_figure_fixture.h"

namespace xe::hid {
namespace {
class SessionFixture {
 public:
  SessionFixture() {
    for (int i = 0; i < 100; ++i) {
      root = std::filesystem::temp_directory_path() /
             ("xenia-portal-session-" + std::to_string(std::random_device{}()));
      if (std::filesystem::create_directory(root)) {
        return;
      }
    }
    throw std::runtime_error("Cannot create session test directory");
  }
  ~SessionFixture() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  std::filesystem::path Figure(const std::filesystem::path& relative) {
    const auto path = root / relative;
    auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return path;
  }
  std::filesystem::path root;
};
class FailedWriter : public AtomicFileWriter {
 public:
  AtomicWriteResult Write(const std::filesystem::path&,
                          std::span<const uint8_t>) override {
    return {AtomicCommitOutcome::kNotReplaced,
            AtomicWriteError::kWriteFailed,
            {},
            std::make_error_code(std::errc::io_error)};
  }
};
}  // namespace
TEST_CASE("Session manifest round trips Unicode paths and pending state",
          "[skylanders][session]") {
  SessionFixture fixture;
  const auto relative = std::filesystem::path(u8"café figure.sky");
  auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
  fixture.Figure(relative);
  PortalSessionStore store(fixture.root, CreateNativeAtomicFileWriter());
  PortalSession session;
  session.entries.push_back(
      {3, relative, XXH3_64bits(bytes.data(), bytes.size())});
  session.pending_saves.push_back(
      {relative, std::string(64, '0'), std::string(64, '1')});
  REQUIRE(store.Save(session).success());
  auto loaded = store.Load();
  REQUIRE_FALSE(loaded.fatal);
  REQUIRE(loaded.errors.empty());
  REQUIRE(loaded.session.entries.size() == 1);
  REQUIRE(loaded.session.entries[0].slot == 3);
  REQUIRE(loaded.session.entries[0].relative_path == relative);
  REQUIRE(loaded.session.entries[0].fingerprint ==
          session.entries[0].fingerprint);
  REQUIRE(loaded.session.pending_saves.size() == 1);
  REQUIRE(loaded.session.pending_saves[0].relative_path == relative);
}
TEST_CASE("Session restore skips invalid entries and keeps valid ones",
          "[skylanders][session]") {
  SessionFixture fixture;
  auto valid = fixture.Figure("valid.sky");
  auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
  const uint64_t hash = XXH3_64bits(bytes.data(), bytes.size());
  PortalSessionStore store(fixture.root, CreateNativeAtomicFileWriter());
  PortalSession session;
  session.entries.push_back({0, valid.filename(), hash});
  session.entries.push_back({1, "missing.sky", hash});
  session.entries.push_back({2, "valid.sky", hash});
  session.entries.push_back({3, "wrong-hash.sky", hash});
  fixture.Figure("wrong-hash.sky");
  session.entries.back().fingerprint ^= 1;
  // Duplicate paths are rejected on save. Write a manifest with valid TOML
  // directly to exercise partial restore from an external/corrupt session.
  std::ofstream file(store.manifest_path());
  file << "version = 1\npending_saves = []\nentries = ["
       << "{slot=0,path='valid.sky',fingerprint='";
  const auto hex = [](uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
      out[i] = digits[value & 15];
      value >>= 4;
    }
    return out;
  };
  file << hex(hash) << "'},"
       << "{slot=1,path='missing.sky',fingerprint='" << hex(hash) << "'},"
       << "{slot=2,path='valid.sky',fingerprint='" << hex(hash) << "'},"
       << "{slot=3,path='wrong-hash.sky',fingerprint='" << hex(hash ^ 1)
       << "'}]";
  file.close();
  auto loaded = store.Load();
  REQUIRE_FALSE(loaded.fatal);
  REQUIRE(loaded.session.entries.size() == 1);
  REQUIRE(loaded.session.entries[0].slot == 0);
  REQUIRE(loaded.errors.size() == 3);
}
TEST_CASE("Session manifest rejects unsafe paths and invalid pending metadata",
          "[skylanders][session]") {
  SessionFixture fixture;
  PortalSessionStore store(fixture.root, CreateNativeAtomicFileWriter());
  PortalSession session;
  session.pending_saves.push_back(
      {"../escape.sky", std::string(64, '0'), std::string(64, '1')});
  REQUIRE_FALSE(store.Save(session).success());
  REQUIRE_FALSE(std::filesystem::exists(store.manifest_path()));
  session.pending_saves.clear();
  session.entries.push_back({0, "/absolute.sky", 1});
  REQUIRE_FALSE(store.Save(session).success());
  REQUIRE_FALSE(std::filesystem::exists(store.manifest_path()));
  {
    std::ofstream file(store.manifest_path());
    file << "version = 1\npending_saves = [{path='../escape',old_header='"
         << std::string(64, '0') << "',candidate_header='"
         << std::string(64, '0') << "'}]\nentries=[]\n";
  }
  REQUIRE(store.Load().fatal);
}
TEST_CASE("Session write failure leaves the old manifest",
          "[skylanders][session]") {
  SessionFixture fixture;
  PortalSessionStore native(fixture.root, CreateNativeAtomicFileWriter());
  REQUIRE(native.Save({}).success());
  std::ifstream before(native.manifest_path());
  const std::string old((std::istreambuf_iterator<char>(before)), {});
  PortalSessionStore failed(fixture.root, std::make_unique<FailedWriter>());
  PortalSession changed;
  changed.pending_saves.push_back(
      {"figure.sky", std::string(64, '0'), std::string(64, '1')});
  REQUIRE_FALSE(failed.Save(changed).success());
  std::ifstream after(native.manifest_path());
  REQUIRE(std::string((std::istreambuf_iterator<char>(after)), {}) == old);
}
TEST_CASE("A stale slot record does not block a later valid restore",
          "[skylanders][session]") {
  SessionFixture fixture;
  fixture.Figure("valid.sky");
  auto bytes = testing::MakeSyntheticFigureBytes(1, 2, {3, 4, 5, 6});
  const auto hash = XXH3_64bits(bytes.data(), bytes.size());
  PortalSessionStore store(fixture.root, CreateNativeAtomicFileWriter());
  // The second record uses the slot of a missing record. It is safe to use.
  std::ofstream file(store.manifest_path());
  file << "version=1\npending_saves=[]\nentries=["
       << "{slot=0,path='missing.sky',fingerprint='0000000000000000'},"
       << "{slot=0,path='valid.sky',fingerprint='";
  constexpr char digits[] = "0123456789abcdef";
  for (int i = 15; i >= 0; --i) {
    file << digits[(hash >> (i * 4)) & 15];
  }
  file << "'}]";
  file.close();
  const auto loaded = store.Load();
  REQUIRE_FALSE(loaded.fatal);
  REQUIRE(loaded.session.entries.size() == 1);
  REQUIRE(loaded.session.entries[0].relative_path == "valid.sky");
}
}  // namespace xe::hid
