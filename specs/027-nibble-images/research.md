# 027 nibble images: what is already known

Findings gathered before this spec was written, so the work does not start by
rediscovering them. Everything here was verified against the tree or a cited
source on 2026-08-28/29.

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
