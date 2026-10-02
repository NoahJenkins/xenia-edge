# Portal figure manager UI

## Summary

Added the figure manager on branch `agent/skylanders-virtual-portal` using the
approved design. It opens from Tools and the in-game context menu. This is a
local management UI; virtual Xbox guest reports remain disconnected. No retail
figure data or physical portal was used. The user authorized continuation and
previously authorized commits and pushes to the feature branch.

## Findings

### Behavior

- Uses the existing ImGui theme, font, navigation, and guest input blocker.
- Provides scrollable slots and a searchable, filtered library. Rows and
  buttons are at least 44 logical pixels high. Actions and result text stay
  outside the scrolling detail region.
- Shows filenames, numeric identities, structural validation, read-only
  state, external changes, and pending-save recovery. It does not invent
  character names or claim full crypto validity.
- Calls the manager for Add, Remove, Replace, Move, Import, Export, and Recover.
  Creation and reset remain unavailable pending format evidence.
- Requires inline confirmation for export replacement and recovery. Import
  creates a new basename inside the managed library and does not overwrite.
- Slot operations check the generation captured when the action starts.
- Native file pickers run through the drawer's deferred callback queue. A
  lifetime token prevents callbacks from using a closed dialog. Teardown
  releases the input blocker before input-system destruction.
- Backend changes require a stopped title and persist an explicit global
  preference. The existing subsystem setup path draws the dialog before a
  game starts; closing restores the game list.

Library inspection checks canonical containment before reads, does not follow
directory symlinks, and limits recursion, examined entries, and displayed
files. Pending-save paths remain visible even if the file is missing. Explicit
recovery can durably clear a missing failed import and report success without
creating a new handle. An arbitrary missing file is still an error.

### Verification

- Checked macOS ARM64 portal suite: **61 cases / 1,306 assertions passed**.
  Added tests cover filtering, action rules, read-only display, recovery text,
  bounded selection, stale slot generations, invalid and changed files,
  backend change restrictions, outside-library symlinks, and missing imports.
- Full checked Mac app build: **passed**.
- Repository lint: **1,093 files passed** with clang-format 21.1.8. The
  build-path regression test, workflow YAML parse, and staged diff check passed.
- Native UI check used a private ignored build directory and generated
  synthetic structural files. Tools opened the dialog without a game.
  Arrow keys and Enter selected a figure; Tab reached Add; Add populated
  slot 1 and showed completion. The manifest recorded the filename and
  fingerprint. A restart restored slot 1. Export required confirmation and
  produced 1,024 bytes equal to the source. Escape restored the game list
  and its search focus. The macOS save panel also showed its native replace
  warning before the manager confirmation.
- The native Move flow selected empty slot 2, reported completion, and
  persisted the new slot. Native Import opened the source picker, used the
  naming screen, reported completion, and added a managed file with the
  same 1,024 bytes as the source. The test app exited after these checks.
- The UI check found skipped child controls and hidden action buttons. The
  dialog now shares the child focus path and keeps actions outside scrolling
  details. Focus returns to the selected slot after an operation.
- Computer-use coordinate clicks returned `noWindowsAvailable` for this app,
  while menu actions and keyboard input worked. Mouse/touch/controller checks
  and the complete native picker/confirmation matrix remain unverified.
- Hosted run [37046851191](https://github.com/NoahJenkins/xenia-edge/actions/runs/37046851191)
  tests the pre-UI head `09f6e71f1`. Linux portal tests, the full Linux build,
  artifact packaging, lint, both Mac builds, and the Windows app build
  passed. This does not verify the new UI head or an AppImage launch.
- The existing Windows job now runs the portal suite before its full build.
  UI run [37050335849](https://github.com/NoahJenkins/xenia-edge/actions/runs/37050335849)
  passed Linux portal tests, but Windows test compilation failed. The test
  helper forced `/Zi` over Release `/Z7`, causing shared compiler PDB and
  cache output errors. Release tests now retain `/Z7`; other configurations
  retain `/Zi` to prevent Edit-and-Continue from changing Catch2 line numbers.
  Native Windows test execution remains pending the corrected run.
- Corrected run [37051484200](https://github.com/NoahJenkins/xenia-edge/actions/runs/37051484200)
  compiled and ran the Windows portal suite. Two assertions failed: the
  export test kept a destination reader open, blocking Windows replacement,
  and the session path validator accepted a root-directory-only path.
  The test now closes its reader before export. The validator rejects all
  root paths, including paths without a Windows drive name. Windows cases
  also check backslash-rooted, drive-relative, drive-absolute, and UNC paths.
  Linux portal tests passed; full build results and the final corrected
  Windows suite remain pending.

Review was performed by the author without a separate agent. No accepted ADR
was changed. The UI exposes the operations already covered by the approved
design. A useful review rule from this pass is to check keyboard focus and
footer visibility in a live overlay before treating a successful build as
UI acceptance. This is recorded here, not added as a global instruction.

## Open Questions

- Run the full picker/confirmation and controller/touch matrix at 1280x800,
  including replacement, pending recovery, and shutdown while a picker
  is open. Repeat on SteamOS Gaming Mode and native Windows.
- Verify hosted results for the UI head, Docker parity, and actual AppImage
  execution. Native Windows storage tests must pass before a durability claim.
- Resolve Xbox write replies, status timing, and XAM empty polls using public
  primary evidence. No virtual guest packets were added in this continuation.
- Establish independent crypto/checksum vectors and catalog provenance before
  creator/reset work. Complete all six game acceptance checks before calling
  the virtual portal playable.
