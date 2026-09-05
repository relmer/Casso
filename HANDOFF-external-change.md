# Handoff: the external-change subsystem, eight findings and a simplification

Continue on branch `fix-ignored-question-reservation` in the Casso repo. It
carries two gated, unmerged commits that this work builds on and partly
replaces. Nothing but git crosses between machines; fetch, do not look for
local files.

```
git fetch origin
git checkout fix-ignored-question-reservation      # at 2552903f, or later
```

Read `.github/copilot-instructions.md` first. Then these, which cost real time
when learned cold:

- American spelling. Commit subjects take a scope (`fix(disk): ...`), state the
  defect in the past tense with the cause as the grammatical subject, and carry
  **no Claude attribution** -- the repo rule overrides any session instruction
  to add a trailer.
- Comments never cite specs, FRs or tasks. Helpers are class statics, not free
  functions. A new function is spliced in ahead of the next one's `////` banner,
  never ahead of its signature. Non-ASCII goes through `Dxui/Core/UnicodeSymbols.h`.
  Angle-bracket std includes live only in `Pch.h`.
- Source files are CRLF and CP1252. The Edit tool preserves both. A Python
  patch must open with `newline=''` and convert its pattern's `\n` to `\r\n`.
- `scripts/CheckStyle.ps1 -Mode Tree` before every push; the pre-push hook runs
  it too.
- Master takes a push from another session every few minutes. Expect to
  re-sync more than once. Only `CHANGELOG.md` has ever conflicted; keep both
  sides' entries.

## What this document is

The investigation is finished. Every finding below was verified by reading the
code named, and the two data-loss ones were reproduced in a test rig. **Do not
re-investigate.** Open the function named, at the line named, and make the
change described. Where a design choice remained, it has been made here; the
executor's judgment is needed for wording and style, not for what to build.

Line numbers are against `2552903f`. They will drift as batch A lands; the
function names will not.

## Vocabulary

- **bay** -- one drive slot, `DiskImageStore::Entry`. Holds the in-memory
  `DiskImage`, the `path` of the file it came from, and the per-mount record
  `sharedState` (`MountedImageState`).
- **reservation** -- `preservedPath`: a timestamped name such as
  `Loader.20260905-134000-01.dsk`, chosen when a question is put so the dialog
  can show the name the rescue file *will* have. `preservedWritten` is the
  separate fact that the file now exists.
- **question / answer** -- `m_askSink` raises a modal dialog
  (`EmulatorShell::AskAboutChange`); the click comes back as
  `ResolvePendingChange`. While it is up, `sharedState.IsAskOutstanding()` is
  true and the pending change is deliberately left standing.
- **report / banner** -- `m_reportSink` puts a non-modal strip over the picture
  (`EmulatorShell::ShowChangeBanner`).
- **repoint** -- `RepointBayToFile`: the drive keeps its in-memory disk but
  reads and writes a different file from now on.
- **the two conflict paths** -- `FlushEntry` (the guest is about to write and
  the file changed underneath it) and `ApplyPendingReloadToBay` (the watcher
  saw the file change while the guest has unsaved writes). Both preserve the
  guest's version first; that guarantee is sound and tested from both ends.

## Batch A -- the store

Files: `CassoEmuCore/Devices/Disk/DiskImageStore.{h,cpp}`,
`MountedImageState.{h,cpp}`, `ExternalChangePolicy.{h,cpp}`,
`UnitTest/EmuTests/SharedImageTests.cpp`. One gate run for the whole batch;
separate commits per item.

### A1. Move the reservation into `MountedImageState` (structural, do first)

**Why.** `preservedPath` and `preservedWritten` sit on `Entry`
(`DiskImageStore.h:395-396`). Every other per-mount field lives in
`MountedImageState`, whose `Mount()` and `Eject()` (`MountedImageState.cpp:15,
41`) reset it. Those two were the exception, and all three reservation bugs
fixed on this branch were that exception expressing itself. After the move,
the mount-time clear at `DiskImageStore.cpp:274-284` and the helper
`ReleaseUnwrittenReservation` (`.cpp:2991`, `.h:495`) become deletions.

**Do.** Add to `MountedImageState`: `m_preservedPath`, `m_preservedWritten`,
reset in both `Mount()` and `Eject()`; accessors `GetPreservedPath()`,
`SetPreservedPath()`, `IsPreservedWritten()`, `SetPreservedWritten()`,
`ClearPreserved()` (both), and `ReleaseUnwrittenReservation()` (path only,
and only when not written -- the existing helper's body and its comment, moved).
Replace every `entry.preservedPath` / `entry.preservedWritten` in
`DiskImageStore.cpp` (about 25 sites; the compiler finds them all). Where a
site binds `entry.preservedPath` as a `string &` out-parameter of
`SaveLoadedImage`, copy to a local first and `Set` afterwards -- `FlushEntry`
at `.cpp:828-836` already does exactly this with its `preservedPath` local.
Delete the `MountFromBytes` clear and its comment. Delete the static helper
from `.h` and `.cpp`.

**Test.** No new test. The three existing reservation tests
(`AnIgnoredQuestionDoesNotNameTheNextCopyAfterIt`,
`ADismissedSaveFailureDoesNotNameTheNextCopyAfterIt`,
`AReservationDoesNotOutliveTheDiskItWasMadeFor`) must still pass; they are the
proof that the move changed no behavior.

### A2. Gate both entry points on "a question is outstanding"

**Why.** Four findings share one cause: something proceeds while a question is
on screen.

- A stale answer acts on the wrong disk. `ResolvePendingChange`
  (`.cpp:2275`) checks `mounted && image` but never `IsAskOutstanding()`, even
  though `Mount()`/`Eject()` reset that flag at exactly the moment the disk
  changes. Reproduced: answer Keep after a re-mount and, on master, the bay is
  repointed onto the rescue file, which the next flush then overwrites --
  **the rescued data is destroyed**. (This branch's mount clear reduces that
  to a redundant extra file; the gate removes it.)
- A stated intent swaps the disk under the dialog. `ApplyPendingReloadToBay`
  applies `ReloadInPlace`/`Restart` directly (`NeedsAnAnswer` is false for
  them) while the question stands; the dialog's "keep your current version"
  then means CassoCli's version.
- The whole image is re-read on every idle tick while a question stands. The
  `IsAskOutstanding()` guard sits at `.cpp:2175` inside the `NeedsAnAnswer`
  branch -- *after* `ReadIdentity` (`.cpp:2100`), `ReadImageFile` (`.cpp:2115`)
  and a throwaway trial load. Pending stays set by design while the user
  reads, so that is sixty full reads and nibblizations a second until they
  answer.

**Do.** Two lines.

```cpp
// ApplyPendingReloadToBay, immediately after `if (!settled) return;` (.cpp:2086)
if (entry.sharedState.IsAskOutstanding())
{
    return;
}

// ResolvePendingChange, first statement inside the `Entry & entry` block,
// BEFORE SetAskOutstanding (false) at .cpp:2291
if (!entry.sharedState.IsAskOutstanding())
{
    return;
}
```

**Decided semantics, state them in the comment:** while a question stands, the
user owns the bay. Every later change -- stated intent included -- waits.
`NoteChange` keeps refreshing the pending record, and `ResolvePendingChange`
already re-reads the file when the answer comes ("the bytes are read again
here rather than kept from the question"), so the answer lands on the newest
bytes and nothing is lost. One consequence to write down: a `--on-change
reboot` arriving under an open question reloads without rebooting once
answered, because the answer's `CarryOutChangeAction` consumes the record.
That is accepted.

**Tests** (in `SharedImageTests`, using the existing `Rig`):

- `AnAnswerToAQuestionAboutADiskThatHasLeftIsDropped` -- mount, unexplained
  change, question up; `rig.store.Eject` then `Mount` again;
  `ResolvePendingChange (KeepHeld)`; assert `reports.size() == 0`,
  `PreservedPaths().size() == 0`, and `GetSourcePath` is still `kImagePath`.
  Prove it: comment out the `ResolvePendingChange` gate, watch it fail.
- `AStatedIntentWaitsWhileAQuestionStands` -- question up; `FireAndSettle
  (kImagePath, ExternalChangeIntent::ReloadInPlace)`; assert `reports.size()
  == 0` and the image's first track byte is unchanged; then answer
  `ReloadInPlace` and assert the newest bytes are what arrived.
- `AStandingQuestionDoesNotReReadTheImage` -- add `int reads = 0;` to `Rig`
  and `reads++` inside its `SetImageReader` lambda (`SharedImageTests.cpp:110`).
  Question up; record `reads`; `nowMs += 5000; store.ApplyPendingReload()`
  three times; assert `reads` unchanged. Prove it: move the gate back below
  `ReadImageFile`, watch the count climb.

### A3. `ChangeAction::Discard`, and the lost-file answers read as written

**Why.** `KeepHeld` means "keep the disk in the drive" in the change question
and "**throw it away**" in the lost-file question -- `ComposeLostFile` pushes
`{ L"Discard", ChangeAction::KeepHeld }` (`ChangePrompt.cpp:431`), and
`ResolvePendingChange`'s file-lost branch (`.cpp:2361-2400`) ejects on
anything that is not `PreserveCopy`. That inversion is what makes the branch
hard to read and is half of finding B1.

**Do.** Add `Discard` to `ChangeAction` (`ExternalChangePolicy.h:21`), with a
comment in the enum's register: an answer, not a decision; the user was told
the file is gone and chose to let the disk go. `ComposeLostFile` uses it. The
file-lost branch becomes: `PreserveCopy` with a path -> save and repoint;
`Discard` -> `EjectLostImage`; anything else is a coding error --
`CBRAEx (false, E_INVALIDARG)` -- because the shell can only post an answer
the prompt offered. Update `DecliningToSaveStillEmptiesTheDrive` and any other
test passing `KeepHeld` as the lost-file answer.

### A4. Lost-file conditions found while acting are asked, not reported

**Why.** `CarryOutChangeAction`'s fallback at `.cpp:2622` --
`ChangePrompt::Compose (original, drive, action)` -- composes the lost-file
prompt for `Unusable` (set at `.cpp:2487` when the real mount fails after the
trial load passed) and `Deleted` (set at `.cpp:2419` when the answer's re-read
fails), then hands it to `m_reportSink`. The banner's `SetOnAction`
(`EmulatorShell.cpp:13817-13835`) routes only `Restart`; every other button
just hides the strip. The user sees "Save as..." and it does nothing.

**Do.** In that fallback, if `ExternalChangePolicy::IsFileLost (action)` and
`m_askSink` is set: `SetAskOutstanding (true)`, `SetAskedAction (action)`,
`m_askSink (slot, drive, ChangePrompt::Compose (original, drive, action))`,
and do not touch `m_reportSink`. Pending was already cleared above
(`.cpp:2557`), so there is no re-ask loop.

**Test.** `ALostFileFoundWhileActingIsAskedNotReported` -- question up;
remove the file the way `ADeletedImageAndAnUnreadableOneAreDistinguished`
does; answer `ReloadInPlace`; assert `questions.size() == 2` and
`reports.size() == 0`, and that the second question's `answers` offer
`PreserveCopy` and `Discard`.

### A5. Delete what is dead

Each is its own small commit; grep before each deletion and let the compiler
confirm.

- `PreserveGivenBytes` (`.cpp:3045-3090` including its banner, `.h:507`). No
  callers anywhere.
- `reportStanding` (`MountedImageState.h:98-105`, field at `:142`; written at
  `DiskImageStore.cpp:2627` and in `ClearChangeReport` `.cpp:2706`). Never
  read. Its comment claims it absorbs later changes and keeps the restart
  reachable; nothing does either with it. `ClearChangeReport` then has an
  empty body: delete it, its `.h` declaration, and its two shell callers
  (`EmulatorShell.cpp:13892`, `:14319`), along with `m_changeBannerDrive` and
  the hardcoded slot 6.
- `Situation::heldByOther` (`ExternalChangePolicy.h:117`) and the `Decide`
  branch at `ExternalChangePolicy.cpp:43`. It is hardcoded `false` at
  `DiskImageStore.cpp:2132`; the hold check happens before `Decide` and
  returns. Update `AFileSomebodyElseHoldsOutranksEverything` and the sweep
  `EveryOutcomeThePolicyCanReachIsReachedByAKnownSituation`. Delete
  `ChangeAction::Defer` and the `case ChangeAction::Defer:` at `.cpp:2535`
  **only if** a repo-wide grep shows the policy was its last user.
- The `ImageIdentity.h:31` comment. `modifiedUnix` is
  `file_time_type::time_since_epoch().count()` (`ImageIdentity.cpp:79`) --
  100 ns ticks on NTFS, 2 s on FAT32 -- so say that instead of "the
  filesystem's timestamp resolution". Leave the field name; renaming crosses
  into CassoCli for no behavior.

## Batch B -- the prompt and the shell

Files: `CassoEmuCore/Devices/Disk/ChangePrompt.{h,cpp}`,
`Casso/EmulatorShell.{h,cpp}`, `Dxui/Window/IDxuiHostClient.h`,
`Dxui/Window/DxuiHwndSource.cpp`. The shell has no unit tests for this; the
gate is the build plus one launch to see a question dialog appear
(`--machine Apple2e --disk1 <scratch copy of a demo disk>`, overwrite the file
in place, wait one second).

### B1. Enter and the close box on the lost-file dialog destroy the only copy

**Why.** `AskAboutChange` (`EmulatorShell.cpp:14038`) makes the *last* answer
both the default button and the close-box result (`:14068`), on the stated
rule that dismissing must not act. For `ComposeLostFile` the last answer is
Discard, which ejects the in-memory disk -- and the file is gone, so that disk
is the only copy. Enter, or the X, throws it away.

**Do.** Add `size_t safeAnswer = 0;` to `ChangePrompt` (`ChangePrompt.h:115`):
the index of the answer the default button and the close box both take. Each
composer sets it:

| Prompt | Answers | `safeAnswer` |
|---|---|---|
| `Compose (Ask)` | Insert / Keep | 1, Keep |
| `ComposeSaveFailure` | Save as... / Dismiss | 1, Dismiss |
| `ComposeLostFile` | Save as... / Discard | **0, Save as...** |

`AskAboutChange` reads `safeAnswer` for `isDefault`, `isCancel` and
`closeBoxResult` in place of `isLast`. **Decided:** the lost-file dialog is
deliberately sticky. The X opens the picker; cancelling the picker returns to
the question (already implemented, `:14076-14098`); the only ways out are a
completed save or an explicit click on Discard. That is correct for "the only
copy of your disk is in memory," and the comment should say so.

Add a policy-test asserting each composer's `safeAnswer` names a
non-destructive action -- for the lost-file prompt, that
`answers[safeAnswer].action == PreserveCopy`.

### B2. Any modal dialog silently drops questions, notices and flush alerts

**Why.** `WM_APP_CHANGE_ASK`, `WM_APP_CHANGE_REPORT`, `WM_APP_NOTIFY_USER` and
`WM_APP_DXUI_UPDATE_TITLE` are handled only by `RunMessageLoop`'s pre-dispatch
checks (`EmulatorShell.cpp:7460-7510`). `DxuiWindow::ShowModalDialog`
(`DxuiWindow.cpp:214`) runs its own `GetMessageW (nullptr)` /
`DispatchMessageW` pump (`:240`), and the shell's window procedure has no case
for any of the four -- `DxuiHwndSource::DispatchClientMessage`
(`DxuiHwndSource.cpp:2513`) is a per-message switch with no `WM_APP` range and
`IDxuiHostClient` has no generic virtual. So while Settings, a picker, a
notification or another change question is open, the message reaches
`DefWindowProc`, the heap payload leaks, and the user is never told. Because
the store set `askOutstanding` before posting, it never re-asks either: the
bay is stuck with a pending change until the disk is ejected. A flush-loss
alert posted during any dialog vanishes the same way.

**Do.** One virtual and one case.

- `IDxuiHostClient.h`: `virtual DxuiMessageResult OnAppMessage (UINT msg,
  WPARAM wp, LPARAM lp) { return DxuiMessageResult::NotHandled; }`, documented
  as the route for `WM_APP`-range messages a client posts to its own window.
- `DxuiHwndSource::DispatchClientMessage`: before the switch, `if (msg >=
  WM_APP && msg <= 0xBFFF) { isHandled = IsClaimed (m_client->OnAppMessage
  (msg, wp, lp), RepaintOnClaim::No); }` -- match how `WM_COPYDATA` is
  dispatched at `:2535`.
- `EmulatorShell`: override `OnAppMessage`; move the four handler bodies out
  of `RunMessageLoop` into it, verbatim, and delete the four `if
  (msg.message == ...) { ...; continue; }` blocks. `DispatchMessage` now
  delivers them from every pump. A disabled owner window still receives posted
  messages, so a question arriving under a modal opens as a nested modal --
  ordinary Win32, and acceptable.

Verify by launch: open Settings, overwrite the mounted disk in place, wait one
second, close Settings; the question must be on screen. Before the fix it is
not, and never will be for that bay.

### B3. Banner cleanup

With `reportStanding` gone (A5), delete the banner's `Restart` branch
(`EmulatorShell.cpp:13829-13832`; no prompt offers that answer) and
`m_changeBannerDrive`. The banner then has one job: show the text, hide on
any click or when the countdown ends.

## Batch C -- decisions, not fixes. Ask before building.

- **One file in both drives.** `MountDiskInSlot6` (`DiskManager.cpp:415`) and
  the store have no guard. Two independent in-memory copies diverge silently,
  each flush overwrites the other's, and each external change yields two
  conflicts, two copies, two sequential dialogs. Refuse the second mount, or
  warn and allow? Refusing is a one-line `CBR` with a message; allowing means
  sharing one `DiskImage` between bays, which is a different feature.
- **Preserve-failure has two faces.** Found by the watcher it is a dialog with
  Save as... (`ApplyPendingReloadToBay` `.cpp:2154-2183`); found by the flush
  it is an `EhmNotifyUser` MessageBox with no recovery (`FlushEntry`
  `.cpp:842`). Route the flush-side failure through the same
  `ComposeSaveFailure` question? It would need the ask sink reachable from
  `FlushEntry`, which it is (`m_askSink` is a member).

## Order of work and commits

1. A1 (one commit), A2 (one commit, three tests), A3, A4, A5 (one commit per
   deletion). Gate. Push.
2. B1, B2, B3 (one commit each). Gate. Launch-verify B2. Push.
3. `/code-review` at **high**, once. Fix what it finds.
4. `CHANGELOG.md` under `## [Unreleased]` / `### Fixed`: one entry per
   user-visible fix (A2 stale-answer and swap-under-dialog, A4, B1, B2). Net
   effect, not the path. Deletions and the field move get no entry.
5. Re-sync master into the branch. Gate again if anything but `CHANGELOG.md`
   moved under `CassoEmuCore/Devices/Disk/` or `Casso/`.
6. **Delete this file.** `git rm HANDOFF-external-change.md`, one commit,
   subject `docs(disk): retire the external-change handoff`. This is the last
   commit on the branch and it happens **only after** steps 1-5 are complete
   and green. Do not delete it earlier; it is the record the executor works
   from until the merge.
7. Merge to master with `--no-ff` from the primary worktree -- master is
   checked out only there; use `git -C C:/Users/relmer/source/repos/relmer/Casso`
   rather than `cd`. Subject `merge(disk): ...`. Fast-forward this worktree to
   the merge commit and run CheckStyle, Debug and Release against that exact
   tree before pushing master. Push. `gh run watch <id> --exit-status` takes
   longer than ten minutes; run it in the background and confirm all six jobs.

## Testing the store: what the Rig gives you

`SharedImageTests.cpp:77`. Nothing touches the filesystem.

- `rig.WriteImage (path, fill)` puts an image "on disk" with every byte `fill`
  and stamps a fresh identity. `rig.files[path][0]` reads it back.
- `rig.FireAndSettle (path, intent = Unstated)` fires the watcher, optionally
  states an intent, advances the clock past the quiet period and pumps the
  store once.
- `rig.wallClock` (`:88`) drives the timestamp in preserved names; advance it
  to get a different stamp. `rig.nowMs` is the settle clock.
- `rig.refusePreserve = true` (`:93`) makes the next preserved write fail.
- `rig.questions`, `rig.reports`, `rig.PreservedPaths()` (`:194`),
  `rig.store.GetSourcePath (kSlot, kDrive)`.
- Dirty the guest: `GetImage(...)->GetTrackBitsForWrite (0)[0] = 0x7F;
  GetImage(...)->SetLoadedForTest (true, true);` then `rig.store.Flush`.
- **The flush sink treats any path other than `kImagePath` as a preserved
  copy** (`:126`). Never mount a second image path in a test; re-mount
  `kImagePath` instead.
- To see state without asserting, write a temporary `TEST_METHOD
  (ZZProbe...)` that calls `Logger::WriteMessage` per line, run it with
  `-Filter`, then `git checkout -- UnitTest/EmuTests/SharedImageTests.cpp`.

Prove every data-loss fix against its own defect: revert only that fix,
confirm only its test fails, restore. Skip the proof for deletions.

## Gates

```powershell
scripts\CheckStyle.ps1 -Mode Tree
scripts\RunTests.ps1 -Build -Configuration Debug
scripts\RunTests.ps1 -Build -Configuration Release
scripts\RunTests.ps1 -Configuration Release -Scenario
scripts\Build.ps1 -RunCodeAnalysis -Configuration Release -Target Rebuild
```

The scenario suite is required: `CassoEmuCore/Devices/Disk/` is on its trigger
list and CI does not run it. Code analysis must be a clean rebuild. Use
`-Filter <name|name>` and `| Select-Object -Last N` during development; the
full run once per batch. At `2552903f` on relmer-desktop: CheckStyle 1285 OK,
Debug 4779, Release 4777, scenario 22, code analysis 0 warnings.

No CPU or assembler code changes, so Dormann and Harte do not apply. ARM64 is
build-only.
