# Data Model: Disk integrity

The entities the design adds or changes. Field types are indicative; the
contracts in [contracts/internal-interfaces.md](contracts/internal-interfaces.md)
are binding.

## ThreadOwnership (new, `CassoEmuCore/Core/`)

| Field | Meaning |
|---|---|
| owner thread id | The thread allowed to touch what this token guards; the constructing thread at first; none between `Release` and `Claim` |

**Transitions**: owned by T --`Release` on T--> unowned --`Claim` on U--> owned by U.
`Release` on a thread that is not the owner, or `Claim` while owned, asserts.

## BayStatus (new, `CassoEmuCore/Devices/Disk/DriveStatus.h`)

One per bay (8 slots x 2 drives), a value copy of the bay at publish time.

| Field | From |
|---|---|
| mounted | `Entry::mounted` |
| path | `Entry::path` (a copy) |
| format | `Entry::format` |
| writeProtect | `DiskImage::GetWriteProtectInfo`: the six causes and the damaged quarter tracks |
| salvageOffered | `Entry::salvageOffered`, false when not mounted |

## DriveActivity (new)

One per slot 6 drive, sampled at every publish.

| Field | From |
|---|---|
| motorOn | `Disk2NibbleEngine::IsMotorOn` |
| headQuarterTrack | `Disk2NibbleEngine::GetCurrentTrack`, -1 when there is no controller |
| readNibbles, writeNibbles | the engine's lifetime counters |

## DriveStatus (new)

| Field | Meaning |
|---|---|
| bays | `shared_ptr<const BayTable>`, shared between publishes until the generation moves |
| bayGeneration | the store generation the table was built at |
| activity | slot 6 `DriveActivity` for each drive |
| hasSlot6Controller | whether the machine has a slot 6 Disk II |
| sequence | increases with every publish |

**Rule**: a `DriveStatus` is built only on the store's owner and never changed
after it is published.

## DiskImageStore (changed)

| Addition | Meaning |
|---|---|
| ownership token | the store's `ThreadOwnership`, shared with its controllers |
| status generation | moves on every write of a bay field or a write-protect cause |
| external-change inbox | `(path, intent, time)` records appended by any thread under `m_pendingMutex`, drained by the owner |

## MountedImageState (changed)

| Addition | Meaning |
|---|---|
| recovery record | the recovery copy's path, its file identity at write time, the image id and track generations it holds, and which failure reports were shown |

Cleared on mount, eject, a successful save, and an external reload.

## DiskImage (changed)

| Change | Meaning |
|---|---|
| raw source bytes | replaced with the committed bytes after each successful save (FR-014) |
| reserved empty slots | WOZ only: one empty bit slot per unmapped whole track, mapped at qt/4 placement at load |
| `TryCreateTrack` | sizes a reserved slot to 51,200 bits on the first written bit |

## MountFailure (changed)

| Addition | Meaning |
|---|---|
| `UnsavedWrites` | the change was declined because the outgoing disk's writes could not be saved |

## Disk2Controller (changed)

| Change | Meaning |
|---|---|
| `kMaxQuarterTrack` | 158 (was 139) |
| ownership | checks its store's token, or its own when built outside a machine |
