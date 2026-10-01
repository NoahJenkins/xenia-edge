# ADR 0003: Persist pending portal figure saves

## Status

Accepted on 2026-10-01 under the user's instruction to proceed with restart-safe
recovery. This extends ADR 0002; it does not change its commit outcomes.

## Context

ADR 0002 blocks a figure handle after a replacement whose final flush fails.
An in-memory block disappears if the emulator stops. A new store could then
load the same file without knowing that recovery is required.

The planned versioned portal session manifest already holds paths and figure
state. It can also hold a pending-save record. A record written only after a
failed flush leaves a crash window between figure replacement and that record.

## Options Considered

1. Durably record a pending save in the session manifest before the figure
   write. Clear it durably after a verified save. This uses the planned session
   format and does not retain an original generation. Selected.
2. Write the record after a failed figure flush. A crash before the record
   would let a new store miss the uncertain save.
3. Add a separate rollback journal with an original image. ADR 0002 rejected
   that added format and its rollback complexity.

## Decision

Before a managed figure write or import, atomically save a pending entry with
the library-relative path and both possible 32-byte figure headers. If that
save is not durable, do not write the figure. After a durable figure save,
verify the actual file and durably remove the pending entry before returning
success. Any uncertain figure save leaves the entry pending and the handle
unavailable. A failed final session update also blocks the handle.

On a new store instance, a pending entry blocks managed load, read-only load,
read, and export for that path. Explicit recovery reopens and structurally
validates the actual file, checks it against the recorded old or candidate
header, durably saves those actual bytes, clears the pending entry, and issues
a fresh handle. If an import never published its destination, recovery can
clear its pending entry after confirming the file is absent.

The versioned TOML manifest contains no figure bytes or rollback generation.
Slot entries use relative paths and fingerprints. Restore skips missing,
changed, invalid, and duplicate figures while reporting errors. A malformed
pending-save list blocks library I/O. The manifest's parent directory must
already exist. The manager may pass a separate session root; the store's local
fallback places the manifest in the library root.

## Consequences

A managed block write makes an additional durable manifest update before and
after the figure update. This can add latency; game timing needs direct
acceptance testing. The data layer now prevents a restart from silently
bypassing recovery. The emulator manager, XAM commands, and title behavior
remain unconnected and unverified.
