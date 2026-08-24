# ADR 0001: Native Virtual Skylanders Portal

## Status

Accepted on 2026-08-24.

## Context

Xenia Edge currently routes Xbox 360 portal traffic through the XAM
non-controller raw-input exports to a 32-byte `Portal` interface. The only
backend is a Windows-only libusb implementation for a physical Portal of
Power. Linux and macOS do not construct a portal backend.

The project needs a virtual Portal of Power that works without portal
hardware, libusb, elevated permissions, or a separate desktop application.
SteamOS is the primary acceptance platform. The protocol and figure-file
layers must remain portable and testable without a game, GUI, or GPU.

Dolphin's implementation is GPL-2.0-or-later. Cemu's implementation is
MPL-2.0. Xenia Edge and the relevant Xenia Canary portal work are
BSD-3-Clause. The destination must not copy incompatible code, metadata
tables, tests, or figure dumps.

## Options Considered

### Option A: Independent native Xenia subsystem

Implement an independent BSD-licensed protocol core, virtual backend,
figure-image service, atomic persistence layer, and in-game ImGui interface.
Use Dolphin and Cemu only as behavioral and interoperability references.

### Option B: Cemu-derived isolated component

Adapt Cemu's MPL implementation in separately licensed files and add an Xbox
360 report adapter.

This could accelerate some figure and slot behavior. It would retain MPL
source obligations and still require substantial work because Cemu's virtual
portal transport is primarily the Wii/Wii U 64-byte USB protocol rather than
Xenia's Xbox 360 32-byte XAM path.

### Option C: External sidecar or virtual USB service

Run portal emulation outside Xenia and connect through IPC or host virtual USB.

This reduces initial Xenia changes but creates focus, lifecycle, packaging,
permissions, and Gaming Mode problems. It also conflicts with the requirement
for a controller-accessible in-emulator workflow.

## Decision

Use Option A.

Keep the existing XAM raw-report boundary. Add a `PortalManager` that owns one
explicitly selected backend: `physical`, `virtual`, or `disabled`. Preserve
the existing physical backend as an independent implementation. Do not make
Linux physical-portal support a dependency of the virtual backend.

The virtual path will use these narrow components:

- `VirtualPortalBackend` for Xbox 360 32-byte report transport.
- `Xbox360PortalProtocol` for the deterministic protocol state machine.
- `PortalSlotState` for logical slots and insertion/removal transitions.
- `FigureImage` for the compatible 1,024-byte figure representation.
- `FigureStore` for validation, creation, import, export, atomic persistence,
  conflict detection, and recovery.
- `FigureCatalog` for independently verified metadata and provenance.
- `PortalSessionStore` for sidecar-only slot and library state.
- An ImGui gamepad dialog for controller, touch, keyboard, and mouse use.
- A later optional `PortalAudioSink` for Trap Team audio.

Use raw 1,024-byte Dolphin/Cemu-compatible figure files. Store Xenia-only
metadata separately. Never overwrite a source file that cannot be parsed
safely. A game write is committed through a same-directory atomic replacement
before the in-memory image is changed and before success is reported, unless
the protocol evidence spike proves that a different acknowledgement order is
required.

Backend switching may require a title restart in the first implementation.
Live figure insertion, removal, replacement, movement, reads, and writes do
not require a restart.

## Consequences

- The implementation remains compatible with the repository's BSD license.
- Protocol and figure logic can be tested on macOS and Linux without a GPU or
  game.
- SteamOS needs no root access, portal hardware, or libusb for virtual use.
- Physical and virtual portal behavior can evolve independently.
- Initial work is larger than adapting a reference implementation.
- Protocol facts, special figure creation, and Trap Team audio need explicit
  evidence before compatibility can be claimed.
- The repository must add portal tests to native Linux CI and later verify a
  side-by-side AppImage in Steam Deck Gaming Mode.

<!-- Related design: [Skylanders virtual portal design](../context/2026-08-24-skylanders-virtual-portal-design.md) -->
