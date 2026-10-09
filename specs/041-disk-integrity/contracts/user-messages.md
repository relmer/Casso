# Contract: user messages

**Status: draft, needs the owner's approval before it ships.** Every text the
user can read that this work adds or changes. All UI text is sentence case.
`<path>` is the image's full path, `<reason>` is `ChangePrompt::DescribeError`'s
text for the failure, and `<n>` is the drive number.

## Unchanged, now true

`DiskImageStore::FormatFlushLossMessage` keeps its wording. Its last sentence,
"Your recent writes have not been saved. The disk in the drive still has them.",
was false on re-insert, reset, power cycle, machine switch and eject, which
discarded the writes. After FR-010 a disk cannot leave the drive with unsaved
writes, so the sentence holds on every path.

## New: a change declined because of unsaved writes

`MountFailure::UnsavedWrites`, shown by the mount completion for an insert,
re-insert or salvage insert that FR-010 declines.

> Casso did not change the disk in drive `<n>`, because the disk in it has
> changes that could not be saved:
>
> `<path>`
>
> Error: `<reason>`
>
> The disk stays in the drive with its changes. Fix the problem and try again,
> or eject the disk and save its changes to another file.

## New: the eject question

Asked when an eject's save fails and no current recovery copy exists.

- **Title**: Disk not saved
- **Body**:

  > The changes to this disk could not be saved:
  >
  > `<path>`
  >
  > Error: `<reason>`
  >
  > Save them to another file before ejecting, or eject the disk and discard
  > them.

- **Answers**: **Save as...** (the safe answer, for Enter and the close box) and
  **Eject and discard**.

## New: an eject declined

Shown when the eject's save fails and no question can be raised (no ask sink,
the post failed, or a question is already open).

> The disk was not ejected, because its changes could not be saved:
>
> `<path>`
>
> Error: `<reason>`

## Changed cadence, not wording

A save failure nobody asked for (motor spin-down, the idle pick-up) is reported
once per mount, until a save succeeds or the disk leaves the drive. A failure
during an action the user took (insert, eject, reset, power cycle, machine
switch) always reports.
