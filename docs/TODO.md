# Docs TODO

Last Updated: 2026-10-02

## Open

- [ ] Resolve remaining Xbox status/poll timing and legacy portal evidence.
      Four software-reference R/A/M replays exist; no hardware captures exist.
- [ ] Verify query/write failure and retry behavior before enabling figure I/O.
- [ ] Establish independent Key A, AES input, and checksum-family 1/2/3/6
      vectors. Correct the plan's UID-only key-derivation signature first.
- [ ] Establish record-level provenance and blank rules for figure creation.
- [ ] Finish creator/reset after format and provenance verification.
      Creation remains gated on verified figure format and provenance.
- [ ] Connect verified figure commands and save errors to XAM. The manager
      exposes local store errors, but virtual guest traffic stays closed.
- [ ] Generate PPC test maps before treating the Mac default CPU suite as
      verified. The runner currently reports zero loaded CPU tests.
- [ ] Run Docker parity and actual AppImage launch checks.
- [ ] Finish native picker, recovery, shutdown, controller, and touch checks
      at 1280x800 on Windows and SteamOS. Mac keyboard Add, Move, Import,
      Export, restore, and Escape checks passed with synthetic files.
- [ ] Complete SteamOS acceptance and the six-game compatibility matrix.

## Done

- [x] Add the figure manager to Tools and the in-game context menu.
- [x] Add library search, filters, validation/recovery state, and safe actions.
- [x] Pass checked Mac UI build and 61 portal tests; verify native keyboard
      Add, Move, Import, Export, restore, and Escape with synthetic files.
- [x] Pass all four hosted app builds and artifact packaging for the UI source
      head. Pass Linux 61 cases / 1,306 assertions and Windows 60 cases /
      1,309 assertions. Correct Windows compiler flags and rooted paths.
- [x] Verify all hosted builds for packaging head `2f931a2a8`; download the
      Windows archive and confirm it excludes test executables and symbols.

- [x] Add explicit portal backend settings and manager delegation to XAM.
- [x] Restore safe virtual slots and persist local management operations.
- [x] Add the current runtime architecture document.

- [x] Install the Apple Metal Toolchain and pass the checked Mac app build.
- [x] Fix the previously reported `trace_viewer.cc` size-format errors.
- [x] Pass the Mac base and portal suites; record the CPU load gap.

- [x] Persist pending saves before figure writes; block I/O across restarts
      until file validation and durable recovery (ADR 0003).
- [x] Add safe raw figure import, confirmed export, and partial session restore.

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

See [UI continuation and verification](context/2026-10-02-portal-manager-ui.md)
and [core implementation and verification](context/2026-10-01-portal-implementation-status.md)
for exact scope, checks, design rulings, and remaining blockers.
