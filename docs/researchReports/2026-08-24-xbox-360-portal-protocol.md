# Xbox 360 Skylanders Portal Protocol Evidence

## Purpose

Record the evidence that is safe to use for the independent BSD-licensed
virtual Portal of Power implementation. This report separates current Xenia
interfaces, corroborated reference behavior, single-source observations, and
unknown behavior.

This report does not claim game compatibility. It contains no retail figure
data, proprietary artwork, USB capture, or copied reference implementation.

## Methodology

- Inspected Xenia Edge at
  `4d9da9994c6b8ce3ec29975b544b305d0bb91090`. Its base source commit is
  `9d8210b32`.
- Inspected the current heads and public descriptions of Xenia Canary pull
  requests 909 and 1157.
- Inspected Dolphin at
  `123d32248e455ec242866e3a75fea70dcecfd567` and Cemu at
  `5ead58008dd984f614e2cb38bd9cb69bd77fd1bb`.
- Compared behavior only. No Dolphin or Cemu source text, test data, metadata
  table, or fixture was copied.
- Used `Confirmed` only when current Xenia code establishes the interface or
  two current source implementations independently agree on the byte-level
  behavior. `Observed once` means one implementation or one reported hardware
  test. `Unknown` means that implementation must wait for better evidence.

The reviewed repository remotes were:

- `origin`: <https://github.com/NoahJenkins/xenia-edge.git>
- `upstream`: <https://github.com/has207/xenia-edge.git>

## Findings

### Evidence table

Direction uses `console -> portal` for an Xbox game output report and
`portal -> console` for a portal input report.

| Fact | Status | Exact bytes or offset | Direction | Portal generation | Source and revision | License | Redistribution status | Verification method |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Xenia's XAM portal boundary accepts device IDs 5 and 6 and rejects buffers larger than 32 bytes. | Confirmed | Device IDs `5`, `6`; maximum `0x20` bytes | Both | All routed devices | Xenia Edge `4d9da9994`, `src/xenia/kernel/xam/xam_input.cc`, `src/xenia/hid/portal/portal.h` | BSD-3-Clause | Interface facts can be independently implemented. Preserve Xenia notices for adapted Xenia code. | Direct source inspection and successful checked build of `xenia-hid-portal`. |
| Xbox 360 portal data reports use a two-byte wrapper before the standard portal payload. | Confirmed | Prefix `0B 14`; total data report is capped at 32 bytes | Both | Xbox 360 physical wrapper | Cemu `5ead58008d`, `SkylanderXbox360.h/.cpp`; Xenia Canary PR 1157 `f40c7d8718`, wrapped identification parser | MPL-2.0 and BSD-3-Clause | Treat as a protocol fact. Do not copy Cemu implementation text. | Cross-source byte comparison. No hardware capture is in this repository. |
| A wrapped identification response starts with the data wrapper followed by ASCII `R`. | Confirmed | `0B 14 52`, then major and minor ID bytes | Portal -> console | Legacy or Traptanium | Xenia Canary PR 1157 `f40c7d8718`; Cemu `5ead58008d` wraps standard portal data with `0B 14` and its core uses `52` for identification | BSD-3-Clause and MPL-2.0 | The prefix and field positions are usable facts. Exact version replies are not yet replay evidence. | Cross-source structure comparison. |
| PR 1157 classifies selected version IDs as Traptanium. | Observed once | Major `02`; minor `18` through `1B`, or `27` | Portal -> console | Traptanium | Xenia Canary PR 1157 `f40c7d8718`, `HardwarePortal::IdentifyPortalFromPacket` | BSD-3-Clause | May be adapted with notice, but do not make a compatibility claim from this alone. | Direct source inspection. PR author reports one original Xbox 360 Traptanium Portal test on Windows. |
| PR 1157 treats an empty asynchronous poll as success with zero bytes and state 0. | Observed once | `bytes_read = 0`, `state = 0` | Portal -> console | Traptanium mode in PR 1157 | Xenia Canary PR 1157 `f40c7d8718`, `Portal::Read` and `HardwarePortal::ReadInternal` | BSD-3-Clause | Adaptation is license-compatible. Behavior still needs replay or title verification. | Direct source inspection and PR test statement. |
| PR 1157 sends Traptanium physical output as one zero-padded 32-byte transfer. | Observed once | Exactly `0x20` bytes | Console -> portal | Traptanium mode in PR 1157 | Xenia Canary PR 1157 `f40c7d8718`, `HardwarePortal::WriteInternal` | BSD-3-Clause | Adaptation is license-compatible. It is a physical-I/O behavior, not proof of the virtual command protocol. | Direct source inspection. |
| Xbox portal audio uses a different wrapper from data traffic. | Observed once | Prefix `0B 17` | Console -> portal | Traptanium/audio-capable portal | Cemu `5ead58008d`, `SkylanderXbox360.h/.cpp` | MPL-2.0 | Treat only as a behavioral lead. Do not copy the implementation. | Single-source inspection. |
| Cemu's Xbox wrapper converts pairs of 16-bit PCM samples into two G.721 4-bit codes packed into one byte. | Observed once | Low nibble is the first encoded sample; high nibble is the second | Console -> portal | Traptanium/audio-capable portal | Cemu `5ead58008d`, `SkylanderXbox360.cpp` and bundled `g721` files | Wrapper is MPL-2.0; the codec has a Sun Microsystems unrestricted-use notice and disclaimer | The codec notice permits copying or modification without charge, but exact portal audio behavior still needs independent verification. Preserve the notice if that codec is later reused. | Single-source inspection. |
| The standard reference protocol recognizes commands `A`, `C`, `J`, `L`, `M`, `Q`, `R`, `S`, `V`, and `W`. | Observed once | First payload byte is the ASCII command | Console -> portal | Cemu virtual Wii/Wii U core | Cemu `5ead58008d`, `Skylander.cpp` | MPL-2.0 | Behavioral lead only. It is not proof of Xbox framing, response length, timing, or full command semantics. | Single-source inspection of a non-Xbox virtual core. |
| Cemu models 16 logical figure slots with two-bit status transitions. | Observed once | 16 slots; states include `0`, `1`, `2`, `3` | Portal -> console | Cemu virtual Wii/Wii U core | Cemu `5ead58008d`, `Skylander.h/.cpp` | MPL-2.0 | May guide a spike, but must not become an Xbox compatibility claim without Xbox evidence. | Single-source inspection of a non-Xbox virtual core. |
| Exact Xbox startup sequence and response timing are known. | Unknown | Unknown | Both | Legacy and Traptanium | No redistributable Xbox report capture reviewed | Not applicable | Absent from replay and production tables. | Proof gap recorded. |
| Exact Xbox status packet layout, slot-to-zone mapping, and transition timing are known. | Unknown | Unknown | Portal -> console | Legacy and Traptanium | No redistributable Xbox report capture reviewed | Not applicable | Absent from replay and production tables. | Proof gap recorded. |
| Exact Xbox query and write reports, acknowledgements, retry rules, and durable-write ordering are known. | Unknown | Unknown | Both | Legacy and Traptanium | No redistributable Xbox report capture reviewed | Not applicable | Absent from replay and production tables. | Proof gap recorded. |
| Exact Trap Team audio framing, G.721 state reset, sample rate, scheduling, and shutdown behavior are known. | Unknown | Unknown | Console -> portal | Traptanium | One Cemu behavior was reviewed; no independent capture or specification was reviewed | Not applicable | Audio remains outside the first protocol implementation. | Proof gap recorded. |

### Current upstream state

- Xenia Canary PR 909 is open at
  `ac7f92cb767233d82039424f74878fbe82c2a6d0`. Its description says that Linux
  physical-portal hotplug is not supported.
- Xenia Canary PR 1157 is open at
  `f40c7d87185ff53da3175ea63d88dc58669203ab`. The author reports testing one
  original Xbox 360 Traptanium Portal with WinUSB and Trap Team. This is useful
  physical-hardware evidence, but it is not merged and does not test a virtual
  backend, Linux, SteamOS, or the other five games.
- Xenia issue 2320 remains an open feature request. No current Xenia virtual
  portal implementation was found in the reviewed source.

### Replay gate

Update 2026-10-01: [public evidence refresh](2026-10-01-portal-public-evidence.md)
adds four synthetic software-reference exchanges. The original findings below
describe the August snapshot; no hardware captures have since been obtained.

No end-to-end Xbox host-report and expected guest-report pair meets the
confirmed replay standard yet. The checked-in replay table is intentionally
empty. Correlated prefixes and response classifiers are not a substitute for
an independently recorded exchange.

Before protocol command implementation, obtain or create redistributable
evidence for at least:

1. Identification and activation.
2. Empty and changed status polls.
3. Figure block query.
4. Successful and failed figure block write.
5. Removal and reinsertion.
6. Legacy and Traptanium version behavior.

## Recommendations

1. Keep Recommendation A and the existing 32-byte XAM boundary.
2. Do not import Cemu's 64-byte virtual portal state machine as Xbox behavior.
3. Use the next spike to record clean-room Xbox report pairs from legally
   controlled hardware or another redistributable primary source.
4. Keep the replay table empty until the evidence reference contains the exact
   request, response, length, timing context, portal generation, and capture
   provenance.
5. Treat PR 1157 as a candidate physical-backend fix, separate from the
   virtual portal protocol.

## References

- [Xenia Edge license](../../LICENSE)
- [Xenia Edge XAM raw input](../../src/xenia/kernel/xam/xam_input.cc)
- [Xenia Edge portal interface](../../src/xenia/hid/portal/portal.h)
- [Xenia Canary PR 909](https://github.com/xenia-canary/xenia-canary/pull/909)
- [Xenia Canary PR 1157](https://github.com/xenia-canary/xenia-canary/pull/1157)
- [Xenia virtual portal issue 2320](https://github.com/xenia-project/xenia/issues/2320)
- [Cemu Xbox 360 portal wrapper at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/SkylanderXbox360.cpp)
- [Cemu virtual portal core at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/Skylander.cpp)
- [Cemu bundled G.721 notice at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/g721/g721.h)
