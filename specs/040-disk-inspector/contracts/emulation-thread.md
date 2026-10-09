# Contract: Inspector and the emulation thread (Casso host)

**Covers**: FR-062 to FR-071, FR-103, FR-104, FR-107, FR-117. **Decisions**:
research R6, R7, R11 to R14.

## Rule

The window thread never reads or changes a drive's `DiskImage`, engine or
controller. It reads the published `DriveHeadState` and the replies to its
requests. Everything that touches the disk runs on the emulation thread
between service passes.

## Published per frame

```cpp
std::shared_ptr<const InspectorDriveStatus> InspectorStatusPublisher::Take() const;
```

- Published at the end of `RunCpuThreadFrame`, swapped under a leaf mutex;
  `Take` works from any thread and never returns null.
- Holds a `DriveHeadState` for drive 1 and drive 2 of slot 6
  (data-model.md, section 5), and `hasController`.
- Write and visit counts are included only when `countsGeneration` changes.
- When 041's `DriveStatus` is on master, these fields move into its
  `DriveActivity` and this publisher is removed. Field names already match
  041's where they overlap.

## Write hook

`Disk2Controller::OnLatchLoad (drive, nibble, quarterTrack, isDropped)`:
called where `Write` loads the data latch with Q6 and Q7 set while the motor
runs (spindown included); not with the motor stopped; not for the //c IWM
mode-register load. Not dropped: increments the record's `guestWriteCount`
and sets the drive's activity to Writing. Dropped: sets WriteBlocked only.
Same rule as 035's FR-161; built once, by whichever spec merges first.

## Requests

Posted with `InspectorRequestQueue::Post`, drained at the end of each service
pass; replies land in a mailbox the window takes once per window frame. Every
request holds the drive's `mediaId`; a mismatch gets the reply `DiskChanged`
and does nothing.

| Request | Effect | Reply |
|---|---|---|
| `CopyDisk` | none | `DiskCopy` |
| `CopyTracks (records)` | none | `TrackCopy` per record |
| `CopyAsInserted` | none | the bay's as-inserted `DiskCopy` |
| `Export (quarterTrack)` | none | `TrackCopy` |
| `Apply`, `Undo`, `Redo` | at a safe point: write, re-analyze, save or roll back | `Applied`, `AppliedToPreservedCopy`, `NotApplied`, `Cancelled`, `WaitEnded` |
| `CancelApply` | ends a wait | `Cancelled` |

## Safe point

All must hold, checked at the end of a service pass:

- the drive is not turning, or
- no write latch since the last pass, Q7 off, no open flux write burst, and
  the head's angle is outside the span of every data field the apply changes.

From the moment the disk changes until the save finishes or the records are
restored, the emulation thread does not run the guest. A guest write to a
record the apply changes, while it waits, ends the wait with `WaitEnded`.

Once 035 is on master: Apply, Undo and Redo are state-changing for its
divergence gate and get the reply `NotApplied` while history replays or flushes
are held.

## As-inserted copies

Taken at mount and reload in the same step that resets the counts, with
records shared until the guest first writes them.
