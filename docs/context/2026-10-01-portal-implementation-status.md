# Skylanders portal implementation status

## Summary

Verified locally on 2026-10-01 on macOS ARM64, on branch
`agent/skylanders-virtual-portal`. This is a tested core foundation, not a
working in-game virtual portal. The existing emulator runtime still uses its
original physical-only Windows portal path.

The user approved continuation and specified public evidence because no
physical portal is available. Work stayed in the existing isolated worktree.
No push, pull request, deployment, Steam Deck access, or user figure access
was performed.

## Findings

### Implemented

| Component | Current behavior | Local commit |
| --- | --- | --- |
| Test target and runner | Portable Catch2 portal suite; corrected Mac output path in `xb test` | `4a715f723` |
| Slot state | 16 logical slots; bounded operations; immediate access invalidation on removal; generations; ordered replacement transitions | `6bfa6c623` |
| Protocol foundation | Four synthetic R/A/M exchanges; 32-byte replies; length checks; ordered bounded queue; reset; explicit unsupported errors | `0dc1ce165` |
| Figure parser | Exact 1,024-byte input; identity, BCC, tag/access fields; optional bounded reads; exact block replacement; explicit incomplete-validation warnings | `6fc6bc7d0` |
| Identifier checksum | Type-0 CRC with independent vectors and single-bit corruption tests | `4dbecf3d6` |
| Atomic writer | Three commit outcomes, retained complete candidates, native POSIX/Windows adapters; Mac tests pass | `e209b0dda` plus Mac full-flush follow-up |
| Figure storage | Managed/read-only handles, import/export, XXH3 conflict checks, uncertain-save I/O block, explicit durable recovery | Local continuation |
| Session state | Versioned TOML; path and fingerprint checks; durable pending saves before figure writes; recovery across restarts | Local continuation |
| Test wiring | Portal target in default test list and existing Linux CI build job; Python build-path regression in lint job | `113227b58` |

The protocol source is not connected to XAM. Activation acknowledgements do
not implement unverified activation/status side effects. Status scheduling,
query/write commands, virtual backend selection, library UI,
creation/reset, special objects, and audio remain unimplemented. Session slot
restoration is parsed but not connected to a manager.

The four replays are software-reference tests assembled from corroborated
public behavior. They are not captures and do not prove Xbox title behavior.
The [public evidence report](../researchReports/2026-10-01-portal-public-evidence.md)
records exact revisions and unresolved source disagreements.

### Verification

- Native checked portal suite: **49 test cases, 1,192 assertions passed**.
- The same core and tests compiled separately with Apple Clang AddressSanitizer
  and UndefinedBehaviorSanitizer: **49 cases, 1,192 assertions passed**, no
  sanitizer report. This harness uses Catch2 directly and has no emulator
  startup, GPU, or game dependency.
- Python build-runner test: **passed**, covering CMake output discovery for
  macOS, Windows, and Linux path conventions. This does not claim native
  execution on all three operating systems.
- Formatting: clang-format **21.1.8**, matching repository CI; changed C++
  files pass the dry-run check. `git diff --check` passed.
- Workflow YAML parsed locally. No hosted CI result is claimed.
- Full app/base/CPU checked build: **blocked** at Metal shader generation with
  `cannot execute tool 'metal' due to missing Metal Toolchain`. The portal
  library and test target build successfully. `./xb test --no_build` cannot
  run the default suite because `xenia-base-tests` was not produced.
- The earlier `trace_viewer.cc` Clang error was not reached in this attempt;
  it is not claimed fixed or reproduced.
- No native Linux/Windows, Docker, AppImage, gameplay, or SteamOS acceptance
  result exists for these commits.

The stale CMake paths to a deleted `/tmp` tool environment were repaired using
an ignored `build/portal-tools` virtual environment. It contains Ninja, Meson,
Mako, PyYAML, packaging, and the CI-matching formatter. No production package
dependency was added.

### Review and rulings

Reviewed the changed production code, tests, and workflow against the accepted
design. Review was performed inline by the author because the approved plan
prohibits subagents. There was no independent reviewer. The full feature is
not ready for merge or release.

1. Independent report/slot/parser work proceeded while complete protocol
   evidence remained open. These components do not require invented commands;
   later captures may require protocol adapter changes.
2. Corrected the `xb test` Macosx/macOS path mismatch, demonstrated by a failing
   regression test and then a passing test. Incorrect platform naming would
   prevent test discovery; all three path cases are covered.
3. Moved shared size and handle types forward from task 6 to task 4. A later
   interface change would require a header adjustment, not duplicate types.
4. Internal removal immediately invalidates figure access. Replacement exposes
   removing, empty, added, and ready phases. These phases do not assert a
   packet timing rule; the eventual protocol controls when to advance them.
5. Accepted four corroborated software-reference R/A/M exchanges under the
   existing evidence definition. They are explicitly not hardware captures.
   A direct title test may reveal behavior the references omit.
6. Limited the pending reply queue to 64 and return an explicit full-queue
   error instead of dropping replies. This is a resource bound; a future
   backend must map backpressure without claiming a hardware queue limit.
7. Changed `FigureImage::ReadBlock` to return `optional<FigureBlock>` so an
   invalid block is distinguishable from a valid zero block. Future store
   code must handle the empty result explicitly.
8. Implemented identifier CRC only. The planned UID-only encryption-key API
   conflicts with the documented need for header blocks 0 and 1. Implementing
   that signature would derive incorrect keys; it remains absent.
9. Added portal testing to the existing Linux job rather than duplicating its
   compiler/dependency setup. Hosted verification and Docker parity remain
   pending, so Linux build compatibility is not yet proven.

Review did not treat game compatibility, full crypto validity, physical
hardware regressions, cross-platform durable storage, or power-loss testing
as passed. Those paths remain explicit acceptance gates. Storage review added
the macOS full-drive flush described below. One durable workflow improvement is to keep local build
tools below the ignored build directory and match CI's formatter version,
rather than caching disposable `/tmp` executable paths.

### Restart recovery and library continuation

[ADR 0003](../adr/0003-persist-pending-portal-figure-saves.md) records a
pending save durably in the session manifest before each managed figure write
or import. A crash cannot silently discard an uncertain figure-save block.
Another live store also checks the pending state and file version before reads
and exports. On restart, a pending entry blocks loads until explicit recovery
validates and durably saves the actual file. The manifest uses relative paths,
version 1, and figure fingerprints. Its parser skips unsafe, missing, changed,
and invalid slot entries and reports the errors separately. Manager-driven slot
restoration and backend selection are still absent.

Import validates raw image size and structure before creating a managed file.
Export requires overwrite confirmation; a new destination uses atomic
no-replace publication. All tests use generated synthetic files only.

The extra durable session writes can add latency to every figure block save.
There is no title timing or power-loss measurement yet. macOS is the only
native runtime tested for these additions; Windows and Linux adapters need
native checks. The session root is configurable for future manager use; the
standalone store defaults it to the library root.

### Approved save correction

[ADR 0002](../adr/0002-portal-save-commit-outcomes.md) was accepted on
2026-10-01. The writer reports `NotReplaced`, `Durable`, or
`ReplacedDurabilityUnknown`, plus the failed stage, system error, and retained
candidate path. Only a complete temporary candidate can be retained.

The store keeps old memory only before replacement. After an uncertain
replacement, it keeps the candidate and blocks all I/O on that handle. Managed
and read-only loads cannot bypass this block in the same store. Recovery
reopens and structurally validates the actual file, compares UID/character/
variant identity, and durably saves those actual bytes. A failed recovery
leaves the handle blocked. Successful recovery issues a new handle; the old
handle stays disabled. No guest acknowledgement path is connected yet.

Tests use private synthetic files only. They cover actual replacement followed
by injected flush failure, each earlier failure stage, incomplete temporary
cleanup, retained complete candidates, Unicode paths, read-only directories,
stale content with unchanged size/timestamp, invalid bounds, failed recovery,
changed identity, and the candidate identity after uncertain replacement.
The synthetic files are not claimed playable or fully crypto-valid. Explicit
managed load preserves all validation warnings; it does not repair encryption
or gameplay checksums. Raw import and export are implemented. Export
requires confirmation before replacing an existing destination, and creating a new destination uses an
atomic no-replace operation. Creator/reset are not implemented.

The atomic writer requires an existing parent directory. It does not silently
create unsynced ancestors. Linux uses file and directory `fsync`. macOS also
requires `F_FULLFSYNC`, because [Apple documents that ordinary fsync need not
flush a drive cache](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html).
Windows uses wide paths, exclusive creation, file flush, replacement with
[MoveFileExW write-through](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw),
and a final [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers).
Windows code is not natively tested here. No platform result proves power-loss
behavior. Concurrent external writers are unsupported; version checks detect
changes observed before saving, but are not an interprocess compare-and-swap.

## Open Questions

- Connect the tested storage and session layers to the manager, XAM, and
  backend-neutral UI. Session slot restoration remains pending.
- Resolve status timing, XAM empty-poll semantics, legacy profiles, and write
  acknowledgement/failure behavior using further public primary evidence.
- Establish independent crypto/checksum vectors and catalog provenance.
- Provide the missing host Metal Toolchain for full Mac checks, then recheck
  the previously recorded Clang issue if it occurs.
- Complete runtime, UI, native platform, and six-game acceptance work before
  describing the portal as usable.

<!-- Related ADR: [ADR 0001](../adr/0001-native-virtual-skylanders-portal.md) -->

<!-- Related ADR: [ADR 0003](../adr/0003-persist-pending-portal-figure-saves.md) -->
