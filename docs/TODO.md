# Docs TODO

Last Updated: 2026-10-01

## Open

- [ ] Resolve remaining Xbox status/poll timing and legacy portal evidence.
      Four software-reference R/A/M replays exist; no hardware captures exist.
- [ ] Verify query/write failure and retry behavior before enabling figure I/O.
- [ ] Establish independent Key A, AES input, and checksum-family 1/2/3/6
      vectors. Correct the plan's UID-only key-derivation signature first.
- [ ] Establish record-level provenance and blank rules for figure creation.
- [ ] Finish library import/export/reset, XAM backend selection, and UI.
- [ ] Connect save errors and recovery to the manager and session restoration;
      prevent restore or store recreation from bypassing recovery.
- [ ] Install/enable the host Metal Toolchain before full Mac app verification.
      The full checked build currently fails in Metal shader generation.
- [ ] Recheck the previously recorded Apple Clang `trace_viewer.cc` failure
      after the build can reach it; it was not reached in the current run.
- [ ] Run native Linux CI, Docker parity, Windows tests, and AppImage checks.
      The Linux workflow now includes the portal suite, but has not run for
      this local branch.
- [ ] Add the runtime architecture document after Xenia integration exists.
- [ ] Complete SteamOS acceptance and the six-game compatibility matrix.

## Done

- [x] Accept ADR 0002 save-error correction (user approval, 2026-10-01).
- [x] Add atomic save outcomes and managed-handle recovery with failure tests.

- [x] Enabled the repository docs workflow and accepted ADR 0001.
- [x] Saved the approved design and implementation plan.
- [x] Recorded original protocol, figure-format, and license evidence.
- [x] Refreshed public evidence and added four synthetic software-reference
      replays with exact source revisions and explicit limits.
- [x] Added the portable portal test target and fixed the Mac test-runner path.
- [x] Implemented deterministic slots and the limited R/A/M reply core.
- [x] Implemented exact-size raw figure parsing and structural checks.
- [x] Added independently verified identifier CRC checks and corruption tests.
- [x] Added the portal target to default tests and the native Linux build job.

See [current implementation and verification](context/2026-10-01-portal-implementation-status.md)
for exact scope, checks, design rulings, and remaining blockers.
