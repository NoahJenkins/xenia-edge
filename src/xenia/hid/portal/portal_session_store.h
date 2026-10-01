/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2026 Xenia Edge contributors. All rights reserved.               *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */
#ifndef XENIA_HID_PORTAL_PORTAL_SESSION_STORE_H_
#define XENIA_HID_PORTAL_PORTAL_SESSION_STORE_H_

#include <string>
#include <vector>

#include "xenia/hid/portal/atomic_file_writer.h"
#include "xenia/hid/portal/figure_types.h"

namespace xe::hid {
struct PortalSessionEntry {
  PortalSlot slot = 0;
  std::filesystem::path relative_path;
  uint64_t fingerprint = 0;
};
struct PortalPendingSave {
  std::filesystem::path relative_path;
  std::string old_header;
  std::string candidate_header;
};
struct PortalSession {
  uint32_t version = 1;
  std::vector<PortalSessionEntry> entries;
  std::vector<PortalPendingSave> pending_saves;
};
struct SessionLoadResult {
  PortalSession session;
  // Safe syntax and unique slots/paths, including files that cannot yet be
  // restored. Save operations must preserve these records.
  std::vector<PortalSessionEntry> persisted_entries;
  std::vector<std::string> errors;
  bool fatal = false;
};
struct SessionStoreResult {
  AtomicWriteResult persistence;
  std::string error;
  bool success() const {
    return persistence.outcome == AtomicCommitOutcome::kDurable &&
           persistence.error == AtomicWriteError::kNone && error.empty();
  }
};

// The manifest is in an existing library directory. A pending-save entry is
// durable before the figure write starts. An invalid manifest fails closed.
class PortalSessionStore {
 public:
  PortalSessionStore(std::filesystem::path library_root,
                     std::unique_ptr<AtomicFileWriter> writer,
                     std::filesystem::path session_root = {});
  SessionLoadResult Load() const;
  SessionStoreResult Save(const PortalSession& session);
  std::filesystem::path manifest_path() const {
    return session_root_ / "portal-session.toml";
  }
  static bool SafeRelativePath(const std::filesystem::path& path);
  static std::string Utf8(const std::filesystem::path& path);

 private:
  std::filesystem::path library_root_;
  std::filesystem::path session_root_;
  std::unique_ptr<AtomicFileWriter> writer_;
};
}  // namespace xe::hid
#endif  // XENIA_HID_PORTAL_PORTAL_SESSION_STORE_H_
