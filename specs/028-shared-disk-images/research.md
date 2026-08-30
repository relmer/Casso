# Research: Disk Images Shared with a Running Emulator

**Feature**: `028-shared-disk-images` | **Date**: 2026-08-30

Everything below was read out of the tree or measured, not assumed. Where a
finding contradicts the spec as first written, the spec was corrected and the
correction is noted.

## Finding 1: the emulator holds no handle, and records nothing

**Measured.** With `Casso.exe --disk1 boot.dsk` running, `CassoCli as65
boot.a65 --disk boot.dsk --as OTHER.SYSTEM` returns 0 and changes 27 bytes of
the image. An exclusive `File.Open(path, Open, ReadWrite, None)` from a third
process also succeeds.

**Read.** `DiskImageStore::Entry` carries `image`, `path`, `format`, `mounted`
and `salvageOffered`. There is no file stamp, and the store calls `Stat` nowhere.
`IDiskFileIo::IsHeldByAnotherProcess` documents the consequence itself: it
"catches another TOOL, and cannot catch this emulator, which holds no handle on
a mounted image".

**Decision**: record an identity at mount on `Entry`, and re-check it before
every write.
**Rationale**: it is the only thing that can answer "did this change under me",
and the command-line side already proves the pattern works.
**Alternatives**: holding the file open for the whole mount would make the image
unwritable by anything else, which defeats the feature.

## Finding 2: the corruption case is already closed on both sides

**Read.** The emulator commits through `DiskImageStore::WriteFileAtomically`:
write a sibling temporary, verify the stream state after close, then
`fs::rename` over the target. Its comment states the intent -- "readers see
either the old file or the new one". The command line commits through
`CommitPlan::GetTemporaryPath` plus `IDiskFileIo::ReplaceAtomically`, the same
shape.

**Decision**: build no lock.
**Rationale**: a partly written image cannot be read today. The spec's original
User Story 3 asked to prevent something already prevented, and was rewritten.
**Alternatives**: a lock file would add staleness rules, cleanup-on-crash and a
new failure mode to buy a guarantee the tree already has.

**What IS still broken** is narrower and is Finding 3.

## Finding 3: two emulator instances collide on one temporary name

**Read.** `WriteFileAtomically` derives its temporary from a fixed suffix:

```cpp
constexpr const char *  kTempSuffix = ".casso-tmp";
string                  tempPath    = path + kTempSuffix;
```

Two Casso instances flushing the same image therefore write into the *same*
temporary, and one renames the other's bytes over the target as its own.

**The command line already solved exactly this for itself.**
`CommitPlan::GetTemporaryPath` takes an invocation tag, and its comment spells
out the failure the emulator still has: "Both invocations begin at attempt zero,
and both look before they leap, so both can see the name free and take it -- the
loser's bytes then go into the winner's temporary and are committed as though
they were the winner's."

**Decision**: give the emulator the same per-invocation distinctness.
**Rationale**: the reasoning is already written down in this repository; the
emulator simply never received it.
**Alternatives**: none seriously considered. Reusing `CommitPlan` outright is
the obvious route and the plan should try that before adding a second scheme.

## Finding 4: there is no file watcher anywhere in the tree

**Read.** No `ReadDirectoryChangesW`, `FindFirstChangeNotification` or
equivalent appears in any source file.

**Decision**: a new `IImageWatcher` seam in core, AND its `Win32ImageWatcher`
implementation in core too, mirroring `IDiskFileIo` / `Win32DiskFileIo` exactly
-- that shim lives in `CassoEmuCore/Devices/Disk/`, beside the store it serves, and not in an executable. An earlier
draft of this finding said "shim in the shell", which is the platform-boundary
reasoning the constitution deletes.
**Rationale**: Constitution Principle VI. A watcher in an exe is a watcher no
test can drive.
**Alternatives**: polling `Stat` on a timer needs no new platform code, but
either burns cycles at idle or reacts slowly, and SC-006 forbids a cost the user
can feel.

**Watch the DIRECTORY, not the file.** An atomic rename replaces the file, so a
handle-based watch on the image itself sees the rename as a delete. The
directory watch is what survives the very commit pattern both writers use.

## Finding 5: WM_COPYDATA is already allowed through the integrity filter

**Read.** `EmulatorShell::InstallDragDropTarget` already calls
`ChangeWindowMessageFilterEx (m_hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr)`,
because "when Casso runs at a HIGHER integrity level than the drag source [...]
UIPI silently drops the messages OLE uses" and the drop "simply does nothing
with no error anywhere".

That is the identical hazard the intent channel would face: an elevated Casso
and a normal-integrity `CassoCli` would see the message vanish silently.

**The mitigation is NOT unconditional, and an earlier draft of this finding said
it was.** `InstallDragDropTarget` runs only inside `if (m_fOleInitialized)`, so
where OLE initialization failed there is no filter at all. This feature installs
its own rather than inheriting one.

**Read.** The window class is `CassoWindow`, registered once, so enumerating
top-level windows by class finds every running emulator.

**Decision**: send `WM_COPYDATA` to every `CassoWindow` found by enumeration -- NOT `HWND_BROADCAST`, which `WM_COPYDATA` may not use -- each ignoring
paths it has not mounted.
**Rationale**: no discovery protocol, no files, and multi-instance falls out.
The integrity hazard that would otherwise make this silently unreliable is
already handled for a different feature.
**Alternatives**: a named pipe needs instance discovery and a handshake to
deliver metadata that is allowed to be lost.

## Finding 6: the pick-up point already exists

**Read.** `Disk2Controller::SetMotorOffFlushCallback` is "invoked on the CPU
thread at the exact moment the motor spins down [...] right after a disk
operation completes and ~1 second after the last access, a naturally debounced,
race-free point to persist dirty images (this thread owns the writes)".

That is precisely what FR-014 asks for, and the ~1 second debounce is a
head start on FR-013.

**Decision**: apply pending pick-ups from that callback OR from an idle tick on
the same thread, whichever comes first.

**THE CALLBACK ALONE IS NOT ENOUGH, and an earlier draft of this finding said it
was.** It fires only after a motor-on to motor-off transition with the spindown
timer expiring, so a guest sitting at a BASIC prompt -- which is how the build
loop is actually used -- never reaches it, and the headline scenario would never
fire.

**Rationale**: no new thread and no new synchronization either way; the thread
that owns disk writes is the thread that applies the swap.
**Alternatives**: applying from the watcher thread needs locking against the CPU
thread and can land mid-operation.

## Finding 7: the banner widget exists

**Read.** `Dxui/Widgets/DxuiInfoBanner` exists and is used by
`SalvageDialogContent` and `PrintingPage`. **Its own header says "not
clickable, no raised surface"**, so it cannot carry the restart action FR-010
requires, and nothing in the tree hosts a non-modal banner over the running
machine yet.

**Decision**: reuse the banner for the text, pair it with a `DxuiButtonRow` for
the action, and host it where `SalvageDialogContent` is already driven from.
**An earlier draft of this finding said the banner was "what it is for" and
concluded no alternatives were needed. That was wrong** -- it was read as a
name rather than as a widget with a documented limitation.

## Finding 8: a swap cannot be made safe, only chosen

Not a code finding -- a domain one, and the reason the design is shaped the way
it is.

Replacing a mounted disk's contents is a disk swap and carries every hazard of
one. The guest caches structure in its own RAM: DOS 3.3 holds the VTOC, ProDOS a
volume control block and an open file's index blocks. Swap underneath and the
guest's next write allocates against a map belonging to a disk that is gone.
ProDOS defends itself somewhat by checking the volume name on directory access;
DOS 3.3 does not.

None of that is visible from the disk layer. **So no gate the emulator can build
establishes that a swap is safe**, and the feature must not imply otherwise.

**Decision**: the writer states the intent and the emulator carries it out; a
restart is always available; the pick-up is reported rather than silent.
**Rationale**: the person writing the image knows whether they replaced a binary
to be run again or the program the disk boots. The emulator cannot know.
**Alternatives**: a heuristic gate on the dirty bit was in the first draft and
is wrong -- it says only that the guest has not written YET, and it is the write
AFTER the pick-up that corrupts.

## Open, for implementation rather than research

- **How long the quiet period of FR-013 is** -- SETTLED at 1 second, matching the
  spindown debounce, as a named constant so a real multi-command build can tune
  it in one edit.
- **Whether `CommitPlan` can be reused verbatim** by the emulator for Finding 3,
  or whether its invocation-tag source is CLI-specific.
