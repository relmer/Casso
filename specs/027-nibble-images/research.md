# 027 nibble images: what is already known

Findings gathered before this spec was written, so the work does not start by
rediscovering them. Everything here was verified against the tree or a cited
source on 2026-08-28/29.

> **Two claims below were later found to be wrong**, and the corrections are in the
> Phase 0 section at the end of this file rather than edited in above: the track
> size question is settled, and the `Denibblize` defect is already fixed. Read that
> section before acting on anything here.

## Where `.nib` stands today

**Nothing in Casso reads it.** `DiskImageStore::DetectFormatByExtension`
(`CassoEmuCore/Devices/Disk/DiskImageStore.cpp`) maps `dsk`, `do`, `po` and
`woz` to a `DiskFormat` and returns `E_FAIL` for everything else. That is the
single dispatch point for loading.

**It used to be advertised anyway.** `Casso/Ui/DriveWidgetState.h` kept its own
extension list for drag-and-drop and the disk picker's folder scan, and that
list said `.nib`. A dropped nibble image therefore passed the filter, failed to
load, and — before the mount-reporting work landed — did so silently, while
still being recorded in the recent-disks list as though it had mounted.

**That is now resolved in the other direction.** The filter no longer keeps a
list: `IsSupportedDiskImageExtension` forwards to
`DiskImageStore::IsMountableImageExtension`, which answers from the routing
table itself. So the two cannot disagree.

**Which means the filter follows the loader for free.** Add `.nib` to
`DetectFormatByExtension` and drag-and-drop, the picker and the folder scan
begin offering it with no further change. Do not reintroduce a second list.

The CLI refuses it consistently today: *"cannot tell what kind of image
&lt;path&gt; should be / give it a .dsk, .do, .po or .woz extension, or say which
with --type"*, and `disk create --type` accepts only `dsk, do, po, woz`.

## What the format is, and what it costs

A nibble image is the raw GCR byte stream as read off the drive, per track,
with no sector structure. Track size is **not settled**: 6656 bytes/track x 35
tracks = 232,960 bytes is the common convention, but 6384 is also attested in
the format references. **Settle this before writing a loader** and record which
is accepted, or whether both are.

**It is lossy in exactly the way that matters for protection.**
[CiderPress2](https://ciderpress2.com/formatdoc/Nibble-notes.html) is explicit:
nibble images *"record full bytes only. This is easy to do with standard drive
hardware, but loses the self-sync byte information"*, where WOZ *"records all
bits."* Self-sync patterns are what copy protection detects. Its own summary is
even-handed — *"byte-oriented is easier to capture, bit-oriented is more
accurate. For disks with a standard sector format, the choice is largely
irrelevant"* — but note that `.nib`'s advantage is **ease of capture on real
hardware**, which is worth nothing to an emulator that captures nothing.

So the honest expectation: standard disks will work; protected disks will work
only to the extent the format preserved them, which for the schemes that
inspect self-sync patterns is not at all. Spec 022's User Story 2 already words
its acceptance scenario that way ("boots to the extent the format preserves the
protection"), and that wording should survive.

## Why it is worth doing anyway

The fidelity argument does not favor `.nib`; WOZ dominates it for preservation.
The argument that does hold is **compatibility with the `.nib` collections
people already have**. A good deal of archived software circulated in that form
before WOZ existed, and a user with a folder of them currently cannot open any
of them.

For reference, AppleWin has supported `.nib` for a long time; MAME's support is
incomplete ([mamedev/mame#2758](https://github.com/mamedev/mame/issues/2758)).

## The work, as far as it was scoped

**A loader is not a thin adapter over what exists.** `NibblizationLayer`
converts **sectors** to and from bit streams. `.nib` needs **nibble bytes** to
and from a bit stream, with re-sync. That is a different seam and does not
exist yet.

**Load-only is not an option.** Casso's mount path is a write-back path:
`FlushEntry` -> `DiskImage::Serialize` fires on eject, power cycle and reset. So
a `.nib` that is mounted will eventually be written, and the writer has to
re-derive byte-aligned nibbles from a live bit stream whose length and alignment
have changed under guest writes. **This is the hard part of the feature**, not
the reading.

**`DiskFormat` is a total enum switched on across roughly 13 files**, and the
`disk` subcommand's nine commands each need the new type, or a stated reason not
to have it.

## A known defect this work would newly expose

`NibblizationLayer::Denibblize` stops at the first sector it cannot decode on a
track, leaves that sector and every later one on the track as zeros, and returns
`S_OK` anyway. `DiskImage::Serialize` puts it on the emulator's flush path, so a
guest that leaves a track partly written can already lose the rest of it on
eject. Written up in `specs/020-disk-file-access/research.md`.

Implementing `.nib` adds a caller to that path, and a nibble image is more
likely than a sector image to contain tracks that do not decode cleanly — that
is much of the point of the format. Decide deliberately whether this feature
fixes that defect, works around it, or documents it.

## Relationship to other specs

- **Split out of `specs/022-disk-image-formats`**, whose User Story 2 covers
  exactly this, the same way 023 was split out of 019. 022 remains the home for
  2MG, compressed and archived containers, additional filesystems and larger
  media, and stays gated behind 020/021. Its US2 should be marked as delivered
  here rather than duplicated.
- **`specs/007-ui-overhaul` FR-022 and SC-004 require `.nib` drag-and-drop.**
  That requirement has never been satisfiable and is currently unmet. Delivering
  this feature satisfies it.
- 022 also notes that Casso's documentation claimed nibble images could be
  dragged onto a drive when the mounting code did not support it. That was
  resolved by removing the claim; this feature makes the claim true, so the
  README and the drag-and-drop documentation need updating again.

## Sources consulted

- [CiderPress2 nibble notes](https://ciderpress2.com/formatdoc/Nibble-notes.html)
- [Apple II disk image format FAQ](https://stason.org/TULARC/pc/apple2/faq/10-006-What-are-DSK-PO-DO-HDV-NIB-and-2MG-disk-image.html)
- [mamedev/mame issue 2758](https://github.com/mamedev/mame/issues/2758)


---

# Phase 0: decisions taken for the plan

Added 2026-08-29, after the notes above were written and the tree was re-checked.
Where this section disagrees with the notes above, this section is right and the
correction is stated rather than the older claim quietly edited away.

## Two corrections to the notes above

**The track-size question is settled, and the answer is both.**
[CiderPress2's unadorned-image notes](https://ciderpress2.com/formatdoc/Unadorned-notes.html)
state that `.nib` holds 6,656 bytes per track and `.nb2` holds 6,384, both over 35
fixed-length tracks -- 232,960 and 223,440 bytes. The same page warns that the
naming is not reliable, that an image with 6,384 bytes per track is sometimes
called `.nib`, and that the file length is therefore the definitive identifier.
Both lengths are accepted under both extensions.

**The `Denibblize` defect is already fixed, and would not have applied anyway.**
The notes above describe `NibblizationLayer::Denibblize` returning `S_OK` over
sectors it had zero-filled. That was GH #115 and it is fixed on master: the strict
overload now refuses a partly-decoded track, salvage and coverage reporting moved
to separate entry points, and `DiskImage::Serialize` takes the strict one. So there
is nothing for this feature to newly expose.

Separately, and more usefully: **a nibble write-back does not take that path at
all.** A nibble image's file format IS the byte stream, so serializing one means
re-deriving nibble bytes and nothing else. No sector decode is involved, so a track
that does not decode to standard sectors costs the emulator's flush path nothing.
The hazard applies only where a command genuinely needs sectors, which is the
console's file-level commands, and there it meets the existing refusal -- which is
the correct behavior and is left alone.

## The fact the design turns on

**A track's bit length is fixed at mount, and a guest write cannot change it.**
`DiskImage::WriteBit` locates a bit through `TryLocateBit`, which takes
`bitIndex % trackBits` and writes in place. Only bulk loaders call `ResizeTrack`.
So the guest writes onto a circle of fixed length, exactly as a drive does.

What the guest *can* change is how many bytes those bits yield, because a self-sync
byte occupies ten bit cells and still yields one byte. So:

- the maximum derivable byte count is `trackBits / 8`, which for a 6,656-byte track
  is exactly 6,656 -- **overflow is arithmetically impossible, not merely unlikely**
- a track with any self-sync in it derives **fewer** bytes than the block holds
- under-fill is therefore the normal case after any guest write, and the shortfall
  has to be filled

Worked from the tree's own encoder: `NibblizeDsk` lays down 16 sectors of
20 sync + address field + 6 sync + data field. That is 200 + 112 + 60 + 2,792 =
3,164 bits per sector, 50,624 bits per track, of which 416 bytes are 10-bit sync
and 5,808 are 8-bit. It derives to 6,224 bytes, leaving **432 bytes of padding** in
a 6,656-byte block. That padding lengthens the address-prologue gap, which is what
a real formatted track has more of anyway.

## Decisions

**D1 -- Identify by length, offer both extensions.** 232,960 bytes is 35 tracks of
6,656; 223,440 is 35 tracks of 6,384. Either length is accepted under `.nib` or
`.nb2`. Rationale: the extension convention is broken by real files and the two
lengths are unambiguous. Alternative rejected: trusting the extension, which is
knowably wrong for files that exist.

**D2 -- Pad the shortfall with `$FF`.** Every byte the file carries then has its
high bit set, so the padding reads back as an ordinary gap. Alternatives rejected:
zeros, which create a stretch the shift register never assembles a nibble from and
stall the head on the next mount; refusing unless the derivation fills exactly,
which would refuse nearly every real write and make the feature write-protected in
practice; and repeating the derived stream, which invents duplicate address fields.

**D3 -- Rotate to put the padding in the largest gap.** The derived stream is
rotated so its longest run of `$FF` ends the sequence, and the padding is appended
there. The track is a circle and the Disk II controller has no index sensor, so
rotation is undetectable by any guest. A track with no sync run at all falls back
to appending at the derivation seam, which is where the circle was already broken.
Alternative rejected: appending at a fixed offset, which splits whatever field
happens to sit at bit zero.

**D4 -- Untouched tracks are copied, not re-derived.** The writer starts from the
loaded file's own bytes and replaces only the tracks marked dirty. This is what
makes FR-009 and FR-010 true by construction rather than by the derivation
happening to be exact. It also covers a case derivation cannot: a nibble image
holding bytes with the high bit clear -- which real ones do, in gaps -- does not
re-derive byte-identically, because such a byte is absorbed into the next byte's
shift. Copying an untouched track sidesteps that entirely.

**D5 -- An unmodified track re-derives exactly.** Where every byte does have its
high bit set, byte-concatenation and MSB-rule derivation are exact inverses: eight
shifts assemble byte 0 and set the MSB on the eighth. This gives the round-trip
test a strong invariant to assert, independently of D4.

**D6 -- A new codec class, not an extension of `NibblizationLayer`.**
`NibblizationLayer` converts sectors to and from bit streams. This converts nibble
bytes to and from bit streams. Different seam, and the project's rule that a class
with behavior gets its own header/source pair applies. The one thing worth sharing
is the MSB-rule read, which already exists as `ReadNibbleAt`, a file-scope static in
`NibblizationLayer.cpp`; it is promoted rather than copied.

## Sources consulted for this phase

- [CiderPress2 unadorned image notes](https://ciderpress2.com/formatdoc/Unadorned-notes.html)
  -- the track sizes, the total lengths, and the naming ambiguity
- [CiderPress2 nibble notes](https://ciderpress2.com/formatdoc/Nibble-notes.html)
  -- byte-oriented versus bit-oriented capture
- The tree itself, for everything about how tracks are stored, written and flushed
