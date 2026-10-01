# Public Xbox 360 portal evidence refresh

## Purpose

Resume the virtual portal plan with public evidence. The user confirmed on
2026-10-01 that no physical portal is available. No hardware exchange or local
gameplay result is claimed in this report.

## Methodology

Reviewed TheBiemGamer/skylanders-portal at
`581a5f4610eac0ad65ba70e2191d2355b24b9a07`, including its MIT license, Xbox
framing, software portal, XAM bridge, and protocol tests. Its README describes
use by Xbox 360 recompilations of Giants and Trap Team. That is upstream
evidence, not acceptance evidence for Xenia.

Compared its command bytes with Cemu at
`5ead58008dd984f614e2cb38bd9cb69bd77fd1bb` and the already reviewed BSD Xenia
Canary Traptanium classifier at `f40c7d87185ff53da3175ea63d88dc58669203ab`.
The newer library reports that its author studied other emulators. Agreement
between these implementations is corroboration, not independent hardware
measurement. No source code, source test body, catalog, or figure dump was
copied into production or test files.

The existing evidence definition permits confirmed interface facts supported
by agreeing implementations. The synthetic replays below confirm those
software contracts only. They do not establish packet timing, complete startup,
legacy portal behavior, or title compatibility.

## Findings

Each synthetic replay begins with a fresh Traptanium-profile instance, no
figures, and no queued response. Both directions use exactly 32 bytes, starting
with `0B 14`; bytes omitted below are zero. The response is the next queued
command reply. No elapsed delay is inferred. These are independently assembled
test inputs and expected bytes, not captured traffic.

### P1

| Fact | Status | Exact bytes | Direction | Generation | Source revision | License | Redistribution | Verification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Identification exchange | Confirmed | Request `0B 14 52`; reply `0B 14 52 02 1B`; each 32 bytes | Host request then guest reply | Traptanium software profile | New library `581a5f4610`, Cemu `5ead58008d`, Canary `f40c7d8718` | MIT, MPL-2.0, BSD-3-Clause | Synthetic protocol facts only | New library software portal + Xbox frame adapter; Cemu agrees on payload; Canary classifies `02 1B` as Traptanium |

### P2

| Fact | Status | Exact bytes | Direction | Generation | Source revision | License | Redistribution | Verification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Activation acknowledgement | Confirmed | Request `0B 14 41 01`; reply `0B 14 41 01 FF 77`; each 32 bytes | Host request then guest reply | Traptanium software profile | New library `581a5f4610`, Cemu `5ead58008d` | MIT, MPL-2.0 | Synthetic protocol facts only | Both command implementations echo the argument and fixed acknowledgement suffix; new library supplies Xbox framing |
| Zero-argument acknowledgement | Confirmed | Request `0B 14 41 00`; reply `0B 14 41 00 FF 77`; each 32 bytes | Host request then guest reply | Traptanium software profile | Same sources | MIT, MPL-2.0 | Synthetic protocol facts only | Both agree on reply bytes; Cemu's activation state behavior differs, so this row confirms the acknowledgement only |

### P3

| Fact | Status | Exact bytes | Direction | Generation | Source revision | License | Redistribution | Verification |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Version acknowledgement without speaker sink | Confirmed | Request `0B 14 4D 01`; reply `0B 14 4D 01 00 19`; each 32 bytes | Host request then guest reply | Traptanium software profile without audio | New library `581a5f4610`, Cemu `5ead58008d` | MIT, MPL-2.0 | Synthetic protocol facts only | Both agree on payload; new library supplies Xbox framing |

### Evidence still missing

The GiantsRecomp protocol document was also inspected at
`a397a11f98f7f16ec76bfa9b85f379cd69682fdc`. It says `W` sends no reply, whereas
the current portal library queues a `57` acknowledgement. This disagreement
prevents treating that document as a complete write replay reference.
[Brandon Wilson's original notes](https://gist.github.com/skylandersNFC/5c9fb3debc11c1ea194ed6ddc9fb8faf)
describe a third write response, an empty `R` packet. These are protocol
observations, not a complete capture with retry and failure outcomes. The
write acknowledgement remains unconfirmed.

- The new software portal keeps added status for eight reports. Cemu advances
  queued transitions per status read. Neither establishes a common Xbox timing
  rule; timing remains unconfirmed.
- The new XAM bridge alternates data and empty reads and suppresses inactive
  status to avoid title handshake loops. This is useful single-source runtime
  evidence, not a verified general Xenia polling rule.
- Query/write payload layouts agree in the two software implementations, but
  failed durable storage writes and retries are not established. Their storage
  behavior does not meet this project's atomic persistence contract.
- Legacy version replies, portal zones, special figure rules, and speaker
  framing/state still need separate evidence and tests.
- The new catalog cites a figure-dump collection. It is not imported, and its
  records do not satisfy this project's independent metadata provenance gate.
- A public archive of Brandon L. Wilson's original protocol notes corroborates
  the Xbox prefix and 32-byte padding, but includes incomplete and conflicting
  command details. It does not qualify as a complete replay capture.

Canary PR 1157 was rechecked through the GitHub API on 2026-10-01. It remains
open and unmerged at `f40c7d8718`. The comments include a Giants disconnect
report and a later request to retest a hardware-specific fix. No packet
capture was attached in the reviewed discussion.

A further public-source search on 2026-10-01 found no Xbox 360 packet capture
that resolves these disagreements. SkyReader's Xbox support was explicitly an
attempt based on a decompiled web driver; its author said the communication
code was doubtful. Its active figure read/write functions were file stubs, so
it is not a verified Xbox Q/W exchange. A separate Raspberry Pi portal project
explicitly excludes Xbox 360. Canary PR 1157 reports a successful physical
Traptanium test, but gives no raw report sequence or write failure/retry
transcript. These sources do not change the write or status evidence gate.

## Recommendations

1. Implement the four synthetic command replays with explicit unsupported
   errors for all other commands. Keep the core disconnected from XAM until
   status, polling, and figure I/O are justified and tested.
2. Continue the independent slot state and structural image parser.
3. Keep protocol timing, figure creation/reset, crypto completeness, audio,
   and game compatibility open in the tracker.

## References

- [New library README at reviewed revision](https://github.com/TheBiemGamer/skylanders-portal/blob/581a5f4610eac0ad65ba70e2191d2355b24b9a07/README.md)
- [New library license](https://github.com/TheBiemGamer/skylanders-portal/blob/581a5f4610eac0ad65ba70e2191d2355b24b9a07/LICENSE)
- [Software command implementation](https://github.com/TheBiemGamer/skylanders-portal/blob/581a5f4610eac0ad65ba70e2191d2355b24b9a07/src/portal/software/software_portal.cpp)
- [Xbox frame adapter](https://github.com/TheBiemGamer/skylanders-portal/blob/581a5f4610eac0ad65ba70e2191d2355b24b9a07/src/portal/xbox_frame.cpp)
- [XAM bridge](https://github.com/TheBiemGamer/skylanders-portal/blob/581a5f4610eac0ad65ba70e2191d2355b24b9a07/src/portal/xam_bridge.cpp)
- [Software portal tests](https://github.com/TheBiemGamer/skylanders-portal/blob/581a5f4610eac0ad65ba70e2191d2355b24b9a07/tests/software_portal_test.cpp)
- [Cemu command implementation](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/Skylander.cpp)
- [Canary PR 1157](https://github.com/xenia-canary/xenia-canary/pull/1157)
- [Archived original protocol notes](https://gist.github.com/parkerlreed/a19d50deccaedbe15517a19bc70ff2e5)

- [GiantsRecomp protocol notes at reviewed revision](https://github.com/TheBiemGamer/GiantsRecomp/blob/a397a11f98f7f16ec76bfa9b85f379cd69682fdc/docs/portal-protocol.md)
- [SkyReader original Xbox attempt and file stubs](https://github.com/silicontrip/SkyReader/blob/master/portalio.cpp)
- [Pico portal project platform scope](https://github.com/AlexanderShaffer/SkylandersPortalEmulator)
