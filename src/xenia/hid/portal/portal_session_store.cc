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
#include <set>
#include <sstream>

#include "third_party/tomlplusplus/toml.hpp"
#include "xenia/base/xxhash.h"
#include "xenia/hid/portal/figure_image.h"

namespace xe::hid {
namespace {
std::filesystem::path FromUtf8(const std::string& value) {
  const std::u8string bytes(reinterpret_cast<const char8_t*>(value.data()),
                            value.size());
  return std::filesystem::path(bytes);
}
bool ValidHeader(const std::string& header) {
  if (header.size() != 64) {
    return false;
  }
  for (const char c : header) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}
std::string Hex(uint64_t value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result(16, '0');
  for (int i = 15; i >= 0; --i) {
    result[i] = digits[value & 0xF];
    value >>= 4;
  }
  return result;
}
bool ParseHex(const std::string& text, uint64_t& value) {
  if (text.size() != 16) {
    return false;
  }
  value = 0;
  for (char c : text) {
    value <<= 4;
    if (c >= '0' && c <= '9') {
      value |= c - '0';
    } else if (c >= 'a' && c <= 'f') {
      value |= c - 'a' + 10;
    } else {
      return false;
    }
  }
  return true;
}
bool Within(const std::filesystem::path& root,
            const std::filesystem::path& file) {
  auto part = file.begin();
  for (const auto& component : root) {
    if (part == file.end() || *part++ != component) {
      return false;
    }
  }
  return part != file.end();
}
bool ValidRestoredFigure(const std::filesystem::path& root,
                         const std::filesystem::path& relative,
                         uint64_t fingerprint) {
  std::error_code error;
  const auto canonical_root = std::filesystem::canonical(root, error);
  if (error) {
    return false;
  }
  const auto file = std::filesystem::canonical(root / relative, error);
  if (error || !Within(canonical_root, file)) {
    return false;
  }
  if (std::filesystem::file_size(file, error) != kFigureSize || error) {
    return false;
  }
  std::ifstream input(file, std::ios::binary);
  if (!input) {
    return false;
  }
  std::array<uint8_t, kFigureSize> bytes;
  input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  if (input.gcount() != kFigureSize || input.bad() ||
      input.peek() != std::char_traits<char>::eof()) {
    return false;
  }
  FigureValidationReport report;
  return XXH3_64bits(bytes.data(), bytes.size()) == fingerprint &&
         FigureImage::Parse(bytes, report).has_value() && report.IsSafeToLoad();
}
}  // namespace
PortalSessionStore::PortalSessionStore(std::filesystem::path library_root,
                                       std::unique_ptr<AtomicFileWriter> writer,
                                       std::filesystem::path session_root)
    : library_root_(std::move(library_root)),
      session_root_(session_root.empty() ? library_root_
                                         : std::move(session_root)),
      writer_(std::move(writer)) {}
bool PortalSessionStore::SafeRelativePath(const std::filesystem::path& path) {
  if (path.empty() || path.has_root_path()) {
    return false;
  }
  for (const auto& part : path) {
    if (part == ".." || part == ".") {
      return false;
    }
  }
  return true;
}
std::string PortalSessionStore::Utf8(const std::filesystem::path& path) {
  const auto value = path.generic_u8string();
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}
SessionLoadResult PortalSessionStore::Load() const {
  SessionLoadResult result;
  std::error_code error;
  if (!std::filesystem::exists(manifest_path(), error)) {
    if (error) {
      result.fatal = true;
      result.errors.push_back("Cannot inspect session file");
    }
    return result;
  }
  toml::table root;
  const auto length = std::filesystem::file_size(manifest_path(), error);
  if (error || length > 65536) {
    result.fatal = true;
    result.errors.push_back("Cannot read session file safely");
    return result;
  }
  std::ifstream input(manifest_path(), std::ios::binary);
  if (!input) {
    result.fatal = true;
    result.errors.push_back("Cannot open session file");
    return result;
  }
  std::string content(static_cast<size_t>(length), '\0');
  input.read(content.data(), static_cast<std::streamsize>(content.size()));
  if (input.gcount() != static_cast<std::streamsize>(content.size()) ||
      input.bad() || input.peek() != std::char_traits<char>::eof()) {
    result.fatal = true;
    result.errors.push_back("Cannot read session file");
    return result;
  }
  try {
    root = toml::parse(content);
  } catch (const toml::parse_error&) {
    result.fatal = true;
    result.errors.push_back("Invalid session TOML");
    return result;
  }
  const auto version = root["version"].value<int64_t>();
  if (!version || *version != 1) {
    result.fatal = true;
    result.errors.push_back("Unsupported session version");
    return result;
  }
  auto* pending = root["pending_saves"].as_array();
  if (!pending) {
    result.fatal = true;
    result.errors.push_back("Missing pending-save list");
    return result;
  }
  std::set<std::filesystem::path> seen_pending;
  for (auto&& node : *pending) {
    auto* row = node.as_table();
    auto path = row ? (*row)["path"].value<std::string>() : std::nullopt;
    auto old = row ? (*row)["old_header"].value<std::string>() : std::nullopt;
    auto candidate =
        row ? (*row)["candidate_header"].value<std::string>() : std::nullopt;
    if (!path || !old || !candidate || !ValidHeader(*old) ||
        !ValidHeader(*candidate)) {
      result.fatal = true;
      result.errors.push_back("Invalid pending-save entry");
      return result;
    }
    auto relative = FromUtf8(*path);
    if (!SafeRelativePath(relative) || !seen_pending.insert(relative).second) {
      result.fatal = true;
      result.errors.push_back("Unsafe or duplicate pending-save path");
      return result;
    }
    result.session.pending_saves.push_back({relative, *old, *candidate});
  }
  auto* entries = root["entries"].as_array();
  if (!entries) {
    result.fatal = true;
    result.errors.push_back("Missing session entries");
    return result;
  }
  std::set<PortalSlot> seen_slots;
  std::set<std::filesystem::path> seen_paths;
  std::vector<PortalSessionEntry> stale_entries;
  for (auto&& node : *entries) {
    auto* row = node.as_table();
    auto slot = row ? (*row)["slot"].value<int64_t>() : std::nullopt;
    auto path = row ? (*row)["path"].value<std::string>() : std::nullopt;
    auto hash = row ? (*row)["fingerprint"].value<std::string>() : std::nullopt;
    uint64_t fingerprint = 0;
    if (!slot || *slot < 0 || *slot >= 16 || !path || !hash ||
        !ParseHex(*hash, fingerprint)) {
      result.errors.push_back("Invalid session entry");
      continue;
    }
    auto relative = FromUtf8(*path);
    if (!SafeRelativePath(relative)) {
      result.errors.push_back("Unsafe session entry");
      continue;
    }
    const PortalSessionEntry entry{static_cast<PortalSlot>(*slot), relative,
                                   fingerprint};
    if (!ValidRestoredFigure(library_root_, relative, fingerprint)) {
      result.errors.push_back("Missing, changed, or invalid restored figure");
      stale_entries.push_back(entry);
      continue;
    }
    if (seen_slots.contains(entry.slot) ||
        seen_paths.contains(entry.relative_path)) {
      result.errors.push_back("Duplicate session entry");
      continue;
    }
    seen_slots.insert(entry.slot);
    seen_paths.insert(entry.relative_path);
    result.session.entries.push_back(entry);
    result.persisted_entries.push_back(entry);
  }
  for (const auto& entry : stale_entries) {
    if (seen_slots.contains(entry.slot) ||
        seen_paths.contains(entry.relative_path)) {
      continue;
    }
    seen_slots.insert(entry.slot);
    seen_paths.insert(entry.relative_path);
    result.persisted_entries.push_back(entry);
  }
  return result;
}
SessionStoreResult PortalSessionStore::Save(const PortalSession& session) {
  SessionStoreResult result;
  if (session.version != 1) {
    result.error = "Unsupported session version";
    return result;
  }
  toml::table root;
  root.insert("version", 1);
  toml::array pending;
  std::set<std::filesystem::path> seen_pending;
  for (const auto& item : session.pending_saves) {
    if (!SafeRelativePath(item.relative_path) ||
        !seen_pending.insert(item.relative_path).second ||
        !ValidHeader(item.old_header) || !ValidHeader(item.candidate_header)) {
      result.error = "Invalid pending-save entry";
      return result;
    }
    toml::table row;
    row.insert("path", Utf8(item.relative_path));
    row.insert("old_header", item.old_header);
    row.insert("candidate_header", item.candidate_header);
    row.is_inline(true);
    pending.push_back(std::move(row));
  }
  root.insert("pending_saves", std::move(pending));
  toml::array entries;
  std::set<PortalSlot> slots;
  std::set<std::filesystem::path> paths;
  for (const auto& item : session.entries) {
    if (item.slot >= 16 || !SafeRelativePath(item.relative_path) ||
        !slots.insert(item.slot).second ||
        !paths.insert(item.relative_path).second) {
      result.error = "Invalid session entry";
      return result;
    }
    toml::table row;
    row.insert("slot", static_cast<int64_t>(item.slot));
    row.insert("path", Utf8(item.relative_path));
    row.insert("fingerprint", Hex(item.fingerprint));
    row.is_inline(true);
    entries.push_back(std::move(row));
  }
  root.insert("entries", std::move(entries));
  std::ostringstream stream;
  stream << root << '\n';
  const auto bytes = stream.str();
  result.persistence = writer_->Write(
      manifest_path(),
      {reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()});
  return result;
}
}  // namespace xe::hid
