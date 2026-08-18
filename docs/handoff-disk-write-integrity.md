# Handoff — disk write integrity

**Written:** 2026-08-18. **Base:** `master` @ `a25a3c67` (released 1.16.2).
**Purpose:** resume this work cold, on any machine, without re-deriving anything.

Everything in "Shipped" is on `master` and pushed. Everything in "Outstanding" is
not started. Decisions already made by the owner are marked **DECIDED** — do not
re-litigate them.

---

## 1. Shipped in 1.16.2

| Commit | What |
|---|---|
| `1a3beb1e` | `TRKS` chunk size spans the record table plus the block-aligned payload |
| `37cd294f` | Flush writes to a temp file and renames; a failed save no longer destroys the image |
| `a7baf4b6` | WOZ header CRC validated on load, reported, warned about before an overwrite |
| `a25a3c67` | Release 1.16.2, CHANGELOG sectioned, `VERSION_PATCH` 1 → 2 |

Gates at release: Debug 2974/2974, Release 2971/2971, code analysis 0 warnings,
`CheckStyle.ps1 -Mode Tree` clean.

### 1a. What the TRKS fix was

`WozLoader::Serialize` and `BuildSyntheticV2` sized the `TRKS` chunk to the
160-entry record table alone (1280 bytes), leaving the block-aligned bit streams
outside the chunk they belong to. A conformant parser adds 1280 to the chunk
start, lands mid-track-data, finds no valid chunk id and stops — so every chunk
after `TRKS` was unreachable to every tool but ours. Casso survived its own
output only because `Load` stops scanning at the first non-chunk
(`WozLoader.cpp:325-328`), which is exactly the state a short size produces.

Now `trksRecBytes + (nextBlock - kV2FirstDataBlock) * kV2BlockSize`. Verified
against a real dump: output is `TRKS(234240)`, byte-for-byte what Applesauce
writes for the same geometry. The fix changes six bytes of output — two size
bytes and the four CRC bytes. No track data moves.

### 1b. What the atomic-flush fix covered — and did NOT

`DiskImageStore::FlushEntry` now calls `DiskImageStore::WriteFileAtomically`
(public static, because its failure modes are filesystem states and that is what
makes them testable). Writes a `.casso-tmp` sibling, checks the stream state
**after close**, renames over the target, removes the temp and leaves the
original untouched on any failure.

**It did not fix `DiskImage::Flush()`.** See item 3 below — that is a separate,
worse write path.

---

## 2. THE BIG ONE — META / INFO retention (not started)

A round trip through Casso still deletes the `META` chunk and overwrites most of
`INFO`. Every WOZ that passes through a flush is silently degraded.

### Root cause — this was an accident, not a decision

Before `e068a980` (2026-07-08) `DiskImage::Serialize`'s WOZ arm returned the
**untouched source bytes**. That preserved everything byte-for-byte but
discarded every guest write on flush. The fix rebuilt the file from the live
per-track buffers, which made guest writes survive — and a writer that
reconstructs from the model can only emit what the model holds. `Load` never
kept `META` (`WozLoader.cpp:355`, "META chunk is optional, ignored on load",
dating to `c8e7428d`).

|                     | guest writes | metadata  |
|---------------------|--------------|-----------|
| before `e068a980`   | **lost**     | preserved |
| after `e068a980`    | preserved    | **lost**  |

Neither was right. Keep both: retain the raw bytes of chunks we don't model and
re-emit them unchanged.

### What is lost today

`Apple2/Demos/The Print Shop Color side A.woz` is the in-tree casualty. Its
intact original is recoverable — see item 6.

| INFO field | original | after a Casso round trip |
|---|---|---|
| creator | `Passport.py by 4am (2019-02-17)` | `Casso` |
| synchronized | 1 | 0 |
| boot sector format | 1 (16-sector) | 0 (unknown) |
| compatible hardware | 63 (all models) | 0 (unknown) |
| required RAM | 48K | 0 (unknown) |
| write-protect | 0 | 0 — **correctly preserved** |
| META (253 bytes) | title, publisher, developer, copyright, language, requires_machine, requires_ram, side, image_date, contributor | **chunk deleted** |

### Head start

`DiskImage::m_rawSourceBytes` **already holds the complete original file bytes
for every mount**, WOZ included (`DiskImage.cpp:542`, `LoadFromBytes`). The
source data for retention is already in memory; this is mostly a matter of
re-emitting rather than re-parsing.

Note spec 020 added `WozLoader::Describe`, which already walks the chunk table
and parses META for display. `Describe` is **not on master** — it is on the 020
branch. Retention and display should share one walk rather than growing a second
parser, so coordinate rather than duplicating.

### Creator policy — **DECIDED**

- **Disks Casso authors** (`BlankDiskBuilder`): stamp `Casso <version>`.
- **Disks Casso merely edits**: preserve the source creator verbatim.

Field spec, verified against <https://applesaucefdc.com/woz/reference2/>:

> INFO Creator — 32 bytes at INFO-data offset **+5** (file offset 25 in the
> standard layout), UTF-8, **space-padded to 32, not null-terminated**. The
> spec's own example is `"Applesauce v1.0                 "` — name plus version.

The existing `memset (info + 5, ' ', 32)` padding is already correct; it needs
the version appended and needs to become conditional.

Record the edit as a **custom META key**. The spec permits this: *"Implementors
are free to add additional keys to the metadata as long as they follow the same
rules laid out here."* Format is tab-delimited `key<TAB>value`, LF-terminated
rows, UTF-8 no BOM; keys are case-sensitive; neither keys nor values may contain
tab, LF, or pipe (pipe is the multi-value separator).

```
casso_modified	2026-08-18T07:18:45Z Casso 1.16.2
```

Two prohibitions:

- **Do not write into `notes`.** It is a standard key holding the *curator's*
  notes (4am's, in these files). Overwriting it destroys provenance while
  recording ours.
- **Never touch `image_date`.** It is the date of the flux imaging pass
  (`2019-01-17T18:41:42.228Z` on side A), not a modification time.

Standard META keys, for reference: `title`, `subtitle`, `publisher`,
`developer`, `copyright`, `version`, `language`, `requires_ram`,
`requires_machine`, `notes`, `side`, `side_name`, `contributor`, `image_date`.

---

## 3. ForceFlush must not be able to corrupt a disk or lose data — **DECIDED, period**

This is a hard requirement from the owner, and the guarantee must be
**structural** rather than contingent on retention being complete and correct.

### Why it is the most dangerous path in the tree

A WOZ's write-protect flag lives inside the file at INFO byte 2, so flipping it
must reach disk. Two gates normally prevent that write — `FlushEntry` skips
images that aren't dirty, and skips write-protected images. `ForceFlush` bypasses
both, legitimately. The problem is what it bypasses *into*:
`DiskImage::Serialize`, the full rebuild-from-model writer.

`DiskManager::ToggleImageWriteProtect` (`Casso/Shell/DiskManager.cpp:232`):

1. **No guest write is needed.** Every other route to damage requires the
   emulated machine to write something. This one is a menu click.
2. **It fires in both directions.** `if (protecting) { Flush }` is conditional;
   the `ForceFlush` after it is not. Un-protecting rewrites the file too — and
   un-protecting is exactly what a user does to a preservation dump before
   trying to write to it. **7 of 11 demo images ship `wp=1`.**
3. **Protecting costs two full rewrites** — the pending-writes `Flush`, then the
   `ForceFlush`.

### The plan

1. **The toggle stops re-serializing.** Flipping write-protect means changing one
   byte: read the file, set INFO byte 2, recompute the header CRC32 over
   everything after the 12-byte header, write through
   `DiskImageStore::WriteFileAtomically`. Bytes we never parse are bytes we never
   touch — preservation by construction, not by remembering to retain each chunk.
2. **Delete `ForceFlush`.** After (1) it has **no production caller** —
   confirmed on `master` and on both `origin/019-assembler-dialects` and
   `origin/020-disk-file-access`, neither of which adds one. Three tests in
   `UnitTest/EmuTests/DiskWritePathTests.cpp` (~lines 348-413) move to the new
   operation. You cannot misuse an API that does not exist.
3. **Fix `DiskImage::Flush()`** — see below.

### `DiskImage::Flush()` — a second, worse write path (`DiskImage.cpp:~700`)

```cpp
hr = Serialize (bytes);

if (FAILED (hr))
{
    BAIL_OUT_IF (m_rawSourceBytes.empty(), S_OK);
    bytes = m_rawSourceBytes;      // silently write the ORIGINAL bytes
    hr    = S_OK;                  // and report success
}

ofstream file (m_filePath, ios::binary);   // truncates; unchecked after write
```

If `Serialize` fails, this writes the file's **pre-session bytes** over the
user's disk and returns `S_OK`. Every guest write that session is gone, silently.
It also has the truncate-then-write and missing post-write check that
`37cd294f` fixed in the store — that fix covered `DiskImageStore::FlushEntry`
only. Reached via `DiskImage::Eject()`, which `Disk2Controller::EjectDisk`
(`Disk2Controller.cpp:678`, `:893`) calls.

**Route it through `WriteFileAtomically` and delete the fallback outright.**
Writing stale bytes and reporting success is never the right answer; a failed
serialize must fail loudly and keep the image dirty.

Behavior change to expect: a WOZ whose `Serialize` fails will refuse to save and
report an error, where today it silently reverts the file.

---

## 4. Damaged images become write-protected — **DECIDED**

A CRC mismatch (already detected as of `a7baf4b6`) should make the image
read-only.

- **Mechanism:** a **fifth source** in `WriteProtectInfo`
  (`CassoEmuCore/Devices/Disk/IDiskImage.h:46`), which is already an OR of four
  independent reasons — `imageFlag`, `userSetting`, `readOnlyFile`,
  `noPermission` — with a struct carrying each so the UI can explain *why*.
- **It must be session-level**, like `userSetting`, **never the `imageFlag`**.
  The image flag lives in the WOZ's INFO chunk, so setting or clearing it means
  writing the file — which is what item 3 exists to prevent.
- **Distinct visual treatment — not the padlock.** The owner's call: this is a
  different and more worrying state than ordinary write protection and needs its
  own badge and message. `DriveWidget::DrawPadlock`
  (`Casso/Ui/Chrome/DriveWidget.cpp:107`, drawn at `:462` and `:892`).
- **Scope it to CRC mismatch only.** Do **not** trigger on "this file has
  metadata we would lose" — 10 of 11 demo WOZs carry META, so that condition
  makes essentially every real preservation dump read-only, which reads to users
  as "WOZ support broke."
- Write protection is **guest-visible** — it feeds the write-protect sense line
  at `Disk2Controller.cpp:329` and the engine at `Disk2NibbleEngine.cpp:310,341`,
  not just the flush gate at `DiskImageStore.cpp:301`. That is accepted: telling
  the guest the truth beats silently discarding its writes.
- The flush gate currently calls `ClearDirty()` before bailing for protected
  images. For this state, that is the moment to speak up rather than stay quiet.

---

## 5. GH #115 — `Denibblize` early-stop (not started)

`NibblizationLayer::Denibblize` (`NibblizationLayer.cpp:766`) `break`s on the
first sector that will not decode, leaves the rest of that track as the zeros
from `out.assign (kImageByteSize, 0)`, and returns **S_OK**. `FlushEntry` then
writes that buffer over the user's file. Affects `.dsk` / `.do` / `.po` only;
WOZ takes the `WozLoader::Serialize` path.

**Note the atomic-write fix does not help here.** It guarantees the file is
replaced only by bytes that were written completely — it says nothing about
whether those bytes are correct. Same corruption, delivered more dependably.

**Coordination — DECIDED:** build the per-track decode-report API on `master`;
**merge master into 020** (not rebase). 020's FR-017 needs the same mechanism and
should consume it rather than growing a parallel one.

Related, unresolved: Karateka, Choplifter, Space Quarks, Lode Runner and Carmen
Sandiego side A all report **0 tracks decoding as standard 16-sector data**,
while Karateka demonstrably boots. Possibly the same early-stop defect
under-reporting. Worth checking in the same pass.

---

## 6. Repository cleanup — after item 2 lands

`Apple2/Demos/The Print Shop Color side A.woz` in the tree is the damaged copy.
**The intact original is in git history and needs no re-import.**

```powershell
git cat-file blob 61f89c9d:"Apple2/Demos/The Print Shop Color side A.woz" `
  > "Apple2/Demos/The Print Shop Color side A.woz"
```

History: added intact at `61f89c9d` (234,757 bytes), dropped at `e55e0b7e`,
re-added already damaged at `6c4948eb` (234,496 bytes). The damage happened
between those two commits, outside version control.

**Restore only after the writer preserves metadata**, or the next flush
re-damages it.

**Restoring also reverts two tracks of genuine guest writes.** Round-tripping
the recovered original through the current writer differs from the committed
damaged file by 715 bytes: 4 CRC bytes plus two contiguous runs — 356 bytes at
offset 116787 (TRKS slot **17**) and 355 bytes at 136195 (slot **20**). The
writer is bit-faithful (`orig[256:] == roundtrip[256:]` is `True`), so those runs
are guest writes — Print Shop storing its configured printer/interface details on
the disk. Discarding them is right for a preservation dump, but it is a discard,
not a pure metadata repair. Say so in the commit.

Afterwards, re-scan for other Casso-written files — `creator='Casso'` in INFO is
the signature. Currently side A is the only hit across all 11 demo WOZs.
`UnitTest/Fixtures/copyprotected.woz` and `sample.woz` are **0 bytes** in this
checkout (an earlier note described them as hand-built WOZ1 fixtures with
`TRKS=0` — that does not match this tree; unresolved, low priority).

---

## 7. Tests

**Not yet written (all pass-today regression value):**

- **Structural invariant checker** over writer output, run across a table of
  image shapes (empty, 1 track, 35 tracks, half-track TMAP, gap slots, v1
  source): TRK records that do not overlap, every claimed block inside the file,
  `blockCount * 512 >= ceil(bitCount / 8)`, whole-block file size, `INFO == 60`,
  `TMAP == 160`.
- **Input the writer did not produce** — a hand-built v2 image with legal but
  unusual geometry (non-contiguous blocks, slots out of order, `INFO` longer than
  60 bytes). Follow the existing `BuildSyntheticV1` pattern in
  `WozLoaderTests.cpp`. The half asserting what we *do* preserve passes today;
  the byte-for-byte META/INFO half is the acceptance test for item 2.

**Acceptance tests owed by their fixes:**

- One undecodable sector costs **one sector**, not the rest of the track (#115).
- A `ForceFlush`-equivalent flag flip preserves every byte it does not own.

**Known gap, documented rather than papered over:** a write that fails *part
way* is not unit-testable without a filesystem seam in `DiskImageStore`. The
test section in `DiskImageStoreTests.cpp` says so explicitly.

### Why this class of bug survived — worth keeping in mind

Every WOZ test fed the writer's own output back through its own reader. A round
trip through one internal model proves the model is self-consistent, not that
the file is right. `Serialize_WritesValidHeaderCrc` was the single place with an
independent oracle (`Crc32Ref`), and it is the one field the bug did not touch.
The new guards use a **strict** chunk walker that refuses to stop at the first
non-chunk, because `Load`'s tolerance is precisely what hid the defect.

---

## 8. Issues to file

- **WOZ metadata loss** (item 2) — unfiled. Include the root-cause chain from
  `e068a980`, the INFO/META table above, and the note that `m_rawSourceBytes`
  already holds the source.
- **The `ForceFlush` toggle bug** (item 3) — unfiled. One menu click rewrites a
  preservation dump.

GH #115 already exists (open, `bug` / `priority: high` / `impact: user`).

---

## 9. Suggested order

1. **META/INFO retention** (item 2) — the actual data loss, no decisions pending.
2. **ForceFlush + `DiskImage::Flush()`** (item 3) — hard requirement; structural.
3. **Restore side A** (item 6) — gated on 1.
4. **Damaged-image write-protect + badge** (item 4).
5. **#115** (item 5).

Fold the invariant checker (item 7) into whichever WOZ change lands first — it is
the same test file.

---

## Appendix A — tools

`CassoCli disk` does **not** exist on master (that is spec 020, unreleased), so
drive the core entry points directly. Build Release, then compile against
`x64/Release/CassoEmuCore.lib` + `CassoCore.lib`.

```cpp
// wozrepro.cpp -- load a WOZ and re-serialize it, exactly as a flush does.
#include "Pch.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/WozLoader.h"

int main (int argc, char ** argv)
{
    vector<Byte>  raw;
    vector<Byte>  outBytes;
    DiskImage     img;

    {
        ifstream    file (argv[1], ios::binary | ios::ate);
        streamsize  size = file.tellg();
        file.seekg (0);
        raw.resize (static_cast<size_t> (size));
        file.read (reinterpret_cast<char *> (raw.data()), size);
    }

    printf ("WozLoad   hr=0x%08X  tracks=%d  wp=%d\n",
            (unsigned) WozLoader::Load (raw, img),
            img.GetTrackCount(), img.IsWriteProtected() ? 1 : 0);
    printf ("WozSerial hr=0x%08X  bytes=%zu\n",
            (unsigned) WozLoader::Serialize (img, outBytes), outBytes.size());

    FILE * f = fopen (argv[2], "wb");
    fwrite (outBytes.data(), 1, outBytes.size(), f);
    fclose (f);
    return 0;
}
```

Note `DiskImage::Load(path)` is the **legacy DSK-only** path — it reads the first
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

A healthy image walks cleanly to `(end of file)`. A Casso-written one stops at
offset 1536 — that is the signature of the bug fixed in `1a3beb1e`, and the
regression guard for it.

---

## Appendix B — build and gate commands

```powershell
scripts\Build.ps1 -Configuration Release -Platform Auto -Target Build
scripts\RunTests.ps1 -Configuration Release -Build
scripts\RunTests.ps1 -Configuration Debug -Build        # the pre-merge gate
scripts\Build.ps1 -Configuration Debug -Platform Auto -Target Build -RunCodeAnalysis
scripts\CheckStyle.ps1 -Mode Tree
```

Two traps worth knowing:

- **`RunTests.ps1` refuses to run a stale assembly.** Editing `CassoCore/Version.h`
  trips the guard even though no test links it (it is in no Pch and compiles only
  into `Casso.exe` / `CassoCli.exe`). `-AllowStale` is the intended escape hatch
  for exactly that case. A build that "succeeds" in ~1 second is a skipped build.
- **The pre-push style gate rejects Claude attribution in commit messages**
  (`CS0008`). Do not add a `Co-Authored-By: Claude` trailer — the push is
  refused. Also `CS0011` (a condition that calls a function — hoist it into a
  named local first) and `CS0016` (exactly three blank lines after a declaration
  block) bite easily.

---

## Appendix C — unrelated open item

Shift+Enter does not insert a newline in Claude Code on `RELMER-SLS2`; it
submits. `Ctrl+J` works and is the documented default for `chat:newline`.

Measured, not assumed: with a raw-stdin reader in a separate WT tab, Windows
Terminal 1.24.11911.0 ignores the kitty protocol (`CSI >1u` gives bare `0d` for
Shift+Enter, identical to the no-protocol baseline) and reports modifiers only
under win32-input-mode (`CSI ?9001h`), where Shift+Enter arrives as
`ESC[13;28;13;1;16;1_` — `ctrlState=16` versus `0` for plain Enter.

`RELMER-DESKTOP` works with the **same** WT version, same Windows build, no
Windows Terminal `shift+enter` binding, and Claude Code 2.1.234. The only
difference found was a `~/.claude/keybindings.json` binding `shift+enter` →
`chat:newline`. Replicating that file on `RELMER-SLS2` and restarting did **not**
fix it. Not resolved; not a Casso issue. The known-good workaround, if wanted, is
a Windows Terminal `actions` entry:

```json
{ "command": { "action": "sendInput", "input": "\n" }, "keys": "shift+enter" }
```

`RELMER-SLS2` was left in stock configuration: Claude Code 2.1.234,
auto-updates enabled, no `keybindings.json`, Windows Terminal settings untouched.
