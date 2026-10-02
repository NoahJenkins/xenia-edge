# Skylanders portal runtime

## Purpose

Select one portal backend and restore the virtual figure session without
changing the Xbox guest protocol until its packet behavior is verified.

## Current state

`Emulator` passes its storage root to `InputSystem`. `InputSystem` creates one
`PortalManager` from the persisted `portal_backend` and
`skylanders_figure_library` settings. Windows defaults to `physical`; other
platforms default to `disabled`. An unknown backend value selects `disabled`.
The portal dialog can change the backend while the title is stopped. Its
explicit choice is saved in the global configuration. A running or launching
title blocks a backend change.

```text
XAM raw input -> InputSystem -> PortalManager
                                  | physical (Windows) -> HardwarePortal -> libusb
                                  | virtual -> VirtualPortal -> FigureStore
                                  |                         -> PortalSessionStore
                                  |                         -> PortalSlotState
                                  + disabled
```

The physical route delegates reads, writes, and USB arrival/removal to the
existing `HardwarePortal`. Disabled and virtual routes return device not
connected to guest reads and writes. The virtual route cannot yet make a game
recognize a portal. It has no status timing, figure query/write packets, or
guest success replies.

Virtual management creates `<storage_root>/skylanders/` and, by default,
`figures/` inside it. The versioned session manifest is
`<storage_root>/skylanders/portal-session.toml`. A configured library path
replaces only the figure directory. Session restoration loads structurally
safe files with matching fingerprints into slots. It reports and skips bad
entries. Pending figure saves remain blocked until explicit recovery.

The manager serializes portal operations. Add, remove, replace, and move save
the resulting session durably before changing the live slots. An unrelated
stale entry or pending save remains in the manifest. Import and export use
`FigureStore` validation and atomic publication. Operation results carry the
store error and atomic commit outcome. If a session save fails, management
stops until the session is inspected and the manager is reconstructed.

## Key decisions

- [ADR 0001](../adr/0001-native-virtual-skylanders-portal.md): independent
  native backend and explicit selection.
- [ADR 0002](../adr/0002-portal-save-commit-outcomes.md): uncertain file
  replacement requires recovery.
- [ADR 0003](../adr/0003-persist-pending-portal-figure-saves.md): durable
  pending-save record before a figure write.

The virtual guest route stays closed because public Xbox sources conflict on
the write reply and status schedule. The [evidence report](../researchReports/2026-10-01-portal-public-evidence.md)
records those limits. Verified guest commands, native platform acceptance,
and game acceptance remain open.

## Figure manager UI

Tools > Skylanders Portal and the in-game context menu open one
`ImGuiSkylandersPortalDialog`. It uses the existing gamepad dialog and guest
input blocker. Before a game starts, the window shows the render surface and
sets up graphics/audio through the existing subsystem path so the same dialog
can draw. Closing it returns to the game list when no title is active.

The manager supplies slot paths, structural validation, numeric identities,
read-only state, external conflicts, and pending saves. Library scans do not
follow directory symlinks. Canonical containment is checked before file reads.
Scans inspect at most 4,096 directory entries through five directory levels
and return at most 1,024 figures, with recovery entries kept visible first.
Known raw extensions are .sky, .bin, .dump, and .dmp. The native import picker
also accepts other extensions after validation.

`PortalDialogModel` keeps bounded selection, filename search, filters, action
availability, and result text independent of ImGui. The overlay uses full-row
selection, controls of at least 44 logical pixels, and a visible action/status
footer. Wide views use two columns; narrow views stack the lists. Keyboard and
gamepad use ImGui navigation. B/Escape returns one step or closes the dialog.

Add, remove, replace, move, import, export, and recovery call the manager.
Slot actions carry an expected generation, so a file picker or confirmation
cannot apply an operation to a slot that changed. Import uses a new basename
inside the library. Export replacement and recovery require an inline
confirmation with Cancel first. Deferred native pickers use a lifetime token;
dialog destruction cancels their callbacks and releases the input blocker.

The virtual mode shows its game-support limit. Creation, reset, and catalog
names remain unavailable. A structural check does not prove a playable file.
See [UI verification](../context/2026-10-02-portal-manager-ui.md).
