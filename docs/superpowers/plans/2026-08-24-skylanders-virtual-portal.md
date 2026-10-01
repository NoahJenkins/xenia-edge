# Skylanders Virtual Portal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:executing-plans` to execute this plan task-by-task in the primary
> session. The user prohibited subagent dispatch and hidden delegation. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a portable virtual Xbox 360 Skylanders Portal of Power with
compatible figure files, durable persistence, an in-game handheld interface,
and evidence-based SteamOS acceptance.

**Architecture:** Keep the current 32-byte XAM portal boundary. Add a manager
that selects an independent physical, virtual, or disabled backend. The virtual
backend composes a deterministic Xbox protocol core, logical slot state, a raw
1,024-byte figure service, atomic persistence, and a sidecar session store. The
ImGui UI uses only backend-neutral manager operations.

**Tech Stack:** C++20, CMake/Ninja, Catch2 through `xe_test_suite`, Xenia cvars
and filesystem utilities, toml++, the existing AES-128 and FFmpeg MD5 code,
wxWidgets, ImGui gamepad dialogs, Linux AppImage, GitHub Actions, and Docker.

**Spec:** `docs/context/2026-08-24-skylanders-virtual-portal-design.md`

## Execution status: 2026-10-01

Tasks 1-6 are recorded as complete for their stated layers. Task 2's research
is documented; the full protocol evidence gate is still open. Task 5 implements
only corroborated R/A/M replies and remains disconnected from XAM. Task 7 has
identifier CRC validation only. Task 17 has default-test and Linux workflow
wiring only; hosted/platform checks are pending. Task 9 has native atomic
writers and passing Mac tests. Task 10 has managed/read-only handles, import/export, conflict checks, durable
writes, and recovery; creator/reset remain evidence-gated. Task 12 has a
versioned session manifest and durable pending-save records; manager-driven
slot restoration remains pending.

The [current status note](../../context/2026-10-01-portal-implementation-status.md)
records actual implementation, test results, and rulings. Accepted
[ADR 0002](../../adr/0002-portal-save-commit-outcomes.md) governs save outcomes
and recovery. [ADR 0003](../../adr/0003-persist-pending-portal-figure-saves.md)
extends the block across process restarts. The original Task 7 UID-only key signature is a
known defect and must not be transcribed into code.

## Global Constraints

- SteamOS is the primary acceptance platform. The core must compile on Linux,
  macOS, and Windows.
- Do not place Steam Deck-specific behavior in protocol, figure, slot, or
  persistence code.
- The virtual backend must not require libusb, portal hardware, root access,
  elevated permissions, or system-directory writes.
- Preserve the existing physical portal path and keep it independent from the
  virtual implementation.
- Do not copy Dolphin GPL-2.0-or-later or Cemu MPL-2.0 code, tables, tests, or
  fixtures into BSD source files.
- Use independently written code, synthetic fixtures, and evidence with
  recorded provenance.
- Compatible figure files remain exact raw 1,024-byte images. Optional Xenia
  metadata stays in sidecar files.
- Never silently overwrite an invalid, truncated, externally changed, or
  unparseable figure source.
- Use same-directory atomic replacement for figure writes and session data.
- Inject time, counters, and random UID generation into testable components.
- Do not claim game, portal-audio, or special-figure compatibility without a
  completed evidence row.
- Do not modify a Steam Deck, stable emulator installation, ROM, profile, save,
  or user figure file during local development.
- Later Deck tests use a side-by-side AppImage and copied test figure data.
- Do not push, open a pull request, merge, publish binaries, or modify an
  upstream repository without separate user approval.
- Execute all work inline in the primary Codex task. Do not use subagents.

## File and Interface Map

### Evidence and current architecture

- `docs/researchReports/2026-08-24-xbox-360-portal-protocol.md` records packet
  facts, provenance, license status, and unknown behavior.
- `docs/researchReports/2026-08-24-skylanders-figure-format.md` records file
  structure, validation, creation inputs, and rights boundaries.
- `docs/researchReports/2026-08-24-skylanders-xbox-360-compatibility.md` is the
  six-game evidence matrix.
- `docs/architecture/skylanders-virtual-portal.md` is created after integration
  changes the running architecture.

### Core portal files

- Keep `portal.h` and `.cc` as the backend contract.
- Add `portal_manager.h` and `.cc` for selection, XAM delegation, snapshots,
  UI operations, and errors.
- Add `portal_flags.h` and `.cc` for backend and library cvars.
- Keep `hardware_portal.h` and `.cc` physical-only.
- Add `virtual_portal.h` and `.cc` for virtual report transport.
- Add `portal_report.h`, `portal_slot_state.*`,
  `xbox360_portal_protocol.*`, `protocol_figure_io.h`, and
  `portal_figure_io_adapter.*` for the pure core and slot-to-store adapter.

The stable protocol signatures are:

~~~cpp
constexpr size_t kFigureSize = 1024;
constexpr size_t kFigureBlockSize = 16;
constexpr size_t kFigureBlockCount = 64;

using PortalSlot = uint8_t;
using FigureHandle = uint64_t;
using PortalReport = std::array<uint8_t, kPortalBufferSize>;

struct QueuedPortalReport {
  PortalReport data{};
  uint8_t size = 0;
};

class PortalClock {
 public:
  virtual ~PortalClock() = default;
  virtual uint64_t NowTicks() const = 0;
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
  std::array<uint8_t, kFigureBlockSize> data{};
};

struct FigureBlockWriteResult {
  FigureIoError error = FigureIoError::kNone;
};

enum class ProtocolError {
  kNone,
  kMalformedReport,
  kUnsupportedCommand,
  kInvalidSlot,
  kInvalidBlock,
  kFigureUnavailable,
  kPersistenceFailed,
};

class Xbox360PortalProtocol {
 public:
  Xbox360PortalProtocol(ProtocolFigureIo& figure_io,
                        PortalSlotState& slots, PortalClock& clock);
  ProtocolError SubmitHostReport(std::span<const uint8_t> report);
  std::optional<QueuedPortalReport> PopGuestReport();
  void Reset();
};
~~~

### Figure and storage files

- Add `figure_types.h`, `figure_image.*`, and `figure_crypto.*` for the exact
  1,024-byte representation, validation, checksums, and encryption.
- Add `figure_catalog.*`, `figure_catalog_data.inc`, and `figure_creator.*` for
  verified metadata, blank creation, and reset.
- Add `atomic_file_writer.h`, `atomic_file_writer_posix.cc`, and
  `atomic_file_writer_win.cc` for durable replacement.
- Add `figure_store.*` for import, export, managed load, conflict detection,
  game writes, and recovery.
- Add `portal_session_store.*` for a versioned TOML sidecar.

The stable figure signatures are:

~~~cpp
struct FigureIdentity {
  uint16_t character_id = 0;
  uint16_t variant_id = 0;
};

enum class FigureValidationCode {
  kWrongSize,
  kInvalidBcc,
  kInvalidSectorTrailer,
  kInvalidChecksum,
  kDecryptFailure,
  kUnknownIdentity,
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
  bool IsSafeToLoad() const;
  bool IsFullyValid() const;
};

enum class FigureStoreError {
  kNone,
  kInvalidImage,
  kReadOnly,
  kAlreadyExists,
  kOverwriteNotConfirmed,
  kExternalConflict,
  kPersistenceFailed,
};

struct FigureStoreResult {
  FigureStoreError error = FigureStoreError::kNone;
  std::string message;
};

struct FigureLoadResult : FigureStoreResult {
  std::optional<FigureHandle> handle;
  FigureValidationReport validation;
};

class FigureImage {
 public:
  static std::optional<FigureImage> Parse(
      std::span<const uint8_t> bytes, FigureValidationReport& report);
  std::span<const uint8_t, kFigureSize> bytes() const;
  std::array<uint8_t, kFigureBlockSize> ReadBlock(uint8_t block) const;
  bool ReplaceBlock(uint8_t block,
                    std::span<const uint8_t, kFigureBlockSize> data);
  FigureIdentity identity() const;
  FigureValidationReport Validate() const;
};

class AtomicFileWriter {
 public:
  virtual ~AtomicFileWriter() = default;
  virtual AtomicWriteResult Write(
      const std::filesystem::path& destination,
      std::span<const uint8_t> bytes) = 0;
};

class FigureStore {
 public:
  FigureStore(std::filesystem::path library_root,
              std::unique_ptr<AtomicFileWriter> writer);
  FigureLoadResult Import(const std::filesystem::path& source,
                          const std::filesystem::path& destination);
  FigureLoadResult LoadManaged(const std::filesystem::path& path);
  FigureStoreResult Export(FigureHandle handle,
                           const std::filesystem::path& destination,
                           bool overwrite_confirmed);
  FigureStoreResult Reset(FigureHandle handle,
                          const FigureCatalogEntry& entry,
                          bool destructive_action_confirmed);
  FigureBlockReadResult ReadBlock(FigureHandle handle,
                                  uint8_t block) const;
  FigureBlockWriteResult WriteBlock(
      FigureHandle handle, uint8_t block,
      std::span<const uint8_t, kFigureBlockSize> data);
};

class PortalFigureIoAdapter final : public ProtocolFigureIo {
 public:
  PortalFigureIoAdapter(FigureStore& store, PortalSlotState& slots);
  FigureBlockReadResult ReadBlock(PortalSlot slot,
                                  uint8_t block) const override;
  FigureBlockWriteResult WriteBlock(
      PortalSlot slot, uint8_t block,
      std::span<const uint8_t, kFigureBlockSize> data) override;
};
~~~

### UI, integration, and tests

- Add `portal_dialog_model.*` for pure presentation state.
- Add `src/xenia/ui/imgui_skylanders_portal_dialog.*` for rendering and input.
- Modify `emulator_window.*` for dialog ownership and the context-menu action.
- Modify `input_system.*` and `emulator.cc` to construct the manager with the
  storage root.
- Keep `xam_input.cc` functionally unchanged unless an integration test proves
  an adapter correction is required.
- Add `src/xenia/hid/portal/testing/CMakeLists.txt` and focused `*_test.cc`
  files. The target name is `xenia-hid-portal-tests`.
- Add the test target to `xenia-build.py` and native Linux CI after it is
  stable.

---

### Task 1: Commit the approved specification boundary

**Files:**
- Verify: `docs/AGENTS.md`
- Verify: `docs/adr/0001-native-virtual-skylanders-portal.md`
- Verify: `docs/context/2026-08-24-skylanders-virtual-portal-design.md`
- Verify: `docs/TODO.md`
- Verify: this plan

**Interfaces:**
- Consumes: the user-approved Option A architecture.
- Produces: the durable specification for all later tasks.

- [x] **Step 1: Verify files and cross-references**

~~~bash
test -f docs/AGENTS.md
test -f docs/adr/0001-native-virtual-skylanders-portal.md
test -f docs/context/2026-08-24-skylanders-virtual-portal-design.md
test -f docs/superpowers/plans/2026-08-24-skylanders-virtual-portal.md
rg -n "ADR 0001|Related ADR|Spec:" docs
~~~

Expected: every file exists and every repository link resolves.

- [x] **Step 2: Commit only approved documentation**

~~~bash
git add docs/AGENTS.md docs/TODO.md \
  docs/adr/0001-native-virtual-skylanders-portal.md \
  docs/context/2026-08-24-skylanders-virtual-portal-design.md \
  docs/superpowers/plans/2026-08-24-skylanders-virtual-portal.md
git commit -m "docs: approve native Skylanders portal design"
~~~

Expected: a local commit only. Do not push it.

### Task 2: Establish protocol, format, and license evidence

**Files:**
- Create: `docs/researchReports/2026-08-24-xbox-360-portal-protocol.md`
- Create: `docs/researchReports/2026-08-24-skylanders-figure-format.md`
- Create: `src/xenia/hid/portal/testing/confirmed_protocol_replays.h`

**Interfaces:**
- Consumes: current primary sources and independently recorded observations.
- Produces: `ConfirmedProtocolReplay` records used by protocol tests.

~~~cpp
struct ConfirmedProtocolReplay {
  std::string_view name;
  std::span<const uint8_t> host_report;
  std::span<const uint8_t> expected_guest_report;
  std::string_view evidence_reference;
};
~~~

- [x] **Step 1: Record each fact with exact evidence fields**

Use one row with Fact, Status, Exact bytes or offset, Direction, Portal
generation, Source revision, License, Redistribution status, and Verification
method. Allowed statuses are `Confirmed`, `Observed once`, and `Unknown`.

- [x] **Step 2: Record the exact repository and reference revisions**

~~~bash
git rev-parse HEAD
git remote -v
git show HEAD:LICENSE | sed -n '1,35p'
~~~

Also record the reviewed Canary, Dolphin, and Cemu SHAs. Do not copy reference
source text, fixtures, tables, or retail figure bytes.

- [x] **Step 3: Record the independent figure-format rules**

Cover exact length, block layout, identity offsets, tag fields, sector
trailers, checksum families, encrypted block selection, byte order, and raw
extensions. Mark whether two independent implementations confirm each rule.

- [x] **Step 4: Add synthetic replay arrays only for confirmed facts**

Each replay has an evidence-reference comment. An unknown behavior remains
absent from production and test replay tables.

- [x] **Step 5: Verify replay-to-report traceability**

~~~bash
rg -n "evidence_reference" \
  src/xenia/hid/portal/testing/confirmed_protocol_replays.h
rg -n "Confirmed" \
  docs/researchReports/2026-08-24-xbox-360-portal-protocol.md
~~~

Expected: every replay maps to one confirmed evidence row.

- [x] **Step 6: Commit the evidence gate**

~~~bash
git add docs/researchReports/2026-08-24-xbox-360-portal-protocol.md \
  docs/researchReports/2026-08-24-skylanders-figure-format.md \
  src/xenia/hid/portal/testing/confirmed_protocol_replays.h
git commit -m "docs: record Skylanders portal protocol evidence"
~~~

### Task 3: Add portable portal tests and report types

**Files:**
- Create: `src/xenia/hid/portal/portal_report.h`
- Create: `src/xenia/hid/portal/testing/CMakeLists.txt`
- Create: `src/xenia/hid/portal/testing/portal_report_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: `kPortalBufferSize`.
- Produces: `PortalReport`, `QueuedPortalReport`, and `ProtocolError` from the
  file map.

- [x] **Step 1: Write the failing size test**

~~~cpp
TEST_CASE("Portal report keeps the Xbox XAM size", "[skylanders][portal]") {
  STATIC_REQUIRE(sizeof(PortalReport) == kPortalBufferSize);
  QueuedPortalReport report{};
  report.size = kPortalBufferSize;
  REQUIRE(report.size == 32);
}
~~~

- [x] **Step 2: Add the test target**

~~~cmake
xe_test_suite(xenia-hid-portal-tests ${CMAKE_CURRENT_SOURCE_DIR}
  LINKS fmt xenia-base xenia-hid-portal
)
~~~

Add the testing subdirectory only when `XENIA_BUILD_TESTS` is enabled.

- [x] **Step 3: Verify the test fails before implementation**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][portal]"
~~~

Expected: compilation fails because the report types are absent.

- [x] **Step 4: Add the minimal types and rerun the test**

Expected: PASS with a 32-byte static assertion and explicit queued length.

- [x] **Step 5: Commit the test foundation**

~~~bash
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/portal_report.h \
  src/xenia/hid/portal/testing/CMakeLists.txt \
  src/xenia/hid/portal/testing/portal_report_test.cc
git commit -m "test: add portable portal test target"
~~~

### Task 4: Implement deterministic slot state

**Files:**
- Create: `src/xenia/hid/portal/portal_slot_state.h`
- Create: `src/xenia/hid/portal/portal_slot_state.cc`
- Create: `src/xenia/hid/portal/testing/portal_slot_state_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: `PortalSlot` and `FigureHandle`.
- Produces: `Insert`, `Remove`, `Replace`, `Move`, `Advance`, and immutable
  snapshots for 16 logical slots.

~~~cpp
enum class PortalSlotPhase { kEmpty, kRemoving, kAdded, kReady };

struct PortalSlotSnapshot {
  PortalSlot slot;
  PortalSlotPhase phase;
  uint64_t generation;
  std::optional<FigureHandle> figure;
};
~~~

- [x] **Step 1: Write failing transition tests**

Test added-to-ready, removing-to-empty, generation changes on replacement,
occupied move rejection without mutation, and rejection of slot 16.

- [x] **Step 2: Verify the slot tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][slots]"
~~~

Expected: compilation fails because slot state is absent.

- [x] **Step 3: Implement a fixed-array state machine**

Use no time, path, metadata, protocol packet, or UI type. Every successful
insert, replace, move, or final removal changes the affected generation.

- [x] **Step 4: Run focused and full portal tests**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][slots]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde
~~~

Expected: PASS.

- [x] **Step 5: Commit slot state**

~~~bash
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/portal_slot_state.h \
  src/xenia/hid/portal/portal_slot_state.cc \
  src/xenia/hid/portal/testing/portal_slot_state_test.cc
git commit -m "feat: add virtual portal slot state"
~~~

### Task 5: Implement the confirmed protocol foundation

**Files:**
- Create: `src/xenia/hid/portal/protocol_figure_io.h`
- Create: `src/xenia/hid/portal/xbox360_portal_protocol.h`
- Create: `src/xenia/hid/portal/xbox360_portal_protocol.cc`
- Create: `src/xenia/hid/portal/testing/xbox360_portal_protocol_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: confirmed replays, `PortalSlotState`, `PortalClock`, and
  `ProtocolFigureIo`.
- Produces: `Xbox360PortalProtocol` from the file map.

~~~cpp
class ProtocolFigureIo {
 public:
  virtual ~ProtocolFigureIo() = default;
  virtual FigureBlockReadResult ReadBlock(PortalSlot slot,
                                          uint8_t block) const = 0;
  virtual FigureBlockWriteResult WriteBlock(
      PortalSlot slot, uint8_t block,
      std::span<const uint8_t, kFigureBlockSize> data) = 0;
};
~~~

- [x] **Step 1: Write a fake figure service and injected clock**

The fake clock always returns 100 ticks until the test changes it. The fake
figure service records each read or write without opening a file.

- [x] **Step 2: Write failing replay and robustness tests**

~~~cpp
TEST_CASE("Confirmed portal reports replay byte for byte",
          "[skylanders][protocol]");
TEST_CASE("Malformed report does not mutate slot state",
          "[skylanders][protocol]");
TEST_CASE("Unsupported command returns an explicit error",
          "[skylanders][protocol]");
TEST_CASE("Reset clears reports but preserves loaded figures",
          "[skylanders][protocol]");
~~~

- [x] **Step 3: Verify the protocol tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][protocol]"
~~~

Expected: compilation fails because the protocol types are absent.

- [x] **Step 4: Implement only confirmed commands**

Use a dispatch table keyed by confirmed command and portal generation. Check
length before every field access. Queue fixed reports with explicit lengths.
Return `kUnsupportedCommand` for behavior without a confirmed evidence row.

- [x] **Step 5: Run each replay twice**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][protocol]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][protocol]"
~~~

Expected: both runs PASS with identical output.

- [x] **Step 6: Commit the protocol foundation**

~~~bash
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/protocol_figure_io.h \
  src/xenia/hid/portal/xbox360_portal_protocol.h \
  src/xenia/hid/portal/xbox360_portal_protocol.cc \
  src/xenia/hid/portal/testing/xbox360_portal_protocol_test.cc
git commit -m "feat: add confirmed Xbox portal protocol core"
~~~

### Task 6: Parse and structurally validate figure images

**Files:**
- Create: `src/xenia/hid/portal/figure_types.h`
- Create: `src/xenia/hid/portal/figure_image.h`
- Create: `src/xenia/hid/portal/figure_image.cc`
- Create: `src/xenia/hid/portal/testing/figure_image_test.cc`
- Create: `src/xenia/hid/portal/testing/synthetic_figure_fixture.h`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: independently recorded file structure.
- Produces: `FigureImage`, `FigureIdentity`, `FigureValidationIssue`, and
  `FigureValidationReport`.

~~~cpp
enum class FigureIssueSeverity { kWarning, kError };

struct FigureValidationIssue {
  FigureValidationCode code;
  FigureIssueSeverity severity;
  size_t offset;
  std::string message;
};

struct FigureValidationReport {
  std::vector<FigureValidationIssue> issues;
  bool IsSafeToLoad() const;
  bool IsFullyValid() const;
};
~~~

- [x] **Step 1: Add a synthetic fixture generator**

~~~cpp
std::array<uint8_t, kFigureSize> MakeSyntheticFigureBytes(
    uint16_t character_id, uint16_t variant_id,
    std::array<uint8_t, 4> uid);
~~~

The generator follows documented rules and contains no retail dump bytes or
copied GPL/MPL fixture data.

- [x] **Step 2: Write failing parser tests**

Test exact 1,024-byte size, little-endian identity, invalid BCC, invalid sector
trailers, rejection of block 64, unchanged source bytes after failure, and a
byte-exact serialization round trip.

- [x] **Step 3: Verify parser tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][figure]"
~~~

Expected: compilation fails because `FigureImage` is absent.

- [x] **Step 4: Implement structural parsing**

Require exact size and copy into `std::array<uint8_t, 1024>`. Use explicit
little-endian helpers. Do not modify bytes during parsing. Do not report crypto
or checksum validity until Task 7 implements those checks.

- [x] **Step 5: Run tests and commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][figure]"
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/figure_types.h \
  src/xenia/hid/portal/figure_image.h \
  src/xenia/hid/portal/figure_image.cc \
  src/xenia/hid/portal/testing/figure_image_test.cc \
  src/xenia/hid/portal/testing/synthetic_figure_fixture.h
git commit -m "feat: parse raw Skylanders figure images"
~~~

Expected: PASS before the local commit.

### Task 7: Add independent crypto and checksum validation

**Files:**
- Create: `src/xenia/hid/portal/figure_crypto.h`
- Create: `src/xenia/hid/portal/figure_crypto.cc`
- Create: `src/xenia/hid/portal/testing/figure_crypto_test.cc`
- Modify: `src/xenia/hid/portal/figure_image.cc`
- Modify: `src/xenia/hid/portal/testing/synthetic_figure_fixture.h`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: existing `aes_128`, FFmpeg MD5, and confirmed format rules.
- Produces: block-key derivation, AES block operations, and each confirmed
  checksum routine.

~~~cpp
using FigureBlock = std::array<uint8_t, kFigureBlockSize>;
using FigureKey = std::array<uint8_t, 16>;

FigureKey DeriveFigureBlockKey(std::span<const uint8_t, 4> uid,
                               uint8_t block);
FigureBlock EncryptFigureBlock(const FigureBlock& plaintext,
                               const FigureKey& key);
FigureBlock DecryptFigureBlock(const FigureBlock& ciphertext,
                               const FigureKey& key);
~~~

- [ ] **Step 1: Write failing synthetic crypto tests**

Test encrypt/decrypt inversion, deterministic derivation, a changed UID byte,
every confirmed checksum family, mirrored checksum areas, and single-bit
corruption. Expected values come from independently calculated synthetic
inputs recorded in the figure-format report.

- [ ] **Step 2: Verify crypto tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][crypto]"
~~~

Expected: compilation fails because the crypto functions are absent.

- [ ] **Step 3: Implement narrow crypto wrappers**

Keep portal constants in `figure_crypto.cc` with report references. Do not log
keys or decrypted data. Zero temporary key and plaintext buffers where
practical.

- [ ] **Step 4: Run crypto and figure tests**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][crypto]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][figure]"
~~~

Expected: PASS, including corruption detection.

- [ ] **Step 5: Commit validation**

~~~bash
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/figure_crypto.h \
  src/xenia/hid/portal/figure_crypto.cc \
  src/xenia/hid/portal/figure_image.cc \
  src/xenia/hid/portal/testing/figure_crypto_test.cc \
  src/xenia/hid/portal/testing/synthetic_figure_fixture.h
git commit -m "feat: validate Skylanders figure crypto and checksums"
~~~

### Task 8: Create and reset figures from verified metadata

**Files:**
- Create: `src/xenia/hid/portal/figure_catalog.h`
- Create: `src/xenia/hid/portal/figure_catalog.cc`
- Create: `src/xenia/hid/portal/figure_catalog_data.inc`
- Create: `src/xenia/hid/portal/figure_creator.h`
- Create: `src/xenia/hid/portal/figure_creator.cc`
- Create: `src/xenia/hid/portal/testing/figure_creator_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: metadata rows with confirmed provenance and creation rules.
- Produces: catalog lookup, `CreateBlankFigure`, and `ResetFigure`.

~~~cpp
enum class SkylandersGame : uint8_t {
  kSpyrosAdventure,
  kGiants,
  kSwapForce,
  kTrapTeam,
  kSuperChargers,
  kImaginators,
};

enum class FigureType : uint8_t {
  kStandard,
  kTrap,
  kSwapCombination,
  kVehicle,
  kTrophy,
  kCreationCrystal,
  kUnknown,
};

struct FigureCatalogEntry {
  uint16_t character_id;
  uint16_t variant_id;
  SkylandersGame game;
  FigureElement element;
  FigureType type;
  std::string_view name;
  std::string_view provenance;
  bool creation_supported;
};

class FigureUidGenerator {
 public:
  virtual ~FigureUidGenerator() = default;
  virtual std::array<uint8_t, 4> Generate() = 0;
};

struct FigureCreationResult {
  std::optional<FigureImage> image;
  FigureValidationReport validation;
  std::string error;
};
~~~

- [ ] **Step 1: Write failing catalog tests**

Test unique ID/variant pairs, nonempty provenance, valid UTF-8 names, and the
rule that creation support requires confirmed blank-image rules.

- [ ] **Step 2: Write deterministic creation and reset tests**

Use UID `{0x01, 0x02, 0x03, 0x04}`. Verify size, BCC, identity offsets,
trailers, checksums, encryption, and parse/serialize round trip. Reset preserves
the UID and restores initial gameplay state.

- [ ] **Step 3: Verify creator tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][creator]"
~~~

Expected: compilation fails because the catalog and creator are absent.

- [ ] **Step 4: Add only evidence-approved production entries**

Each production entry cites its figure-format report row. If no entry has
confirmed creation rules, keep the production catalog empty and keep creation
unavailable while import remains supported.

- [ ] **Step 5: Implement creation, run all figure tests, and commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][creator]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][figure]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][crypto]"
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/figure_catalog.h \
  src/xenia/hid/portal/figure_catalog.cc \
  src/xenia/hid/portal/figure_catalog_data.inc \
  src/xenia/hid/portal/figure_creator.h \
  src/xenia/hid/portal/figure_creator.cc \
  src/xenia/hid/portal/testing/figure_creator_test.cc
git commit -m "feat: create verified blank Skylanders figures"
~~~

Expected: all focused tests PASS before the local commit.

### Task 9: Implement durable atomic replacement

**Files:**
- Create: `src/xenia/hid/portal/atomic_file_writer.h`
- Create: `src/xenia/hid/portal/atomic_file_writer.cc`
- Create: `src/xenia/hid/portal/atomic_file_writer_posix.cc`
- Create: `src/xenia/hid/portal/atomic_file_writer_win.cc`
- Create: `src/xenia/hid/portal/testing/atomic_file_writer_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: a destination path and immutable bytes.
- Produces: `AtomicFileWriter::Write` and a native-writer factory.

~~~cpp
enum class AtomicWriteError {
  kNone,
  kCreateTemporaryFailed,
  kWriteFailed,
  kFlushFailed,
  kReplaceFailed,
  kCommitFlushFailed,
};

enum class AtomicCommitOutcome {
  kNotReplaced, kDurable, kReplacedDurabilityUnknown
};

struct AtomicWriteResult {
  AtomicCommitOutcome outcome = AtomicCommitOutcome::kNotReplaced;
  AtomicWriteError error = AtomicWriteError::kNone;
  std::filesystem::path recovery_path;
  std::error_code system_error;
};
~~~

- [x] **Step 1: Write failing filesystem tests**

Test spaces and non-ASCII paths, replacement, read-only destination directory,
forced write failure, and preservation of old bytes before replacement.
Apply accepted ADR 0002: distinguish NotReplaced, Durable, and
ReplacedDurabilityUnknown; assert the new disk bytes after a final flush error. Use a
unique directory below the host temporary directory and delete only that exact
validated test directory.

- [x] **Step 2: Verify atomic tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][atomic-write]"
~~~

Expected: compilation fails because the writer is absent.

- [x] **Step 3: Implement POSIX replacement**

Require an existing parent directory. Use exclusive same-directory temporary
creation, complete write, file `fsync`, rename replacement, and parent-directory
`fsync`. On macOS, require `F_FULLFSYNC` before replacement and after the
parent-directory flush; do not downgrade failures. Retain a recovery file only
when it contains a complete candidate.

- [x] **Step 4: Implement Windows replacement**

Use wide paths, exclusive temporary creation, `FlushFileBuffers`, then
`ReplaceFileW` or `MoveFileExW` with replacement and write-through behavior.

- [x] **Step 5: Run focused tests and commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][atomic-write]"
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/atomic_file_writer.h \
  src/xenia/hid/portal/atomic_file_writer_posix.cc \
  src/xenia/hid/portal/atomic_file_writer_win.cc \
  src/xenia/hid/portal/testing/atomic_file_writer_test.cc
git commit -m "feat: add atomic portal persistence"
~~~

Expected: PASS on the current platform. Windows remains unverified until
native Windows CI runs.

### Task 10: Implement safe figure library operations

**Files:**
- Create: `src/xenia/hid/portal/figure_store.h`
- Create: `src/xenia/hid/portal/figure_store.cc`
- Create: `src/xenia/hid/portal/testing/figure_store_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: `FigureImage`, creator, `AtomicFileWriter`, and XXH3 fingerprints.
- Produces: handle-based `FigureStore` operations from the file map.

- [ ] **Step 1: Add a fake writer and failing library tests**

Cover valid import, truncated and 1,025-byte rejection, invalid image without
destination creation, duplicate destination, confirmed overwrite, read-only
load, export confirmation, spaces/non-ASCII paths, reset confirmation,
external modification conflict, failed atomic write, successful block write,
and reload after reinsertion.

- [ ] **Step 2: Verify store tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][store]"
~~~

Expected: compilation fails because the store is absent.

- [ ] **Step 3: Implement managed handles and snapshots**

Use monotonically increasing nonzero handles. Store canonical path, size,
write timestamp, XXH3 fingerprint, read-only state, validation report, and
in-memory image. Do not use a pathname as a handle.

- [ ] **Step 4: Implement durable game writes**

Clone the image, replace one valid block, compare current file identity and
fingerprint, atomically write the complete candidate, then commit memory. A
failure before replacement returns `kPersistenceFailed` and keeps old disk
and memory bytes. Under accepted ADR 0002, failure after replacement keeps
the candidate in memory, reports uncertain durability, and blocks all figure
I/O until explicit recovery validates and durably saves the actual file.

- [ ] **Step 5: Run store and atomic tests, then commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][store]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][atomic-write]"
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/figure_store.h \
  src/xenia/hid/portal/figure_store.cc \
  src/xenia/hid/portal/testing/figure_store_test.cc
git commit -m "feat: add safe Skylanders figure library"
~~~

Expected: PASS before the local commit.

### Task 11: Connect reports to durable figure I/O

**Files:**
- Create: `src/xenia/hid/portal/portal_figure_io_adapter.h`
- Create: `src/xenia/hid/portal/portal_figure_io_adapter.cc`
- Modify: `src/xenia/hid/portal/xbox360_portal_protocol.cc`
- Modify: `src/xenia/hid/portal/figure_store.h`
- Modify: `src/xenia/hid/portal/figure_store.cc`
- Create: `src/xenia/hid/portal/testing/portal_figure_io_test.cc`

**Interfaces:**
- Consumes: confirmed query/write replays, `PortalSlotState`, `FigureStore`,
  and `ProtocolFigureIo`.
- Produces: byte-exact block reads and durable-before-success block writes.

- [ ] **Step 1: Write failing report-to-file tests**

Submit a confirmed query for a synthetic managed figure and compare the reply
byte for byte. Submit a confirmed write, reload from disk, and verify exactly
one block changed. Force atomic failure and verify memory and disk stay exact.

- [ ] **Step 2: Verify integration tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][protocol-figure-io]"
~~~

Expected: tests fail because the slot-to-handle adapter is absent.

- [ ] **Step 3: Route only confirmed query and write commands**

Resolve the current slot snapshot to a `FigureHandle`, validate generation,
block, and payload size, then call the handle-based store. Queue success only
after durable `WriteBlock` success. If evidence defines no failure packet,
return `kPersistenceFailed` without inventing one.

- [ ] **Step 4: Run focused and full tests, then commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][protocol-figure-io]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde
git add src/xenia/hid/portal/xbox360_portal_protocol.cc \
  src/xenia/hid/portal/portal_figure_io_adapter.h \
  src/xenia/hid/portal/portal_figure_io_adapter.cc \
  src/xenia/hid/portal/figure_store.h \
  src/xenia/hid/portal/figure_store.cc \
  src/xenia/hid/portal/testing/portal_figure_io_test.cc
git commit -m "feat: persist virtual portal figure writes"
~~~

Expected: PASS before the local commit.

### Task 12: Persist and safely restore portal sessions

**Files:**
- Create: `src/xenia/hid/portal/portal_session_store.h`
- Create: `src/xenia/hid/portal/portal_session_store.cc`
- Create: `src/xenia/hid/portal/testing/portal_session_store_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: the atomic writer, slot snapshots, library root, relative paths,
  and figure fingerprints.
- Produces: a versioned TOML manifest at
  `<storage_root>/skylanders/portal-session.toml`.

~~~cpp
struct PortalSessionEntry {
  PortalSlot slot;
  std::filesystem::path relative_path;
  uint64_t fingerprint;
};

struct PortalSession {
  uint32_t version = 1;
  std::vector<PortalSessionEntry> entries;
};

struct SessionLoadResult {
  PortalSession session;
  std::vector<std::string> errors;
};

struct SessionStoreResult {
  bool success = false;
  std::string error;
};
~~~

- [ ] **Step 1: Write failing session tests**

Cover round trip, invalid TOML, unsupported version, absolute-path rejection,
path traversal, missing file, changed fingerprint, duplicate file, duplicate
slot, invalid figure, non-ASCII relative path, and atomic save failure.

- [ ] **Step 2: Verify session tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][session]"
~~~

Expected: compilation fails because the session store is absent.

- [ ] **Step 3: Implement versioned TOML and partial safe restore**

Link `tomlplusplus`. Resolve each path against the canonical library root and
reject any escape. Return safe restored entries and a separate error list; one
bad entry must not discard other safe entries.

- [ ] **Step 4: Run session and store tests, then commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][session]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][store]"
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/portal_session_store.h \
  src/xenia/hid/portal/portal_session_store.cc \
  src/xenia/hid/portal/testing/portal_session_store_test.cc
git commit -m "feat: restore safe virtual portal sessions"
~~~

Expected: PASS before the local commit.

### Task 13: Add the virtual backend and explicit manager

**Files:**
- Create: `src/xenia/hid/portal/portal_flags.h`
- Create: `src/xenia/hid/portal/portal_flags.cc`
- Create: `src/xenia/hid/portal/portal_manager.h`
- Create: `src/xenia/hid/portal/portal_manager.cc`
- Create: `src/xenia/hid/portal/virtual_portal.h`
- Create: `src/xenia/hid/portal/virtual_portal.cc`
- Create: `src/xenia/hid/portal/testing/portal_manager_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`
- Modify: `src/xenia/hid/input_system.h`
- Modify: `src/xenia/hid/input_system.cc`
- Modify: `src/xenia/emulator.cc`

**Interfaces:**
- Consumes: existing `Portal`, virtual core, store, session, storage root, and
  persisted cvars.
- Produces: XAM-facing manager delegation and backend-neutral UI operations.

~~~cpp
enum class PortalBackendKind { kDisabled, kPhysical, kVirtual };

enum class PortalOperationKind {
  kAdd,
  kCreate,
  kRemove,
  kReplace,
  kMove,
  kImport,
  kExport,
  kReset,
};

struct PortalOperation {
  PortalOperationKind kind;
  PortalSlot source_slot = 0;
  PortalSlot destination_slot = 0;
  std::optional<FigureHandle> figure;
  std::filesystem::path path;
  bool destructive_action_confirmed = false;
};

struct PortalManagerSnapshot {
  PortalBackendKind backend = PortalBackendKind::kDisabled;
  bool connected = false;
  std::array<PortalSlotSnapshot, PortalSlotState::kSlotCount> slots{};
};

struct PortalOperationResult {
  bool success = false;
  std::string error;
};

class PortalManager {
 public:
  PortalManager(std::filesystem::path storage_root,
                PortalBackendKind requested_backend);
  bool IsConnected() const;
  X_STATUS Read(std::span<uint8_t> data, uint32_t& bytes_read,
                uint16_t& state);
  X_STATUS Write(std::span<uint8_t> data);
  void OnDeviceArrival();
  void OnDeviceRemoval();
  PortalManagerSnapshot Snapshot() const;
  PortalOperationResult Apply(const PortalOperation& operation);
};
~~~

- [ ] **Step 1: Write failing manager tests**

Test disabled behavior, virtual connection without libusb, platform default,
invalid cvar fallback, physical hotplug delegation, rejection above 32 bytes,
empty virtual read behavior, and queued virtual report delivery.

- [ ] **Step 2: Verify manager tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][manager]"
~~~

Expected: compilation fails because the manager is absent.

- [ ] **Step 3: Add explicit persisted configuration**

Define `portal_backend` in `HID` and `skylanders_figure_library` in `Storage`.
Default to `physical` on Windows and `disabled` elsewhere. Resolve an empty
library path to `<storage_root>/skylanders/figures`.

- [ ] **Step 4: Implement manager and virtual delegation**

Keep all libusb code inside the physical backend and Windows guards. The
virtual target must not link libusb on Linux or macOS. Disabled mode returns
`X_ERROR_DEVICE_NOT_CONNECTED`.

- [ ] **Step 5: Integrate with InputSystem**

~~~cpp
InputSystem::InputSystem(xe::ui::Window* window,
                         const std::filesystem::path& storage_root);
PortalManager* GetPortal() { return portal_manager_.get(); }
~~~

Construct it with `Emulator::storage_root_`. Keep XAM call sites using only
`GetPortal()->Read/Write`.

- [ ] **Step 6: Run tests, build the app, and commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde
./xb build --target xenia-app --config=checked
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/portal_flags.h \
  src/xenia/hid/portal/portal_flags.cc \
  src/xenia/hid/portal/portal_manager.h \
  src/xenia/hid/portal/portal_manager.cc \
  src/xenia/hid/portal/virtual_portal.h \
  src/xenia/hid/portal/virtual_portal.cc \
  src/xenia/hid/portal/testing/portal_manager_test.cc \
  src/xenia/hid/input_system.h src/xenia/hid/input_system.cc \
  src/xenia/emulator.cc
git commit -m "feat: integrate selectable virtual portal backend"
~~~

Expected: tests and native app build PASS before the local commit.

### Task 14: Preserve the physical backend contract

**Files:**
- Modify: `src/xenia/hid/portal/hardware_portal.h`
- Modify: `src/xenia/hid/portal/hardware_portal.cc`
- Create: `src/xenia/hid/portal/testing/portal_backend_contract_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: existing physical behavior and the manager backend factory.
- Produces: one tested backend contract for physical and virtual paths.

- [ ] **Step 1: Write backend contract tests with fakes**

Verify connection, maximum size, current timeout behavior, device removal
closing only physical state, and rejection of backend changes while a title is
active.

- [ ] **Step 2: Run the contract tests**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][backend-contract]"
~~~

Expected: tests expose any manager refactor regression.

- [ ] **Step 3: Apply only the minimum physical adaptation**

Preserve current VID/PID values, endpoints, Windows guards, and default. A
Canary PR #1157 adaptation must preserve authorship and cite the PR. Do not add
Linux physical portal support in this task.

- [ ] **Step 4: Run all portal tests and commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde
git add src/xenia/hid/portal/hardware_portal.h \
  src/xenia/hid/portal/hardware_portal.cc \
  src/xenia/hid/portal/testing/portal_backend_contract_test.cc \
  src/xenia/hid/portal/CMakeLists.txt
git commit -m "refactor: preserve physical portal backend contract"
~~~

Expected: PASS. Fake tests do not prove physical hardware acceptance.

### Task 15: Add the pure handheld dialog model

**Files:**
- Create: `src/xenia/hid/portal/portal_dialog_model.h`
- Create: `src/xenia/hid/portal/portal_dialog_model.cc`
- Create: `src/xenia/hid/portal/testing/portal_dialog_model_test.cc`
- Modify: `src/xenia/hid/portal/CMakeLists.txt`

**Interfaces:**
- Consumes: manager snapshots, catalog entries, and library entries.
- Produces: selected slot, filtered list, ordered actions, confirmations, and
  status text without ImGui types.

- [ ] **Step 1: Write failing model tests**

Cover empty and occupied slots, controller traversal, touch selection,
case-insensitive name search, filters, invalid/read-only/unsaved/conflict
badges, disabled reset without creation support, and reset/overwrite
confirmation.

- [ ] **Step 2: Verify model tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][dialog-model]"
~~~

Expected: compilation fails because the model is absent.

- [ ] **Step 3: Implement fixed controller ordering**

Order actions as Add, Create, Remove, Replace, Move, Import, Export, Reset.
Exclude unavailable actions so they cannot trap focus. Derive each message
from a structured store or manager error.

- [ ] **Step 4: Run focused tests and commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][dialog-model]"
git add src/xenia/hid/portal/CMakeLists.txt \
  src/xenia/hid/portal/portal_dialog_model.h \
  src/xenia/hid/portal/portal_dialog_model.cc \
  src/xenia/hid/portal/testing/portal_dialog_model_test.cc
git commit -m "feat: add controller-first portal dialog model"
~~~

Expected: PASS before the local commit.

### Task 16: Add the in-game ImGui portal interface

**Files:**
- Create: `src/xenia/ui/imgui_skylanders_portal_dialog.h`
- Create: `src/xenia/ui/imgui_skylanders_portal_dialog.cc`
- Modify: `src/xenia/app/emulator_window.h`
- Modify: `src/xenia/app/emulator_window.cc`

**Interfaces:**
- Consumes: `ImGuiGamepadDialog`, dialog model, manager, confirmation dialog,
  and file picker.
- Produces: `ToggleSkylandersPortalDialog()` and the context-menu action.

- [ ] **Step 1: Add dialog ownership and lifecycle**

~~~cpp
void ToggleSkylandersPortalDialog();
ui::ImGuiSkylandersPortalDialog* skylanders_portal_dialog_ = nullptr;
~~~

Follow existing dialog ownership. Clear the pointer on close and close the
dialog from `ClearDialogs()`.

- [ ] **Step 2: Add the context-menu entry**

Add `Skylanders Portal` near Profiles and Audio. Keep it available without a
running title so the library can be managed before launch.

- [ ] **Step 3: Render controller and touch-safe controls**

Use full-row selectables, at least 44 logical pixels of touch height, no
hover-only action, A/Enter activation, and B/Escape close. Show selection
without color alone.

- [ ] **Step 4: Route all operations through the manager**

Use explicit operations for add, create, remove, replace, move, import, export,
and reset. Require confirmation for reset and overwrite. Keep errors visible
without closing the dialog.

- [ ] **Step 5: Build and run model tests**

~~~bash
./xb build --target xenia-app --config=checked
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][dialog-model]"
~~~

Expected: build and tests PASS.

- [ ] **Step 6: Perform local macOS interaction checks**

Use a temporary storage root and synthetic library. Verify opening, closing,
focus, confirmation, invalid-file display, and app restart with mouse,
keyboard, and any controller that is actually available. Record untested input
methods as unverified.

- [ ] **Step 7: Commit the handheld UI**

~~~bash
git add src/xenia/ui/imgui_skylanders_portal_dialog.h \
  src/xenia/ui/imgui_skylanders_portal_dialog.cc \
  src/xenia/app/emulator_window.h src/xenia/app/emulator_window.cc
git commit -m "feat: add in-game Skylanders portal interface"
~~~

### Task 17: Add Linux test and Docker parity coverage

**Files:**
- Modify: `xenia-build.py`
- Modify: `.github/workflows/build.yml`
- Create: `tools/build/linux/Dockerfile`
- Create: `tools/build/linux/verify-portal-build.sh`
- Modify: `docs/building.md`

**Interfaces:**
- Consumes: `xenia-hid-portal-tests`, Clang 21 Linux CI, and the AppImage job.
- Produces: Apple Silicon development parity and authoritative native x86-64
  CI.

- [ ] **Step 1: Add the portal suite to default tests**

Add `xenia-hid-portal-tests` to `TestCommand` after its existing targets. Run:

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde
~~~

Expected: PASS before CI changes.

- [ ] **Step 2: Add a native Linux portal-test job**

Use the pinned compiler and dependencies from the existing Linux job. Build
and run `xenia-hid-portal-tests`, then build `xenia-app`. Repository-critical
scripts must not depend on `rg` unless CI installs it explicitly.

- [ ] **Step 3: Add the Docker parity image and script**

Use the same Ubuntu release and Clang 21. Install packages as root, then run
build commands as a non-root user. The script runs:

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde
./xb build --target xenia-app --config=release
~~~

- [ ] **Step 4: Run Apple Silicon x86-64 parity**

~~~bash
docker build --platform linux/amd64 \
  -f tools/build/linux/Dockerfile -t xenia-edge-portal-linux .
docker run --rm --platform linux/amd64 \
  xenia-edge-portal-linux tools/build/linux/verify-portal-build.sh
~~~

Expected: tests and Linux app build PASS. This is parity evidence, not native
performance or SteamOS evidence.

- [ ] **Step 5: Verify native Linux CI and AppImage**

Expected: portal test, app build, and existing AppImage job pass for the exact
commit. Do not publish the AppImage.

- [ ] **Step 6: Commit build coverage**

~~~bash
git add xenia-build.py .github/workflows/build.yml \
  tools/build/linux/Dockerfile \
  tools/build/linux/verify-portal-build.sh docs/building.md
git commit -m "ci: verify virtual portal on native Linux"
~~~

### Task 18: Report special-object and audio capability explicitly

**Files:**
- Modify: `src/xenia/hid/portal/figure_types.h`
- Modify: `src/xenia/hid/portal/figure_catalog.h`
- Modify: `src/xenia/hid/portal/portal_manager.h`
- Modify: `src/xenia/hid/portal/portal_dialog_model.cc`
- Create: `src/xenia/hid/portal/testing/portal_capability_test.cc`

**Interfaces:**
- Consumes: evidence for traps, Swap Force combinations, vehicles, trophies,
  Creation Crystals, Traptanium data, and portal audio.
- Produces: `Supported`, `ReadOnly`, `Experimental`, or `Unsupported` with a
  reason for each capability.

- [ ] **Step 1: Write failing capability tests**

Verify that a standard figure pass does not imply any special-object support,
Trap Team data does not imply portal audio, unsupported imported types may be
shown without creation, and each UI limitation has a reason.

- [ ] **Step 2: Verify capability tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][capability]"
~~~

Expected: tests fail because explicit capability reporting is absent.

- [ ] **Step 3: Implement evidence-backed lookup**

Default every unlisted special type and portal-audio feature to `Unsupported`.
Do not add a general `portal_working` boolean.

- [ ] **Step 4: Run capability and dialog tests, then commit**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][capability]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][dialog-model]"
git add src/xenia/hid/portal/figure_types.h \
  src/xenia/hid/portal/figure_catalog.h \
  src/xenia/hid/portal/portal_manager.h \
  src/xenia/hid/portal/portal_dialog_model.cc \
  src/xenia/hid/portal/testing/portal_capability_test.cc
git commit -m "feat: report portal capability limits explicitly"
~~~

Expected: PASS before the local commit.

### Task 19: Add evidence-backed special figure behavior

**Files:**
- Modify: `src/xenia/hid/portal/figure_catalog_data.inc`
- Modify: `src/xenia/hid/portal/figure_creator.cc`
- Modify: `src/xenia/hid/portal/xbox360_portal_protocol.cc`
- Create: `src/xenia/hid/portal/testing/special_figure_test.cc`
- Modify: `docs/researchReports/2026-08-24-skylanders-figure-format.md`

**Interfaces:**
- Consumes: confirmed initialization and report evidence for traps, Swap Force
  combinations, vehicles, trophies, and Creation Crystals.
- Produces: import, query, write, reset, and creation support separately for
  each evidence-approved type.

- [ ] **Step 1: Add parameterized failing tests for confirmed types**

Each case names one type, its evidence row, the supported operations, and the
expected byte-exact synthetic result. Do not add a case from retail dump data.

- [ ] **Step 2: Verify the special-type tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][special-figure]"
~~~

Expected: confirmed special operations fail before their type rules exist.

- [ ] **Step 3: Implement one type at a time**

Keep import, read, write, creation, and reset capability flags independent.
An imported type may be readable and writable even when creation or reset is
unsupported. Do not generalize one special type's layout to another.

- [ ] **Step 4: Run special, creator, crypto, and protocol tests**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][special-figure]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][creator]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][protocol]"
~~~

Expected: supported cases PASS and unconfirmed cases stay unsupported.

- [ ] **Step 5: Commit only confirmed special-type support**

~~~bash
git add src/xenia/hid/portal/figure_catalog_data.inc \
  src/xenia/hid/portal/figure_creator.cc \
  src/xenia/hid/portal/xbox360_portal_protocol.cc \
  src/xenia/hid/portal/testing/special_figure_test.cc \
  docs/researchReports/2026-08-24-skylanders-figure-format.md
git commit -m "feat: add verified special Skylanders figures"
~~~

### Task 20: Add Trap Team portal audio after protocol proof

**Files:**
- Create: `src/xenia/hid/portal/portal_audio.h`
- Create: `src/xenia/hid/portal/portal_audio_decoder.cc`
- Create: `src/xenia/hid/portal/testing/portal_audio_test.cc`
- Modify: `src/xenia/hid/portal/xbox360_portal_protocol.cc`
- Modify: `src/xenia/hid/portal/virtual_portal.cc`
- Modify: `src/xenia/emulator.cc`
- Modify: `docs/researchReports/2026-08-24-xbox-360-portal-protocol.md`

**Interfaces:**
- Consumes: confirmed Xbox audio framing, codec state, reset, timing, sample
  format, and a license-compatible decoder basis.
- Produces: decoded PCM through an optional sink that does not affect figure
  data behavior.

~~~cpp
class PortalAudioSink {
 public:
  virtual ~PortalAudioSink() = default;
  virtual void SubmitMonoPcm(uint32_t sample_rate,
                             std::span<const int16_t> samples) = 0;
  virtual void Reset() = 0;
};

struct PortalAudioDecodeResult {
  bool success = false;
  uint32_t sample_rate = 0;
  std::vector<int16_t> samples;
  std::string error;
};

class PortalAudioDecoder {
 public:
  PortalAudioDecodeResult Decode(std::span<const uint8_t> packet);
  void Reset();
};
~~~

- [ ] **Step 1: Complete the audio evidence gate**

Record Xbox wrapper direction, payload length, G.721 bit ordering, predictor
state, reset command, sample rate, and timing. Record the exact license of any
decoder reference. If one field remains unknown, keep audio unsupported and
do not add guessed production bytes.

- [ ] **Step 2: Write failing decoder tests from synthetic vectors**

Test silence, positive and negative ramps, packet boundaries, reset state,
malformed length, and repeatability. Generate vectors independently and record
their method in the protocol report.

- [ ] **Step 3: Verify audio tests fail**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][portal-audio]"
~~~

Expected: compilation fails because the audio interfaces are absent.

- [ ] **Step 4: Implement decoding and optional host output**

Keep decoder state inside the virtual portal. Connect the sink only after the
emulator audio system exists. A missing sink discards audio safely and reports
audio unsupported without affecting data reports, figures, or title progress.

- [ ] **Step 5: Run audio, protocol, and app verification**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde -- "[skylanders][portal-audio]"
./xb test --target xenia-hid-portal-tests --no_build \
  --config=checked --no_sde -- "[skylanders][protocol]"
./xb build --target xenia-app --config=checked
~~~

Expected: tests and build PASS. Audible output is still unverified until a
real Trap Team Gaming Mode test.

- [ ] **Step 6: Commit audio support or its explicit unsupported gate**

Commit decoder code only when Step 1 is fully confirmed. Otherwise commit only
the updated evidence and capability reason.

~~~bash
git add src/xenia/hid/portal/portal_audio.h \
  src/xenia/hid/portal/portal_audio_decoder.cc \
  src/xenia/hid/portal/testing/portal_audio_test.cc \
  src/xenia/hid/portal/xbox360_portal_protocol.cc \
  src/xenia/hid/portal/virtual_portal.cc \
  src/xenia/emulator.cc \
  docs/researchReports/2026-08-24-xbox-360-portal-protocol.md
git commit -m "feat: add verified Trap Team portal audio"
~~~

### Task 21: Perform side-by-side SteamOS acceptance

**Files:**
- Create: `docs/researchReports/2026-08-24-steamos-portal-acceptance.md`
- Modify: `docs/TODO.md`

**Interfaces:**
- Consumes: an exact-SHA native Linux AppImage, copied test figures, and a
  separate Steam entry.
- Produces: Gaming Mode evidence without changing stable Xenia or user data.

- [ ] **Step 1: Obtain separate approval for Steam Deck contact**

Do not contact or change the Deck before this approval. The approval request
must state the exact AppImage, separate storage root, copied test data, and
rollback boundary.

- [ ] **Step 2: Record the side-by-side test identity**

Record commit SHA, AppImage checksum, separate storage root, separate Steam
shortcut, test-copy paths, and the stable-installation no-change boundary.

- [ ] **Step 3: Verify Gaming Mode startup and renderer**

Record AppImage startup, AMD Vulkan selection, title launch, context-menu
opening, and portal-dialog opening. Desktop Mode does not satisfy this check.

- [ ] **Step 4: Verify each input path**

Record controller focus order and A/B behavior, touchscreen selection and
scroll, keyboard, mouse, native picker focus, and return to game. Mark any
unavailable path unverified.

- [ ] **Step 5: Verify lifecycle and persistence with test copies**

Check insertion, removal, replacement, game write, reinsertion, title restart,
emulator restart, suspend, resume, invalid file, read-only file, and storage
failure.

- [ ] **Step 6: Verify stable state and commit the report**

Compare stable executable, config, profile, save, ROM, and original figure
locations with the pre-test record. Then run:

~~~bash
git add docs/researchReports/2026-08-24-steamos-portal-acceptance.md \
  docs/TODO.md
git commit -m "docs: record SteamOS portal acceptance"
~~~

### Task 22: Build the six-game compatibility matrix

**Files:**
- Create: `docs/researchReports/2026-08-24-skylanders-xbox-360-compatibility.md`
- Create: `docs/architecture/skylanders-virtual-portal.md`
- Modify: `docs/TODO.md`

**Interfaces:**
- Consumes: exact-SHA tests for all six Xbox 360 games.
- Produces: one evidence table per game, a special-feature matrix, and the
  implemented current architecture document.

- [ ] **Step 1: Create all six game sections**

Create sections for Skylanders: Spyro's Adventure, Skylanders Giants,
Skylanders Swap Force, Skylanders Trap Team, Skylanders SuperChargers, and
Skylanders Imaginators. Each section includes Portal detection,
Title-screen progression, Figure
insertion/removal, Figure identification, Game reads, Game writes and durable
persistence, Multiple figures or special objects, Game restart, Emulator
restart, and Known unsupported functions.

Use only `Pass`, `Fail`, `Partial`, `Blocked`, and `Not tested` as results.

- [ ] **Step 2: Add separate special-feature rows**

Track Traptanium data, Trap Team portal audio, traps, Swap Force combinations,
vehicles, trophies, and Creation Crystals. A standard figure pass changes none
of these rows.

- [ ] **Step 3: Test one title at a time**

Record title ID, region/build, Xenia commit, test-copy checksum, report
evidence, persistence result, and recovery action. Do not record ROM, save,
profile, or figure contents.

- [ ] **Step 4: Evaluate both milestones**

The usable milestone requires one game to pass every minimum acceptance row,
native Linux and AppImage checks, and non-Skylanders plus physical-path
regression checks. The all-games milestone requires evidence for all six games
and explicit limitations.

- [ ] **Step 5: Write only the implemented current architecture**

Document runtime flow, configuration paths, backend behavior, persistence
guarantees, and operational boundaries. Do not describe unimplemented target
behavior as current.

- [ ] **Step 6: Run final local verification**

~~~bash
./xb test --target xenia-hid-portal-tests --build-tests \
  --config=checked --no_sde
./xb build --target xenia-app --config=checked
git diff --check
git status --short
~~~

Expected: tests and build PASS, `git diff --check` has no output, and status
contains only intended evidence and architecture changes.

- [ ] **Step 7: Commit final evidence**

~~~bash
git add docs/researchReports/2026-08-24-skylanders-xbox-360-compatibility.md \
  docs/architecture/skylanders-virtual-portal.md docs/TODO.md
git commit -m "docs: record Skylanders portal compatibility"
~~~

## Required Review Gates

Stop for focused review after Tasks 2, 5, 8, 11, 14, 16, 17, 19, 20, 21, and
22. At each gate, compare source with the design, run the full portal suite,
inspect the exact diff, and record remaining evidence gaps. A green test suite
proves only the tested layer.

Before Steam Deck contact, deployment, user-file access, upstream action, or
binary publication, obtain separate user approval for that live boundary.
