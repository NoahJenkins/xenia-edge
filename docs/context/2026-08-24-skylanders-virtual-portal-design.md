# Skylanders Virtual Portal Design

## Summary

Status: approved for implementation on 2026-08-24.

This design adds an independent native virtual Skylanders Portal of Power to
Xenia Edge. It preserves the existing physical-portal path, keeps the core
portable across Linux, macOS, and Windows, and provides a controller-accessible
in-game interface for Steam Deck Gaming Mode.

The implementation must not claim support for a game, portal feature, or
special figure type until the compatibility matrix contains direct evidence.
No retail figure dumps, proprietary artwork, or user dump data may enter the
repository.

## Findings

### Current repository state

- `src/xenia/hid/portal/portal.h` limits portal reports to 32 bytes and exposes
  synchronous read/write plus device arrival/removal operations.
- `HardwarePortal` is a Windows-only libusb backend for a physical portal.
- `XamInputNonControllerGetRaw*` and `XamInputNonControllerSetRaw*` route device
  IDs 5 and 6 to `InputSystem::GetPortal()`.
- Linux and macOS do not construct a portal backend.
- Xenia already has ImGui gamepad dialogs that block guest input and an in-game
  context menu that can open controller-accessible dialogs.
- The Linux x86-64 and AppImage build workflows already exist. Portal unit
  tests and Gaming Mode acceptance do not.
- Per-user data paths already use `XDG_DATA_HOME` or `$HOME/.local/share` on
  Linux and avoid the AppImage mount.

### Interoperability format

The current Dolphin and Cemu tools use a raw 1,024-byte figure image made from
64 blocks of 16 bytes. The figure identifier is a little-endian 16-bit value
at offset `0x10`; the variant is a little-endian 16-bit value at offset
`0x1C`. The image includes tag identity fields, sector trailers, checksums,
and encrypted blocks.

The compatible file stays raw and has no Xenia header. Xenia session state,
validation reports, source provenance, and UI metadata use sidecar files.
Import accepts recognized extensions only as a user-interface filter; the
parser uses exact content and size rather than trusting an extension.

### License and content boundaries

- Xenia Edge and relevant Xenia Canary portal changes are BSD-3-Clause and can
  be adapted with preserved notices and authorship.
- Dolphin's Skylanders sources are GPL-2.0-or-later. Do not copy their code,
  metadata tables, tests, or fixtures into Xenia Edge.
- Cemu is MPL-2.0. Use it as a behavioral and byte-compatibility reference;
  do not copy its implementation into ordinary BSD Xenia files.
- Use independently written code and synthetic deterministic fixtures.
- Record provenance for each character and variant metadata record.
- Do not include retail dumps, extracted artwork, or user data.
- Source-code compatibility does not by itself settle distribution rights for
  copied metadata collections or nonstandard key material. Review provenance
  before distribution.

Primary references:

- Xenia Canary Linux physical portal PR:
  <https://github.com/xenia-canary/xenia-canary/pull/909>
- Xenia Canary Traptanium physical portal PR:
  <https://github.com/xenia-canary/xenia-canary/pull/1157>
- Xenia virtual portal issue:
  <https://github.com/xenia-project/xenia/issues/2320>
- Dolphin license and current figure implementation:
  <https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/COPYING>
  <https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/Source/Core/Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.cpp>
- Cemu license and current figure implementation:
  <https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/LICENSE.txt>
  <https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/Skylander.cpp>

### Component boundaries

```text
Xbox game
  -> XAM non-controller 32-byte reports
  -> PortalManager
       -> PhysicalPortalBackend -> libusb -> physical portal
       -> VirtualPortalBackend
            -> Xbox360PortalProtocol
                 -> PortalSlotState
                 -> FigureImage
                      -> FigureStore -> AtomicFileWriter

ImGui portal dialog -> PortalManager management API
PortalSessionStore  -> validated slot and library restoration
PortalAudioSink     -> optional later Trap Team audio output
```

#### PortalManager

`PortalManager` keeps the current XAM-facing `Portal` behavior stable and owns
one selected backend. The management interface returns immutable snapshots and
accepts explicit operations. The UI does not downcast a backend or modify
protocol buffers.

Backend values are `physical`, `virtual`, and `disabled`. Defaults preserve
current behavior: `physical` on Windows and `disabled` on Linux and macOS.
Virtual use is an explicit persisted selection. Backend changes can require a
title restart in the first milestone.

#### Xbox360PortalProtocol

The protocol layer has no filesystem, UI, libusb, game, or GPU dependency. It
accepts one host report, changes deterministic state, and queues zero or more
guest reports. Time, counters, and random input are injected.

The evidence spike must identify Xbox data wrapping, commands, response
lengths, status transitions, legacy portal differences, and Traptanium portal
differences before those behaviors become compatibility claims.

#### PortalSlotState

The state layer provides 16 logical protocol slots until packet evidence shows
that a different count is required. Each slot uses a generation value so a
pending game write cannot target a figure that the UI has replaced. Insertion,
removal, replacement, and movement produce ordered portal-state transitions.

#### FigureImage

`FigureImage` owns exactly 1,024 bytes. It parses identity and structural
fields, reads and updates data blocks, verifies supported checksums and sector
data, and encrypts or decrypts applicable blocks. It does not open files.

Validation returns a structured report with errors and warnings. Loading an
invalid image never mutates it. Game-written block updates are bounds-checked
and persisted as exact raw bytes; the service does not silently repair or
normalize game data.

#### FigureCatalog and creation

The catalog contains independently verified records for game, character,
variant, element, and type. Each record carries provenance and a support level.
The creation interface exposes only records with verified blank-image rules.

Standard figure creation is the first target. Traps, Swap Force combinations,
vehicles, trophies, and Creation Crystals stay unavailable for creation until
their initial structures and write behavior are verified. Import can support a
valid image before creation support exists for its type.

#### FigureStore and atomic persistence

The default library is below Xenia's normal user storage root. The configured
library path may contain spaces, non-ASCII characters, and case-sensitive
components. No path contains a hardcoded username.

Import validates before copying into the library. A direct external source is
read-only unless the user explicitly chooses managed use. Export never
overwrites without confirmation. Reset creates a validated replacement image
and uses the same atomic writer as game updates.

For each accepted game write:

1. Copy the loaded image and apply the bounds-checked block update.
2. Compare the source size, modification identity, and content fingerprint
   with the version that was loaded.
3. Write a unique temporary file in the destination directory.
4. Flush the file to durable storage.
5. Atomically replace the destination and flush the parent directory where the
   platform supports it.
6. Commit the in-memory candidate and report success.

If any step fails, retain the previous in-memory and on-disk image and report a
read-only, conflict, storage, or recovery error. Never overwrite a source that
failed parsing. On startup, inspect recognized temporary recovery files and
offer recovery only when their content validates.

#### PortalSessionStore

The session manifest stores the library directory, backend selection, and safe
slot restoration data. It records relative paths when files are inside the
library and a content fingerprint for each entry. Restore skips missing,
invalid, conflicting, or duplicate files and reports each skipped item.

#### Handheld interface

Add a Skylanders Portal action to the existing in-game context menu. The
dialog derives from `ImGuiGamepadDialog`, blocks input to the game while open,
and uses ImGui gamepad navigation. It provides large slot controls, a library
list, metadata, state badges, destructive confirmations, and explicit add,
create, remove, replace, move, import, export, and reset flows.

The overlay library browser is the normal workflow in Gaming Mode. The native
wxWidgets picker is used only to bring files into or out of the managed
library, subject to SteamOS acceptance. No hover-only or pointer-only action is
permitted.

#### Trap Team audio

Portal audio uses a separate optional interface. Data and figure support do
not depend on an audio decoder. Xbox G.721 framing, state reset, timing, and
audio routing require direct evidence. The compatibility matrix reports portal
audio separately and never hides an unsupported state behind a general portal
status.

### Verification phases

0. Record protocol, licensing, and provenance evidence. Produce synthetic or
   legally redistributable report replays.
1. Implement the deterministic protocol core and slot transitions with unit
   tests.
2. Implement image parsing, validation, serialization, atomic persistence, and
   recovery with synthetic fixtures.
3. Add independently verified metadata, standard blank creation, reset, and
   byte-level Dolphin/Cemu interoperability checks.
4. Integrate the virtual backend and explicit backend selection while
   preserving the physical path.
5. Add the handheld ImGui interface and test every input method.
6. Add native Linux test coverage and complete side-by-side AppImage acceptance
   in Steam Deck Gaming Mode.
7. Build the six-game compatibility matrix with separate rows for portal
   audio, traps, vehicles, Swap Force figures, and Creation Crystals.

## Open Questions

- What exact Xbox 360 report sequences do all six titles use for startup,
  status polling, figure query, figure write, lighting, and shutdown?
- How do legacy and Traptanium portal version replies change title behavior?
- What acknowledgement and retry behavior follows a failed durable figure
  write?
- How do logical slot numbers map to portal zones and special objects?
- What type-specific blank and reset data is valid for traps, Swap Force
  combinations, vehicles, trophies, and Creation Crystals?
- What are the exact Xbox G.721 audio packet and decoder reset rules?
- Does the native wxWidgets picker remain controllable and focused in Gamescope?
- Which controller shortcut should open the portal dialog when controller
  hotkeys are enabled?

<!-- Related ADR: [ADR 0001](../adr/0001-native-virtual-skylanders-portal.md) -->
