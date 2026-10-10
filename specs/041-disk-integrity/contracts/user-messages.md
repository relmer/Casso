# Contract: user messages

**Status: every text below is approved by the owner on 2026-10-09 (tasks.md
T005).** This is every text the user can read that this work adds or changes.
All UI text is sentence case. `<path>` is an image's full path, `<file>` its
file name, `<copy file name>` the file name only of a recovery copy (for
example `Game.recovered.woz`), `<reason>` is `ChangePrompt::DescribeError`'s
text for the failure, `<n>` is the drive number and `<ext>` the image's format
extension. Line breaks inside a text are blank lines in the dialog.

## 1. Unchanged, now true

`DiskImageStore::FormatFlushLossMessage` keeps its wording for a disk that
stays in the drive. Its last sentence, "Your recent writes have not been
saved. The disk in the drive still has them.", was false on re-insert, reset,
power cycle, machine switch, eject and quit, which discarded the writes. After
FR-010 a disk cannot leave the drive with unsaved writes, so the sentence holds
wherever it is used; the eject and quit cases get their own tails (section 2).

## 2. The save-failure notice (`FormatFlushLossMessage`)

Every case shares the existing head, except the one marked below:

> Casso could not save changes to the disk image:
>
> `<path>`
>
> Error: `<reason>`
>
> The file on disk is unchanged.

| Case | Tail after the head |
|---|---|
| A recovery copy was written | The existing text at `DiskImageStore.cpp:903-905` (unchanged) |
| The format cannot hold the writes, and the recovery copy failed | No "Error: `<reason>`" line in the head. The tail is: Its changes cannot be stored as a `<ext>` image. Casso tried to save them to `<copy file name>` instead, but failed: `<reason>`. |
| A save nobody asked for, or a user save | Your recent writes have not been saved. The disk in the drive still has them. (unchanged) |
| An eject with no question possible (no question sink, the post failed, or a question is already open) | The disk was not ejected, and the drive still has your changes. |
| Quit, when no rescue happened | Casso is closing, so these changes cannot be kept. |

A save just before a mount (`FlushMoment::Replacing`, the save of the disk
being replaced) raises none of these tails except the recovery-copy one: it
notifies only for a first recovery copy, and when its failure declines the
mount, the mount's own text reports it (section 5), unless this mount has
already reported a save failure of either kind (`MountDiagnosis::ShouldReport`,
which reads both report fields). So one failed save gets one text:

- A re-insert or a reset over the disk's own file: section 5's same-file text.
  For a disk whose format cannot hold the writes and that has no current
  copy, the save writes a first recovery copy instead and shows the
  recovery-copy tail above, and the decline that follows is silent.
- A power cycle or a machine switch: each saves every disk at `Background`
  before its remount (`MachineHost::PowerCycle`'s `FlushAllUnlessHeld`; the
  switch's `FlushAll`), so the text is this table's tail for a save nobody
  asked for, or the recovery-copy tail, and the remount's decline is silent
  because that save has already been reported.
- Nothing at all, when the failure was already reported for this mount
  (section 11).

## 3. The preserve-failure notice (`FormatPreserveFailureMessage`)

Replaces `FormatDiscardedWritesMessage`. Shown when another program changed the
file and saving the guest's version to a separate file failed. There is no
separate "Error:" line; the reason is inline. The first three paragraphs are
the same for both cases:

> Casso could not save your changes to the disk image:
>
> `<path>`
>
> Another program changed this file after it was inserted, so Casso did not
> overwrite it. Casso tried to save your changes to `<copy file name>`
> instead, but failed: `<reason>`.

Then, by case:

- **Ejecting**:

  > The disk is still in drive `<n>` with your changes. Eject it again to
  > choose another place to save them. The file on disk has the other
  > program's version.

- **Quit**:

  > Casso is closing, so your changes are lost. The file on disk has the other
  > program's version.

## 4. The eject question (`ChangePrompt::ComposeSaveFailure`)

Raised when an eject's save fails and no current recovery copy exists. The
eject waits for the answer.

- **Title**: Disk not saved
- **Body, `SaveFailureCause::Unwritable`**:

  > Your changes to `<file>` could not be saved to
  >
  > `<path>`
  >
  > Error: `<reason>`
  >
  > The disk is still in drive `<n>`. Save the changes to another file before
  > ejecting, or eject the disk and discard them.

- **Body, `SaveFailureCause::Unserializable`**:

  > Your changes to `<file>` cannot be stored as a `<ext>` image. Casso tried
  > to save them to `<copy file name>` instead, but failed: `<reason>`.
  >
  > The disk is still in drive `<n>`. Save a WOZ copy to another file before
  > ejecting, or eject the disk and discard the changes.

- **Answers**: **Save as...** (the safe answer, for Enter and the close box)
  and **Eject and discard**. For the unserializable case the file picker opens
  on `<file>.recovered.woz` with WOZ listed first; the picker's labels are
  unchanged.

## 5. A mount declined (`FormatMountFailureMessage`, `MountDiagnosis::Describe`)

- **`MountFailure::UnsavedWrites`, another file** (an insert or a salvage
  insert):

  > Casso did not insert this disk image:
  >
  > `<new path>`
  >
  > The disk already in drive `<n>` has changes that could not be saved to its
  > file:
  >
  > `<kept path>`
  >
  > Error: `<reason>`
  >
  > That disk is still in the drive with its changes. Eject it to save them to
  > another file.

  When no save was attempted (`saveError` is `S_OK`), the middle paragraph
  reads "has changes that have not been saved to its file yet:" and the Error
  paragraph is left out.

- **`MountFailure::UnsavedWrites`, the same file** (a re-insert, reset, power
  cycle or machine-switch remount):

  > Casso could not save changes to the disk image:
  >
  > `<path>`
  >
  > Error: `<reason>`
  >
  > The file on disk is unchanged, and the disk was not read from it again. The
  > drive still has your changes.

- **Any other decline over an occupied drive**: the existing text, followed by
  a new paragraph, "The disk already in the drive was not changed."
- **`MountDiagnosis::Describe`, `UnsavedWrites`**: "was not inserted, because
  the disk has changes that could not be saved"
- **`MountDiagnosis::Describe`, `BehindLive`**: "was not inserted, because the
  machine was behind live in its history when the insert ran"

## 6. An insert over a file another program changed (`ChangePrompt::ComposeReplacedReport`)

Shown when, at the save before an insert of another file, the disk's own file
had been changed by another program, so Casso kept the guest's version in a
new file and then inserted the new disk.

> Another program changed `<file>` while drive `<n>` had unsaved changes to it.
> Casso saved your version to
>
> `<moved-to path>`
>
> and then inserted
>
> `<new path>`
>
> The original file keeps the other program's version.

## 7. A state load declined (`MachineStateFile::SaveDisksFirst`)

    Error: disk not saved
           A disk in a drive has changes that could not be saved, so the state
           was not loaded.

## 8. Salvage

- **The disk changed before the copy was written** (`ERROR_MEDIA_CHANGED`):

  > Casso did not write the salvaged copy, because the disk in drive `<n>`
  > changed after it was assessed. Salvage again to copy the disk now in the
  > drive.

- The salvage dialog, the result, the failure dialog and the Insert / Not now
  question keep their wording; they are now shown from `WM_APP_SALVAGE_DONE`.

## 9. A command that reached the machine behind live

- **Eject**:

  > Casso did not eject the disk in drive `<n>`, because the machine was behind
  > live in its history when the eject ran. Go live and try again.

- **Write protection** (Settings and the Disk menu):

  > Casso did not change write protection for drive `<n>`, because the machine
  > was behind live in its history when the change ran. Go live and try again.

- **Insert**: the mount failure text with the `BehindLive` description
  (section 5).
- A write-protect command whose value equals the drive's current setting
  raises no notice, behind live or not: Settings sends the command for both
  drives on every apply, changed or not (`Ui/Settings/SettingsPanelState.cpp:1054-1057`),
  and an apply that only changed a color is not a write-protect change.
- "Go live" is the term, matching the history band's link
  (`Ui/Debugger/HistoryBand.h:68`).

## 10. The command-line tools (`DiskImageSession`)

No message for a metadata copy failure. Copying the image's attributes,
creation time and permissions to the new version is best-effort: when
`CopyFileMetadata` fails, nothing is shown and the commit goes ahead with the
flush and the replace, so the new version may lose hidden or system flags or
custom permissions. This holds for every `DurableCommit` caller.

The temporary-write and replace failures keep their existing texts.

## 11. Changed cadence, not wording

A save failure nobody asked for (motor spin-down, the idle pick-up, a reset,
power cycle or machine switch) is reported once per mount, until a save
succeeds or the disk leaves the drive. A failure during a save the user asked
for (an explicit flush, a write-protect change) and during an insert or eject
always reports, an insert's through the mount's decline text (section 5). A same-file remount whose failure was already reported for
this mount stays silent (`MountDiagnosis::ShouldReport`).

## 12. Unchanged texts with a new source

- The damaged-disk report is built from the published `BayStatus`
  (`DamagedMountReport::FormatBody (writeProtect, file name)`) and must read
  exactly as the text built from the image does today (tasks.md T044).
- The other mount failure texts, the external-change questions and the rescue
  question are unchanged.

## 13. An update installed behind live (`DeploySave`)

Every update install returns the machine to the live end before its save
(owner confirmed 2026-10-09). The notice is shown when the disks were not, or
will not be, saved at the live end the machine had: the machine was still
behind live after the return (it stopped short at a gap or ran out of time,
or a later command moved the machine back), or the return to live diverged
and history was cut at its last good keyframe (FR-019;
`DeploySave::IsSavedBehindLive`). It is shown before an MSIX update
installed at once goes ahead, judged once the emulation thread is held for
the save, and before a zip update installed now closes Casso, judged right
after the return:

> Casso could not go live before installing the update, so each disk was saved
> as it was at the point in history you had stepped back to. Changes made after
> that point are not in the saved disks.

When the disks are saved at the live end, nothing is shown, as the owner
decided on 2026-10-08, including when the request's own answer arrived late
or the machine was already live when the request ran. An update left until
Casso closes also returns to live first, but shows no notice: its return runs
while the main window is being destroyed (`OnDestroy`), so there is no
window left to show it in.

## Considered and not added

- `MountFailure::AwaitingAnswer` and its sentence ("Drive 1 is waiting for an
  answer about the disk in it.", audit.md entry-flags-race Part B): R2's
  decline already keeps the disk when an insert arrives while an eject waits
  on its answer (tasks.md T088), so the value is not part of the design.
