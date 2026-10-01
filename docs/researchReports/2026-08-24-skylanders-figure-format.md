# Skylanders Figure-File Format Evidence

## Purpose

Define the byte-compatibility target for figure import, export, validation,
creation, reset, and safe persistence. This report distinguishes the raw file
rules confirmed by Dolphin and Cemu from advanced behavior currently visible
in only one reference implementation.

No retail figure dump, user data, reference fixture, proprietary artwork, or
copied metadata table was used.

## Methodology

- Inspected Dolphin at
  `123d32248e455ec242866e3a75fea70dcecfd567`.
- Inspected Cemu at
  `5ead58008dd984f614e2cb38bd9cb69bd77fd1bb`.
- Compared the current loaders, creators, serializers, and figure utilities.
- Recorded exact offsets and algorithms as interoperability facts. No source
  text or data table was copied.
- Used `Confirmed` when Dolphin and Cemu independently agree. Used
  `Observed once` when only one current implementation contains the rule.
  Used `Unknown` when evidence is not sufficient for safe production use.

## Findings

### Evidence table

`File` in the Direction column means the fact applies to the raw figure image,
not a portal transport report.

| Fact | Status | Exact bytes or offset | Direction | Portal generation | Source and revision | License | Redistribution status | Verification method |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| The compatible file is one raw 1,024-byte image with no emulator header. | Confirmed | `0x400` bytes = `0x40` blocks x `0x10` bytes | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.h` and UI loader; Cemu `5ead58008d`, `Skylander.h` and UI loader | GPL-2.0-or-later; MPL-2.0 | The dimensions are interoperability facts. Keep Xenia metadata in a sidecar. | Independent constant and loader comparison. |
| Common raw file extensions are UI filters, not distinct containers. | Confirmed | `.sky`, `.bin`, `.dmp`, `.dump` | File | Figure tag | Dolphin `123d32248e`, `SkylanderPortalWindow.cpp`; Cemu `5ead58008d`, `EmulatedUSBDeviceFrame.cpp` | GPL-2.0-or-later; MPL-2.0 | Extension strings may be used as compatibility filters. Parse by content and exact length. | Independent UI loader comparison. |
| Character identity is an unsigned 16-bit little-endian value. | Confirmed | Offset `0x10`, bytes `0x10..0x11` | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp`; Cemu `5ead58008d`, `Skylander.cpp` and `EmulatedUSBDeviceFrame.cpp` | GPL-2.0-or-later; MPL-2.0 | Offset and byte order are interoperability facts. | Independent creator and explicit loader comparison. |
| Variant identity is an unsigned 16-bit little-endian value. | Confirmed | Offset `0x1C`, bytes `0x1C..0x1D` | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp`; Cemu `5ead58008d`, `Skylander.cpp` and `EmulatedUSBDeviceFrame.cpp` | GPL-2.0-or-later; MPL-2.0 | Offset and byte order are interoperability facts. | Independent creator and explicit loader comparison. |
| The first four bytes hold the four-byte tag identifier used by both creators. | Confirmed | Offset `0x00..0x03` | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp`; Cemu `5ead58008d`, `Skylander.cpp` | GPL-2.0-or-later; MPL-2.0 | Layout fact is usable. Tests must use injected deterministic synthetic values. | Independent creator comparison. |
| Byte 4 is the XOR block-check character for the four tag-ID bytes. | Confirmed | `data[4] = data[0] ^ data[1] ^ data[2] ^ data[3]` | File | Figure tag | Dolphin `123d32248e`; Cemu `5ead58008d` | GPL-2.0-or-later; MPL-2.0 | Algorithmic fact is usable in independent code. | Independent creator comparison. |
| Both creators initialize the tag configuration bytes to the same values. | Confirmed | ATQA at `0x05..0x06` = `81 01`; SAK at `0x07` = `0F` | File | Figure tag | Dolphin `123d32248e`; Cemu `5ead58008d` | GPL-2.0-or-later; MPL-2.0 | Byte values are format facts. | Independent creator comparison. |
| Sector access bytes use one value for sector 0 and another for sectors 1 through 15. | Confirmed | Sector 0 at `0x36..0x39` = `0F 0F 0F 69`; each later sector at `sector * 0x40 + 0x36` = `7F 0F 08 69` | File | Figure tag | Dolphin `123d32248e`; Cemu `5ead58008d` | GPL-2.0-or-later; MPL-2.0 | Byte values and positions are format facts. | Independent creator comparison, accounting for little-endian storage of the source constants. |
| The identifier checksum is CRC-16/CCITT with polynomial `0x1021` and initial value `0xFFFF`, stored little-endian. | Confirmed | Input `0x00..0x1D`; output `0x1E..0x1F` | File | Figure tag | Dolphin `123d32248e`, `SkylanderCrypto.cpp`; Cemu `5ead58008d`, `Skylander.cpp` | GPL-2.0-or-later; MPL-2.0 | Algorithm parameters and byte positions are interoperability facts. Implement independently and verify with synthetic vectors. | Independent algorithm and creator comparison. |
| Each sector trailer contains a six-byte Key A derived from the tag ID and sector. | Observed once | Trailer starts at `sector * 0x40 + 0x30`; Key A occupies the first six trailer bytes | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp` and `SkylanderCrypto.cpp` | GPL-2.0-or-later | Do not copy code. Independently verify the derivation and any redistribution boundary before implementation. | Single-source inspection. Cemu's creator leaves these bytes zero. |
| Sector 0 Key A is a fixed 48-bit value; later Key A values derive from a 48-bit CRC over the four-byte tag ID and sector number. | Observed once | Sector-0 constant and per-sector derived six-byte values | File | Figure tag | Dolphin `123d32248e`, `SkylanderCrypto.cpp` | GPL-2.0-or-later | Algorithm and constant need independent test vectors and provenance review before BSD production use. | Single-source inspection. |
| Data blocks after the first eight blocks are AES-128-ECB encrypted except every fourth sector-trailer block. | Observed once | Encrypt blocks `8..63` where `(block + 1) % 4 != 0` | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp` | GPL-2.0-or-later | Behavioral fact only. Independently derive and test before implementation. | Single-source inspection. Cemu does not implement this transform in its current figure core. |
| The per-block AES key is an MD5 digest over tag blocks 0 and 1, the block index, and a fixed compatibility byte sequence. | Observed once | 16-byte MD5 output used as the AES-128 key | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp` | GPL-2.0-or-later; compatibility byte-sequence rights not separately established | Do not copy the source construction. Legal and provenance review is required before distributing the compatibility sequence. | Single-source inspection. |
| Checksum families labeled 1, 2, 3, and 6 protect gameplay regions and mirrored areas. | Observed once | Type 1: `0x10`-byte logical input; type 2: `0x30`; type 3: `0x110`; type 6: `0x40` | File | Figure tag | Dolphin `123d32248e`, `SkylanderCrypto.cpp` and `SkylanderFigure.cpp` | GPL-2.0-or-later | Reimplement only after independent offsets and synthetic expected values are recorded. | Single-source inspection. |
| Two gameplay copies use sequence fields to choose the current area. | Observed once | Main areas near `0x80` and `0x240`; additional areas near `0x110` and `0x2D0` | File | Figure tag | Dolphin `123d32248e`, `SkylanderFigure.cpp` | GPL-2.0-or-later | Advanced editing is deferred. Do not normalize or repair these areas during import. | Single-source inspection. |
| Current Dolphin and Cemu loaders provide a strict full-structure validator before accepting a file. | Unknown | Both read exactly `0x400` bytes; full validation behavior is not present in the reviewed UI load paths | File | Figure tag | Dolphin `123d32248e`; Cemu `5ead58008d` | GPL-2.0-or-later; MPL-2.0 | Xenia must implement its own safe validator and must not treat successful loading in either tool as proof of validity. | Direct loader inspection. |
| A Cemu-created blank is sufficient as a valid encrypted blank for all six Xbox 360 games and all figure types. | Unknown | Cemu initializes identity, tag fields, access bytes, and type-0 checksum, but leaves other data and Key A bytes zero | File | All | Cemu `5ead58008d`, `Skylander.cpp` | MPL-2.0 | Do not claim general validity. Creation remains gated by independent game-, type-, and variant-specific evidence. | Creator comparison found material differences. |
| Reset rules are uniform for standard figures, Swap Force halves, traps, vehicles, trophies, and Creation Crystals. | Unknown | Type-specific | File | All | No sufficient primary evidence reviewed | Not applicable | Do not expose reset for a type until its blank rules and writes are verified. | Proof gap recorded. |

### Confirmed raw layout

The initial parser may safely establish only these minimum facts before deeper
validation:

| Range | Confirmed meaning |
| --- | --- |
| `0x000..0x003` | Four-byte tag identifier |
| `0x004` | XOR block-check character |
| `0x005..0x006` | ATQA bytes |
| `0x007` | SAK byte |
| `0x010..0x011` | Character ID, little-endian |
| `0x01C..0x01D` | Variant ID, little-endian |
| `0x01E..0x01F` | Identifier CRC-16, little-endian |
| Every `0x40` bytes | One sector containing four 16-byte blocks |
| Last block of each sector | Sector trailer; exact Key A derivation remains independently unverified |

This minimum layout is not a complete validity test. A 1,024-byte file with
plausible identity bytes can still be corrupt, unsupported, or unsafe to
overwrite.

### Persistence observations

Both reviewed reference implementations write the full 1,024-byte image back
to the open file. Neither reviewed save path provides the same-directory
temporary-file, durable flush, atomic replace, parent-directory flush, and
conflict check required by this design. Xenia must not copy that persistence
behavior.

### Licensing and content boundary

| Source | Exact reviewed license | Safe use in this BSD destination |
| --- | --- | --- |
| Xenia Edge and Xenia Canary portal files | BSD-3-Clause text in repository `LICENSE` | Adapt Xenia code with required notices and authorship. |
| Dolphin Skylanders files | Per-file SPDX `GPL-2.0-or-later` | Behavioral study only. Do not copy code, tests, fixtures, or metadata tables into BSD Xenia files. |
| Cemu repository and reviewed files | MPL-2.0 in `LICENSE.txt` | Behavioral study only for Recommendation A. Do not copy implementation text into ordinary BSD Xenia files. |
| Retail figure images and user dumps | Copyright, ownership, and personal-data status varies | Never commit or redistribute them. Use synthetic deterministic fixtures. |
| Character and variant catalog | Provenance not yet established record by record | Do not import Dolphin or Cemu tables. Add only independently verified records with source and redistribution notes. |
| Compatibility constants and key derivation | Some behavior is visible only in GPL reference source | Record independent observations and legal provenance before production inclusion. |

This is an engineering boundary, not legal advice.

## Recommendations

### Identifier checksum verification on 2026-10-01

The type-0 identifier checksum is now implemented. Expected test values were
computed independently using Python `binascii.crc_hqx(input, 0xFFFF)`:

| Synthetic input | Expected CRC |
| --- | --- |
| Empty input | `FFFF` |
| ASCII `123456789` | `29B1` |
| Thirty zero bytes | `2A45` |
| Thirty bytes increasing from `00` through `1D` | `3554` |

The parser checks bytes `00..1D` against the little-endian value at `1E..1F`.
Tests also flip each bit of a synthetic 30-byte header and reject a changed
stored checksum. These vectors contain no figure dump data.

The structural test fixture uses the independently tested CRC routine to
populate its identifier checksum; its gameplay data, sector keys, and crypto
are still synthetic and unverified. A successful structural parse is suitable
for inspection, not proof of a valid playable figure or permission to write
an imported source. `IsFullyValid()` remains false for these fixtures.

The saved plan's UID-only `DeriveFigureBlockKey` signature is insufficient for
the recorded blocks-0-and-1 derivation input. It has not been implemented.
Independent derivation vectors and a corrected interface remain required.

### Remaining recommendations

1. Implement the exact-size raw parser and the independently confirmed fields
   first.
2. Reject truncation and trailing bytes. Report structural failures without
   mutating or overwriting the source.
3. Keep the independent CRC vectors above as the type-0 validation reference.
4. Hold AES, Key A, checksum families 1/2/3/6, creation, and reset behind the
   evidence gate until independent vectors are available.
5. Keep advanced gameplay editing outside the first milestone.
6. Use an Xenia sidecar for metadata and session state. Preserve the raw
   1,024-byte file for Dolphin and Cemu compatibility.

## References

- [Dolphin licensing at `123d32248e`](https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/COPYING)
- [Dolphin figure interface at `123d32248e`](https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/Source/Core/Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.h)
- [Dolphin figure implementation at `123d32248e`](https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/Source/Core/Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.cpp)
- [Dolphin crypto implementation at `123d32248e`](https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/Source/Core/Core/IOS/USB/Emulated/Skylanders/SkylanderCrypto.cpp)
- [Dolphin portal UI at `123d32248e`](https://github.com/dolphin-emu/dolphin/blob/123d32248e455ec242866e3a75fea70dcecfd567/Source/Core/DolphinQt/SkylanderPortal/SkylanderPortalWindow.cpp)
- [Cemu license at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/LICENSE.txt)
- [Cemu figure interface at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/Skylander.h)
- [Cemu figure implementation at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/Cafe/OS/libs/nsyshid/Skylander.cpp)
- [Cemu figure UI at `5ead58008d`](https://github.com/cemu-project/Cemu/blob/5ead58008dd984f614e2cb38bd9cb69bd77fd1bb/src/gui/wxgui/EmulatedUSBDevices/EmulatedUSBDeviceFrame.cpp)
