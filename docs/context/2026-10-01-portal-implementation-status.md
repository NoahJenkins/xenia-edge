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
| Test wiring | Portal target in default test list and existing Linux CI build job; Python build-path regression in lint job | `113227b58` |

The protocol source is not connected to XAM. Activation acknowledgements do
not implement unverified activation/status side effects. Status scheduling,
query/write commands, virtual backend selection, persistence, library UI,
creation/reset, special objects, and audio remain unimplemented.

The four replays are software-reference tests assembled from corroborated
public behavior. They are not captures and do not prove Xbox title behavior.
The [public evidence report](../researchReports/2026-10-01-portal-public-evidence.md)
records exact revisions and unresolved source disagreements.

### Verification

- Native checked portal suite: **23 test cases, 991 assertions passed**.
- The same core and tests compiled separately with Apple Clang AddressSanitizer
  and UndefinedBehaviorSanitizer: **23 cases, 991 assertions passed**, no
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
hardware regressions, or durable storage as passed. Those paths are absent or
unverified, and remain explicit acceptance gates. No source fix was required
by the core review. One durable workflow improvement is to keep local build
tools below the ignored build directory and match CI's formatter version,
rather than caching disposable `/tmp` executable paths.

## Open Questions

- Decide [proposed ADR 0002](../adr/0002-portal-save-commit-outcomes.md) before
  writing the persistence layer. The current design's guarantee about old
  bytes after every failure cannot hold after a successful replacement and a
  failed directory flush. An approval request is pending; it is not accepted.
- Resolve status timing, XAM empty-poll semantics, legacy profiles, and write
  acknowledgement/failure behavior using further public primary evidence.
- Establish independent crypto/checksum vectors and catalog provenance.
- Provide the missing host Metal Toolchain for full Mac checks, then recheck
  the previously recorded Clang issue if it occurs.
- Complete runtime, UI, native platform, and six-game acceptance work before
  describing the portal as usable.

<!-- Related ADR: [ADR 0001](../adr/0001-native-virtual-skylanders-portal.md) -->
