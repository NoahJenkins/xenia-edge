# Skylanders portal runtime

## Purpose

Select one portal backend and restore the virtual figure session without
changing the Xbox guest protocol until its packet behavior is verified.

## Current state

`Emulator` passes its storage root to `InputSystem`. `InputSystem` creates one
`PortalManager` from the persisted `portal_backend` and
`skylanders_figure_library` settings. Windows defaults to `physical`; other
platforms default to `disabled`. An unknown backend value selects `disabled`.
Backend changes take effect when the input system is constructed, normally on
the next app start.

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
records those limits. UI, verified guest commands, native Windows/Linux
checks, and game acceptance remain open.
