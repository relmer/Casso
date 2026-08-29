# Data Model: Nibble Disk Images

Entities are as the format and the existing track model define them. This feature
adds no persistent state of its own -- a nibble image carries no header, no
metadata and no flags, so there is nothing to model beyond the bytes and what the
track layer already holds.

## Nibble Image File

The whole file, and the only thing that identifies it.

| Field | Value | Notes |
|---|---|---|
| Total length | 232,960 or 223,440 bytes | The only identifying property. Nothing else in the file distinguishes it from noise. |
| Track count | 35, always | Not stored. Derived by dividing the length by the track size. |
| Track size | 6,656 or 6,384 bytes | For a file that exists, determined by total length and never by extension. For one being created, by the name -- see below. |
| Header | none | |
| Metadata | none | No title, creator, write-protect flag, or track map. |

**Validation**

- Length MUST be exactly one of the two totals. Any other length is
  `WrongSizeForNibbleImage`, reported with the length found and both accepted
  lengths.
- Content MUST yield at least one nibble on at least one track. A file of the right
  length carrying no assemblable nibble anywhere is `NotANibbleStream`.
- Individual bytes are NOT validated. Bytes with the high bit clear are legal in
  the file, appear in real images, and MUST NOT cause refusal.

**Where the track size comes from**: the file's length, for any file that exists. A
file being *created* has no length yet, so its name supplies it instead -- `.nib` and
the `nib` container word mean 6,656, `.nb2` and `nb2` mean 6,384. `init` reformats an
existing file and therefore measures rather than reading the name, so a reformat
never changes a file's size.

**Derived, not stored**: whether tracks decode to standard sectors. That question is
never asked at mount and never asked on the emulator's write-back path. Only the
console's file-level commands ask it.

## Nibble Track

One track's fixed-length block within the file.

| Field | Value |
|---|---|
| Length | the image's track size, identical for all 35 |
| Content | the GCR byte stream a drive would read, gaps included |
| Ordering | track 0 first, ascending, no map or index |

**Relationship to the live model**: track *n* of the file becomes track slot *n* of
the `DiskImage`, at `trackSize * 8` bits. Quarter-track resolution is the whole-track
map every sector format uses (`quarterTrack / 4`); there is no per-track map to
install, because the format has none to carry.

## Derived Nibble Stream

The transient produced when a dirty track is written back. Not persisted in this
shape; it becomes the track's block in the file.

| Property | Rule |
|---|---|
| Derivation | shift bits until the MSB sets, that byte is a nibble, repeat |
| Bound | one revolution. A track with no high-bit-set byte terminates rather than spinning |
| Maximum length | `trackBits / 8`, i.e. exactly the block size |
| Typical length | less, by two bits for every 10-bit self-sync byte |
| Rotation | rotated so its longest `$FF` run ends the sequence |
| Padding | `$FF` appended to the block size |

**The invariant worth testing**: where every byte of a track has its high bit set,
byte-concatenation and MSB-rule derivation are exact inverses. A track loaded and
immediately re-derived produces its original bytes, in order, with zero padding.

## State Transitions

A mounted nibble image moves through the states the track model already defines;
this feature adds no state of its own.

```text
                mount                  guest write              flush
  file bytes ──────────▶ clean track ──────────────▶ dirty track ──────▶ file bytes
       │                      │                           │                  │
       │                      │  (no write occurs)        │                  │
       └──────────────────────┴───────────────────────────┘                  │
                    copied verbatim on flush ◀────────────────────────────────┘
```

- **Clean track**: copied byte-for-byte from the loaded file's own bytes on flush.
  Never re-derived, so a track holding high-bit-clear bytes survives untouched.
- **Dirty track**: re-derived, rotated, padded. Its shape may change; its content
  must not.
- **No dirty tracks at all**: no write to the file occurs.

## Refusal Reasons

Two enumerators join the existing `MountFailure` set. Both describe something the
load path can actually tell apart, which is the standing rule for that enum -- there
is no enumerator for a distinction the code cannot make.

| Reason | Meaning | What the clause says |
|---|---|---|
| `WrongSizeForNibbleImage` | length is neither accepted total | the size found, and both sizes accepted, with the 35-track arithmetic that explains them |
| `NotANibbleStream` | length is right, contents assemble no nibble | that the file is the right size but holds no readable nibble, so it was most likely renamed from something else |

`WrongSizeForFormat` is not reused. Its clause names the single 143,360-byte sector
size, and a nibble image has two valid sizes and different arithmetic behind them; a
shared clause would have to be vague about both.
