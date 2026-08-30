# Data Model: Disk Images Shared with a Running Emulator

**Feature**: `028-shared-disk-images` | **Date**: 2026-08-30

Six things carry state. Four are new, two are existing types gaining fields.

## ImageIdentity (new)

What is compared to answer "has this file changed since I read it".

| Field | Type | Notes |
|---|---|---|
| `sizeBytes` | `size_t` | From `IDiskFileIo::Stat` |
| `writeTime` | `int64_t` | Same |
| `recorded` | `bool` | False until a stat has succeeded |

**`recorded` IS NOT DERIVABLE FROM THE OTHER TWO.** A zero size and a zero time
are both legal, so a caller comparing against a default-constructed identity
would read "unchanged" from a stat that never ran. `DiskImageSession::OpenedImage`
already carries a `stampRecorded` flag for the same reason, and its comment says
so; this is that idiom, not a new one.

**Rules**

- Two identities compare equal only when both are recorded and both fields match.
- An identity recorded from a write the emulator itself performed replaces the
  mounted one, so the emulator's own commit is never seen as an external change
  (FR-004).

## MountedImageState (new, held per `DiskImageStore::Entry`)

What the emulator knows about one mounted image beyond its bytes.

| Field | Type | Notes |
|---|---|---|
| `identity` | `ImageIdentity` | Taken at mount, refreshed at every commit |
| `pending` | `PendingChange` | Empty until a change is noticed |
| `watching` | `bool` | False where the directory could not be watched |

**Rules**

- Set at mount, cleared at eject.
- `watching` false is not an error. FR-022 requires the feature to degrade to
  the write-time re-check where notification cannot be trusted, and a network
  share is the case that produces it.

## PendingChange (new)

A noticed change that has not been acted on. This is what the banner stands for.

| Field | Type | Notes |
|---|---|---|
| `seen` | `bool` | A change is outstanding |
| `intent` | `PickUpIntent` | What the writer said, or `Unstated` |
| `firstSeenAt` | `int64_t` | For the quiet period of FR-013 |
| `lastSeenAt` | `int64_t` | Reset by every further change |

**Rules**

- Further changes UPDATE this record rather than creating a second (FR-011).
  `lastSeenAt` moves, and a later stated intent replaces an earlier one.
- Acting on it reads the image fresh at that moment, so it takes the most recent
  contents rather than those current when `firstSeenAt` was set (FR-012).
- The quiet period is measured from `lastSeenAt`, which is what makes a
  multi-command build settle once rather than once per command.

## PickUpIntent (new, enumeration)

| Value | Meaning |
|---|---|
| `Unstated` | Nothing said; the fallback answer decides (FR-007) |
| `TakeUpInPlace` | Swap the contents, leave the machine running |
| `Restart` | Swap the contents and restart the machine |

**`Unstated` IS A REAL VALUE, not a missing one.** A change from a text editor
or another emulator carries no intent, and that is the ordinary case for
everything except `CassoCli`. Treating it as a distinct value is what keeps the
fallback from being a guess about which of the other two was meant.

## Conflict (new)

An external change to an image the guest has also written to. Lives only until
the user resolves it.

| Field | Type | Notes |
|---|---|---|
| `guestBytes` | `vector<Byte>` | What the emulator holds |
| `externalIdentity` | `ImageIdentity` | What is on disk now |
| `resolved` | `bool` | |

**Rules**

- Exists only when the image is dirty AND an external change was seen. Either
  alone is not a conflict.
- Resolves to exactly one surviving image plus one backup (FR-020, FR-021).
- **No stated intent and no preference resolves it** (FR-019). The intent says
  how the guest continues, not whether work may be discarded.
- Where the backup cannot be written, the conflict stays unresolved and both
  versions stay live (FR-023).

## CommandLineOptions (existing, gains one field)

| Field | Type | Notes |
|---|---|---|
| `pickUpIntent` | `PickUpIntent` | Defaults to `Unstated` |

Lives beside `imagePath`, `onDiskName` and `imageTypeName`, which spec 026 added
for the same reason: they are assembler-and-disk options rather than anything
nested under a subcommand.

## DiskImageStore::Entry (existing, gains one field)

| Field | Type | Notes |
|---|---|---|
| `sharedState` | `MountedImageState` | Everything above, per bay |

Nothing existing changes meaning. `image`, `path`, `format`, `mounted` and
`salvageOffered` keep their current roles.

## State transitions

```text
mounted ──(watcher fires)──> change seen ──(quiet period)──> ready to act
   │                              │                               │
   │                              └──(further change)─────────────┘
   │                                   lastSeenAt moves
   │
   └──(commit by this emulator)──> identity refreshed, no change seen

ready to act ──(image not dirty)──> pick up ──> banner stands
             │
             └──(image dirty)─────> conflict ──> user resolves ──> backup + pick up
                                        │
                                        └──(backup fails)──> stays unresolved
```

**The write-time re-check is not on this diagram and is the point.** Every
commit re-reads the identity regardless of what the watcher has or has not said
(FR-003), so a missed notification costs promptness and never correctness.
