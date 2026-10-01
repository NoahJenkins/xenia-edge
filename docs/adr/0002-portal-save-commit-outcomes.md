# ADR 0002: Report portal save commit outcomes

## Status

Accepted on 2026-10-01 by explicit user approval.

This supersedes only ADR 0001's promise that every failed save
leaves the old on-disk image intact. Its format, ownership, and durability
requirements otherwise remain in effect.

## Context

The approved design writes a temporary file, flushes it, replaces the
destination, and flushes the parent directory. Tasks 9 and 10 also require
old disk and memory bytes after every error.

A successful replacement changes the visible destination before the final
directory flush. If that flush fails, returning an ordinary failed-write result
while retaining the old in-memory image would misrepresent the file state.
Attempting automatic rollback would add another replacement that can also fail.

The Linux manual specifies [atomic replacement by rename](https://man7.org/linux/man-pages/man2/rename.2.html)
and the need for a separate [directory flush](https://man7.org/linux/man-pages/man2/fsync.2.html).
The design therefore needs an explicit result for failure after replacement.

## Options Considered

1. Report the actual commit outcome and stop further writes when durability
   cannot be confirmed. Recommended.
2. Add a durable journal and retained original generation, with explicit
   recovery and rollback states. This adds a second persistence format and
   still cannot promise successful rollback during persistent I/O failure.

## Decision

Adopt option 1:

- `NotReplaced`: replacement did not happen. Keep old memory and destination
  bytes. Preserve a complete temporary candidate only when useful for recovery.
- `Durable`: replacement and the required flushes succeeded. Commit the new
  in-memory image and acknowledge the game write.
- `ReplacedDurabilityUnknown`: replacement succeeded but final durability
  confirmation failed. Keep the exact replacement candidate in memory, mark
  the handle unavailable for further game I/O, and report a storage/recovery
  error. Never report success or claim the old file survived.

The atomic result carries the commit outcome, failing stage, system error, and
any retained recovery path. The figure store disables the affected handle on
an uncertain result. Reload must reopen the actual file, validate its bytes,
and compare its identity before a fresh handle can be used. If the storage
problem persists, reload does not enable writes.

Tests inject failures before replacement and after replacement. They assert
the actual disk state for each outcome, explicit error propagation, and the
absence of a success acknowledgement for an uncertain commit. They do not
assert impossible rollback guarantees.

## Consequences

Save errors report the true state and prevent follow-up writes from stale
memory. A storage failure after replacement can leave the latest update
visible without a confirmed durability guarantee. The user must resolve the
storage error and reload that figure before game I/O resumes.

The user approved this change to save-error behavior under the global
AGENTS.md requirement for written approval of material workflow semantics
changes. ADR 0001 remains unchanged; this ADR supersedes its conflicting
save-error guarantee.
