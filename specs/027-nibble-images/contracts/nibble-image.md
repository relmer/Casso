# Contract: Nibble Disk Images

The observable surfaces this feature changes. Behavior, not signatures -- these are
what a test asserts on and what a user meets.

## 1. File format

**Accepted**

| Total bytes | Track size | Tracks |
|---|---|---|
| 232,960 | 6,656 | 35 |
| 223,440 | 6,384 | 35 |

Extensions offered: `.nib`, `.nb2`. Either extension is accepted at either length;
the length decides the track size and the extension decides nothing.

**Layout**: track 0's block first, ascending, no header, no padding between blocks,
no trailing data. Each block is that track's GCR byte stream as a drive would read
it. Bytes with the high bit clear are legal and appear in real images.

## 2. Mounting

- A file at an accepted length mounts, whichever of the two extensions it carries.
- Mounting installs 35 track slots of `trackSize * 8` bits, packed MSB-first, one
  file byte per eight bits, in file order.
- Quarter-track resolution is the whole-track mapping used by every sector format.
- A mounted nibble image is write-protected only by the host file's state or the
  user's setting. The format carries no flag of its own, and the interface must
  attribute the protection accordingly.

**Refusals** -- each a clause following the file's name, per the existing
diagnosis wording:

| Condition | Clause says |
|---|---|
| length is neither total | the length found, both accepted lengths, and that 35 tracks of 6,656 or 6,384 bytes are what those numbers are |
| length right, no nibble assembles anywhere | the file is the right size but carries no readable nibble, so it was most likely renamed from something else |
| file unreadable / empty | unchanged; the existing clauses already cover these |

No refusal path may assert. A malformed image is user input.

## 3. File filters

`.nib` and `.nb2` become available in drag-and-drop, the disk picker and the disk
folder scan **as a consequence of the routing table**, with no change to any filter.

**This is a testable contract, not a note.** A test asserts that the set of
extensions the filter accepts equals the set the router resolves, so a future
format added to one and not the other fails. `Casso/Ui/DriveWidgetState.h` must
contain no list of extensions.

## 4. Write-back

Triggered by the existing flush points: eject, power cycle, reset.

| Precondition | Result |
|---|---|
| no track dirty | no write to the file at all |
| some tracks dirty | those tracks re-derived; every other track copied byte-for-byte from the loaded file |
| image write-protected | no write; the existing protection reporting applies |
| file cannot be written | the loss is reported, naming the image and what became of the writes |

**Per dirty track**

1. Derive bytes from the bit stream by the MSB rule, bounded by one revolution.
2. Rotate the derived sequence so its longest run of `$FF` ends it. With no `$FF`
   run at all, leave the rotation as derived.
3. Append `$FF` until the block is exactly the track size.

**Guarantees**

- The derived count never exceeds the block size. This is arithmetic, not a check:
  the maximum is `trackBits / 8`, which is the block size by construction.
- Every byte written to the file has its high bit set unless it was copied from an
  untouched track.
- A track loaded and re-derived without modification yields its original bytes
  exactly, provided every byte had its high bit set.
- Repeated write / eject / remount cycles do not accumulate loss.

## 5. Console commands

All nine accept nibble images.

| Command | Behavior |
|---|---|
| `list` `get` `put` `delete` `boot` `sectorread` `sectorwrite` | as on the equivalent `.dsk`; they decode to sectors, so a track that will not decode is refused with the surface named, and nothing is written |
| `create` | `--type nib` or a `.nib` name writes 232,960 bytes; `--type nb2` or a `.nb2` name writes 223,440. The name fixes the size, because there is no file to measure |
| `init` | reformats an existing nibble image in place; the container is unchanged, `init` still takes no `--type`, and the track size comes from the file's LENGTH, never its name |

**Which side decides the track size**, since these are the two rules that look like
they contradict and do not:

| Situation | Decided by | Why |
|---|---|---|
| mounting a file | its length | the bytes are there to measure, and the name is known to lie |
| a file-level command | its length | same |
| `init` | its length | the file exists, and reformatting must not resize it |
| `create` | its name | there is no file yet, so the name is the only thing that can say |

The system never writes a `.nb2` holding 6,656-byte tracks, or a `.nib` holding
6,384-byte tracks. Producing that mismatch is what the length-decides rule exists to
cope with in files from elsewhere, and manufacturing more of it would be
indefensible.

`create`'s unknown-type refusal continues to name the types that exist, now
including the two new ones. The list a user is shown and the list the tool accepts
come from the one table, as they do today.

## 6. What this feature does not promise

- **Self-sync patterns are not preserved.** The format records whole bytes. Copy
  protection that inspects self-sync will not work from a nibble image, and no
  amount of care in the loader changes that.
- **Half and quarter tracks cannot be represented.** The format has no track map.
- **Round-tripping is not bit-exact for a written track.** A track the guest wrote
  is re-derived and padded, so its timing changes even where its data does not. A
  track the guest did not write is byte-identical.
- **WOZ remains the format to archive into.** This is compatibility with existing
  collections, not a second preservation path.
