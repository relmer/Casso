# Contract: user messages

**Status: draft. Every text below needs the owner's approval (tasks.md T005)
before a task composes it or a test asserts it.** This is every text the user
can read that this work adds or changes, collected from research.md R2 and R3
and from audit.md. All UI text is sentence case. `<path>` is an image's full
path, `<file>` its file name, `<reason>` is `ChangePrompt::DescribeError`'s
text for the failure, `<n>` is the drive number and `<ext>` the image's format
extension. Line breaks inside a text are blank lines in the dialog.

Where research.md R2 and an earlier draft of this file disagreed, the text
below is the one settled for approval, and R2's message table follows it.

On 2026-10-08 the owner was shown three texts from that earlier draft (a
change declined for unsaved writes, the eject question, an eject declined)
and asked for no changes. Each of the three is marked where it appears below,
with the wording shown and how the text here differs; the full list still
needs sign-off under T005.

## 1. Unchanged, now true

`DiskImageStore::FormatFlushLossMessage` keeps its wording for a disk that
stays in the drive. Its last sentence, "Your recent writes have not been
saved. The disk in the drive still has them.", was false on re-insert, reset,
power cycle, machine switch, eject and quit, which discarded the writes. After
FR-010 a disk cannot leave the drive with unsaved writes, so the sentence holds
wherever it is used; the eject and quit cases get their own tails (section 2).

## 2. The save-failure notice (`FormatFlushLossMessage`)

Every case shares the existing head:

> Casso could not save changes to the disk image:
>
> `<path>`
>
> Error: `<reason>`
>
> The file on disk is unchanged.

| Case | Tail after the head | Status |
|---|---|---|
| A recovery copy was written | The existing text at `DiskImageStore.cpp:903-905` | Unchanged |
| The format cannot hold the writes, and the recovery copy failed (the Error line gives the copy's error) | Its changes cannot be stored as a `<ext>` image, and the complete copy Casso tried to write beside it could not be saved. | New |
| A save nobody asked for, or a user save | Your recent writes have not been saved. The disk in the drive still has them. | Unchanged |
| An eject with no question possible (no question sink, the post failed, or a question is already open) | The disk was not ejected, and the drive still has your changes. | New. Shown to the owner 2026-10-08, no changes asked; final sign-off with the full list (T005). The wording shown was the earlier draft below, which differs |
| Quit, when no rescue happened | Casso is closing, so these changes cannot be kept. | New |

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

The eject text shown to the owner on 2026-10-08 was an earlier draft, a
notice of its own rather than this table's tail:

> The disk was not ejected, because its changes could not be saved:
>
> `<path>`
>
> Error: `<reason>`

It differs from the text above in its opening sentence (above, the shared
head "Casso could not save changes to the disk image:" comes first and the
tail ends the notice), in having no "The file on disk is unchanged." line,
and in ending without the tail's "and the drive still has your changes".

## 3. The preserve-failure notice (`FormatPreserveFailureMessage`)

Replaces `FormatDiscardedWritesMessage`. Its head is unchanged.

| Case | Tail | Status |
|---|---|---|
| Ejecting | The disk was not ejected, and the drive still has your changes. The file on disk keeps the other program's version. | New |
| Quit | Casso is closing, so these changes cannot be recovered. The file on disk keeps the other program's version. | Replaces "The disk is leaving the drive, so these changes cannot be recovered." |

## 4. The eject question (`ChangePrompt::ComposeSaveFailure`)

Raised when an eject's save fails and no current recovery copy exists. The
eject waits for the answer.

Shown to the owner 2026-10-08, no changes asked; final sign-off with the full
list (T005). The wording shown was an earlier draft, with the same title and
answers and this body for every cause:

> The changes to this disk could not be saved:
>
> `<path>`
>
> Error: `<reason>`
>
> Save them to another file before ejecting, or eject the disk and discard
> them.

The `Unwritable` body below differs from it in its opening ("Your changes to
`<file>` could not be saved to" in place of "The changes to this disk could
not be saved:") and in its last paragraph, which first says "The disk is
still in drive `<n>`." and reads "Save the changes" for "Save them". The
`Unserializable` body is new since that draft.

- **Title**: Disk not saved
- **Body, `SaveFailureCause::Unwritable`** (new):

  > Your changes to `<file>` could not be saved to
  >
  > `<path>`
  >
  > Error: `<reason>`
  >
  > The disk is still in drive `<n>`. Save the changes to another file before
  > ejecting, or eject the disk and discard them.

- **Body, `SaveFailureCause::Unserializable`** (new):

  > Your changes to `<file>` cannot be stored as a `<ext>` image, and the
  > complete WOZ copy could not be saved to
  >
  > `<copy path>`
  >
  > Error: `<reason>`
  >
  > The disk is still in drive `<n>`. Save a WOZ copy to another file before
  > ejecting, or eject the disk and discard the changes.

- **Answers**: **Save as...** (the safe answer, for Enter and the close box)
  and **Eject and discard**. For the unserializable case the file picker opens
  on `<file>.recovered.woz` with WOZ listed first; the picker's labels are
  unchanged.

## 5. A mount declined (`FormatMountFailureMessage`, `MountDiagnosis::Describe`)

- **`MountFailure::UnsavedWrites`, another file** (an insert or a salvage
  insert; new):

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
  cycle or machine-switch remount; new):

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
- **`MountDiagnosis::Describe`, `UnsavedWrites`** (new): "was not inserted,
  because the disk already in the drive has changes that could not be saved to
  its file"
- **`MountDiagnosis::Describe`, `BehindLive`** (new): "was not inserted,
  because the machine was behind live in its history when the insert ran"

The earlier draft "Casso did not change the disk in drive `<n>`, because the
disk in it has changes that could not be saved..." is withdrawn in favor of
the two texts above.

The two `UnsavedWrites` texts above: shown to the owner 2026-10-08, no changes
asked; final sign-off with the full list (T005). The wording shown was that
earlier draft, one text for both cases:

> Casso did not change the disk in drive `<n>`, because the disk in it has
> changes that could not be saved:
>
> `<path>`
>
> Error: `<reason>`
>
> The disk stays in the drive with its changes. Fix the problem and try
> again, or eject the disk and save its changes to another file.

The texts above differ from it: the other-file text opens "Casso did not
insert this disk image:" with the new path, then gives the kept disk's path
after "The disk already in drive `<n>` has changes that could not be saved to
its file:", and ends "That disk is still in the drive with its changes.
Eject it to save them to another file."; the same-file text uses the
save-failure head ("Casso could not save changes to the disk image:") and
ends "The file on disk is unchanged, and the disk was not read from it again.
The drive still has your changes."; neither has "Fix the problem and try
again".

## 6. An insert over a file another program changed (`ChangePrompt::ComposeReplacedReport`, new)

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

## 7. A state load declined (`MachineStateFile::SaveDisksFirst`, new)

    Error: disk not saved
           A disk in a drive has changes that could not be saved, so the state
           was not loaded.

## 8. Salvage

- **The disk changed before the copy was written** (`ERROR_MEDIA_CHANGED`,
  new):

  > Casso did not write the salvaged copy, because the disk in drive `<n>`
  > changed after it was assessed. Salvage again to copy the disk now in the
  > drive.

- The salvage dialog, the result, the failure dialog and the Insert / Not now
  question keep their wording; they are now shown from `WM_APP_SALVAGE_DONE`.

## 9. A command that reached the machine behind live (new)

- **Eject**:

  > Casso did not eject the disk in drive `<n>`, because the machine was behind
  > live in its history when the eject ran. Return to live and try again.

- **Write protection** (Settings and the Disk menu):

  > Casso did not change write protection for drive `<n>`, because the machine
  > was behind live in its history when the change ran. Return to live and try
  > again.

- **Insert**: the mount failure text with the `BehindLive` description
  (section 5).
- A write-protect command whose value equals the drive's current setting
  raises no notice, behind live or not: Settings sends the command for both
  drives on every apply, changed or not (`Ui/Settings/SettingsPanelState.cpp:1054-1057`),
  and an apply that only changed a color is not a write-protect change.
- "Return to live" stands for whatever the history band calls going to the
  live end; the owner confirms the term with the rest. The history band's
  link reads "Go live" (`Ui/Debugger/HistoryBand.h:68`).

## 10. The command-line tools (`DiskImageSession`, new)

When a put, delete or other change could not copy the image's attributes and
permissions to the new version:

    Error: could not keep the image's attributes
           The attributes and permissions of <path> could not be copied to the
           new version, so the image was not changed.

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

## 13. An update installed behind live (`DeploySave`, new)

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

> Casso could not return the machine to the live end of its history before
> installing the update, so each disk was saved as it was at the point in
> history where the machine stood.

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
