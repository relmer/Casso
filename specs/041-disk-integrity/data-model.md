# Data Model: Disk integrity

The entities the design adds or changes. Field types are indicative; the
contracts in [contracts/internal-interfaces.md](contracts/internal-interfaces.md)
are binding.

## ThreadOwnership (new, `CassoEmuCore/Core/`)

| Field | Meaning |
|---|---|
| holder thread id | The thread allowed to touch what this token guards; the constructing thread at first; none between `Release` and `Claim` |

**Transitions**:
- held by T --`Release` on T--> unowned --`Claim` on U--> held by U.
- `Claim` on the holder, and `Release` of an unowned token, change nothing.
- `Claim` while another thread holds the token, or `Release` on a thread
  that is not the holder while another thread holds it, asserts and leaves the
  holder unchanged.

## BayStatus (new, `CassoEmuCore/Devices/Disk/DriveStatus.h`)

One per bay (8 slots x 2 drives), a value copy of the bay at publish time.

| Field | From |
|---|---|
| isMounted | `Entry::mounted` |
| path | `Entry::path` (a copy) |
| format | `Entry::format` |
| writeProtect | `DiskImage::GetWriteProtectInfo`: the six causes and the damaged quarter tracks |
| isSalvageOffered | `Entry::salvageOffered`, false when not mounted |
| wozRequirements | after the `master` merge (tasks.md T136): `WozCompatibility::ReadRequirements (image->GetWozMetadata())`, read on the owning thread, for the WOZ info icon; empty when not mounted |

## DriveActivity (new)

One per slot 6 drive, sampled at every publish.

| Field | From |
|---|---|
| isMotorOn | `Disk2NibbleEngine::IsMotorOn` |
| headQuarterTrack | `Disk2NibbleEngine::GetCurrentTrack`, -1 when there is no controller |
| readNibbles, writeNibbles | the engine's lifetime counters |

## DriveStatus (new)

| Field | Meaning |
|---|---|
| bays | `shared_ptr<const BayTable>`, shared between publishes until the generation moves |
| bayGeneration | the store generation the table was built at |
| activity | slot 6 `DriveActivity` for each drive |
| hasController | whether the machine has a slot 6 Disk II |
| sequence | increases with every publish |

**Rule**: a `DriveStatus` is built only on the store's owning thread and never
changed after it is published. Readers use accessors (`GetBay`, `GetActivity`,
`GetMountedSources`).

## DiskImageStore (changed)

| Addition | Meaning |
|---|---|
| ownership token | the store's `ThreadOwnership`, shared with its controllers; every public instance member is checked against it, the members this design adds and the two 035's defect e fix adds (`IsRetainingMedia`, `ReportSeatedMedia`) included, except `NoteExternalChange`, `GetThreadOwnership` and R5's other exemptions |
| status generation | moves on every write of a bay field or a write-protect cause |
| external-change inbox | one `(intent, time)` record per path, the latest wins, written by any thread under `m_pendingMutex` and drained by the owning thread after the replay check |
| file-backed flag | false for scratch stores, which then write nothing |

## SalvageAssessment (changed)

| Addition | Meaning |
|---|---|
| mediaId | the bay's `GetMediaId` when assessed; `SalvageToFile` writes nothing if the bay no longer holds that medium |
| sourcePath | the bay's path when assessed, used by the dialog instead of a second read of the bay |

## MountCompletion (changed)

| Addition | Meaning |
|---|---|
| bay | the drive's `BayStatus`, copied on the owning thread just before the completion is posted; the damage report reads it |

## MountedImageState (changed)

| Addition | Meaning |
|---|---|
| recovery record | the recovery copy's path, its file identity at write time, the image id and track generations it holds, and which failure reports were shown: the copy path last reported, and whether a plain save failure (no copy) was reported. Those two report fields are the store's only save-failure report state; the same-file decline reads them through `IsSaveFailureReported` (either one set) for `MountDiagnosis::wasReported` |
| asked cause | the `SaveFailureCause` of the open eject question |

The recovery record, with its report fields, is cleared on mount, eject, a
successful save and an external reload; the asked cause on mount and eject.

## DiskImage (changed)

| Change | Meaning |
|---|---|
| `MarkSaved` | after each successful save, the raw source bytes become the bytes just written, and dirty clears (FR-014) |
| reserved empty slots | WOZ only, and only on an image with no damage (`DiskImage::IsDamaged` false; a damaged image is write-protected, so the reservation does nothing and every map entry stays as loaded): one empty bit slot per unmapped whole track, mapped at qt/4 placement at load. A quarter track a TMAP entry maps to an empty bit slot counts as unmapped and is reserved too; under spec 040's record rule (`51ad867e8`, which tasks.md T111 and T136 take) that is a record with a zero bit count, whatever its start block and block count (owner confirmed 2026-10-09 for a start block of 3 or more). An empty FLUX record stays an empty flux track, which the guest can already write. A record with a count above zero at a misstated location (`RecordLocationMissing`, `RecordInHeader`) or with more data than its blocks hold (`CountExceedsBlocks`), and a map entry from 160 to 254 (`DamagedQuarterTrack`), are damage |
| `TryCreateTrack` | sizes a reserved slot to 51,200 bits on the first written bit |
| `GetTrackGenerations` | the per-track generations the recovery record compares |

## MountFailure and MountDiagnosis (changed)

| Addition | Meaning |
|---|---|
| `UnsavedWrites` | the change was declined because the outgoing disk's writes could not be saved |
| `BehindLive` | the insert reached the emulation thread while the machine was behind live |
| `keptPath`, `saveError`, `isSameFile`, `wasReported` | what an `UnsavedWrites` decline kept and why; `ShouldReport` is false only for a same-file decline already reported |

## ChangePrompt (changed)

| Addition | Meaning |
|---|---|
| imagePath | the file the prompt was composed from, used after the modal instead of the bay |
| suggestedSavePath | the file name the Save as picker opens on |
| `SaveFailureCause::Unwritable`, `Unserializable` | the two eject-question causes |

## Drive widgets (changed)

| Addition | Meaning |
|---|---|
| `SyncAction::DoorRestore` | a declined insert or eject puts the door back |
| `DriveWidgetState::isEjectPosted` | an eject was posted and neither its path change nor a `DoorRestore` (the eject was declined) has arrived yet; while it is set, a create-disk into that drive needs no replace prompt |

## CommitPlan (changed)

| Addition | Meaning |
|---|---|
| `Step::CopyMetadata`, `Step::FlushTemporary` | between `WriteTemporary` and `Replace`; a failure at `FlushTemporary` removes the temporary, and a failure at `CopyMetadata` does not stop the commit (best-effort, owner decision 2026-10-09) |

## Disk2Controller (changed)

| Change | Meaning |
|---|---|
| `kMaxQuarterTrack` | 158 (was 139) |
| ownership | checks its store's token, or its own when built outside a machine; `SetThreadOwnership` checks that the current and the new token are both held by the calling thread |

## CpuManager (changed)

| Addition | Meaning |
|---|---|
| deploy hold request and held flag | written under `m_pauseMutex`, apart from 035's `m_isParked`, which marks a weaker state: a paused thread that still drains commands and runs the service function every 20 ms, never moves disk ownership, and is reported parked with no CPU thread at all (`m_isParked` starts true); while held for deploy the thread runs no frame, command drain or service pass, has released the disks, and the request wakes `WaitWhilePaused` |

## DeploySave (new, `CassoEmuCore/Shell/`)

The update installer's save while the machine is behind live (FR-019). Its return to live serves every install path: the MSIX deploy, which then holds the thread and saves, and the zip update installed now and an update left until Casso closes, which then save through quit (owner confirmed 2026-10-09; tasks.md T139).

| Field | Meaning |
|---|---|
| request serial | increases with every return-to-live request |
| awaited | whether a request still waits on the newest serial; false after its deadline, so a `GoLiveForDeploy` for it that has not started does nothing |
| outcome | the `LiveReturnOutcome` the emulation thread reported for the newest serial, a late one included, under the object's mutex; `TimedOut` while none has arrived |

`DeployFlushResult` (returned by `Flush`): `wasBehindLive`, whether the hold or
the replay flag was set once the thread was held, and `wasReplaying`, the
replay flag's value before the save cleared it, which `Resume` puts back on a
failed deploy. `IsSavedBehindLive` reads the outcome and the result: the user
is told when `wasBehindLive` is set or the outcome is `HistoryCut`.
