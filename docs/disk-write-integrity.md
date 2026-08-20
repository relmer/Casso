# Disk write integrity — status

**Updated:** 2026-08-18. **Base:** `master` @ `a25a3c67` (released 1.16.2).

Everything the 2026-08-18 handoff listed as outstanding is now on
`claude/disk-write-integrity-b99179` and pushed, unreleased. What remains is
the follow-ups in §6.

The reference material at the end — WOZ field offsets, the repro drivers, the
build traps — is kept because it is what made the work possible and will make
the next disk change cheaper.

---

## 1. What shipped in 1.16.2 (already on master)

| Commit | What |
|---|---|
| `1a3beb1e` | `TRKS` chunk size spans the record table plus the block-aligned payload |
| `37cd294f` | `DiskImageStore::FlushEntry` writes to a temp file and renames |
| `a7baf4b6` | WOZ header CRC validated on load and reported |
| `a25a3c67` | Release 1.16.2 |

---

## 2. What this branch adds

| Commit | What |
|---|---|
| `9746d07d` | META/INFO retention: a flush no longer degrades a WOZ |
| `e2f50f93` | Write-protect flip is a one-byte patch; `ForceFlush` deleted; `DiskImage::Flush` fixed |
| `e3339bb1` | The intact Print Shop Color side A restored from history |
| `39831aa4` | A checksum-mismatched image is held read-only, with its own badge |
| `2db5e118` | A partly-decoded sector image refuses to save (GH #115) |

Gates on each commit: Debug and Release suites green, code analysis 0 warnings,
`CheckStyle.ps1 -Mode Tree` clean. Final counts: Debug 2999, Release 2996.

### 2a. Metadata retention

`DiskImage` carries a `WozMetadata` (`CassoEmuCore/Devices/Disk/WozMetadata.h`):
the source INFO chunk verbatim, plus every chunk the loader walked past without
modeling, in source order. `WozLoader::Serialize` re-emits them unchanged and
overwrites only the four INFO fields Casso owns — version, disk type, write
protect, largest track.

The chunk walk no longer treats an unrecognized id as end-of-table; any
four-uppercase-letter tag is stepped over and retained. Bit-stream blocks
cannot be mistaken for one, because 6-and-2 nibbles all have the high bit set.

**Verified:** all six intact WOZ 2 images in `Apple2/Demos` round-trip
byte-for-byte through load → serialize. The four WOZ 1 images necessarily
change (v1 → v2 relayouts track data) but keep creator, synchronized, cleaned
and their whole META.

**Creator policy, as decided:** Casso stamps `Casso <version>` only when there
is no retained source INFO, which is exactly the case for a disk it authored.
A disk it edited keeps whoever imaged it.

### 2b. Write-protect is an edit, not a rewrite

`DiskImageStore::SetImageWriteProtect` replaced `ForceFlush`: flush pending
guest writes, patch INFO's flag byte, recompute the header CRC, write back
atomically, then move the live image's flag to match. `WozLoader::SetWriteProtectFlag`
does the byte-level part and touches nothing else.

**Measured across all eleven demo images:** un-protecting one changes **five
bytes** (the flag plus the four CRC bytes), and zero bytes for images already
unprotected. The old path rewrote the whole file — up to 223,700 bytes on a v1
image — and dropped META.

`ForceFlush` and `FlushEntry`'s `force` parameter are gone. There is no way to
ask for a flush past the dirty and write-protect gates.

`DiskImage::Flush` — the second write path, reached on eject and //e soft reset
— no longer falls back to writing the file's pre-session bytes and returning
`S_OK`, and now goes through `WriteFileAtomically`.

`SetImageReader` mirrors the existing `SetFlushSink`, so a read-modify-write is
testable without the filesystem. `Mount` shares that read path.

### 2c. Damaged images

A checksum mismatch is a fifth `WriteProtectInfo` source, `checksumMismatch`.
Session state, never the image flag — that flag lives in the file, so setting it
would mean writing the file being protected from writes. Guest-visible, as
decided. `SetImageWriteProtect` refuses a damaged image, because patching its
flag recomputes the CRC and that CRC failing to match *is* the damage report.

The flush path's launder warning was deleted rather than left: the write-protect
gate returns before it, so it had become unreachable.

**UI:** an amber warning triangle in the padlock's place, on the faceplate and
in the compact drive row. Validated by running the emulator with a damaged image
in drive 1 and an intact write-protected one in drive 2.

### 2d. GH #115 — the issue's diagnosis was wrong

The issue attributes the loss to the early `break` that abandoned a track after
one bad sector. **It is not that.** `DecodeOneSector` only fails after sweeping
the whole track without finding a prolog, and from that position every later
attempt fails identically — `break` and `continue` produce byte-identical
output on the same damaged images. Verified both ways.

The loss is the **silence**. With one sector's data field destroyed, that sector
returns zeros *and a second sector returns the following sector's data*, because
the scan for the missing data field runs on and finds the next one, filing it
under the number the address field gave. Two sectors wrong from one point of
damage, reported as a clean save.

`Denibblize` now fills a `DenibblizeReport` and fails when a track decoded some
sectors but not all. A track that decodes **nothing** still succeeds — an
unformatted track in a sector image legitimately is zeros, and treating that as
damage would make every blank disk refuse to save.

The `break` is still replaced by a `continue`, as defense for the day the
decoder gains a per-sector failure mode. The comment says so rather than
claiming a fix it does not make.

### 2e. Two open questions from the old handoff, now answered

**"Karateka, Choplifter, Space Quarks, Lode Runner and Carmen Sandiego side A
report 0 tracks decoding as standard 16-sector data — the same early-stop
defect under-reporting?"** No. Surveyed with the new report: they genuinely
decode almost nothing as 16-sector data (Karateka: 34 of 35 tracks
unformatted). They are copy-protected disks that do not use standard
formatting, which is why they are WOZ images and why Karateka boots — Casso
runs them at nibble level and never takes that path. Four of the eleven demo
images decode fully as 16-sector data.

**"`UnitTest/Fixtures/copyprotected.woz` and `sample.woz` are 0 bytes —
unresolved."** They are Phase 0 path placeholders (spec 004 T002 says so
explicitly). The tests naming them pass those strings as *virtual* paths to
`MountFromBytes` over synthesized bytes and never read the files. Nothing is
broken; the files are vestigial.

---

## 3. Print Shop Color side A

Restored byte-for-byte from `61f89c9d`, which had it intact. Done only after
retention landed, and verified stable: round-tripping the recovered original
through the current writer reproduces it exactly, so the next flush will not
re-damage it.

**It is not a pure metadata repair.** Relative to the damaged file the original
also lacks two runs of genuine guest writes — 356 bytes in TRKS slot 17 and 355
in slot 20, Print Shop storing its configured printer/interface details. Right
for a preservation dump, but a discard.

A re-scan finds no remaining WOZ in the tree with `creator = Casso`.

---

## 4. Decisions, still standing

- Creator is stamped only on disks Casso authors, preserved otherwise.
- Damaged images are write-protected rather than silently non-persisting.
- The damaged badge is distinct from the padlock, not a variant of it.
- 020 merges master rather than rebasing.
- **No modification audit trail in META.** The previous handoff proposed a
  private `casso_modified` key recording that Casso had edited a disk. Rejected
  by the owner 2026-08-18: Casso stamps the disks it creates and does not
  annotate disks it merely edits. It also would have cost the property
  retention just bought — that a WOZ can pass through Casso completely
  unchanged, which is verifiable and is the stronger guarantee for a
  preservation dump. Do not re-propose it.

---

## 5. Test coverage added

- **Retention** (`WozLoaderTests`): META byte-for-byte, INFO fields Casso does
  not own, creator policy in both directions, an unmodeled chunk surviving,
  a v1 source keeping what v1 recorded, owned fields still winning, a whole-file
  byte-identical round trip, and guest writes surviving alongside metadata.
  Six of the eight fail without the fix and name what was lost.
- **Structural invariants** over writer output across six image shapes plus a
  hand-built image the writer did not lay out: no overlapping TRK records,
  every claimed block inside the file, blocks large enough for the bits they
  claim, block-aligned TRKS end, fixed INFO/TMAP sizes, a strict chunk walk
  reaching EOF, and TMAP entries pointing at populated records. Reintroducing
  the short-TRKS bug turns them red and names the shape.
- **Write-protect flag** (`WozLoaderTests`, `DiskWritePathTests`): only the flag
  and checksum change, the result validates against its own checksum, a v1 file
  stays v1, non-WOZ and INFO-less inputs are refused without a partial write,
  and the store-level toggle persists guest writes first.
- **Damaged images** (`DiskImageStoreTests`, `DriveWidgetStateTests`): mount
  write-protects and says why, flush never writes, the toggle refuses without
  writing a byte, and the tooltip composes correctly (including a test that
  catches the cause list overwriting the damage sentence).
- **#115** (`NibblizationTests`): a clean image reports every track complete,
  one broken data field fails and names the track, the second corrupted sector
  is pinned explicitly, an unformatted track still succeeds, and `Serialize`
  carries the refusal.

**Known gap, unchanged:** a write that fails *part way* is not unit-testable
without a filesystem seam in `DiskImageStore`. Said so in
`DiskImageStoreTests.cpp` rather than papered over.

---

## 6. Follow-ups

- **Map damaged sectors to files.** Salvage reports counts per disk; per FILE
  would be far more useful -- "CATALOG lost 3 sectors, HELLO is intact" tells a
  user whether the damage matters. Feasible: `DenibblizeReport` already carries
  per-track sector masks, and the tree has ProDOS volume structure plus DOS
  catalog handling to walk against them. Deferred deliberately, not forgotten.
- **GH #115** can be closed when this branch merges.
- **The write-protect menu item is still enabled for a damaged image.** Clicking
  it now fails with a clear explanation rather than corrupting anything, but
  disabling it would be better. The enable plumbing is `MainMenu::SetCheckQuery`
  in `EmulatorShell.cpp` (~line 2159).
- **`kQuarterTracksPerTrack` and `kMaxTracks` in `WozLoader.cpp` are unused** —
  pre-existing, not touched here.
- **020 coordination.** `DenibblizeReport` is the per-track decode-report API
  020's FR-017 needs; merge master into 020 rather than growing a parallel one.
  020's `WozLoader::Describe` also walks the chunk table and parses META — it
  should consume the retained `WozMetadata` rather than re-parsing.
- Two bugs the old handoff wanted filed (WOZ metadata loss, the ForceFlush
  toggle) are fixed here and unfiled. Filing them now would only be for the
  record.

---

## Appendix A — WOZ field reference

Verified against <https://applesaucefdc.com/woz/reference2/>.

INFO payload offsets, from the start of the chunk's data (mirrored by the
constants at the top of `WozLoader.cpp`):

| Offset | Field | Owner |
|---|---|---|
| +0 | INFO version | Casso (always emits ≥ 2) |
| +1 | disk type | source |
| +2 | write protect | Casso |
| +3 | synchronized | source |
| +4 | cleaned | source |
| +5..36 | creator, UTF-8, **space-padded to 32, not null-terminated** | source, unless Casso authored the disk |
| +37 | disk sides | source (filled for a v1 source) |
| +38 | boot sector format | source |
| +39 | optimal bit timing | source (filled for a v1 source) |
| +40..41 | compatible hardware | source |
| +42..43 | required RAM | source |
| +44..45 | largest track, in blocks | Casso |

META is tab-delimited `key<TAB>value`, LF-terminated rows, UTF-8 no BOM.
Keys are case-sensitive; neither keys nor values may contain tab, LF or pipe
(pipe is the multi-value separator). Standard keys: `title`, `subtitle`,
`publisher`, `developer`, `copyright`, `version`, `language`, `requires_ram`,
`requires_machine`, `notes`, `side`, `side_name`, `contributor`, `image_date`.
Implementors may add their own keys.

**Nibble-layout gotcha.** A 16-sector track holds only **8** byte-aligned
`D5 AA 96` address prologs and 8 `D5 AA AD` data prologs, not 16 of each:
self-sync gap bytes occupy 10 bits, so nibbles following an odd number of them
sit off the byte boundary. The decoder finds those by bit-level resync. A
byte-wise search over a track's packed bytes sees only half — which is correct,
not a bug, and `BreakOneDataField` in `NibblizationTests.cpp` documents it.

---

## Appendix B — tools

`CassoCli disk` does not exist on master (that is spec 020, unreleased), so
drive the core entry points directly. Build Release, then compile against
`x64/Release/CassoEmuCore.lib` + `CassoCore.lib`:

```
cl /nologo /std:c++20 /EHsc /MD /O2 /DNDEBUG /I<repo>\CassoEmuCore /I<repo>\CassoCore
   /FI<repo>\CassoEmuCore\Pch.h driver.cpp
   <repo>\x64\Release\CassoEmuCore.lib <repo>\x64\Release\CassoCore.lib
   ole32.lib oleaut32.lib propsys.lib windowscodecs.lib mfplat.lib
   mfreadwrite.lib mfuuid.lib shlwapi.lib
```

**Sequence the calls in a driver.** MSVC evaluates function arguments
right-to-left, so `printf("%d", Load(...), img.GetTrackCount())` prints the
track count from *before* the load. That cost a confusing first result.

`DiskImage::Load(path)` is the **legacy DSK-only** path — it reads the first
143,360 bytes and nibblizes them. Do not use it to load a WOZ.

Chunk-table dumper (any Python 3):

```python
import sys, struct
def dump(p):
    d = open(p, 'rb').read()
    print(f"== {p}  ({len(d)} bytes)")
    pos = 12
    while pos + 8 <= len(d):
        cid = d[pos:pos+4]; sz = struct.unpack('<I', d[pos+4:pos+8])[0]
        if not all(32 <= c < 127 for c in cid):
            print(f"   !! at {pos}: not a chunk id -- a strict walker stops here")
            break
        print(f"   {cid.decode():4s} size={sz}")
        if cid == b'INFO':
            i = d[pos+8:pos+8+sz]
            print(f"        wprot={i[2]} sync={i[3]} cleaned={i[4]} "
                  f"creator={i[5:37].decode('latin1').rstrip()!r}")
            print(f"        bootfmt={i[38]} timing={i[39]} "
                  f"hw={struct.unpack('<H', i[40:42])[0]} "
                  f"ram={struct.unpack('<H', i[42:44])[0]}")
        if cid == b'META':
            for line in d[pos+8:pos+8+sz].decode('utf-8', 'replace').split('\n'):
                if line.strip(): print("        " + line.replace('\t', '  ->  '))
        pos += 8 + sz
    else:
        print("   (end of file)")
for p in sys.argv[1:]: dump(p)
```

A healthy image walks cleanly to `(end of file)`.

**Capturing the drive band.** `PrintWindow` with `PW_RENDERFULLCONTENT`, not
`CopyFromScreen`: activating another process's window is refused when the
session is locked or the foreground lock is held, and `CopyFromScreen` then
captures the wallpaper. `PrintWindow` reads the window's own content
regardless; the emulated video area comes back black (composited swap chain),
which does not matter for chrome. Dismiss a modal message box by posting
`BM_CLICK` to its OK button — `SendKeys` needs foreground and an unlocked
session.

---

## Appendix C — build and gate commands

```powershell
scripts\Build.ps1 -Configuration Release -Platform Auto -Target Build
scripts\RunTests.ps1 -Configuration Release -Build
scripts\RunTests.ps1 -Configuration Debug -Build        # the pre-merge gate
scripts\Build.ps1 -Configuration Debug -Platform Auto -Target Build -RunCodeAnalysis
scripts\CheckStyle.ps1 -Mode Tree
```

Traps worth knowing:

- **`CheckStyle -Mode Tree` only scans tracked files.** A new file is invisible
  until `git add`, so it reports OK on code it never read. `git add` first, then
  trust the count.
- **CS0016 (3 blank lines after a declaration block) fires unevenly.** Its
  decl-block walk stops at any line its pattern cannot parse — an array
  declaration like `Byte tmap[kSize] = {};` ends the run — so some existing
  functions with one blank line are invisible to it. New code still has to
  comply where it *is* seen; do not read the survivors as precedent.
- **CS0011 bans calls inside EHM macro conditions**, including
  `report.HasPartialTrack()`. Hoist to a named local first.
- **`RunTests.ps1` refuses to run a stale assembly.** Editing `CassoCore/Version.h`
  trips the guard; `-AllowStale` is the escape hatch. A build that "succeeds"
  in ~1 second is a skipped build.
- **The pre-push style gate rejects Claude attribution in commit messages**
  (CS0008).
