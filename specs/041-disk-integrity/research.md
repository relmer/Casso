# Research: Disk integrity

Phase 0 of the plan. Six design decisions the audit left open, each researched
against the worktree at `fb05ee2ab` (035 at `811a6f727` plus the spec). The
per-defect evidence, fix proposals and regression tests are in
[audit.md](audit.md); 035's threading model is in
[threading-035.md](threading-035.md).

## Decision summary

| # | Decision | Chosen | Rejected |
|---|---|---|---|
| R1 | How other threads read drive state | The emulation thread publishes one immutable `DriveStatus` (every bay, plus slot 6 activity) as a `shared_ptr<const DriveStatus>` under a leaf mutex; the bay table is rebuilt only when a store generation moves; the window takes one copy per window frame | Per-field locks and atomics on `Entry` and the engine (safe per read, inconsistent across reads) |
| R2 | A failed save before a disk change | Invariant: a disk leaves its bay only when every guest write is in a file. Otherwise the change is declined or waits for an answer, and history records nothing. Load first, except over the disk's own file, which is saved first and read after | Emptying the bay on failure; stopping the machine switch |
| R3 | Durable commit | One `DurableCommit` sequence over the existing `IDiskFileIo` seam: write temporary, copy metadata, `FlushFileBuffers`, `MoveFileExW (REPLACE_EXISTING \| WRITE_THROUGH)`. One recovery record per mount in `MountedImageState` | `ReplaceFileW` (partial-failure states, needs the target to exist) |
| R4 | Track on write, head stop | Head stop 158 (the stepper moves in half tracks). WOZ only: empty bit slots reserved at load for every unmapped whole track, filled on the first written bit; 51,200 bits; qt/4 placement | Growing slots on write (breaks 035 keyframes); stop at 159 (odd, strands the head) |
| R5 | Ownership | One `ThreadOwnership` token per store, shared by its controllers; the constructing thread owns; handover is Release then Claim; a Debug `ASSERT_THREAD_OWNERSHIP` at every guarded entry point | A global owner; locking instead of asserting |
| R6 | Race proof | The ownership assertion, a drive-status stress test in Debug, Release and an opt-in AddressSanitizer build, and ASan itself | Application Verifier as a gate; ThreadSanitizer (unavailable on MSVC) |

## Names settled across the records

The researchers worked independently and some chose different names. The
plan uses these, and the records below are read with them substituted:
`ThreadOwnership` (not `ThreadOwner`), `DriveStatus` and `BayStatus` (the
published types, in `CassoEmuCore/Devices/Disk/DriveStatus.h`),
`DriveStatusPublisher` (in `CassoEmuCore/Shell/`), and `DurableCommit`.

Settled after the plan review (2026-10-08), and binding over the records below:
- R5's `ThreadOwnership`, `OwnerThreadStandIn` and `DiskOwnershipTests`
  supersede R6's `ThreadOwner`, `BindOwnerThread`, `ThreadOwnerTests` and
  `DiskStoreOwnershipTests`. The SC-002 stress test takes the store through
  `OwnerThreadStandIn::Take`.
- `CommitMode` (`Replace`, `CreateNew`), a free type in `DurableCommit.h`,
  replaces R3's nested `TargetRule` (`ReplaceExisting`, `RequireNew`).
- The flag-only write-protect setter R1 calls `SetUserWriteProtect` is
  `SetUserWriteProtectFlag`; `SetUserWriteProtect` is FR-015's operation that
  saves first. The replay path uses only the flag-only setter.
- User write protection keeps the existing `IDM_DISK_WRITEPROTECT1/2` command;
  no new command id is added for it.
- The salvage commands are the audit's: `IDM_DISK_SALVAGE1/2` (posted),
  `IDM_DISK_SALVAGE_WRITE`, `WM_APP_SALVAGE_OFFER` and `WM_APP_SALVAGE_DONE`.
- This repo has no `.filters` files; where a record below says to edit one,
  only the `.vcxproj` changes.
- The mount completion's damage report reads the drive's `BayStatus`, copied
  into `MountCompletion` at P3, not the newest or the shown status (R1).
- `DiskManager::PublishDriveStatus` returns the publish's `DriveStatusChange`
  and calls the wake installed with `DiskManager::SetStatusWake` exactly when
  it is `Bays`; the shell's wake sets `m_frameReadyEvent`, and a test counts
  calls to its own (R1). `EmulatorShell::ServiceCpuThread` returns nothing.
- The store's only save-failure report state is R3's two report fields,
  `m_reportedRecoveryPath` and `m_isPlainFailureReported`; R2's same-file
  decline reads them through `IsSaveFailureReported`, true when either is
  set, and adds no flag of its own.
- The behind-live decline sits in entry points only the disk commands reach
  (the new `DiskManager::MountDiskForCommand` for the insert,
  `EjectDiskInSlot6`, `ToggleImageWriteProtect` and
  `SetDriveUserWriteProtect`), not in `DiskManager::MountDiskInSlot6`, which
  the command-line mount and the remount also call (R2, "Divergence").
- `DriveWidgetState::isEjectPosted` is cleared when the door sync handles the
  path change and when a `DoorRestore` arrives (R2, "Other files").
- ScenarioTests already installs the EHM breakpoint handler (R5 Risks).

Settled in the second revision (2026-10-08), and binding over the records
below:
- Defect c (softreset-second-write-path, FR-017) shipped in `e32b2b68c`
  (tasks.md T066, T067). Its tests use disks with no file behind them, not the
  scratch paths R2 and the audit proposed. 035's tip `d3c15b55c` holds no
  cherry-pick of it, only a comment for c (`fe0427676`).
- The six 035 defects reported on 2026-10-08 are lettered a to f (spec.md,
  Assumptions); 035 fixed a, b, d, e and f on `origin/035-debugger` (tip
  `d3c15b55c`), and tasks.md "What 035 brings" gives the commits.
- The audit's park for the update installer is a deploy hold,
  `CpuManager::HoldForDeploy`, `ReleaseDeployHold` and `IsHeldForDeploy`,
  because 035's defect b fix added a pause acknowledgement,
  `CpuManager::IsParked`, with a weaker meaning: a paused thread still drains
  commands and runs the service function every 20 ms, never moves disk
  ownership, and `IsParked` is true with no CPU thread.
- 035 moved the heat replayer's scratch machine into `ScratchReplayMachine`
  and added `ScratchCallReplayer`, which builds one too; R5's scratch brackets
  and R2's `SetFileBacked (false)` follow them there (tasks.md T015, T095).
- The salvage insert's stand-in test calls the audit's static
  `EmulatorShell::InsertSalvagedCopy (DiskManager &, drive, path)` with the
  `DiskManager` rig, and FR-003's test is at the `DiskManager` level, because
  a shell built in a test has no `DiskManager` (R1, Test plan).
- Every update installed behind live saves the disks at the live end (owner
  confirmed 2026-10-09): the emulation thread returns to live on a posted
  request the window waits on; then, for an MSIX bundle, the window holds it
  and saves, and for a zip update installed now and an update left until
  Casso closes, quit saves (plan.md section 8; R5 Risks).
- Every unit test that touches a real file moves to the scenario suite, 65 at
  the inventory of 2026-10-09, `Win32DiskFileIoTests` among them, beside R3's
  six real-file seam cases or 040's; no constitution exception (owner
  decision, 2026-10-09; R3, Test plan; tasks.md T057).
- The WOZ 2 track-record rule is spec 040's FR-053, built in 040's
  `51ad867e8`, which this branch takes rather than building (tasks.md T111,
  T136): a zero-count record is an empty slot, whatever its location (owner
  confirmed 2026-10-09), and a record with a count that claims data at a
  misstated location, or more than its blocks hold, and a map entry from 160
  to 254, are damage. The reservation does nothing for a damaged image (R4,
  Decision; tasks.md T115).
- Test names that read as a component acting were renamed in tasks.md, here
  and in audit.md: `ASalvageWriteThatDoesNotParseDispatchesNothing`,
  `ABuiltControllerUsesItsStoresToken`,
  `TheMountIsPublishedBeforeItsCompletion`,
  `Eject_Unwritable_RaisesTheQuestionAndWaits`,
  `Eject_Unwritable_NoQuestionPossible_KeepsTheDisk`,
  `Eject_UnserializableCopyFailed_OffersAWozCopy`,
  `SetFileBacked_False_WritesNothingAndEjects`,
  `AFailedMountRetiresNothingWhileRetained`,
  `ARemountOverAChangedFileReportsTheReloadAndTheCopy`,
  `AnEjectOverAChangedFileWithNoQuestionPossibleKeepsTheDisk` and
  `MachineSwitchKeepsADiskWhoseFlushFailed`.

## New defect found during research

**Same-file remount reads before it saves** (R2). `DiskImageStore::Mount` reads
the file, then `MountFromBytes` flushes the old disk into that file, then
builds the new image from the bytes read before the flush. A reset taken while
a dirty disk's motor is still spinning writes the guest's writes to the file
but mounts a disk without them, and the next flush writes that stale image
back over the file. The audit did not record it; FR-009 and SC-006 now cover it.

## R1. Published drive status (FR-002, FR-003, FR-008, SC-005)

Paths are relative to `C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity` at `fb05ee2ab`, which is 035 at `811a6f727` plus the spec. The measurement source is in `C:\Users\relmer\AppData\Local\Temp\claude\C--Users-relmer-source-repos-relmer-Casso\76c7343d-9eb8-42be-850a-498f31e14796\scratchpad\drivestatus_bench\bench.cpp`. Nothing in the worktree was modified.

### Decision

1. **The emulation thread publishes one immutable object.** It is a `DriveStatus`, held as `std::shared_ptr<const DriveStatus>` under a leaf mutex. This is 035's debugger snapshot pattern (`PublishDebugSnapshot` at `CassoEmuCore/Shell/EmulatorShellDebugger.cpp:1781-1789`, `TakeDebuggerUpdate` at `:1098-1116`), minus the consume-once freshness flag. The object holds:
   - **Every bay (8 slots × 2 drives):** whether it is mounted, path, format, the full `WriteProtectInfo` (the six causes plus `damagedQuarterTracks`), and the salvage verdict.
   - **Each slot 6 drive's activity:** motor, head quarter track, and the read and write nibble counters.
   - **Whether a slot 6 controller exists.**
   - No field points into the store, an image or the controller.
2. **The bay table is rebuilt only when a new store status generation moves** (`DiskImageStore::GetStatusGeneration`). The generation is bumped wherever a bay field or an image's write-protect cause is written. Between rebuilds, every snapshot shares one `shared_ptr<const BayTable>`. Debug builds rebuild anyway when the generation has not moved and `ASSERT` the result is unchanged, so a missed bump fails the Debug suite.
3. **Activity is sampled at every publish.** A change that is only activity is published only when activity is due:
   - while paused, on every service pass;
   - while running, at the end of each frame, using the `ShouldPublishFrame` result, so about 60 Hz at Maximum.
4. **Publish points, all on the emulation thread unless noted:**
   - **(P1) The end of every service tick.** It runs on every pass, after `DrainCommandQueue`, paused or running (`CpuManager.cpp:583-588`). This covers drained commands and FR-003.
   - **(P2) The end of `RunCpuThreadFrame`.** This covers activity and the changes made inside `Tick`.
   - **(P3) `DiskManager::MountDiskInSlot6`, just before `m_onMountCompleted`.** The mount is already published when the completion message's handler runs.
   - **(P4) Once at the end of `EmulatorShell::Initialize`**, on the window thread, before the emulation thread exists.
   - **Not after each drained command.** P1 follows the drain in the same pass, and only the latest state matters.
5. **Waking the window.** A publish that changed a bay calls the wake `DiskManager::SetStatusWake` installed, which in the shell is `SetEvent (m_frameReadyEvent)`, so the idle window loop wakes at once. A publish that changed only activity does not. A test installs a wake that counts, so the rule is checked with no kernel object.
6. **How the window reads it.** The window takes the newest status once per window frame, at the top of `DiskManager::UpdateDriveWidgets`, and keeps it as the "shown" status. Every window-thread reader uses that copy. The one exception is the mount-completion handler, which reads the drive's `BayStatus` that `MountDiskInSlot6` copies into `MountCompletion` at P3 on the emulation thread. It neither takes the newest status nor reads the shown one, because by the time the window handles the message either can describe a later or a different medium.
7. **`DriveWidgetController`'s sync queue gets its own leaf mutex (FR-008).** It guards both the event vector and the id counter.
8. **`Disk2NibbleEngine`'s counters stay plain members.** Their only reader becomes the emulation thread, which removes the engine-counters-not-atomic defect without atomics.

### Rationale

- **Only one object built at one instant meets "consistent within itself" (FR-002).**
  - Today's frame reads the path, the engine and the image at three different moments (`DiskManager.cpp:803`, `:867-871`, `:895-897`).
  - The menu label reads the image and the path separately (`EmulatorWindow.cpp:777`, `:786`, `:794`).
  - The tooltips pair a live path with a `writeProtect` sampled earlier in the frame (`EmulatorWindowInput.cpp:500-507`; `EmulatorShellScene.cpp:1009` vs `:1020`).
  - The per-defect fixes in `audit.md` (a mutex-guarded `BayView`, `SetBayPath` under `m_pendingMutex`, atomic `m_bayFlags`, atomic engine counters) make each read safe, but not the set of reads.
- **Building on the owning thread means a publish never reads a change half done.** `MountFromBytes` sets `mounted = true` before the load (`DiskImageStore.cpp:274`) and clears it on failure (`:282-285`). A publish can only run between calls, so the window can never see that intermediate state. This also retires the audit's lock and atomic proposals for:
  - **ui-derefs-store-diskimage:** the window holds no `DiskImage *`, so a freed or reseated image cannot be dereferenced (`RetireBay :2216`, `SeatMedia :2152`, the in-place move at `:3525`).
  - **entry-path-string-race and entry-flags-race:** the window reads no `Entry`.
  - **engine-counters-not-atomic:** the window reads no engine.
  - **The watcher thread is the one remaining off-thread reader** (`NoteExternalChange`, `DiskImageStore.cpp:2692-2715`). FR-005 removes it. After that, `m_pendingMutex` guards only the pending records, as its comment at `DiskImageStore.h:715-719` claims.
- **Polling at the points every pass reaches catches writers that bypass every notification.** 035 added three:
  - `SeatMedia` reports no bay change by design (`DiskImageStore.cpp:2110-2113`).
  - `DiskImage::LoadState` rewrites both write-protect flags (`DiskImage.cpp:1303-1304`).
  - `Replayer::ApplyInput` writes through a raw `GetImage` pointer (`Replayer.cpp:481-485`).
  - `NotifyMediaChanged` is suppressed while replaying (`:2233-2239`).

  The generation is bumped where the fields are written, not where notices are sent, so none of these can be missed. A missed bump is caught by the Debug verifier.
- **Why P1 is the right paused hook (FR-003).**
  - Every pass runs `DrainCommandQueue` and then `m_onService` (`CpuManager.cpp:583-588`).
  - A paused thread passes at least every 20 ms (`kServiceIntervalMs`, `CpuManager.cpp:566-576`), and at once on `PostCommand` (`:163-169`).
  - The wake from P1 then lands the change on screen within one window loop iteration. Without it, the idle window loop can sleep up to 50 ms (`s_kIdleUpkeepMs`, `EmulatorShellInternal.h:123`; `EmulatorWindow.cpp:1502-1532`).
- **A publish after each drained command would not fix message ordering.** The mount completion is posted from inside the command:
  1. `DiskManager.cpp:485-488` calls the callback.
  2. `EmulatorShellDisks.cpp:188` posts `WM_APP_MOUNT_COMPLETED`.
  3. On the window thread, `HandleMountCompletion → ReportDamagedMount` (`:244`, `:694-753`) reads the drive.

  P3 publishes before step 2, which is the only placement that orders the two.
- **P2 covers changes made inside `Tick`:**
  - the spindown flush and `ApplyPendingReload` (`MachineBuilder.cpp:1519-1546`);
  - reverse replays started from `ReverseHost::OnFrame` (`EmulatorShellCpuThread.cpp:997-1001`);
  - drive activity.

  Reusing the `ShouldPublishFrame` decision (`EmulatorShellPresent.cpp:1171-1180`) keeps activity at the picture's cadence at Maximum.
- **Activity publishes do not wake the window.** Today, on a static screen, the window samples live counters at the 20 Hz upkeep. Waking at 60 Hz for activity would raise the present rate, and so the cost of the CRT chain, while a drive spins.
- **No consume-once flag.** `TakeDebuggerUpdate` clears `m_isDebugViewFresh` (`EmulatorShellDebugger.cpp:1106-1110`) because the debugger window is its only consumer. The drive status's consumer is the window frame, which needs the current status on every frame, not only fresh ones; the completion handler reads its own `BayStatus` copy from `MountCompletion` (decision 6) and never calls `Take`. `Take` returns the newest pointer, and readers compare `GetSequence` and `GetBayGeneration`.
- **One shown copy per window frame.** It keeps the drive label, padlock, tooltip and Disk menu describing the same moment as the painted frame.
- **The generation check was chosen over rebuild-and-compare.** Both were measured (Design detail, last section). Rebuilding costs about 250 ns plus one allocation per mounted bay on every call. The idle generation check costs about 6 ns and allocates nothing (Constitution IV: avoid allocations in hot paths). The Debug verifier keeps rebuild-and-compare's robustness where it pays off, in the test suite.

### Alternatives considered

- **Per-field synchronization inside the store and the engine** (the audit's individual proposals). Rejected:
  - It is not one snapshot, which FR-002 requires.
  - It keeps the window calling into objects the emulation thread owns, which makes SC-003's ownership assertion impossible.
  - It adds five separate primitives.
- **Publishing from notification chokepoints** (`EmitBayChange`, `OnBayChange`, the media-change listener). Rejected: `SeatMedia`, `DiskImage::LoadState`, `Replayer::ApplyInput` and the failed-load branch of `MountFromBytes` bypass them all (see the 035 impact sections of `audit.md`).
- **Rebuilding and comparing every publish, with no generation.** Viable on cost, at about 0.002% of a frame. Rejected because it allocates every frame and walks every damaged disk's track map 60 times a second. Kept as the Debug verifier.
- **A per-image generation counter in `DiskImage`.** Rejected:
  - The store has to bump anyway at the in-place move `*entry.image = std::move (*loaded)` (`DiskImageStore.cpp:3525`), where the moved-in counter can equal the old one.
  - It cannot see bay-level fields: path, format, salvage verdict.

  One counter in one class is simpler. The three outside writers are routed through store setters instead (Design detail, "Outside the store").
- **Lock-free hand-off** (`std::atomic<std::shared_ptr>`, a seqlock, a triple buffer). Rejected:
  - MSVC's `atomic<shared_ptr>` is lock-based internally.
  - A seqlock cannot hold strings.
  - The measured mutex take costs 21 ns.
- **Activity as CPU-written atomics in `DriveWidgetState`.** This is what the stale comment at `DriveWidgetState.h:17-34` describes. Rejected:
  - `DriveWidgetState` belongs to the window and its lifetime is not tied to the controller.
  - It would be a second channel beside the bay data.
  - The four reads would not form one snapshot.
- **Posting each change to the window as a message** (push instead of pull). Rejected: activity would flood the queue at frame rate, ordering against frames gets harder, and 035 established the pull model.
- **The swap door from a per-bay swap counter in the snapshot**, instead of the `DoorReinsert` event. Not chosen, because FR-008 keeps the queue and a mutex is a few lines. A counter would be lossless and could replace the queue later.
- **Each window reader calling `Take` itself.** Rejected for the consistency reason above. The completion handler is the one deliberate exception, and it does not call `Take` either: it reads the `BayStatus` copied into `MountCompletion` at P3.

### Design detail

#### New types

`CassoEmuCore/Devices/Disk/BayStatus.h` (header only, plain data; the store produces it, so it lives beside the store):

```cpp
#pragma once

#include "Pch.h"

#include "IDiskImage.h"


struct BayStatus
{
    bool              isMounted        = false;
    string            path;
    DiskFormat        format           = DiskFormat::Dsk;
    WriteProtectInfo  writeProtect;
    bool              isSalvageOffered = false;

    bool  operator== (const BayStatus &) const = default;
};
```

`CassoEmuCore/Shell/DriveStatus.h` / `.cpp` (class with accessors, so it gets its own pair):

```cpp
struct DriveActivity
{
    bool      isMotorOn        = false;
    int       headQuarterTrack = -1;      // -1: no controller, position unknown
    uint64_t  readNibbles      = 0;
    uint64_t  writeNibbles     = 0;

    bool  operator== (const DriveActivity &) const = default;
};


class DriveStatus
{
public:
    using BayTable      = std::array<BayStatus, DiskImageStore::kSlotCount * DiskImageStore::kDriveCount>;
    using ActivityTable = std::array<DriveActivity, DiskImageStore::kDriveCount>;

    DriveStatus ();
    DriveStatus (uint64_t                          sequence,
                 uint64_t                          bayGeneration,
                 std::shared_ptr<const BayTable>   bays,
                 bool                              hasController,
                 const ActivityTable             & activity);

    uint64_t               GetSequence      () const { return m_sequence; }
    uint64_t               GetBayGeneration () const { return m_bayGeneration; }
    bool                   HasController    () const { return m_hasController; }
    const BayStatus      & GetBay           (int slot, int drive) const;   // empty BayStatus for a bad bay
    const DriveActivity  & GetActivity      (int drive) const;             // slot 6

    std::vector<DiskImageStore::MountedSource>  GetMountedSources () const;   // isMounted && !path.empty()

private:
    uint64_t                         m_sequence      = 0;
    uint64_t                         m_bayGeneration = 0;
    std::shared_ptr<const BayTable>  m_bays;
    bool                             m_hasController = false;
    ActivityTable                    m_activity      {};
};
```

`CassoEmuCore/Shell/DriveStatusPublisher.h` / `.cpp`:

```cpp
enum class DriveStatusChange
{
    None,
    Activity,
    Bays,
};


class DriveStatusPublisher
{
public:
    DriveStatusPublisher ();   // publishes an empty DriveStatus, so Take is never null

    DriveStatusChange                   Publish (const DiskImageStore  & store,
                                                 const Disk2Controller * controller,
                                                 bool                    isActivityDue);
    std::shared_ptr<const DriveStatus>  Take    () const;

private:
    static void  BuildBays      (const DiskImageStore & store, DriveStatus::BayTable & out);
    static void  SampleActivity (const Disk2Controller * controller, DriveStatus::ActivityTable & out);

    //  Publishing thread only.
    uint64_t                                      m_sequence      = 0;
    uint64_t                                      m_bayGeneration = 0;
    std::shared_ptr<const DriveStatus::BayTable>  m_bays;
    bool                                          m_hasController = false;
    DriveStatus::ActivityTable                    m_activity      {};

    //  Guards m_published alone, held only to copy or swap the pointer.
    mutable std::mutex                            m_mutex;
    std::shared_ptr<const DriveStatus>            m_published;
};
```

What `Publish` does, in order. It uses a vestigial `hr`, `BAIL_OUT_IF` and a single exit.

1. `SampleActivity` (always).
2. `isBaysMoved = store.GetStatusGeneration() != m_bayGeneration`.
3. `isActivityMoved = activity != m_activity || hasController != m_hasController`.
4. In Debug only, when `!isBaysMoved`: rebuild into a local and `ASSERT (rebuilt == *m_bays)`.
5. `BAIL_OUT_IF (!isBaysMoved && !(isActivityMoved && isActivityDue), S_OK)`.
6. If `isBaysMoved`: `make_shared<BayTable>`, `BuildBays`, adopt it, and record the generation.
7. Build the next `DriveStatus` with the sampled activity and `++m_sequence`.
8. Swap it into `m_published` under the lock, and let the previous one go out of scope after the lock is released.
9. Return `Bays`, `Activity` or `None`.

`Publish` checks the owner thread with SC-003's macro, not a second mechanism. It takes the store as a `const DiskImageStore &`, so it hoists the token among its declarations, `const ThreadOwnership & ownership = store.GetThreadOwnership();` through the const accessor, and then calls `ASSERT_THREAD_OWNERSHIP (ownership);`. The macro's argument is never a call.

#### `DiskImageStore` (`CassoEmuCore/Devices/Disk/DiskImageStore.h`, `.cpp`)

Public additions, all emulation-thread only:

```cpp
    BayStatus     GetBayStatus        (int slot, int drive) const;
    uint64_t      GetStatusGeneration () const { return m_statusGeneration; }
    void          MarkStatusChanged   ()       { m_statusGeneration++; }
    void          SetUserWriteProtectFlag (int slot, int drive, bool isProtected);   // flag only; never saves
    void          SetFileWriteProtect (int slot, int drive, bool isReadOnly, bool hasNoPermission);
```

Private: `uint64_t  m_statusGeneration = 1;`. It starts at 1 so the first `Publish` always builds.

`GetBayStatus` copies `entry.mounted`, `entry.path`, `entry.format`, `entry.image ? entry.image->GetWriteProtectInfo() : WriteProtectInfo()` and `entry.salvageOffered`. It returns an empty `BayStatus` for a bad bay.

The `MarkStatusChanged()` sites are every write the grep shows to `path`, `mounted`, `format`, `salvageOffered`, `image`, or an image flag:

| Site | Line |
|---|---|
| `MountFromBytes` | once, at the end of the bay block, after `:271-299`; covers both success and failure |
| `RetireBay` | after `:2220`; covers `Eject :2322`, `EjectLostImage :3875`, `MountFromBytes :268`, `MountRestored :685`, `SeatMedia :2137` |
| `SeatMedia` | after `:2157` |
| `MountExternallyModifiedDisk` | after `:3532`; covers the in-place move at `:3525` |
| `RepointBayToFile` | after `:3583` |
| `SetImageWriteProtect` | after `:1481` |
| `FlushEntry` | after `:1177` and `:1242`, where the CRC mismatch is cleared |
| The two new setters | in their bodies |

Read-only entry points do not bump. That includes `ApplyPendingReload`'s idle walk, which runs about once a frame from the idle callback (`MachineBuilder.cpp:1542-1545`). This is what keeps the idle cost at the generation compare.

`GetImage`, `IsMounted`, `GetSourcePath`, `GetMountedSourcePaths` and `IsSalvageOffered` (`:2411-2485`, `:1602`) keep their signatures. They become emulation-thread only by contract, which SC-003 asserts.

#### Outside the store

- **`MachineHost::LoadStateSeating`:** after `Error:` (`MachineHost.cpp:1153`), call `m_diskStore->MarkStatusChanged();`. Both a full and a partial load have rewritten image flags (`DiskImage.cpp:1303-1304`).
- **`DiskManager::ApplyExternalWriteProtect`** (`DiskManager.cpp:152-177`): the signature becomes `(int drive, const std::string & path)`. After the probe it calls `m_diskStore.SetUserWriteProtectFlag` and `SetFileWriteProtect`. Call sites: `:268` and `:571`; declaration: `DiskManager.h:146`.
- **`EmulatorShell::SetDriveUserWriteProtect`** (`EmulatorShellDisks.cpp:810-815`): this already runs on the emulation thread, through `IDM_DISK_WRITEPROTECT1/2` at `CpuCommandDispatcher.cpp:97-101`, which 035 journals as `DriveWriteProtect` and treats as state-changing. It switches to `GetDiskStore().SetUserWriteProtectFlag (6, drive, wp)` now, and to FR-015's saving `SetUserWriteProtect` when that lands (tasks.md T104). The command id stays the same.
- **`Replayer::ApplyInput`** (`Replayer.cpp:479-487`): switches to `GetDiskStore().SetUserWriteProtectFlag (kDiskControllerSlot, record.value, record.detail != 0)`. A replay never saves.
- **`Disk2Controller.h:179`:** add `const Disk2NibbleEngine &  GetEngine (int drive) const { return m_engine[drive]; }`.

#### `DiskManager` (`CassoEmuCore/Shell/DiskManager.h`, `.cpp`)

```cpp
    DriveStatusChange                   PublishDriveStatus    (bool isActivityDue);   // emulation thread
    void                                SetStatusWake         (std::function<void ()> wake);   // called on a Bays publish
    std::shared_ptr<const DriveStatus>  TakeLatestDriveStatus () const;               // any thread
    const DriveStatus &                 GetShownDriveStatus   () const { return *m_shownStatus; }   // window thread
private:
    void     ApplySyncEvents    (DriveWidgetState & st, int drive,
                                 const std::vector<DriveWidgetController::DriveSyncEvent> & events, int64_t nowMs);
    void     SyncDoorFromBay    (DriveWidgetState & st, int drive, const BayStatus & bay, int64_t nowMs);
    void     SyncDriveActivity  (DriveWidgetState & st, int drive, const DriveStatus & status, int64_t nowMs);

    DriveStatusPublisher                m_statusPublisher;
    std::shared_ptr<const DriveStatus>  m_shownStatus        = m_statusPublisher.Take();
    uint64_t                            m_shownBayGeneration = 0;
```

- **`PublishDriveStatus`** returns `m_statusPublisher.Publish (m_diskStore, FindSlot6Controller(), isActivityDue)`, and calls the wake `SetStatusWake` installed when that result is `Bays`.
- **`MountDiskInSlot6`** (`:481-488`): in `Error:`, call `PublishDriveStatus (true)` before `m_onMountCompleted`. This is P3.
- **`UpdateDriveWidgets`** (`:791-905`): rewritten and split into the three helpers, since it is about 115 lines today.
  - `m_shownStatus = m_statusPublisher.Take();`
  - `isBaysMoved = status.GetBayGeneration() != m_shownBayGeneration`.
  - Per drive:
    - door from `bay.path`, replacing `:803`;
    - activity from `status.GetActivity (drive)` and `HasController()`, replacing `FindSlot6Controller` at `:793` and the engine reads at `:865-878`;
    - `st.writeProtect = bay.writeProtect` only when `isBaysMoved`, replacing `:894-899` and its per-frame vector copy.
  - Then `m_shownBayGeneration = status.GetBayGeneration()`.
  - Activity behavior is identical to today: active means the counters differ from last frame's.

#### `EmulatorShell`

```cpp
    void                 ServiceCpuThread    ();                     // ServiceDebugger(); ServiceHistoryThumbnails(); m_diskManager->PublishDriveStatus (m_cpuManager.IsPaused())
    const DriveStatus &  GetShownDriveStatus () const;               // m_diskManager's shown status, or a static empty one
```

- **The wake:** the shell installs it with `m_diskManager->SetStatusWake ([this] { SetEvent (m_frameReadyEvent); })` where it creates the `DiskManager` (`InitAssetPathsAndStores`, `EmulatorShell.cpp:687`); it is the same `SetEvent` that `PublishFramebuffer` uses (`EmulatorShellPresent.cpp:1146-1150`). `DiskManager::PublishDriveStatus` calls it exactly when the result is `Bays`, so the FR-003 test asserts the change and the wake through the `DiskManager` rig with a counting wake (tasks.md T030), because a shell built in a test has no `DiskManager` (`EmulatorShell.cpp:687`, reached only from `Initialize` at `:404`).
- **P1:** `EmulatorWindow.cpp:1343` becomes `m_cpuManager.SetServiceFunction ([this] { ServiceCpuThread(); });`. `ServiceCpuThread` is a fixed sequence of three calls with no decision of its own, and the Debug app run confirms it on screen (tasks.md T051).
- **P2:** `EmulatorShellCpuThread.cpp:1006` becomes `isPublishDue = ShouldPublishFrame(); if (isPublishDue)`. After `:1029`, add `m_diskManager->PublishDriveStatus (isPublishDue);`.
- **P4:** `EmulatorShell.cpp:626`, after `MountCommandLineDisks`, add `m_diskManager->PublishDriveStatus (true);`. This runs before the emulation thread starts at `EmulatorWindow.cpp:1346`.

#### Window-thread call sites that switch

| Site | Today | Becomes |
|---|---|---|
| `DiskManager.cpp:793`, `:803`, `:865-878`, `:894-899` (`UpdateDriveWidgets`, every frame from `EmulatorShellPresent.cpp:637`) | `FindSlot6Controller`, `GetSourcePath`, engine getters, `GetImage` and `GetWriteProtectInfo` | the shown status (above) |
| `EmulatorShellPresent.cpp:780` (desk scene label change check) | `GetSourcePath` | `GetShownDriveStatus().GetBay (6, i).path` |
| `EmulatorShellScene.cpp:1009` (`SyncSceneDriveLabels`; reached from `EmulatorShellPresent.cpp:791`, `:939`, `SyncSceneDriveChrome :914` ← `EmulatorWindow.cpp:838`, `:2005`, `EmulatorShellChrome.cpp:789`, `:821`, `:835`, `:873`, `EmulatorShell.cpp:1102`) | `GetSourcePath` | the shown bay's path |
| `EmulatorWindowInput.cpp:505`, `:525`, `:543` (tooltips) | `GetSourcePath` | the shown bay's path |
| `EmulatorWindow.cpp:755-758` → `EmulatorShellDisks.cpp:771-781` (`IsWriteProtectToggleOffered`) | `GetImage`, `IsMounted`, `GetWriteProtectInfo` | `bay.isMounted && ShouldEnableWriteProtectMenuItem (true, bay.writeProtect)` |
| `EmulatorShellDisks.cpp:578` (`IsSalvageOffered`) | store `IsSalvageOffered` | `bay.isMounted && bay.isSalvageOffered` |
| `EmulatorWindow.cpp:777`, `:786`, `:794` (menu label query) | `GetImage`, `GetSourcePath`, `GetWriteProtectInfo` | `bay.isMounted`, `bay.path`, `bay.writeProtect` |
| `EmulatorShellDisks.cpp:696`, `:722`, `:733-734`, `:737` (`ReportDamagedMount`, after `WM_APP_MOUNT_COMPLETED`) | `GetImage`, `IsDamaged`, `FormatBody (*image, …)`, `AssessSalvage` on the window thread | The drive's `BayStatus`, copied into `MountCompletion` with `GetBayStatus` at P3 on the emulation thread (not `TakeLatestDriveStatus()`, which by the time the window handles the message can describe a later or different medium); `bay.writeProtect.IsDamaged()`; `FormatBody (bay.writeProtect, file name)` (`DamagedMountReport.cpp:170`); the button from `bay.isSalvageOffered` |
| `EmulatorShellDisks.cpp:621` (`RunSalvageFlow`) | `GetSourcePath` | `offer.assessment.sourcePath`, the path `AssessSalvage` recorded on the emulation thread with the counts and the media id; the assessment and the write move with the salvage decision (FR-004) |
| `EmulatorShellDisks.cpp:1157` (`AskAboutChange`, after its modal) | `GetSourcePath` | `notice.prompt.imagePath`, per entry-path-string-race; the status is the wrong source, because the question is about the file it was composed from |
| `WindowCommandManager.cpp:1221` | `GetMountedSourcePaths` | `GetShownDriveStatus().GetMountedSources()` |
| `WindowCommandManager.cpp:1281` | `IsMounted (6, drive - 1)` | `DiskManager::IsReplacePromptNeeded (drive - 1)`: the shown bay's `isMounted` and not `DriveWidgetState::isEjectPosted` (entry-flags-race Part B) |

Notes on the `ReportDamagedMount` row:
- `isSalvageOffered` is the same predicate the dialog uses today, `SUCCEEDED && isOffered` (`DiskImageStore.cpp:296-298` vs `EmulatorShellDisks.cpp:739`).
- It holds as long as the disk is in the bay, because a damaged image is held read-only.
- It also removes an 11 to 154 ms decode from the window thread (`EmulatorShellDisks.cpp:574-577`).

These need no change, because they read `DriveWidgetState`, which belongs to the window thread and is now fed from the snapshot:
- `EmulatorShellPresent.cpp:654-703`, `:733`, `:753-768` (the desk scene's `SetDriveVisuals`);
- `EmulatorShellScene.cpp:1020`;
- `EmulatorWindowInput.cpp:500`, `:507`, `:530`;
- `EmulatorShell.h:1072-1121` (theme preview, `SettingsSheet.cpp:432`);
- `EmulatorShellDisks.cpp:446`.

These stay with FR-006 (layout reads machine facts), not with this decision: `DeskSceneDriveCount` (`EmulatorShellScene.cpp:743-765`), `ShouldShowExternalDrive` (`EmulatorShellChrome.cpp:1188-1200`), and the `HasSlot6Controller` window callers (`EmulatorShellPresent.cpp:804`, `EmulatorShellChrome.cpp:933`, `:1094`, `EmulatorShell.cpp:1079`, `EmulatorWindow.cpp:1934`, `:1956`).

#### `DriveWidgetController` (FR-008)

- **`CassoEmuCore/Ui/DriveWidgetController.h:61-62`:** add `std::mutex  m_syncMutex;` ahead of the two members, with the existing column alignment.
- **`.cpp:126-143`:** fill `driveId`, `action` and `timestampMs` first. Then, in one locked scope, `evt.eventId = m_nextSyncEventId++; m_syncEvents.push_back (evt);`. Assigning the id inside the lock keeps ids in push order whichever thread publishes.
- **`.cpp:155-164`:** swap inside the lock.
- **The lock is a leaf:** neither method calls out while holding it.
- **Producers:** `OnBayChange` (`DiskManager.cpp:604`, `:613`, `:624`) on the emulation thread, and today the window thread's salvage insert (`EmulatorShellDisks.cpp:674`). Consumer: `UpdateDriveWidgets :795`. A mutex is correct whether or not salvage moves.

#### Comments and documents (FR-022)

These comments are wrong once this lands and need rewriting:
- `DiskImageStore.h:715-718`;
- `DriveWidgetState.h:17-34`: the atomics are written only by the window thread; keep the atomics and fix the comment;
- `DiskManager.h:30-37`;
- `DiskManager.cpp:783-788` and `:860-863`;
- `Disk2NibbleEngine.h:79-82`: the counters are read on the emulation thread by the publisher;
- `DriveWidgetController.h:12-23`: one line on its threading;
- the threading section of `ARCHITECTURE.md`.

#### Cost

Measured with a scratch microbenchmark of this design (MSVC v145, `/O2`, x64, Ryzen 9 9950X3D, three runs, each within 5% of the others). It models the data structures, not the real `Entry` and engine layout, so treat it as a lower bound and allow 2 to 3 times as much.

| Operation | ns |
|---|---|
| Idle check (nothing changed, nothing published) | 6 |
| Activity publish (`make_shared` + lock + swap) | 61-63 |
| Bay rebuild + publish (2 mounted bays, one damaged with 10 quarter tracks) | 250-260 |
| Window `Take` (lock + `shared_ptr` copy) | 21 |
| Debug verifier (rebuild + compare) | 229-235 |
| Activity publish while another thread spins on `Take` (worst-case contention) | 265-279 |

What that means per frame at 1x, where a frame is 16.67 ms and 1% of a frame is 166.7 µs:
- **Idle:** about 2 × (6 to 20) ns, for the P1 and P2 checks. That is about 0.0002% of a frame, about 4,000 times under the SC-005 limit.
- **Drive running:** add about 63 ns.
- **A bay change:** about 250 ns, once per change.
- **Window thread:** about 21 ns per frame. That replaces today's per-frame `GetWriteProtectInfo`, which allocates a vector every frame for a damaged disk (`DiskImage.cpp:559-562`).
- **Paused:** one check every 20 ms.
- **Maximum speed:** the P1 check runs every pass, but publishes only bay changes; activity is capped at about 60 Hz by P2.

### Test plan

New source files, and new test files, go into `CassoEmuCore.vcxproj` / `UnitTest.vcxproj` in the task that creates them (the repo has no `.filters` files). Run `CheckStyle -Mode Staged` before the commit.

1. **`UnitTest/EmuTests/DriveStatusPublisherTests.cpp` (new).** Built over a `TestMachine ("Apple2e")` store and its slot 6 controller.
   - `TakeBeforeAnyPublishIsAnEmptyMachine`: not null, every bay empty, no controller, head at -1.
   - `PublishWithNothingChangedPublishesNothing`: returns `None`, and `Take` returns the same pointer.
   - `AnActivityChangeRepublishesAndSharesTheBays`: a new sequence, with `&GetBay (6, 0)` the same address in both snapshots.
   - `AnActivityChangeNotDueWaitsForABayChangeToBringIt`: with `isActivityDue` false nothing is published; the next bay change publishes the activity as it is now.
   - `ABayChangeReportsBaysAndShowsTheDisk`: path, format, `isMounted`.
   - `AHeldStatusOutlivesTheEjectAndTheNextPublish`: the ui-derefs scenario. The held snapshot still holds the old path and causes after the image is freed.
   - `EveryBayWriterMovesTheGeneration`: a sweep over `MountFromBytes` (good bytes and bad bytes), `Eject`, `SeatMedia` with retention, `SeatMedia (0)`, `SetImageWriteProtect`, a conflict `Flush` (to `RepointBayToFile`, using the `SharedImageTests` rig seams), `DiscardHeldWrites` (to `MountExternallyModifiedDisk`), the two new setters, and `MachineHost::LoadState`. Each step asserts that the generation moved and that the published bay equals `GetBayStatus`. Mutation check: delete any one bump and confirm its step fails.
   - `AWriteThatSkipsTheGenerationAssertsInDebug`: a raw `GetImage()->SetUserWriteProtected (true)` and then `Publish`, inside `ExpectedEhmAssert` (`UnitTest/EhmTestHelper.cpp:101-161`), expecting one assert. This guards the verifier itself.
   - `MountedSourcesMatchTheStore`: the same result as `GetMountedSourcePaths`, including skipping an empty virtual path.
   - `TenThousandBayChangesAgainstAConcurrentReader` (SC-002), following `Devices/Disk2EventRingTests.cpp:181`. Superseded: SC-002 has one stress test, R6's `DriveStatusStressTests` (tasks.md T124), with the writer taking the store through R5's `OwnerThreadStandIn::Take`; this item is kept for its rules, which that test follows:
     - The writer thread cycles mount (two paths, one of them the damaged WOZ fixture), eject, `DiscardHeldWrites` and `SeatMedia`, then calls `Publish`.
     - The reader thread spins on `Take` and walks every field.
     - Invariants (`isMounted == !path.empty()`, path in the known set, `damagedTracks == !damagedQuarterTracks.empty()`) are counted, not asserted, inside the threads.
     - Every wait has a deadline, and all asserts come after `join`.
     - The writer must use only legal store calls, because the test host's EHM handler throws through `Assert::Fail` (`EhmTestHelper.cpp:31-48`).
     - It runs under the sanitizer configuration SC-002 calls for. None exists in the tree today; otherwise it runs under the Debug CRT heap checks.
2. **`UnitTest/EmuTests/DiskImageStoreTests.cpp`:**
   - `BayStatusReportsEmptyInvalidAndMountedBays`.
   - `TheStatusGenerationStaysPutOnReadsIdleWalksAndCleanFlushes`: `ApplyPendingReload` with nothing pending, `Flush` of a clean disk, and the accessors do not bump. This pins the idle-cost claim.
3. **`UnitTest/EmuTests/DiskManagerDriveStatusTests.cpp` (new).** Uses the `Shell` harness from `DiskResetRemountHoldTests.cpp:44-56`; move it to a shared `EmuTests/DiskManagerRig.h` rather than copying it.
   - `DriveWidgetsShowThePublishedBayNotTheLiveImage`: fails today, because `UpdateDriveWidgets` reads the image (`DiskManager.cpp:895-897`). After a raw flag write with no publish, the widget is unchanged; after `MarkStatusChanged` and a publish, it follows.
   - `DriveWidgetsShowThePublishedActivityNotTheLiveEngine`: fails today (`:867-871`).
   - `AMountWhilePausedShowsAfterTheServicePublish`: FR-003, at the `DiskManager` level: `MountDiskInSlot6`, then `PublishDriveStatus (true)`, then `UpdateDriveWidgets` shows the door closing on the path. It cannot fail before `UpdateDriveWidgets` reads the published status, so tasks.md T035 keeps it as a guard that passes before and after that change.
   - `TheMountIsPublishedBeforeItsCompletion`: a completion callback that calls `TakeLatestDriveStatus` reads its own path. Fails without P3.
   - `APublishWhilePausedGivesAMountAsABayChangeAndOneWake`: FR-003 at the `DiskManager` level, in place of the shell-level test this record first proposed. The rig installs a counting wake with `SetStatusWake`. A mount through the store, then `DiskManager::PublishDriveStatus (true)`, returns `DriveStatusChange::Bays`, the mount shows in `TakeLatestDriveStatus`, and the wake is called once; a second publish with nothing changed returns `None` and calls no wake; an activity-only change returns `Activity` and calls no wake. It fails until the publish calls the wake (tasks.md T031). The shell-level version cannot run: `ServiceCpuThread` publishes through the shell's `DiskManager`, which only `Initialize` creates (`EmulatorShell.cpp:404`, `:687`), and every test-built shell skips `Initialize` (`UnitTest/DebuggerTests/EmulatorDebugWiringTests.cpp:1694`, `UnitTest/ControllerTests/PlayerModeRulesTests.cpp:343`). What is left to the shell, `ServiceCpuThread`'s fixed sequence of three calls, is confirmed on screen in tasks.md T051's Debug run, by an eject while paused.
   - `AUserWriteProtectThroughTheStoreReachesTheWidget`.
4. **`UnitTest/UiTests/AnimationSyncTests.cpp`:** `DriveSyncBroker_ConcurrentPublishAndConsume_LosesNothingAndKeepsOrder`, as specified in `audit.md` (drive-widget-sync-events-unlocked). `DriveSyncBroker_PublishAndConsumeWithinFrame` (`:94`) must keep passing unchanged.
5. **`UnitTest/EmuTests/DiskFlushHoldTests.cpp`:** `ThePublishedStatusFollowsSeatingDiscardAndEject`, using retention (`:143`) and `PrepareDirtyDisk` (`:220`).
6. **`UnitTest/EmuTests/DiskHistoryTests.cpp`:** `SeekingAcrossAnEjectRepublishesTheEmptyBay`, modeled on `:313` and `:370`. `SeatMedia` reports no bay change, so this fails if the generation bump in `SeatMedia` or `RetireBay` is missing.
7. **`UnitTest/EmuTests/MachineHostStateTests.cpp`:** `LoadingAStateMovesTheDriveStatusGeneration`, using `MountPatternDisk` (`:351-371`). Save with the user write-protect off, set it on, load, and assert the generation moved and `userSetting` is false.
8. **`UnitTest/EmuTests/PerformanceTests.cpp`:** `DriveStatusPublish_IdleCostsUnderOnePercentOfAFrame`, Release only (`#ifdef NDEBUG`), following `SharedImageIdleProbe_CostsNothingTheUserCanFeel` (`:244-313`). It asserts the idle publish is under 1% of a frame, and logs the activity and bay publish costs.
9. **Unchanged:**
   - `Devices/Disk2StateTests.cpp`: engine fields stay plain, so the `StateProbeEngine` comparisons still compile.
   - `UiTests/DriveWidgetStateTests.cpp`.
   - `DiskImageStoreTests MountedSourcePaths_ReportsEveryMountedBay`.

Count from this decision: 21 tests in Debug and 22 in Release (the extra one is the Release-only performance test), on top of SC-004's baseline.

### Risks

- **A missed generation bump shows a stale drive in Release.**
  - Mitigation: the Debug verifier, the writer sweep, and the history and state-load tests.
  - 035 is still adding writers (it added three); re-run the Debug suite at every 035 merge, which is when a new writer would appear.
- **Messages from the emulation thread whose handler reads drive status.** P3 covers the mount completion. Any new emulation-thread code that posts a message whose handler reads drive status has to publish before it posts; document this at `PublishDriveStatus`. The change prompt and the write-protect notice bring their own data and need nothing.
- **Hard ordering dependency on the salvage move (FR-004).**
  - `RunSalvageFlow` still calls `MountDiskInSlot6` on the window thread (`EmulatorShellDisks.cpp:674`). With P3, that would publish from the window thread while the emulation thread publishes.
  - Land salvage-on-the-emulation-thread first or in the same change. The Debug owner assert in `Publish` catches any other caller.
- **The window's view lags by at most one service tick (20 ms) or one frame.** Bay changes wake the loop, so in practice it is one loop iteration. A menu opened inside that window shows the state before the change, as it can today for a posted mount.
- **Two sources for "has a controller".** `DriveStatus::HasController` serves the drive widgets; FR-006's copy serves layout. They can disagree for one frame during a machine switch. Layout must keep using FR-006's.
- **The "active" lamp at window frame rates above the emulation thread's** behaves the same as today, because the counters are compared frame to frame either way. The 700 ms fade covers it.
- **`DriveWidgetState`'s atomics remain** (`DriveWidgetState.h:63-64`, `:82`). They are harmless but misleading until the comment is fixed; leaving them avoids churn in `DriveWidget` and the theme preview.
- **Adjacent race, outside this decision.** `DiskManager::m_coldBootMountWindow` (`DiskManager.h:204`) is a plain `bool`:
  - it is written on the window thread at `EmulatorWindow.cpp:1364`, after the emulation thread starts at `:1346`;
  - it is read on the emulation thread in `OnBayChange` (`DiskManager.cpp:558`).

  It should be an atomic, or set through a command.
- **SC-002 requires "no sanitizer report"**, but the tree has no address sanitizer configuration (no `EnableASAN` in any project or props file). Until one is added, the stress test runs under the Debug CRT heap checks only.

## R2. What each action does when the save before it fails (FR-009, FR-010)

All paths are relative to `C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity` at `fb05ee2ab`.

### Decision

**Invariant.** A disk leaves its bay only when every write the guest made to it is already in a file. That means one of two things:

- the bay's own file holds the writes, so the image is clean after the save; or
- the change goes to a *different* file (or is an eject), and a lossless recovery copy was written from exactly the current content. This uses FR-016's per-mount recovery record.

If neither is true, the disk stays in its bay: attached to the drive, still watched, not retired. The change is either declined or waits for an answer. A declined change does not call `RetireBay`, `EmitBayChange` or `NotifyMediaChanged`, so reverse execution records no boundary, drops no future and retains no medium for it.

**Load first, except over the disk's own file.** A mount of a different file reads and loads the new image before the old disk is touched (FR-009). A remount of the bay's own file has to work the other way round: save the old disk first, then read the file. Today's order is wrong:

- `Mount` reads the file at `CassoEmuCore/Devices/Disk/DiskImageStore.cpp:616`.
- `MountFromBytes` then flushes the old disk into that file at `:265`.
- It then builds the new image from the bytes read *before* the flush (`:271-276`).

So a Reset taken while a dirty disk's motor is still spinning writes the guest's writes to the file but puts a disk without them in the drive. The next flush then writes that stale base back over the file, and the writes are gone from both places. `audit.md` does not record this defect.

**Spec amendment needed.** FR-009 should read: "load the new image before retiring the old one; when the new file is the old disk's own file, the old disk is saved first and the file is read after the save."

#### Per action

| Action | Save attempted | When the writes end up in no file | Does the action go ahead? | What the user sees |
|---|---|---|---|---|
| Insert of another file into an occupied drive (`IDM_DISK_INSERTn`, a tool's `InsertDisk` intent at `Shell/Window/EmulatorWindow.cpp:2761-2768`) | Read and load the new file; then `FlushEntry (old, Replacing)` | Old disk kept; `MountFailure::UnsavedWrites`. An unserializable disk with a current recovery copy counts as saved, so the insert goes ahead. | No | One dialog from `HandleMountCompletion` with the save error. A tool gets the same text as its reply. The door closes again. |
| Re-insert of the same file (user) | Save first, then read, load and swap. When the save leaves nothing unsaved and the file's identity still matches the bay's recorded identity (both recorded), the mounted image stays: no read, no swap, no `RetireBay` and no media change | Disk kept and the file is not re-read. A recovery copy does **not** allow it: re-reading would replace the session with the file's older content. | No | The save-failure text, unless this mount has already reported it |
| Reset (`Shell/CpuCommandDispatcher.cpp:62-71`) | Same as re-insert for each drive. Then `MachineHost::SoftReset` calls the store's `SoftReset` (`FlushAllUnlessHeld`, Background) at its top, which FR-017 added in `e32b2b68c`; before that commit `MachineHost::SoftReset` never reached the store and `DiskImageStore::SoftReset` had no production caller | Disk kept; the reset runs | Reset yes, re-read no | At most one notice per mount until a save lands |
| Power cycle (`:73-76`) | `MachineHost::PowerCycle` → `FlushAllUnlessHeld` (`Shell/MachineHost.cpp:984`), then the same-file remount | Power cycle runs. `BindDiskDrives` (`:992`) keeps both drives on the bays' images; the remount is declined. | Yes | One notice |
| Machine switch (`Shell/MachineManager.cpp:399-606`) | `FlushAll` (`:406`), then `PowerCycle` (`:587`), then the remount of the disks the switch keeps (`:606`, always the same file) | **The switch is not stopped; the store survives it** (details below). | Yes | One notice, from the switch's `FlushAll` |
| Eject (`EjectDiskInSlot6` → `Eject`) | `FlushEntry (Ejecting)` | Recovery copy current: the eject goes ahead. Otherwise a question (**Save as...** / **Eject and discard**) and the eject waits for the answer (the existing `EjectWhenAnswered` route). If no question can be raised (no ask sink, the post failed, or a question is already open), the eject is declined. | Waits, or no | The question, or "The disk was not ejected..." The door closes again. |
| External-change reload (`ApplyPendingReloadToBay`) | Guest copy through `SaveLoadedImage` when the guest is dirty, after FR-012 commits an open flux write before the dirty test at `:2914` | Unchanged: disk kept, question (**Save as...** / **Dismiss**), pending change dropped (`:2937-2971`) | No | Existing `ComposeSaveFailure (ExternalChange)` text, which is already true |
| Salvage insert | Same as insert, once `RunSalvageFlow` posts it (salvage-mount-on-ui-thread fix) | The damaged disk is write-protected through `IsDamaged` (`Devices/Disk/DiskImage.cpp:527-534`), so its save always succeeds. Only an unloadable salvaged copy (FR-009) declines the insert. | Per FR-009 | FR-009 message |
| Shutdown (`Shell/EmulatorShell.cpp:299`) | `FlushAllForShutdown` | **Unwritable:** `RescueOnTheWayOut` first. Today it runs only for the external-change case (`DiskImageStore.cpp:368-380`); afterward a notice states the changes cannot be kept. **Unserializable:** recovery copy as today; if that copy fails, the rescue writes WOZ. | n/a | Rescue dialog, or the "Casso is closing" notice |
| Load state (the only caller of `MountRestored`) | Every bay saved *before* the other-machine switch and `StopReverseRecording` (`Shell/EmulatorShellState.cpp:172-187`), so a declined load of another machine's state leaves the machine, its disks and its history as they were | Load declined with "disk not saved"; history kept | No | State error notice |

**Why the machine switch is not stopped.**

- `MachineHost` owns the store (`Shell/MachineHost.cpp:41`), and the teardown at `MachineManager.cpp:484-527` never touches the bays.
- `PowerCycle`'s `BindDiskDrives` points the new controller at the kept image.
- The switch's remount is the same file, so it is declined and the disk stays.
- With no slot-6 Disk II on the new machine, the bay still holds the disk and `FlushAllForShutdown` still reaches it.

**Reporting cadence.** A user action always reports. A flush nobody asked for reports a failing bay once per mount, until a save succeeds or the disk leaves. This is FR-016's "report once", applied to write failures as well as recovery copies.

### Rationale

**Today the save result is ignored and the disk is thrown away anyway.**

- `MountFromBytes` drops `FlushEntry`'s result and retires the bay (`DiskImageStore.cpp:263-268`). `Eject` does the same (`:2304-2305`, `:2319-2322`).
- `FlushEntry` keeps the dirty bit on failure (`:1105-1110`), but on an image that `RetireBay` has already taken out of the bay.
- With retention on, that image sits in `m_retained` (`:2203-2213`). Nothing flushes it: `FlushEveryBay` walks only `m_entries` (`:2262-2273`). `PruneRetainedMedia` and `SetMediaRetention (false)` later free it (`:2179-2182`, `:2065-2068`).

**The message is false.** `FormatFlushLossMessage` ends with "The disk in the drive still has them" (`:909-912`) even on the eject that then empties the bay. The same text appears at shutdown.

**A failed load leaves the drive on a dead disk.** `MountFromBytes` empties the bay (`:278-288`). `Mount`'s `CHR` (`:620`) then skips `EmitBayChange`, so the controller keeps a pointer to a retired or freed image (audit `failed-mount-dangling-disk-image`).

**Declining before `RetireBay` is the one cut point that needs no compensation.**

- `Mount`'s failure path already skips `sharedState.Mount`, `BeginWatching`, `EmitBayChange` and `NotifyMediaChanged` (`:626-640`). Every one of those is correct for a bay that still holds its old disk.
- The flush hold needs nothing new: the save moves after a successful load, so a declined mount writes nothing, held or not.

**The eject question already exists.** `ReportPreserveFailure` sets `EjectWhenAnswered` (`:410-413`), `Eject` waits on it (`:2312-2315`), and `ResolvePendingChange`'s Conflict branch finishes the eject for either answer (`:3107-3176`). Plain write failures join that route instead of adding a second one.

**Insert is declined rather than asked, for three reasons.** Asking would need a second pending-action mechanism that keeps a path until the answer comes and then re-runs a mount that can itself fail. A tool's `InsertDisk` would wait on a dialog. And the eject question already offers both answers.

**The decline test is "writes are in a file", not `FAILED (hr)`.** The two differ in exactly one case: `FlushEntry` returns `S_OK` while replaying (`:1090`) and leaves the image dirty. `ReverseController::Stop` clears the hold but not the replay flag (`Debugger/Reverse/ReverseController.cpp:137-144`). So after a forward step from the past, a machine switch's `FlushAll` writes nothing and the switch's remount would retire the dirty disk with retention already off (Stop ran at `MachineManager.cpp:401`). Testing the data catches that case without adding a flush the hold or replay rules forbid. 035 fixed `Stop` (041 audit defect a, `c913c9247`, which tasks.md T008 brings); the data test does not depend on that fix and stays, because it also keeps any other skipped save from letting a disk leave its bay, and without the fix it still keeps the disk (tasks.md T008 and T137 confirm the fix before the merge).

### Alternatives considered

1. **Empty the bay and emit `Ejected` on a failed load** (master's second option). Rejected:
   - the guest's disk goes into `m_retained` after a flush that wrote it anyway;
   - a Reset while a build tool is mid-write would leave an empty drive (`audit.md:26`).
2. **Let retention keep the outgoing disk.** Rejected: retained disks are never flushed and are freed by pruning or by `Stop` (`DiskImageStore.cpp:2061-2069`, `2179-2182`).
3. **Write a lossless recovery copy for every failed save, then proceed.** Rejected for write failures:
   - the same folder is usually the one that is full or denied;
   - on a same-file remount it would revert the drive to the file's older content.

   Kept only for unserializable images, where the WOZ copy is the only lossless save that exists, and only for a different file or an eject.
4. **Ask on insert** (Save as / Discard and insert / Keep). Rejected: see Rationale. Revisit if the owner wants a one-step path.
5. **Stop the machine switch.** Rejected:
   - nothing is at risk, because the store survives the switch;
   - a pre-flight would have to run before `AdoptInputModeForMachine` and `StopReverseRecording` (`MachineManager.cpp:396-401`);
   - it would block an unrelated user action.
6. **Decline on `FAILED (hr)` only** (the audit's proposal). Rejected: misses the replay-flag case above.
7. **Load first even for the same file** (FR-009 taken literally). Rejected: stale-read data loss.
8. **Journal media commands after dispatch, so a declined one leaves no record.** Rejected:
   - it changes 035's journal-before-dispatch rule (`Shell/EmulatorShellCpuThread.cpp:350-355`);
   - the record is inert in any case, because `Replayer::ApplyInput` skips media kinds (`Debugger/Reverse/Replayer.cpp:470-475`).

### Design detail

#### Types

`CassoEmuCore/Devices/Disk/DiskImageStore.h` (private), replacing `FlushMoment` at `:520-540`:

```cpp
    enum class FlushMoment
    {
        Running,        // one bay, a person asked: explicit Flush, write-protect changes. Reports every failure.
        Background,     // every bay, nobody asked: spindown, reset, power cycle, switch, state load. Reports once per mount.
        Replacing,      // the save before a mount over the bay. Reports nothing but a first recovery copy; the mount reports a decline.
        Ejecting,       // raises the question; the eject waits for the answer.
        ShuttingDown,   // blocking rescue, then a notice.
    };

    //  What a save that missed the bay's own file left behind.
    struct FlushOutcome
    {
        HRESULT  saveError        = S_OK;
        bool     isUnserializable = false;
        string   recoveryPath;
        HRESULT  recoveryError    = S_OK;
        string   movedFrom;    // the file another program changed, when the guest copy moved off it
        string   movedTo;      // where the guest copy went
    };
```

`FlushAll` and `FlushEveryBay` use `Background`. `Flush`, `SetImageWriteProtect` and `SetUserWriteProtect` (FR-015) use `Running`.

Order of landing (tasks.md): `Background` and `FlushOutcome` come first, with no change in behavior (T063), so FR-016's report-once rule (T065) applies to `Background` saves only and a `Running` save still reports every failure. `Replacing` comes with the different-file mount (T096); `Ejecting` and `ShuttingDown` exist today and gain their R2 rows in T098 and T100.

`Devices/Disk/MountDiagnosis.h`:

- Add `UnsavedWrites` to `MountFailure` (`:57-70`), with its line in the documentation block.
- Add fields to `MountDiagnosis` (`:96-131`):

```cpp
    string   keptPath;             // the disk still in the target drive after any decline over an occupied bay
    HRESULT  saveError   = S_OK;   // UnsavedWrites: why the writes are in no file (S_OK: not yet saved)
    bool     isSameFile  = false;  // UnsavedWrites: the new file was the kept disk's own
    bool     wasReported = false;  // UnsavedWrites: this mount's save failure had already been reported
    bool     ShouldReport () const; // false only for a same-file decline already reported
```

- Change the comment that calls the class "plain data and trivially copyable".

`Devices/Disk/ChangePrompt.h`:

- Add two values to `SaveFailureCause` (`:43-59`), both found only while ejecting:
  - `Unwritable`: the disk's own file could not be written.
  - `Unserializable`: its format cannot hold the writes, and the WOZ copy failed.
- Add `std::string suggestedSavePath;` to `ChangePrompt` (`:124-152`).

`Devices/Disk/MountedImageState.h`: FR-016's recovery record (R3), plus the asked cause. Both are cleared in `Mount` and `Eject` (`MountedImageState.cpp:15-56`).

```cpp
    SaveFailureCause  GetAskedCause () const;
    void              SetAskedCause (SaveFailureCause cause);
```

The save-failure report state is R3's, in the recovery record, and this record uses it rather than adding its own: "has this mount reported a save failure" is `IsSaveFailureReported()`, true when either report field is set (the copy path last reported, or `m_isPlainFailureReported`), so a same-file decline right after its own save reported a first recovery copy is silent and one event gets one text (contracts/user-messages.md section 2); setting the plain field is `MarkSaveFailureReported (string())`, and `ClearRecovery()` clears both with the rest of the record. An earlier draft of this record added `HasReportedSaveFailure` and `SetSaveFailureReported` for the same fact; they are not part of the design.

#### Store functions

`DiskImageStore.h`, public:

```cpp
    bool     AreAllWritesSaved () const;                                   // every bay: clean, or recovery current
    void     SetFileBacked     (bool isFileBacked) { m_isFileBacked = isFileBacked; }   // false: scratch stores
```

`DiskImageStore.h`, private:

```cpp
    HRESULT  FlushEntry          (Entry & entry, FlushMoment moment);
    HRESULT  FlushEntry          (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome);
    bool     AreWritesSaved      (const Entry & entry) const;              // !IsDirty() || IsRecoveryCurrent (entry)
    bool     IsRecoveryCurrent   (const Entry & entry) const;              // FR-016 record + undisturbed identity
    static HRESULT  LoadImage    (DiskFormat fmt, const vector<Byte> & bytes, const string & path,
                                  unique_ptr<DiskImage> & outImage, MountDiagnosis & outDiagnosis);
    HRESULT  SaveBeforeReplace   (Entry & entry, const string & newPath,
                                  MountDiagnosis & outDiagnosis, FlushOutcome & outOutcome);
    HRESULT  ReplaceBay          (int slot, int drive, const string & path, DiskFormat fmt,
                                  const vector<Byte> & bytes, MountDiagnosis & outDiagnosis,
                                  FlushOutcome & outOutcome);
    void     SwapInImage         (Entry & entry, unique_ptr<DiskImage> loaded,
                                  const string & path, DiskFormat fmt);    // EndWatching, RetireBay, seat, AssessSalvage
    void     ReportSaveFailure   (Entry & entry, FlushMoment moment, const FlushOutcome & outcome);
    void     ReportReplaceOutcome (int slot, int drive, const string & newPath, HRESULT hr,
                                   const FlushOutcome & outcome);          // the moved-copy report, after the outcome is known
    HRESULT  PreserveConflictingCopy (Entry & entry, FlushMoment moment, FlushOutcome & outOutcome);  // extracted from FlushEntry :1147-1207
    static wstring  FormatFlushLossMessage       (const string & path, const FlushOutcome & outcome, FlushMoment moment);
    static wstring  FormatPreserveFailureMessage (const string & path, const string & attemptedPath,
                                                  HRESULT reason, FlushMoment moment);   // replaces FormatDiscardedWritesMessage
    static wstring  FormatUnsavedWritesMessage   (const string & path, const MountDiagnosis & diagnosis);
    bool     m_isFileBacked = true;
```

Keep the existing 3-argument `FormatFlushLossMessage` for the patch sites at `:1463`, `:1466` and `:1477`.

**`ReplaceBay` (load, save, swap; the body of `MountFromBytes`).**

```cpp
    CBRAEx (isValid, E_INVALIDARG);

    hr = LoadImage (fmt, bytes, target, loaded, outDiagnosis);
    CHRF (hr, outDiagnosis.keptPath = GetEntry (slot, drive).path);

    hr = SaveBeforeReplace (GetEntry (slot, drive), target, outDiagnosis, outOutcome);
    CHR (hr);

    SwapInImage (GetEntry (slot, drive), std::move (loaded), target, fmt);
```

- `target` is a local copy of the path, because `RetireBay` clears `entry.path`.
- Delete the failure branch at `:278-288`.
- `MountFromBytes` becomes: `ReplaceBay`, then `ReportReplaceOutcome`.

**`SaveBeforeReplace`.**

1. `BAIL_OUT_IF (!entry.mounted, S_OK)`.
2. `hrSave = FlushEntry (entry, Replacing, outOutcome); IGNORE_RETURN_VALUE (hrSave, S_OK)`.
3. `isSameFile = MountedImageState::IsSamePath` over the two paths after `lexically_normal`, so `..` segments and `.\` prefixes cannot make one file look like two. (FR-013 makes command-line paths absolute first; 8.3 names and links stay out of reach, because `ImageIdentity` holds a size and a write time, not a file id.)
4. `canReplace = !entry.image->IsDirty() || (!isSameFile && IsRecoveryCurrent (entry))`.
5. Hoist `hrDecline`: the save's own code when it has one, otherwise `E_PENDING`.
6. `CBRFEx (canReplace, hrDecline, ...)`. The failure action fills the diagnosis: `failure = UnsavedWrites`, `saveError`, `keptPath = entry.path`, `isSameFile`, `wasReported = entry.sharedState.IsSaveFailureReported()`. It then calls `entry.sharedState.MarkSaveFailureReported (string())`.

**`Mount`, between `:614` and `:619`.** When the bay holds the same file:

```cpp
    hr = SaveBeforeReplace (entry, path, outDiagnosis, outcome);
    CHR (hr);
```

This runs before `ReadImageFile`. When the save leaves nothing unsaved and `ReadIdentity (path)` matches the bay's recorded identity (both recorded), the file holds exactly what the mounted image holds, so the mounted image stays: no read, no swap, no `RetireBay`, no `EmitBayChange` and no `NotifyMediaChanged`, and the mount succeeds. That also keeps an ordinary reset from capturing a boundary keyframe between its `Reset` journal record and its `SoftReset` (see "How this fits 035's history model"). Otherwise `ReplaceBay` runs; its own save has nothing left to write and adds nothing to the outcome. Lines `:626-640` are unchanged. The `Error:` label calls `ReportReplaceOutcome`.

**`ReportReplaceOutcome`.** Acts only when `outcome.movedTo` is set.

| Situation | Report sent through `m_reportSink` |
|---|---|
| Swap succeeded, same file | `ComposeReloadReport (movedFrom, drive, false, m_machineName, AnotherProgram, movedTo, true)` |
| Swap succeeded, other file | New `ChangePrompt::ComposeReplacedReport (movedFrom, drive, movedTo, newPath)` |
| Swap failed | `ComposeConflictReport` (the Running sentence; true, because the bay moved onto the copy) |

**`MountRestored`** (`:664-705`):

1. `LoadImage` first; a failure leaves the bay as it was.
2. Hoist `isSafeToSwap = AreWritesSaved (entry) || !entry.mounted;`, then `CBRAEx (isSafeToSwap, E_UNEXPECTED)`. The caller saves first and declines on failure.
3. `SwapInImage`, with no flush.
4. Identity and watch as today (`:692-698`), then `EmitBayChange (Inserted)` and `NotifyMediaChanged`.

The audit's `CHRF (hr, EmitBayChange (Ejected))` hardening becomes unnecessary.

**`FlushEntry`.**

- After the mounted check: when `!m_isFileBacked`, commit the pending write, clear dirty, and `BAIL_OUT_IF (true, S_OK)`.
- The conflict branch moves into `PreserveConflictingCopy`. For `Replacing` it repoints as `Running` does, but records `movedFrom`/`movedTo` instead of calling `m_reportSink`.
- The three `CHRN` sites (`:1218`, `:1223`, `:1236`) fill `outOutcome`, then `CHRF (hr, ReportSaveFailure (entry, moment, outOutcome))`.
- On success, also `ClearRecovery()`, which clears the recovery record and its report flags (R3).
- The replay bail at `:1090` is unchanged.
- The function is roughly 190 lines today, so the extraction is required by Principle V.

**`ReportSaveFailure`, by moment.**

| Moment | Behavior |
|---|---|
| `Replacing` | Notify only for a recovery copy this mount has not reported (FR-016). |
| `Running` | Always notify. |
| `Background` | Notify only when `sharedState.ShouldReportSaveFailure (recoveryPath)` (FR-016's rule, R3: a copy at a path not yet reported, or no copy and no plain failure reported yet), then `MarkSaveFailureReported (recoveryPath)`. |
| `Ejecting` | A copy is a notice under FR-016's rule, and the eject proceeds. Otherwise, with `m_askSink` set and no open question: `SetAskedAction (Conflict)`; `SetAskedCause (isUnserializable ? Unserializable : Unwritable)`; ask with `ComposeSaveFailure`, where `suggestedSavePath = MakeRecoveryPath (path, 0)` for `Unserializable`; set `EjectWhenAnswered` if the question was delivered. Otherwise notify ("not ejected"). |
| `ShuttingDown` | A copy is a notice. Otherwise `RescueOnTheWayOut`; if that returns false, notify ("Casso is closing"). |

**`ReportPreserveFailure`** (`:358-416`):

- `Replacing` and `Background` behave as `Running`.
- `Ejecting` with no question possible no longer lets the eject proceed.
- Wording comes from `FormatPreserveFailureMessage (..., moment)`.

**`Eject`** (`:2290-2330`):

```cpp
        hr = FlushEntry (entry, FlushMoment::Ejecting, outcome);
        IGNORE_RETURN_VALUE (hr, S_OK);

        isWaiting = entry.sharedState.IsEjectWhenAnswered();
        isSaved   = AreWritesSaved (entry);
        BAIL_OUT_IF (isWaiting || !isSaved, S_OK);
```

`EndWatching`, `RetireBay`, `EmitBayChange` and `NotifyMediaChanged` follow as today. A decline with nothing reported (the replay-flag case) raises the "not ejected" notice here.

**`ResolvePendingChange`** Conflict branch (`:3107-3176`):

- **Save as...** serializes natively. If that fails while `finishEject` is set, it writes `WozLoader::Serialize` output, changes the chosen path's extension to `.woz`, and records the copy as the recovery copy. It does not repoint the bay, because the bay's format cannot change.
- The re-ask at `:3128-3135` uses `GetAskedCause()` instead of the hard-coded `ExternalChange`.

**`RescueOnTheWayOut`** (`:435-468`) gets the same WOZ fallback. For an unserializable disk it passes `MakeRecoveryPath (original, 0)` to the sink, so the dialog offers a `.woz` name.

**`AreAllWritesSaved`** walks `m_entries` with `AreWritesSaved`.

#### Other files

- **`Devices/Disk/DiskImageStore.h`**: hold comment at `:133-137`: add "a mount over a disk" to the flushes that still write while held. FR-022 also covers the stale header at `:30-33`, `:35` and `:47-55`. Line 34, the `SoftReset` line, and the `DiskImageStore::SoftReset` banner in the `.cpp` are corrected by tasks.md T130, unless the 035 commit for those two comments that the 035 session described on 2026-10-08 has arrived first; its tip `d3c15b55c` does not hold it.
- **`Devices/Disk/MountDiagnosis.cpp`**: `Describe` case for `UnsavedWrites`; `ShouldReport`.
- **`Devices/Disk/ChangePrompt.cpp`**: `ComposeSaveFailure` (`:366-423`) wording for the two new causes, with the Ejecting answers; new `ComposeReplacedReport`.
- **`Shell/DiskManager.cpp`**
  - New `RestoreDoor (int drive)`: `PublishSyncEvent (drive, SyncAction::DoorRestore, now)` when the bay is still mounted.
  - New `MountDiskForCommand (int drive, const std::string & path)`, which `EmulatorShell::MountDisk` calls for `IDM_DISK_INSERTn`: the behind-live decline ("Divergence", below), then `MountDiskInSlot6`.
  - `MountDiskInSlot6` (`:443-491`): on failure, `RestoreDoor`.
  - `EjectDiskInSlot6` (`:507-519`): `RestoreDoor` after a declined eject, and stop clearing the remembered path there.
  - `OnBayChange` (`:534-631`): clear the remembered path when `change` is `Ejected`, ahead of the audio-source check, so it does not depend on a drive sound. That covers a user eject, an eject finished by an answer, and a lost file. The function is 99 lines today, so its door-and-sound switch moves into a helper first (tasks.md T086).
  - `UpdateDriveWidgets`' `ApplySyncEvents` (`:811-826` today): on `DoorRestore`, clear `DriveWidgetState::isEjectPosted` (entry-flags-race Part B), and if `st.IsMounted()`, `StartDoorTransition (Closing)`. Without the clear, a declined eject leaves the flag set with the disk still in the drive, and a later create-disk into that drive would skip its replace prompt.
  - Rewrite the `RemountSlot6Disks` header (`:641-647`).
- **`Ui/DriveWidgetController.h:28-35`**: add `SyncAction::DoorRestore`. Depends on FR-008's thread-safe queue.
- **`Shell/EmulatorShellDisks.cpp`**
  - `HandleMountCompletion` (`:222-260`): take the message from `FormatMountFailureMessage`. Raise the dialog only when `completion.diagnosis.ShouldReport()`. Tool replies always get the text.
  - `AskAboutChange` (`:1157`) passes `notice.prompt.suggestedSavePath` to `AskWhereToSaveLostDisk`. That function (`:1195-1229`) gains a `const std::string & suggestedPath` parameter that seeds the file name and puts `*.woz` first. This also removes a UI-thread store read.
- **`Shell/MachineStateFile.cpp`**: new `static HRESULT SaveDisksFirst (MachineHost & machine, MachineStateError & outError)`: `FlushAll`, then hoist `areAllSaved = AreAllWritesSaved();` and `CBRFEx (areAllSaved, ...)` with `MakeError ("disk not saved", "A disk in a drive has changes that could not be saved, so the state was not loaded.")`. `Apply` calls it at `:227-230`.
- **`Shell/EmulatorShellState.cpp:172-187`**: call `SaveDisksFirst` before the other-machine switch (`isOtherMachine` runs `SwitchMachine`, which stops and restarts recording) and before `StopReverseRecording`, so a declined load of another machine's state leaves the machine, its disks and its history as they were.
- **`Shell/ScratchReplayMachine.cpp:101`** on 035's tip (moved there from `Shell/ScratchHeatReplayer.cpp:491`): replace the always-`S_OK` sink with `SetFileBacked (false)`, for the heat and the call-history replayers' scratch machines alike.
- **`Cli/DebugBatchRunner.cpp`**
  - `:244`: `SetFileBacked (false)` when not writing through.
  - `:152`: `FlushAllForShutdown`, because the process exits next.
- **Comments only:** `Shell/MachineManager.cpp:403-404`, `:579-586` and `:596-599`; `Shell/CpuCommandDispatcher.cpp:37-41`; `Shell/Window/EmulatorWindow.cpp:2762-2764`; the `FormatMountFailureMessage` banner at `DiskImageStore.cpp:799-802`.

#### Message wording

The texts for approval are in contracts/user-messages.md, which settles the
places where this table and the contract's first draft disagreed; where they
differ, the contract is the one to implement.

All moments share the `FormatFlushLossMessage` head: "Casso could not save changes to the disk image:\n\n<path>\n\nError: <code and text>\n\nThe file on disk is unchanged."

| Function | Case | Text |
|---|---|---|
| `FormatFlushLossMessage` tail | A recovery copy was written (any moment) | Unchanged (`:903-905`). |
| | Unserializable, and the copy failed | The Error line uses the copy's error, followed by "Its changes cannot be stored as a <ext> image, and the complete copy Casso tried to write beside it could not be saved." |
| | Running or Background | "Your recent writes have not been saved. The disk in the drive still has them." (unchanged text, now true wherever it is used) |
| | Replacing, any file | No tail of its own: the save notifies only for a first recovery copy, with that copy's tail, and a mount its failure declines is reported by `FormatMountFailureMessage` below, unless this mount has already reported a save failure of either kind, so a same-file decline after its save reported a first copy is silent. A power cycle and a machine switch report at `Background` before their remount, whose decline is then silent. One event gets one text (contracts/user-messages.md section 2) |
| | Ejecting, no question possible | "The disk was not ejected, and the drive still has your changes." |
| | ShuttingDown, rescue declined or failed | "Casso is closing, so these changes cannot be kept." |
| `FormatPreserveFailureMessage` tail | Ejecting | "The disk was not ejected, and the drive still has your changes. The file on disk keeps the other program's version." |
| | ShuttingDown | "Casso is closing, so these changes cannot be recovered. The file on disk keeps the other program's version." (replaces "The disk is leaving the drive" at `:507`) |
| `ComposeSaveFailure` | `Unwritable` | "Your changes to <file> could not be saved to\n\n<path>\n\nError: ...\n\nYour changes are only in <drive>, which is about to be emptied. Save them somewhere, or discard them." (Superseded by contracts/user-messages.md section 4.) |
| | `Unserializable` | "Your changes to <file> cannot be stored as a <ext> image, and the complete WOZ copy could not be saved to\n\n<copy>\n\nError: ...\n\nYour changes are only in <drive>, which is about to be emptied. Save a WOZ copy somewhere, or discard them." (Superseded by contracts/user-messages.md section 4.) |
| `FormatMountFailureMessage` | `UnsavedWrites`, other file | "Casso did not insert this disk image:\n\n<B>\n\nThe disk already in the drive has changes that could not be saved to its file:\n\n<A>\n\nError: ...\n\nInserting another disk would discard them, so that disk is still in the drive with your changes. Eject it to save them somewhere else." When `saveError` is `S_OK`: "have not been saved to its file yet", with no Error paragraph. (Superseded by contracts/user-messages.md section 5.) |
| | `UnsavedWrites`, same file | "Casso could not save changes to the disk image:\n\n<A>\n\nError: ...\n\nThe file on disk is unchanged, and the disk was not read from it again. The drive still has your changes." |
| | Any other decline over an occupied drive | Existing text, plus "\n\nThe disk already in the drive was not changed." |
| `ComposeSaveFailure` answers | `Unwritable` and `Unserializable` | **Save as...** and **Eject and discard**; `safeAnswer = 0` |
| `MountDiagnosis::Describe` | `UnsavedWrites` | "was not inserted, because the disk already in the drive has changes that could not be saved to its file" |

#### How this fits 035's history model

- **`RetireBay`** runs only inside `SwapInImage` and `Eject`, after the save gate, immediately after `EndWatching` (FR-011). The outgoing medium keeps its own image ID and `retiredAt` (`DiskImageStore.cpp:2203-2213`). Mounts and ejects therefore never leave the only copy of a user's writes in `m_retained`. `SeatMedia` still retires dirty disks by design (`:2134-2138`).
- **Flush hold.** No change to how the hold works. `Background` is the `FlushAllUnlessHeld` path, which skips while held (`:1899`). `RemountSlot6Disks` still returns early under the hold (`Shell/DiskManager.cpp:661-664`). `Replacing`, `Ejecting` and `Running` write while held, as documented.
- **`m_isReplaying`.** The bail is unchanged. The data-based decline test stops a replay-skipped dirty disk from being retired.
- **`NotifyMediaChanged` and keyframes.** Called by a completed swap or eject, and by one decline: a write protection FR-015 declines, so the boundary keyframe records the unprotected drive (the last bullet below). A declined mount or eject never reaches `ReverseController::OnMachineChanged` (`Debugger/Reverse/ReverseController.cpp:227-266`). The `DiskMount`/`DiskEject` journal record written before dispatch is inert.
- **Reset and its boundary keyframe.** `IDM_MACHINE_RESET` journals `Reset`, then remounts, then runs `SoftReset` (`Shell/CpuCommandDispatcher.cpp:62-71`). A remount that swaps calls `NotifyMediaChanged`, and `OnMachineChanged` captures the boundary keyframe at once, before `SoftReset` and at a journal index past the `Reset` record (`ReverseController.cpp:283-296`); a replay that reaches it applies `Reset` and then loads the pre-reset state over it (`Replayer.cpp:175-179`). The same-file rule above keeps the mounted image when the file is unchanged, so an ordinary reset swaps nothing and captures no boundary, with or without 035's fix. A remount that does swap kept this order at `811a6f727`; it was reported to 035 as 041 audit defect d, and 035 fixed it in `5ee85434f`, which tasks.md T008 brings: a disk change while live now becomes a boundary keyframe before the next instruction or reverse command (spec Assumptions, item 6).
- **Divergence.** UI-posted insert, eject and answer commands are gated (`Debugger/Reverse/DivergenceGate.cpp:25-45`), but the gate's check runs on the posting thread, from `HostInputGate::IsHeld`, which is set only when a reverse command runs on the emulation thread (`Shell/EmulatorShellReverse.cpp:303-333`). A command posted while a step back is still queued passes the gate and then runs behind live, with the hold set and the replay flag clear, so its save would write the past image over the file and its media notice would drop the recorded future without the question. The insert, eject and both write-protect commands therefore change nothing while `IsFlushHeld()` or `IsReplaying()` is true, and report it (tasks.md T105). A Settings write-protect command whose value equals the drive's current setting changes nothing and reports nothing, live or behind live, because Settings sends the command for both drives on every apply (`Ui/Settings/SettingsPanelState.cpp:1054-1057`), so an apply that races a queued step back raises no notice for a change the user never made. The check sits in entry points only those commands reach: a new `DiskManager::MountDiskForCommand`, which `EmulatorShell::MountDisk` (the `IDM_DISK_INSERTn` override) calls in place of `MountDiskInSlot6`; `EjectDiskInSlot6` and `ToggleImageWriteProtect`, whose only callers are their command overrides (`Shell/EmulatorShellCpuThread.cpp:449`, `:466`); and `SetDriveUserWriteProtect`. It is not in `MountDiskInSlot6`, which `MountCommandLineDisks` and `RemountSlot6Disks` also call: a machine switch's remount runs after `StopReverseRecording`, is not behind live, and has the data-based decline above for a disk whose save was skipped. The `DiskManager` entry points keep the declines testable through the `DiskManager` rig, which an `EmulatorShell` built without `Initialize` is not. The store's own explicit flushes keep writing while held, as `HoldStillSavesOnEjectSwitchAndExit` requires.
- **A declined write protection.** The `DriveWriteProtect` record is journaled before dispatch with the requested value, so a protection FR-015 declines would replay as applied. `SetUserWriteProtect` calls `NotifyMediaChanged` on a decline, so the boundary keyframe at that position records the unprotected drive, and the boundary wins in a replay (`Replayer.cpp:175-179`).

### Test plan

Each new test must fail before its fix (SC-001). Expected pre-fix failures are given in parentheses.

No test here touches a real file. The store tests use a flush sink or `FakeDiskFileIo`. The shell-level tests in `UnitTest/EmuTests/DiskWritePathTests.cpp` (tasks.md T091 to T093) run through an `EmulatorShell` built in the test, which never calls `InstallSharedImageSupport` (only `EmulatorShell.cpp:703` does), so its store has no file I/O, and after tasks.md T059 a store with no sink commits through a local `Win32DiskFileIo` to the mounted path; each such test therefore installs a recording flush sink and an image reader on the shell's store before it mounts, mounts with `MountFromBytes`, and counts the writes through the sink, so even a save the code before the fix makes (T093's run before T105, while the store's explicit flushes still write while held) goes to the sink.

**`UnitTest/EmuTests/DiskImageStoreTests.cpp`** (uses `ScopedFlushNotifyCapture` at `:119`):

- `Mount_UnloadableOverAMountedDisk_LeavesThatDiskInTheDrive`: the audit's FR-009 test with a `Disk2Controller` and `SoftReset`. (Bay empty; pointer dangling.)
- `MountFromBytes_UnloadableOverADirtyDisk_WritesNothing`. (One write.)
- `MountFromBytes_OtherFileOverAnUnwritableDisk_KeepsTheDisk`, with the sink returning `ERROR_DISK_FULL`:
  - `hr` failed; `failure == UnsavedWrites`; `saveError`, `keptPath` and `!isSameFile` set;
  - same image pointer, still dirty, store notices `== 0`.
  - (Pre-fix: `S_OK` and a new image.)
- `MountFromBytes_OtherFileOverASavableDisk_WritesOnceAndReplaces`. Guards against declining too much.
- `Mount_SameFileOverADirtyDisk_ReadsWhatTheFlushWrote`: the reader and the sink share one in-memory file map. (Guest bit lost.)
- `Mount_SameFileOverAnUnwritableDisk_KeepsTheDisk`: twice; `wasReported` is false, then true.
- `Mount_OtherFileOverARescuedUnserializableDisk_Replaces` and `Mount_SameFileOverARescuedUnserializableDisk_KeepsTheSession`, using `CorruptOneAddressField` as at `:1311`.
- `Eject_Unwritable_RaisesTheQuestionAndWaits`, `Eject_Unwritable_SaveAsFinishes`, `Eject_Unwritable_DiscardFinishes`.
- `Eject_Unwritable_NoQuestionPossible_KeepsTheDisk`: text contains "was not ejected" and not "still has them". (Bay empty.)
- `Eject_RescuedUnserializableDisk_Ejects` and `Eject_UnserializableCopyFailed_OffersAWozCopy`: `suggestedSavePath` ends in `.recovered.woz`; a `.dsk` answer writes WOZ bytes to the `.woz` path.
- `FlushAll_RepeatedFailures_ReportOnceUntilASaveLands`, and `Flush_RepeatedFailures_ReportEveryTime`. (tasks.md drops the first, which is R3's `FlushEntry_LockedImage_ReportsOnceAcrossSpindowns` under another name, and adds the second to T064 as a guard that passes before and after T065, because a `Running` save reports every failure both today and after the report-once rule.)
- `MountRestored_UnloadableBytes_LeavesTheBay`.
- `SetFileBacked_False_WritesNothingAndEjects`.
- `FormatMountFailureMessage_UnsavedWrites_OtherFile`, `_SameFile`, and `_KeptDiskIsStated`.
- Extend the `FormatMountFailureMessage_EveryReason_SaysSomethingOfItsOwn` sweep (`:1841-1885`) to all 12 enumerators. Today it lists 8 of 11.
- Updates: `FlushError_surfacesThroughVoidEjectPath` (`:469-484`) also asserts `IsMounted`; `FlushError_notifies_viaPowerCycle` (`:689-702`) keeps a count of 1.

**`UnitTest/EmuTests/DiskFlushHoldTests.cpp`:**

- `AFailedMountRetiresNothingWhileRetained` (`audit.md:102-106`).
- `DeclinedMountForUnsavedWritesRecordsNothing`: media-change listener count 0, retained count 0, `GetMediaId` unchanged.
- `ReplaySkippedSaveDoesNotLetAMountRetireTheDisk`: `SetReplaying (true)`, retention off. (Image freed.)
- `HoldStillSavesOnEjectSwitchAndExit` (`:73`) stays green.

**`UnitTest/EmuTests/DiskResetRemountHoldTests.cpp`** (`TestMachine` and `DiskManager` rig, `:44-56`):

- `RemountAfterAFailedFlushKeepsTheDisk`.
- `RemountOfAFileThatNoLongerLoadsKeepsTheDisk`: `GetDisk (0) == image`, then `machine.SoftReset()`.
- `ResetRemountOfADirtyDiskKeepsTheGuestsWrites`.
- `MachineSwitchKeepsADiskWhoseFlushFailed`: failing sink, then `FlushAll`, then `machine.PowerCycle()`, then `MountCommandLineDisks (path, "")`. Assert the same image, controller attached, still dirty, one notice.
- `DeclinedInsertAndEjectPutTheDoorBack`.
- `ADeclinedEjectStillNeedsTheReplacePrompt`: an eject posted from the window and declined on the emulation thread (failing sink, no question possible) restores the door, and `IsReplacePromptNeeded` is true again, so a later create-disk into that drive still raises the replace prompt before replacing the disk. (Before the `DoorRestore` clear, `isEjectPosted` stays set.)
- `EjectFinishedByAnAnswerClearsTheRememberedPath`.

**`UnitTest/EmuTests/MachineStateFileTests.cpp`:**

- `AStateIsNotLoadedOverADiskWhoseWritesCouldNotBeSaved`: label "disk not saved"; the bay keeps the same dirty image.

**`UnitTest/EmuTests/SharedImageTests.cpp`:**

- `ARemountOverAChangedFileReportsTheReloadAndTheCopy`: text contains "now has the modified version" and not "remounted in".
- `AnInsertOverAChangedFileSavesTheGuestCopyFirst`.
- `AnEjectOverAChangedFileWithNoQuestionPossibleKeepsTheDisk`.
- The watch-leak tests, using the FR-009 variant: a failed mount keeps its folder watched.

**`UnitTest/EmuTests/ExternalChangePolicyTests.cpp`:**

- `ComposeSaveFailure_Unwritable_OffersSaveAsAndEjectAndDiscard`.
- `ComposeSaveFailure_Unserializable_OffersAWozCopy`, and the same with no "Another program".

**`MountDiagnosis::ShouldReport`:** a three-case table test in `DiskImageStoreTests.cpp`.

### Risks

1. **Behavior change.** With no ask sink (headless, tests) or a post that fails, a failing eject now keeps the disk. Tests that expected an empty bay after a failed eject need updating. A disk whose file can never be written can only leave through the eject question.
2. **Ordering dependencies.** These must land with or before this change:
   - FR-017, which landed in `e32b2b68c`: before it, `Disk2Controller::SoftReset`'s direct `DiskImage::Flush` (`Machines/Apple2/Common/Disk2Controller.cpp:944` at `811a6f727`) wrote the kept dirty image around every check.
   - The salvage-mount posting fix: otherwise the decline logic runs on the UI thread.
   - FR-012: the external-reload dirty test.
   - FR-016: the recovery record.
   - FR-008: the door-restore event.
   - FR-011: `EndWatching` before `RetireBay`.
   - FR-013: command-line paths made absolute, with the CLI's intent sender in the same commit, before the same-file branch, so a relative `--disk1` and a picker re-insert of the same file compare as one file.
   - FR-018: `CommitFile`, which FR-016's recovery copy calls.
   - R3 Risk 1's measurement of the spin-down commit, because the decline acts on the save's result.

   tasks.md orders Phases 2 to 6 by these edges.
3. **035's replay flag.** At `811a6f727`, `ReverseController::Stop` left `m_isReplaying` set. This record keeps such a disk in its bay, but `FlushAllForShutdown` and every automatic flush after a switch wrote nothing until something called `BecomeLive`. It was reported to 035 on 2026-10-08 as 041 audit defect a, and 035 fixed it in `c913c9247` (`threading-035.md`, section 3), which tasks.md T008 brings. Before 041 merges, confirm on the merged code that `Stop` clears the flag (tasks.md T008, and T137, which blocks the merge; without the fix the owner decides).
4. **The scratch machines.** Without `SetFileBacked (false)`, a heat rebuild over an unserializable dirty scratch disk now fails at `MountDisks` (`Shell/ScratchHeatReplayer.cpp:563` at `811a6f727`, `ScratchReplayMachine::MountDisks` on 035's tip). The always-`S_OK` sink also posts a false user notice today.
5. **Divergence before decline.** A "yes" to the divergence question drops the recorded future even when the insert is then declined. Predicting the decline on the UI thread would mean reading bay state there, which FR-001 forbids.
6. **Silent same-file re-insert.** A user re-insert of the same file is silent once the failure has been reported for this mount. Adding a `MountOrigin` parameter to `MountDiskInSlot6` would fix that if the owner wants it.
7. **Not addressed here.**
   - An unserializable dirty disk whose file another program changed still loops on **Save as...** in the conflict path, which uses native `SaveLoadedImage` (`:3821`). The disk is kept, but WOZ preservation for conflicts is follow-up work.
   - `MountExternallyModifiedDisk`'s move-assign (`:3525`) kept no retained medium, so history could not step back across an external reload. That was 035's concern: it was reported as 041 audit defect f, and 035 fixed it in `a7f8e937a`, `0528cba72` and `7de76eeba`. 041 does not depend on the fix, and its own edits to that function (tasks.md T026, T065, T084) are re-applied to 035's version at the merge.

## R3. Durable, attribute-preserving commit, and one recovery copy per session (FR-016, FR-018)

Paths are relative to `C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity`. Nothing was modified.

### Decision

1. **Send every emulator image write through the existing `IDiskFileIo` seam** (`CassoEmuCore/Devices/Disk/IDiskFileIo.h:62-89`). Production already installs `Win32DiskFileIo` into the store (`CassoEmuCore/Shell/DiskManager.cpp:967-970`).

2. **Put one commit sequence above the seam, in a new core class `DurableCommit`.** The emulator, the CLI and Cassque all use it. The steps run in this order:
   1. `WriteAllBytes` to a sibling temporary.
   2. If the target exists, `CopyFileMetadata (target, temp)`. This is new. It copies the hidden, system and not-content-indexed attributes, the creation time, and the DACL when that DACL is protected or has explicit ACEs.
   3. `FlushToStorage (temp)`. This is new and calls `FlushFileBuffers`.
   4. `ReplaceAtomically`, which is `MoveFileExW (MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` and stays as it is. A copy that must never overwrite anything uses a new `RenameWithoutReplacing` instead, which is `MoveFileExW (MOVEFILE_WRITE_THROUGH)`.

3. **Do not use `ReplaceFileW`.**

4. **FR-016: keep a per-mount recovery record in `MountedImageState`.** It holds:
   - the copy's path and the file identity at write time
   - the image ID and track generations it held
   - which reports were already shown

   A later failure rewrites the same copy through the commit, or writes nothing if the image has not changed, and shows nothing more. The record is cleared in four places:
   - `MountedImageState::Mount`
   - `MountedImageState::Eject`
   - after a successful commit (the session is saved)
   - in `MountExternallyModifiedDisk` (the session's content was replaced)

### Rationale

**Today's write path is not durable.**
- `DiskImageStore::WriteFileAtomically` (`DiskImageStore.cpp:1279-1372`):
  - creates the temporary with `CREATE_ALWAYS | FILE_ATTRIBUTE_NORMAL` (1337-1338)
  - writes and closes with no flush (1343-1348)
  - renames with `fs::rename` (1357), which is `MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING` with no write-through (`audit.md:2307`)
- NTFS logs the new file's size but not its data. After a power cut the target can come back zero-filled, which breaks User Story 1, scenario 4.

**Today's write path loses metadata.** Every commit swaps in a new `FILE_ATTRIBUTE_NORMAL` file, so the original's attributes and explicit ACL are lost. That is the defect in scenario 7.

**Why the seam rather than a fix inside the store:**
- **The CLI already has the same sequence, minus the flush.** `DiskImageSession::CommitImage` (`DiskImageSession.cpp:629-635`) calls `WriteAllBytes` and then `ReplaceAtomically`. `Win32DiskFileIo::WriteAllBytes` (`Win32DiskFileIo.cpp:78-123`) never flushes. The comment at `Win32DiskFileIo.cpp:479-482` claims the temporary is "complete and flushed". One implementation fixes both commit paths, so the two cannot drift apart again. That drift is how the asymmetry described in `audit.md:2321` came about.
- **It makes the durable path testable without real files.** The store's own write goes around every seam. The comment at `DiskImageStoreTests.cpp:750-762` states that the test cannot simulate a partial write, and its only coverage of the no-sink path uses real temp files (`DiskImageStoreTests.cpp:782-857`). With the sequence above the seam, `FakeDiskFileIo` can record the order of operations and simulate a power cut.
- **The store already uses this seam for two other questions:** `DoesPathExist` (`DiskImageStore.cpp:3703-3715`) and `IsHeldByAnotherProcess` (`DiskImageStore.cpp:2871`). So the comment at `ImageIdentity.h:70-77`, which says the store "has no other use" for the interface, is already out of date.

**Why flush the temporary and rely on `MOVEFILE_WRITE_THROUGH` for the rename:**
- The data must reach the disk before the rename can. Otherwise the logged rename survives and the data does not, which is exactly the zero-filled case.
- `MOVEFILE_WRITE_THROUGH` is documented as "does not return until the file is actually moved on the disk".
- A second flush of the target after the rename was considered and rejected:
  - It adds a failure that cannot be reported truthfully. The target already holds the new bytes, but `FormatFlushLossMessage` (`DiskImageStore.cpp:895-897`) would tell the user "The file on disk is unchanged".
  - It doubles the device-flush cost on the CPU thread at every spin-down (`threading-035.md:90-94`).

**Why `MoveFileExW` with an explicit metadata copy beats `ReplaceFileW`:**
- It is one journaled rename, with no partial-failure states.
- It works whether or not the target exists. The salvage copy, preserved copies and the first recovery copy are usually new files.
- It keeps the CLI's error contract. A read-only target still fails at the replace with `ERROR_ACCESS_DENIED`, which `DescribeReplaceFailure` relies on (`DiskImageSession.cpp:457-476`, test `DiskFailureModeTests.cpp:506-537`).
- It keeps `CommitPlan::ShouldRemoveTemporary` correct (`CommitPlan.cpp:102-110`).

**Why the recovery record lives in `MountedImageState`:**
- `MountedImageState.h:128-131` explains it: state held on the bay was the field that mount and eject did not reach.
- 035's `RetireBay` (`DiskImageStore.cpp:2210`) and `SeatMedia` (2157) copy `sharedState` whole, so a retained disk keeps its record.
- `RepointBayToFile` already resets the state through `sharedState.Mount` (3587).

### Alternatives considered

1. **Flush plus `MoveFileExW`, written inline in the store** (the audit's minimal fix, `audit.md:2345-2360`).
   - It fixes durability only. Attributes and permissions are still lost, and the store and the CLI keep two implementations.
   - The flush and the write-through rename are kept in the chosen design, but placed on the seam.

2. **`ReplaceFileW`** (with `MoveFileExW` for targets that do not exist). It copies the most metadata: attributes, DACL, alternate data streams such as `Zone.Identifier`, creation time, object ID, short name, compression and encryption. Rejected for these reasons:
   - **It needs the target to exist**, so a second primitive with a second set of error codes is needed anyway.
   - **`REPLACEFILE_WRITE_THROUGH` is documented as unsupported**, so explicit flushes are still required.
   - **It is not documented as atomic, and its documented failures leave intermediate states:**
     - `ERROR_UNABLE_TO_MOVE_REPLACEMENT`: the original is gone and the new bytes exist only under the temporary's name.
     - `ERROR_UNABLE_TO_MOVE_REPLACEMENT_2`: the original survives only under a different name.
   - **The existing cleanup rule would then delete the last copy of the file.** `CommitPlan::ShouldRemoveTemporary` (`CommitPlan.cpp:102-110`) and the cleanup at `DiskImageSession.cpp:639-646` and `DiskImageStore.cpp:1360-1369` both remove the temporary after any failed replace. After the first error above, that temporary is the only copy left. Avoiding this needs recovery moves, which can themselves fail, and a rewritten `CommitPlan`.
   - **A power cut part-way through can leave nothing at the target's path at all**, which breaks scenario 4.
   - **It changes the error codes the CLI gives for a read-only target.**

3. **`ReplaceFileW` with a backup file name**, so the second error above can be recovered. This adds a second set of name collisions to handle and a backup to clean up. It is still not atomic and still needs explicit flushes. Rejected.

4. **Reusing the seam as it is**, with only `WriteAllBytes` and `ReplaceAtomically`. This gives testability but neither durability nor metadata. It is the base of the chosen design, not an alternative to it.

5. **Writing the target in place with write-through.** This keeps all metadata trivially, but brings back the truncate-then-fail loss that `DiskImageStore.h:424-430` and `DiskImage.cpp:966-971` describe. Rejected.

6. **Transactional NTFS** (`MoveFileTransacted`). Microsoft has deprecated it and it is missing on many filesystems. Rejected.

7. **Merging `FlushSink` into `IDiskFileIo`.** Almost every disk test installs a sink. This is a separate refactor and is out of scope.

8. **A rename by handle** (`SetFileInformationByHandle (FileRenameInfoEx)` plus a flush on the same handle). It would hide the sequence below the seam, out of reach of the fake. It could later be an internal detail of `Win32DiskFileIo`.

### Design detail

#### `IDiskFileIo.h` (after line 79)

```cpp
    //  Returns once the file's bytes, size and metadata are on stable storage.
    //  A commit calls it on the temporary before the replace, because the
    //  rename can reach the disk before the data does.
    virtual HRESULT  FlushToStorage   (const std::string & path) = 0;

    //  Gives toPath the attributes, creation time and explicit permissions of
    //  fromPath, so a replacement keeps what the user set on the original.
    virtual HRESULT  CopyFileMetadata (const std::string & fromPath,
                                       const std::string & toPath) = 0;

    //  ReplaceAtomically for a target that must not exist yet; a taken path
    //  fails rather than being overwritten.
    virtual HRESULT  RenameWithoutReplacing (const std::string & tempPath,
                                             const std::string & targetPath) = 0;
```

Also fix the comment at 76-77: the rename is atomic only once the temporary has been flushed.

#### `Win32DiskFileIo.h/.cpp`

**`FlushToStorage`**
- `CreateFileW (GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING)`, then `FlushFileBuffers`.
- Hoist `isFlushed = FlushFileBuffers (h)` and capture `GetLastError` before `CloseHandle`, then `CBREx (isFlushed, HRESULT_FROM_WIN32 (lastErr))`.
- The flush covers cached data written through any handle, because the cache belongs to the file, not to the handle.

**`CopyFileMetadata`**
- Read the source with `GetFileAttributesExW`.
- Set `attributes & (HIDDEN | SYSTEM | NOT_CONTENT_INDEXED)` on the destination with `SetFileAttributesW`. Two attributes are deliberately left out:
  - `ARCHIVE`: changed content must be backed up again.
  - `READONLY`: the replace must still fail on a read-only target, and a read-only temporary could survive cleanup, because `Remove` ignores errors (`Win32DiskFileIo.cpp:190-199`).
- Set the creation time with `SetFileTime (h, &creation, nullptr, nullptr)`. The last-write time is untouched, so `ImageIdentity` does not move (`ImageIdentity.h:53-59`).
- `GetNamedSecurityInfoW (DACL_SECURITY_INFORMATION)`. Only when the DACL is protected or has an ACE without `INHERITED_ACE`, apply it with `SetNamedSecurityInfoW` using `PROTECTED_` or `UNPROTECTED_DACL_SECURITY_INFORMATION` to match. Otherwise there is nothing to do, because the sibling temporary already inherited the same ACEs from the same folder.
- These functions return their Win32 code directly, so hoist it into a local before `CBREx`. Free the security descriptor with `LocalFree` at `Error:`.
- `aclapi.h` is already included (`CassoEmuCore/Pch.h:6`).
- The owner, alternate data streams and object ID are not copied.

**`RenameWithoutReplacing`**
- `MoveFileExW (temp, target, MOVEFILE_WRITE_THROUGH)`, then `CWR`.

**`ReplaceAtomically`**
- Unchanged.
- Its debug abort hook (224-230) now fires after the flush, so the comment at 479-482 becomes true.

#### `CommitPlan.h/.cpp`

- Add `Step::CopyMetadata` and `Step::FlushTemporary` between `WriteTemporary` and `Replace`.
- `ShouldRemoveTemporary` treats all four steps from `WriteTemporary` through `Replace` as "a temporary could exist".

#### New `CassoEmuCore/Devices/Disk/DurableCommit.h/.cpp`

Add both files to `CassoEmuCore.vcxproj` (the repo has no `.filters` files).

The contract replaces the nested `TargetRule` below with a free
`enum class CommitMode { Replace, CreateNew };` in `DurableCommit.h`, because
every caller passes it in (contracts/internal-interfaces.md); read
`ReplaceExisting` as `Replace` and `RequireNew` as `CreateNew` in the rest of
this record.

```cpp
class DurableCommit
{
public:
    enum class TargetRule
    {
        ReplaceExisting,
        RequireNew,
    };

    static HRESULT  Commit (IDiskFileIo              & fileIo,
                            const std::string        & targetPath,
                            const std::vector<Byte>  & bytes,
                            uint64_t                   invocationTag,
                            TargetRule                 rule,
                            CommitPlan::Progress     & progress);

private:
    static HRESULT  ChooseTemporary (IDiskFileIo        & fileIo,
                                     const std::string  & targetPath,
                                     uint64_t             invocationTag,
                                     std::string        & outTempPath);
};
```

`Commit` runs these steps:
1. Loop over `CommitPlan::GetTemporaryPath` with `fileIo.Exists`. When every attempt is taken, fail with `ERROR_ALREADY_EXISTS`.
2. Set `hasTarget = (rule == ReplaceExisting) && fileIo.Exists (targetPath)`.
3. `WriteAllBytes`.
4. If `hasTarget`, `CopyFileMetadata`.
5. `FlushToStorage (temp)`.
6. `ReplaceAtomically` or `RenameWithoutReplacing`, then set `progress.replaceSucceeded = true`.
7. At `Error:`, if `CommitPlan::ShouldRemoveTemporary`, call `Remove (temp)` with `IGNORE_RETURN_VALUE`.

`progress` is passed in and out, so `DiskImageSession` can record Probe and Reverify first.

#### `DiskImageSession.cpp:607-646`

- Replace the temporary-name loop, the write, the replace and the cleanup with a call to `DurableCommit::Commit`.
- Choose the message from `progress.furthestAttempted`:
  - `WriteTemporary` or `FlushTemporary`: `DescribeTemporaryWriteFailure`.
  - `Replace`: `DescribeReplaceFailure`.
  - `CopyMetadata`: needs a new sentence, for the owner to approve.
  - The temporary loop was exhausted: `ERROR_ALREADY_EXISTS` before `WriteTemporary` keeps today's text.

#### `DiskImageStore.h/.cpp`

**New private `CommitFile`.** It generalizes `WritePreserved` (h:656-658, cpp:3779-3795) and replaces the four sink-or-static branches at:
- `FlushEntry` (1220-1237)
- `SetImageWriteProtect` (1468-1475)
- `SalvageToFile` (1725-1732)
- `WritePreserved` (3785-3792)

```cpp
    HRESULT          CommitFile (const string & path, const vector<Byte> & bytes,
                                 DurableCommit::TargetRule rule);
    static uint64_t  GetInvocationTag();
```

- `CommitFile` uses `m_flushSink` when one is installed.
- Otherwise it calls `DurableCommit::Commit` over `*m_fileIo`, or over a local `Win32DiskFileIo` when no seam is installed.
- `GetInvocationTag()` takes over the function-local static from `GetCommitTemporaryPath` (3907), which now calls it.
- Including `Seams/Win32DiskFileIo.h` from `Devices/Disk` is acceptable; `DiskCommandRunner.cpp:4` already includes from `Seams/`.

**`WriteFileAtomically` (public static).** Keep the signature, for `DiskImage::Flush` (`DiskImage.cpp:1000`) and the existing tests. The body becomes `DurableCommit::Commit` over a local `Win32DiskFileIo` with `ReplaceExisting`. Fix the banner (1270-1275, which still describes an `ofstream`), the comment at 1354-1356, and the header contract at h:424-434.

**`TryWriteRecoveryImage (Entry & entry, string & outPath)`, reworked:**
1. Let `undisturbed` be: no identity was recorded for the copy, or `ReadIdentity (copy).Matches (recorded)`, and `!IsFileInAnotherBay (copy, …)` (`DiskImageStore.cpp:531`). This is the same rule `FlushEntry` uses at 1127-1131.
2. If `IsRecoveryCurrent (GetImageId(), GetTrackGenerations())` and `undisturbed`: set `outPath` to the copy's path and write nothing.
3. Otherwise, if a copy is recorded and `undisturbed`: call `CommitFile (copy, woz, ReplaceExisting)`.
4. Otherwise run the free-name loop with `DoesPathExist`, not `fs::exists` at 1007. When a sink is installed and `m_fileIo` is null, keep today's attempt 0. Use `RequireNew`, and on `ERROR_ALREADY_EXISTS` or `ERROR_FILE_EXISTS` try the next name. This keeps the "never overwrites another file" rule (969-971, 1022-1025) while losing the raw `CREATE_NEW` call at 1033-1044.
5. On success, call `SetRecovery (path, imageId, generations, ReadIdentity (path))`.

**`FlushEntry`:**
- At 1210-1218, replace the unconditional `CHRN` with this rule: notify only if `sharedState.ShouldReportSaveFailure (recoveryPath)`, then `MarkSaveFailureReported`, then `CHR (hr)`.
- Apply the same report-once rule to commit failures at 1223 and 1236.
- The rule applies to saves nobody asked for, R2's `FlushMoment::Background`, and to a first recovery copy at `Replacing`. A save the user asked for (`Running`: an explicit flush or a write-protect change) and the eject and shutdown moments report as R2's table gives. tasks.md lands `Background` first (T063), so the rule never reaches a `Running` save, and puts the three report sites behind `ReportSaveFailure` (T024).
- An unserializable disk whose recovery copy could not be written gets the tail contracts/user-messages.md section 2 gives for it, with the Error line giving the copy's error rather than the serializer's.
- After `ClearDirty` (1243), call `sharedState.ClearRecovery()`.
- Every message string stays unchanged.

**`MountExternallyModifiedDisk`:** call `ClearRecovery()` beside `SetIdentity` (3529).

**Leave `ReadIdentity` alone** (2503-2519). Identities from the two stat sources must never be compared (`ImageIdentity.cpp:49-53`).

#### `MountedImageState.h/.cpp`

New members:
- `m_recoveryPath`
- `m_recoveryIdentity`
- `m_recoveryImageId`
- `m_recoveryGenerations`
- `m_reportedRecoveryPath`
- `m_isPlainFailureReported`

```cpp
    const std::string &    GetRecoveryPath         () const { return m_recoveryPath; }
    const ImageIdentity &  GetRecoveryIdentity     () const { return m_recoveryIdentity; }
    bool                   IsRecoveryCurrent       (uint64_t imageId, const std::vector<uint64_t> & generations) const;
    void                   SetRecovery             (const std::string & path, uint64_t imageId,
                                                    const std::vector<uint64_t> & generations,
                                                    const ImageIdentity & identity);
    bool                   ShouldReportSaveFailure (const std::string & recoveryPath) const;
    void                   MarkSaveFailureReported (const std::string & recoveryPath);
    bool                   IsPlainFailureReported  () const { return m_isPlainFailureReported; }
    bool                   IsSaveFailureReported   () const;   // either report field set
    void                   ClearRecovery           ();
```

- `ShouldReportSaveFailure` returns true in two cases:
  - a copy exists at a path that has not been reported yet
  - there is no copy and no plain failure has been reported yet
- `IsSaveFailureReported` is true when `m_reportedRecoveryPath` is not empty or `m_isPlainFailureReported` is set; it is what R2's same-file decline reads for `MountDiagnosis::wasReported`, so a decline right after its own save reported a first recovery copy is silent. These two fields are the store's only save-failure report state; R2 adds none of its own.
- `ClearRecovery()` clears the record and both report fields (`m_reportedRecoveryPath`, `m_isPlainFailureReported`).
- Call `ClearRecovery()` from `Mount()` and `Eject()` (cpp:15-56), next to `ClearPreserved()`.

#### `DiskImage.h:250`

```cpp
    const vector<uint64_t> & GetTrackGenerations () const { return m_trackGeneration; }
```

Comparing generations alone is safe even after a reverse restore, because the generation counter only rises (`audit.md:1961`).

#### Comments and docs (FR-022)

- `ImageIdentity.h:70-77`
- `Win32DiskFileIo.h:15-22` and `Win32DiskFileIo.cpp:209-211`
- `DiskImageStore.cpp:931-933` and `962-973`
- `docs/disk-write-integrity.md:83-85`

### Test plan

**Spec 040's commit.** The 040 session builds `DurableCommit`, the three seam operations with their Win32 and fake implementations, fake-based tests and real-file scenario cases in one commit to this decision's contract, and tasks.md T054 cherry-picks it; the lists below are what T054 checks it against, and what this branch builds when the commit is not available (fallback, T054 to T058). `DiskImageSession::CommitImage` moves with 040's merge as well as with T060, and T136 keeps one version.

**Fakes**
- `UnitTest/EmuTests/FakeDiskFileIo.h` gains:
  - a `durable` map; `WriteAllBytes` writes zeros of the same size into it, modeling NTFS logging the size but not the data
  - `FlushToStorage`, which copies `files` into `durable`
  - `ReplaceAtomically` and `RenameWithoutReplacing`, which move both `files` and `durable` (the rename is logged); `RenameWithoutReplacing` fails with `ERROR_ALREADY_EXISTS` on a taken path
  - a nested plain `Metadata { attributes, permissions }` map; `WriteAllBytes` resets it, `CopyFileMetadata` copies it, and the moves take it along
  - an ordered `operations` log of verb and paths
  - `SimulatePowerLoss()`, which sets `files = durable`
  - `failNextFlush` / `nextFlushError`, `failNextCopyMetadata`
  - a persistent `failWritesContaining` substring paired with `nextWriteError`
- `SharedImageTests.cpp:53-70` `RigFileIo`: add three stubs returning `E_NOTIMPL`. The Rig installs a sink (142-160), so they are never reached.

**New `UnitTest/EmuTests/DurableCommitTests.cpp`** (add to `UnitTest.vcxproj`; the repo has no `.filters` files), 9 tests:
1. `Commit_FlushesTheTemporaryBeforeItReplacesTheTarget`: the operation log is Write, CopyMetadata, Flush, Replace.
2. `Commit_PowerCutAfterItReturns_LeavesTheNewBytesWhole`: fails with zeros if the flush is removed.
3. `Commit_PowerCutBeforeTheReplace_LeavesTheOldBytesWhole`
4. `Commit_OverAnExistingFile_KeepsItsAttributesAndPermissions`
5. `Commit_ToAPathWithNoFile_CopiesNoMetadata`
6. `Commit_FailedFlush_LeavesTheTargetAndRemovesTheTemporary`
7. `Commit_FailedMetadataCopy_LeavesTheTargetAndRemovesTheTemporary`
8. `Commit_RequireNew_LeavesAFileAlreadyAtTheTargetAlone`
9. `Commit_StepsPastAnAbandonedTemporary`

**`UnitTest/EmuTests/CommitPlanTests.cpp`**
- Extend the sweep at 156-188 to seven steps.
- Add `Temporary_IsRemovedWhenTheFlushFailed`.

**`UnitTest/EmuTests/DiskImageStoreTests.cpp`**, next to 782-857 and 1311-1352. Tests 1 to 5 and 7 to 10 use `SetFileIo (&fake)` with no sink, an identity reader over the fake's stamps, and a root folder that cannot exist, as in `audit.md:2384`, and tests 6 and 11 a flush sink. Until its fix lands (tasks.md T059 for 1 to 5, T065 for 7 to 10), each test with no sink would reach the real file system, because the store writes around the seam (`CreateFileW` and `fs::rename` in `WriteFileAtomically`; `fs::exists` and `CreateFileW` in `TryWriteRecoveryImage`), so a run against the old code would touch real files and depend on the machine. tasks.md therefore writes them with their fix, and each is seen to fail under a mutation of the fixed code from the table below instead (tasks.md, SC-001 exceptions). Tests 6 and 11 are seen to fail before T065.
1. `Flush_ThroughFileIo_FlushesTheTemporaryBeforeTheReplace`. Before the fix the store goes around the seam to `CreateFileW` and `fs::rename` on the real path, and the fake records nothing.
2. `Flush_ThroughFileIo_KeepsTheImageFilesAttributesAndPermissions`
3. `SetImageWriteProtect_ThroughFileIo_IsDurableAndKeepsMetadata`
4. `SalvageToFile_ThroughFileIo_FlushesTheCopyBeforeItAppears`
5. `FlushEntry_ConflictCopy_ThroughFileIo_IsDurable`: use `mutateStampOnNextStat` to force the preserved-copy path.
6. `FlushEntry_UnserializableImage_IsPreservedOnceUntilTheGuestWritesAgain`: sink plus three motor cycles, as in `audit.md:1989-2002`. One write and one notice, then a second write after a guest write.
7. `FlushEntry_UnserializableImage_LeavesOneRecoveryFile`: the fake-file version of the audit's real-folder test B, which follows the no-real-files rule.
8. `FlushEntry_RecoveryCopyThatCannotBeWritten_IsReportedOnceAndKeepsTheWrites`
9. `FlushEntry_RecoveryCopyChangedOutsideCasso_IsNotOverwritten`: the next copy goes to `.recovered.1.woz` and is reported once.
10. `FlushEntry_AfterASuccessfulSave_ANewFailureStartsANewCopy`
11. `FlushEntry_LockedImage_ReportsOnceAcrossSpindowns`

**`UnitTest/EmuTests/SharedImageTests.cpp`**
1. `RecoveryRecord_IsClearedByMountAndEject`
2. `SaveFailureReport_IsNewsOncePerPathAndOncePerPlainFailure`
3. `RecoveryRecord_IsCurrentOnlyForTheSameImageAndGenerations`
4. `AReloadAfterARecoveryCopy_StartsANewCopyRatherThanOverwritingIt`: a Rig test that covers `MountExternallyModifiedDisk`.

**`UnitTest/EmuTests/DiskFailureModeTests.cpp`**, near 506
1. `APowerCutRightAfterThePut_LeavesTheImageWhollyOldOrWhollyNew`
2. `APutOverAnImageWithCustomPermissions_KeepsThem`

**`UnitTest/EmuTests/DiskFlushHoldTests.cpp`**
1. `AHeldFlushWritesNoRecoveryCopy`

**`ScenarioTests/Win32DiskFileIoTests.cpp`**, moved from `UnitTest/` with its four cases (owner decisions, 2026-10-08 and 2026-10-09; tasks.md T057). It is the real-file class of the disk seams (banner, lines 11-27); it uses its scratch folder and cleans up before asserting, as at 90-115. In `UnitTest` it broke the constitution's test-isolation rule; copilot-instructions.md allows temp files only in integration tests, and the scenario suite is where they go, with no constitution exception. It is one of the 65 real-file unit tests T057 moves at the inventory of 2026-10-09, among them `Win32ImageWatcherTests` and the store's own real-file cases, `WriteFileAtomically_ReplacesTargetAndLeavesNoTempBehind`, `WriteFileAtomically_UnwritableTarget_LeavesOriginalIntact` and `Flush_ToRealFile_WritesThroughAtomicPath` (`DiskImageStoreTests.cpp:765-857`), and `SharedImageTests.cpp:2200-2302`'s three, so after T059 every run of the store's commit on real files through its local `Win32DiskFileIo` is in the scenario suite. CI runs only `UnitTest.dll` (`.github/workflows/ci.yml:73`), so these cases run under `scripts/RunTests.ps1 -Scenario`, which every 041 phase gate runs, and CI stops running the moved ones. When tasks.md T054 cherry-picks spec 040's durable-commit commit, its real-file cases arrive in a `ScenarioTests` file of its own, and T057 adds only those of the six below it lacks. The six are two each for `FlushToStorage` and `CopyFileMetadata`, one for `RenameWithoutReplacing`, and one for the existing `ReplaceAtomically`; they pin error codes and are not durability checks:
1. `FlushToStorage_OfAMissingFile_ReportsFileNotFound_NotEFail`
2. `FlushToStorage_LeavesTheBytesAsWritten`
3. `CopyFileMetadata_KeepsHiddenAndNotIndexed_ButNotReadOnly`
4. `CopyFileMetadata_KeepsAnExplicitAce`
5. `ReplaceAtomically_OntoAReadOnlyFile_ReportsAccessDenied`
6. `RenameWithoutReplacing_OntoAnExistingFile_ReportsAlreadyExists`

**Totals.** 34 new tests in both Debug and Release, none of them assertion-behavior tests: 28 unit tests and the six scenario tests above, whether this branch writes them or T054's cherry-pick brings them. With T057's 65 moved cases leaving the unit suite, this decision's share of SC-004 is 9,488 Debug and 9,482 Release (9,525 + 28 - 65, and 9,519 + 28 - 65), plus whatever the other FRs add.

**Mutation checks**

| Mutation | Tests that must fail |
|---|---|
| Remove the `FlushToStorage` call | DurableCommit 1 and 2, store 1, 3, 4 and 5, CLI 1 |
| Remove the `CopyFileMetadata` call | DurableCommit 4, store 2 and 3, CLI 2 |
| Always run the free-name loop for recovery | store 6 and 7 |
| Drop the report-once gate | store 6, 8 and 11 |
| Treat a recorded copy as undisturbed without reading its identity | store 9 |
| Drop `ClearRecovery` after a save | store 10 |
| Drop `ClearRecovery` in `MountExternallyModifiedDisk` | SharedImage 4 |

**Must stay green unchanged:**
- `DiskImageStoreTests.cpp:782-857`, now in the scenario suite (tasks.md T057). These now run through the wrapper and `Win32DiskFileIo`; `ERROR_PATH_NOT_FOUND` still comes from `CWR`.
- `SharedImageTests.cpp:2202-2292`, now in the scenario suite
- `Win32ImageWatcherTests.cpp:73`, now in the scenario suite
- `DiskFailureModeTests.cpp:506-537`
- `DamagedDiskFlushTests.cpp:223-254`
- `DiskImageStoreTests.cpp:1311-1352`
- the hold tests

### Risks

1. **CPU-thread hitch.** A spin-down commit now adds one `FlushFileBuffers` (`threading-035.md:90-94`). That is milliseconds on an SSD, but tens to hundreds of milliseconds on a hard disk, USB stick or share. A current recovery copy skips its write, which helps.
   - Measure it before the decline design depends on it: tasks.md T062 runs right after the durable commit lands and before FR-016 and the mount restructure, and records the numbers and the decision here. Measured: not yet.
   - If the hitch is too long, the fix is a worker that commits immutable bytes and posts the result back, which keeps FR-001. Dropping the flush is not the fix.

2. **The rename's durability rests on the documented `MOVEFILE_WRITE_THROUGH` contract.** If NTFS lazily logs a same-volume rename in practice, a power cut seconds later rolls it back. That leaves the old file whole and the new bytes in a durable `.casso-*.tmp` sibling, which the next commit steps past. The file is never torn or zero-filled, so scenario 4 holds, but "durable once reported" would not. A hard power-off in a VM can confirm which happens; adding a flush of the target is the fallback.

3. **Some metadata is still not copied:** the owner (it was not copied before either), alternate data streams (`Zone.Identifier`), object ID, short name, per-file encryption or compression, and cloud pin attributes. `ReplaceFileW` would have copied these. This needs to be accepted explicitly against FR-018's wording, "attributes and permissions".

4. **A failed DACL copy fails the save.** This happens only for images with explicit or protected permissions, for example on an SMB share where `SetNamedSecurityInfoW` is denied. Such saves would fail where they succeed today, though they fail visibly, with the writes kept and the original untouched. The alternative, saving and notifying, gives up on keeping the permissions; that choice is the owner's.

5. **Seam ripple.** All three `IDiskFileIo` implementations change. The CLI and Cassque gain a flush on every put and delete, and need one new user-facing sentence, which the owner must approve.

6. **A second write path, `casso debug --write-disks`, taken into scope.** `casso debug --write-disks` writes images through `IFileSystem::WriteAllText` (`DebugBatchRunner.cpp:235-240` into `Win32FileSystem.cpp:124-157`). That function has a fixed `.tmp` suffix (the two-writer defect `DiskImageStore.cpp:1293-1296` describes), no flush, no metadata copy, and it creates missing folders. The minimal fix: give `DebugBatchRunner` an `IDiskFileIo &` and call `SetFileIo` instead of a sink when `writeDisks` is set. It is also a "completed save", so 041 takes that fix (tasks.md T061) and FR-018 lists the command.

7. **FR-016 interpretation.** This design applies report-once to commit failures as well, such as a locked file or a full volume, not only to images that will not serialize. Without that, every spin-down repeats the notice. Confirm this reading. A refresh that later fails leaves the reported copy behind the session; the drive keeps the writes and the quit-time rescue question still runs.

8. **Overwriting a copy from a different session.** This is prevented by:
   - `ClearRecovery` at save, reload, mount and eject
   - the identity check on the copy
   - the check that no other bay has the copy mounted

   A retained disk returned by `SeatMedia` brings its record back, by design.

9. **Conflicts with 035.** `DiskImageStore.cpp` is one of the 27 files 035 still holds unmerged changes in. The change sits below `FlushAllUnlessHeld` (1893-1906) and the replay bail (1090) and adds no flush trigger. `CommitHeldWrites` (1970) becomes durable without any extra work.

10. **The debug abort switch now reaches emulator commits** when `CASSO_DIAG_DISK_ABORT` is built in. That helps, but the procedure written up in `Win32DiskFileIo.h:46-68` should say so.

## R4. Guest-created tracks and the head stop (FR-020, FR-021)

Paths are relative to `C:\Users\relmer\source\repos\relmer\Casso-worktrees\041-disk-integrity`. Line numbers are at `fb05ee2ab`.

### Decision

**FR-020, head stop**

- `Disk2Controller::kMaxQuarterTrack` becomes `DiskImage::kQuarterTrackCount - 2`, which is **158** (half track 79, inside track 39). It does not become 159.
- FR-020's text "quarter track 159 (track 39)" is corrected to 158.
- Everything else follows the constant:
  - the clamp
  - the bump/step discriminator, and with it the end-stop sound
  - the debugger head ruler
  - the controller's `LoadState` check
- These stay as they are:
  - the engine's own clamp (`kMaxTrack = 159`)
  - the 2D drive-widget rail (`DriveWidget::kMaxQuarterTrack = 139`)
  - every state version

**FR-021, guest-created tracks**

1. **When the track is created.** On the first bit the sequencer actually writes over a position with no data: inside `Disk2NibbleEngine::StepLss`, on the read clock, in write mode, with the motor turning. It is not created when Q7 goes high.
2. **Where its storage comes from.** Storage is reserved when the WOZ loads, not grown when the guest writes.
   - `WozLoader::Load` reserves one empty bit slot for every whole track N in 0..39 whose position 4N has no mapping.
   - A position that a TMAP entry maps to an empty bit slot is first unmapped and then treated the same way, on an image with no damage. The track-record rule is spec 040's (its FR-053), built in 040's `51ad867e8` (on `origin/040-disk-inspector`, not on `master`), which this branch takes rather than building its own (tasks.md T111, T136). Under it a record with a zero bit or byte count is an empty slot that is not damaged, whatever its start block and block count, because the count is tested before any location check: the all-zero record the WOZ format gives an unused record, and the zero-count record `BuildSyntheticV2` writes for a 0-bit track (start block 3, block count 1, `WozLoader.cpp:1102-1105`, `:1147-1149`), which the owner confirmed on 2026-10-09 is empty, not damage. A record with a count above zero and a zero start block or block count (`DamageReason::RecordLocationMissing`), a start block of 1 or 2 (`RecordInHeader`), or a count larger than its blocks hold (`CountExceedsBlocks`, bit records included), is damage, and so is a TMAP or FLUX entry from 160 to 254 (a `DamagedQuarterTrack`, reason `MapEntryOutOfRange`, which `HasDamagedTracks` and `IsDamaged` include); each stays mapped and write-protects the image. An empty record a FLUX entry maps stays an empty flux track, which the guest can already write (below, "Why a bit track"), so it is not reserved. Today `ParseV2Track` (`WozLoader.cpp:288`) and `ParseV2FluxTrack` (`:365`) load every record with a zero start block, block count or count as an empty slot that is not damaged, read a record with start block 1 or 2 and counts above zero from the file's header (`:290-318`, `:367-399`), record only a flux count larger than its blocks as damage (`:377-381`), and skip a map entry from 160 to 254 as unmapped. `51ad867e8` cannot be cherry-picked before tasks.md T136, because it also edits `WozCompatibility.cpp`, which reaches this branch only with `master` (merge `d78d2941a`); until then the reservation runs on today's loader, and the branch does not merge before T136 re-runs its tests on 040's.
   - The slot is TRK index N when that index is free. Otherwise it is the lowest free index.
   - Quarter tracks **4N, 4N+1, 4N+2 and 4N+3** are mapped to that slot wherever they are still unmapped. This is Casso's own qt/4 placement.
   - An empty slot still resolves to -1, so reads, `Serialize` and every report behave exactly as before until the guest writes.
3. **Which images get it.** Only an image that is loaded, is a WOZ, and is not write-protected. Never `.dsk`, `.do`, `.po` or `.nib`, and never an empty drive.
4. **What kind of track.** Always a bit track, including on a WOZ 2.1 image that has flux tracks.
5. **Its length.** 51,200 bits, which is `Disk2NibbleEngine::kUnformattedTrackBits`. A `static_assert` ties it to `NibblizationLayer::kTrackBitCapacity`.
6. **Saving and state.** The slot count is fixed when the disk mounts, and no state field is added.
   - `SaveState` saves the created track as ordinary bits.
   - `Serialize` maps it through its existing TMAP rebuild.
   - Step-back, the flush hold, replay and heat replay all work unchanged.
7. **One compatibility change.** `DiskImage::LoadState` accepts a WOZ state with fewer slots than the image, provided every surplus slot in the live image is an empty bit slot.

This differs from the audit's proposal (`specs/041-disk-integrity/audit.md:2539-2575`) in five places:

- It adds the loaded-WOZ gate.
- It uses qt/4 placement instead of 4N-1..4N+1.
- It works out which TRK indexes are in use from the map *after* empty bit records are unmapped, not from the raw TMAP and FLUX bytes.
- It adds the `LoadState` compatibility change.
- The helper is `TryCreateTrack`, returning `bool`.

### Rationale

#### Head stop

- **The stop has to be even.** Every stepper move is an even number of quarter tracks: the `kPhaseDelta` table is doubled (`Disk2Controller.cpp:394-400`, `:419`). Clamping to an odd value (`:431-434`) strands the head between detents. Today, the stop at 139 makes every later inward step land on an odd position (audit item 5).
- **158 is the right even value.** It is the last even index inside the 160-entry map (`DiskImage.h:106`), and it is inside the engine's own clamp (`Disk2NibbleEngine.h:55`, `Disk2NibbleEngine.cpp:156-159`).
- **156 is too short.** It would keep the head off 158. Casso's own WOZ images map 158 to track 39, and so does the qt/4 reservation below.
- **Every reader takes the constant, so no other code changes:**
  - the bump/step test at `Disk2Controller.cpp:447` and `:465`
  - the diagnostics at `:1077`, which size the debugger ruler (`DiskHeadView.h:73`, now 159 positions)
  - the `LoadState` check at `:1198`
  - `SaveState` (`:1101`) writes the head as one `Byte`, and 158 fits
- **The end-stop sound needs no code change.** `Disk2AudioSource` holds no head position:
  - `OnHeadBump` (`Disk2AudioSource.cpp:575-628`) is driven entirely by the controller's bump test.
  - `OnHeadStep` ignores `newQt` (`:489`).
  - So the thunk moves with the constant. From qt 156, the next outward half-step is an ordinary step to 158, and the step after that bumps. In both cases the head is on track 39 (158/4 = 39), which satisfies US4 scenario 1.
- **The drive-widget display still works.**
  - The head value comes from the engine (`DiskManager.cpp:871`).
  - The redraw signature packs 8 bits (`EmulatorShellPresent.cpp:678-681`), so 158 fits and the unknown value -1 (0xFF) is still past it.
  - The 2D rail already clamps (`DriveWidget.cpp:330-342`), and that clamp is tested over the engine's whole range (`UnitTest/UiTests/DriveWidgetHitTests.cpp:332-378`).

#### Why storage is reserved at load

- **035's keyframes need a fixed slot count.**
  - `SaveState` writes every slot (`DiskImage.cpp:1104-1147`).
  - `LoadState` rejects a state with a different count (`:1276`).
  - The quarter-track map is not saved (`DiskImage.h:252-256`).
  - So if slots grew when the guest wrote, every keyframe taken before the first write would fail to load, and stepping back across a guest INIT would fail.
  - Reserving at load also means the outer slot vectors are never reallocated on the emulation thread.
- **The reservation has to come out the same after a save and reload.**
  - `MachineStateFile` serializes each disk (`MachineStateFile.cpp:77`), reloads it into a fresh image (`:510-511`, `:562`), and then runs `LoadState` on it.
  - Heat history caches the serialized bytes and the live slot count (`HeatHistory.cpp:1366-1386`), and the scratch replayer can only grow the count (`ScratchHeatReplayer.cpp:563-569`).
  - `Serialize` writes each slot's index as its TRK index, and it maps only positions that hold data (`WozLoader.cpp:1379-1411`, `:1496-1504`, `:1516-1523`).
  - Under the rule "tracks in ascending order, index N if free, otherwise the lowest free index", reloading the serialized file reproduces every reservation the guest has not written to:
    - A track the guest created keeps its index and is now mapped explicitly in the file.
    - No earlier track could have chosen that index, because it was still free when the created track chose it.
    - Every later track gets the same free indexes as before.
  - This only holds if the set of used indexes is worked out from the map that `Serialize` actually writes back. `Serialize` writes empty bit records as $FF, so they are unmapped first. Damaged records stay mapped, because `DamagedMountReport.cpp:118-128` reads them, and an image with any damage gets no reservation at all, so its map is written back exactly as loaded.
- **The common index is also the safe one.** Taking index N for track N when it is free matters because sector decoding reads slot N for track N (`NibblizationLayer.cpp:730-733`).

#### Why at the first written bit

- **No creation without rotation.** `Tick` returns at once when the motor is off (`Disk2NibbleEngine.cpp:543-546`).
- **Q7 alone is not a write.** The //c IWM firmware writes its MODE register with Q7 high and the motor off (`Disk2Controller.h:106-113`). Creating a track whenever Q7 went high would add tracks the guest never wrote, and the next flush of any other track would put them in the file.
- **The first bit lands exactly where it would have.** Over a blank position the bit cursor already wraps every 51,200 bits (`Disk2NibbleEngine.cpp:702-707`). So with a created track of the same length, the first bit lands at the same point of the revolution, `PlaceHead` keeps its cursor unchanged (`:289-302`), and for the engine's `LoadState` cursor check (`:986-992`) the length is the same before and after the creation.
- **Creation is invisible to reads.** A created track is all zeros, so the read side still feeds `ApplyHeadWindow (0)` (`:617`), exactly as on an unformatted position. The weak-bit random generator gets identical input, and a replay creates the track at the same instruction.

#### Why a loaded WOZ only

- **Empty drives would otherwise gain a track.** An empty drive points its engine at the controller's own unloaded image (`Disk2Controller.cpp:59-62`, `:976-977`). That image has 35 empty bit slots under the qt/4 map (`DiskImage.cpp:33-42`, `:59-72`). Without the `m_loaded` check, a guest write to an empty drive would create a phantom track that then reads back as a disk.
- **The other formats cannot use it.** Sector and nibble images size all 35 tracks when they load (`NibblizationLayer.cpp:350-354`, `NibbleImageCodec.cpp:133-138`), and their serializers cannot hold tracks 35-39. So FR-021's "save it into the image" can only be met for WOZ.
- **Write protection is checked first, as in `WriteBit`.** `WriteBit` checks protection before anything else (`DiskImage.cpp:488-490`). The new check does the same, so a protected disk never gains an all-zero track that a later flush would write out.

#### Why qt/4 placement (4N..4N+3)

- **It is the placement Casso already uses everywhere:**
  - the `DiskImage` default map (`DiskImage.cpp:59-72`)
  - every WOZ Casso writes (`BlankDiskBuilder.cpp:444-460` goes through `Serialize`'s rebuild)
  - the rule `TrackWritability` applies before any host-side sector edit (`TrackWritability.cpp:44-54`: the slot must equal qt/4; checked in `VolumeImage.cpp:459-464`)
- **Applesauce's placement breaks host-side edits.**
  - Applesauce maps track N at 4N-1, 4N and 4N+1. For example, `Apple2/Demos/Karateka.woz` has a TMAP that begins `00 00 FF 01 01 01 FF`.
  - `TrackWritability` treats 4N-1 → N as half-track data. So a third-party blank WOZ formatted by the guest would then be rejected by the `disk` tools (US4 scenario 2 followed by `disk put`).
- **The only difference the emulator can see is at 4N+2.** The stepper only reaches even positions. Under qt/4, a half-track write over a blank track lands instead of being dropped, and a half-track read returns track N. That is already how every Casso-authored WOZ and sector image behaves.

#### Why a bit track, even on a flux image

- The reserved slot is a bit slot already: `ResizeTrack` makes it one (`DiskImage.cpp:728-732`).
- Casso writes every cell at the nominal timing, so a bit track holds what the guest wrote exactly.
- `Serialize` places each quarter track in the TMAP or FLUX map according to its slot's kind (`WozLoader.cpp:1494-1504`), so one image can mix both, as `BuildSyntheticV21` already does (`:1264-1288`).
- A FLUX-mapped empty record is already writable today: a flux slot counts as data (`DiskImage.cpp:123`), and an empty flux track has the nominal revolution length (`FluxTrack.cpp:115-118`).

#### Flush, hold and reverse execution

- **Flushing.** `ResizeTrack` does not mark the image dirty (`DiskImage.cpp:715-736`). The `WriteBit` that follows does (`:503-504`), so the existing gates (`FlushAllUnlessHeld`, the hold, replay) handle it like any other guest write.
- **Step back.** `LoadState` puts the slot back to 0 bits, marks it dirty (`:1404-1408`) and bumps the layout generation (`:1311`). The engine then looks the slot up again (`Disk2NibbleEngine.cpp:1170`). A flush after the step back writes $FF there, which is correct.
- **Discarding held writes.** `DiscardHeldWrites` reloads the file into a fresh image (`DiskImageStore.cpp:3511-3515`), so no created track survives.

#### Why the `LoadState` compatibility change

- **The slot count of a typical WOZ changes.** A fresh image has 35 slots (`DiskImage.cpp:35-39`), and `EnsureTrackSlots` only grows to the highest TRK index plus one (`WozLoader.cpp:699-715`). So a standard 35-track WOZ goes from 35 slots to 40.
- **Older state files would fail badly.** A `.cassostate` saved by a 035 build holds 35 slots for such a disk. `Apply` replaces the disks in the bays *before* it loads the machine state (`MachineStateFile.cpp:232-236`), so the count check at `DiskImage.cpp:1276` would fail after the bays had already been replaced.
- **The change is narrow.** The surplus slots it accepts are exactly the ones the reservation adds.

### Alternatives considered

| Option | Why rejected |
|---|---|
| Grow slots when the guest writes (`EnsureTrackSlots` plus `SetQuarterTrackSlot`) | `LoadState` rejects the count across the creation; the map is not saved, so a restore or heat replay cannot rebuild it; it reallocates slot vectors on the emulation thread. |
| Every WOZ gets 160 slots | Deterministic, but about four times the per-keyframe header cost (`DiskImage.cpp:1115-1117`), every saved count changes, and nothing is gained. |
| Create the track when Q7 goes high | Q7 can be high with nothing written (IWM MODE writes with the motor off); it would create tracks the guest never wrote. |
| Create a flux track on flux images | More complex (the held-burst splice path) and no gain, since writes are nominal-timed. |
| Applesauce placement 4N-1..4N+1 (the audit's choice) | `TrackWritability` would reject host-side edits of guest-formatted blank WOZs, and a half-track write would still be dropped. It would need a change to `TrackWritability` outside this scope. |
| Separate reservations for half tracks | Up to twice the slots; no formatter writes there. |
| Head stop 159 | Odd: brings back the off-detent fault. |
| Head stop 156 | One half step short of a position Casso's maps assign to track 39. |
| No compatibility change, or bump `DiskImage::kStateVersion` | A version bump would invalidate every state file, including `.dsk` ones that would still load. No change at all leaves the partial apply described in Risks. |
| Gate on a per-slot "reserved" flag instead of loaded and WOZ | Another vector to reset on load and eject, and no case it covers that the format gate misses. |
| Widen the 2D rail to 158 | A visual decision for the owner; the clamp already keeps the core on the rail. |

### Design detail (types, functions, files to change, with signatures)

#### `CassoEmuCore/Machines/Apple2/Common/Disk2Controller.h` and `.cpp`

```cpp
// Disk2Controller.h:43
static constexpr int    kMaxQuarterTrack = DiskImage::kQuarterTrackCount - 2;   // 158
```

At file scope in `Disk2Controller.cpp`:

```cpp
static_assert ((Disk2Controller::kMaxQuarterTrack & 1) == 0,                         "the head stop must be a half-track detent");
static_assert (Disk2Controller::kMaxQuarterTrack <= Disk2NibbleEngine::kMaxTrack,     "the head stop must lie inside the engine's range");
static_assert (Disk2Controller::kMaxQuarterTrack <  DiskImage::kQuarterTrackCount,    "the head stop must lie inside the quarter-track map");
```

No other change in the controller.

#### `CassoEmuCore/Devices/Disk/DiskImage.h` and `.cpp`

```cpp
public:
    bool  TryCreateTrack    (int quarterTrack, size_t bitCount);
private:
    bool  AreSlotsBlankFrom (size_t firstSlot) const;
```

`TryCreateTrack` checks first, then sizes the track. `canCreate` is true only when all of these hold:

- `m_loaded`
- `m_format == DiskFormat::Woz`
- `!IsWriteProtected()`
- `bitCount > 0`
- the mapped slot (`GetMappedSlot`) is in range
- that slot is a bit slot
- that slot holds 0 bits

If `canCreate` is true it calls `ResizeTrack (slot, bitCount)`, which bumps the layout generation and touches the track but does not mark it dirty. It returns `canCreate`. There are no fallible calls, so it does not use EHM.

Changes inside `LoadState` (`DiskImage.cpp:1249-1314`):

- Replace the equality check at `:1276` with `CBREx (trackCount <= diskTracks, ...)`.
- Hoist a local, then check it:
  - `surplusBlank = trackCount == diskTracks || (m_format == DiskFormat::Woz && AreSlotsBlankFrom (trackCount));`
  - `CBREx (surplusBlank, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));`
- Size the four staging vectors to `diskTracks`.
- Read `trackCount` tracks.
- Run the commit and touch loops over `diskTracks`. The surplus slots compare equal, so they stay clean.

Also update the banners on `ResolveQuarterTrack` (`:80-101`) and `LoadState` (`:1234-1246`).

#### `CassoEmuCore/Devices/Disk/DiskTrackSnapshot.cpp`

In `Restore`, add `disk.m_layoutGeneration++;` after the track loop (`:83-95`), the way `DiskImage::LoadState` does at `:1311`. Without it, an engine that cached a created slot keeps it over a restored 0-bit slot.

#### `CassoEmuCore/Machines/Apple2/Common/WozLoader.h` and `.cpp`

New private statics (class members, per the constitution):

```cpp
static void  ReserveBlankTracks  (DiskImage & out);
static void  UnmapEmptyBitSlots  (DiskImage & out);
static int   PickReservedSlot    (const vector<bool> & used, int wholeTrack);
static void  MapReservedTrack    (DiskImage & out, int wholeTrack, int slot);
```

`Load` calls `ReserveBlankTracks (out);` just before `out.ClearDirty()` (`WozLoader.cpp:809`).

`ReserveBlankTracks` is a `void` function using EHM with a vestigial `hr`, ending in `Error: return;`. Its steps:

0. Return at once when a hoisted `isDamaged` (`out.IsDamaged()`) is true: a damaged image is write-protected, so `TryCreateTrack` could never create a track on it, and every map entry stays exactly as loaded, whatever the `DamageReason`, a `DamagedQuarterTrack` included (040 session, 2026-10-09). This replaces the per-slot `IsDamagedBitSlot` test of the second revision.
1. Call `UnmapEmptyBitSlots`. For each qt where `GetMappedSlot >= 0`, the slot is a bit slot and `ResolveQuarterTrack < 0`, it calls `SetQuarterTrackSlot (qt, -1)`.
2. Build `used[160]` from `GetMappedSlot` over all 160 positions.
3. For each track N from 0 to 39 (`kMaxTracks`, `WozLoader.cpp:27`) where `GetMappedSlot (4N) < 0`:
   1. `slot = PickReservedSlot (used, N)`.
   2. `CBRA (slot >= 0)`. This cannot fail: a whole track with 4N unmapped means at most 159 positions are mapped, so at most 159 indexes are used, and each reservation uses one index while mapping at least one position.
   3. Mark `used[slot]`.
   4. `EnsureTrackSlots (slot + 1)`, then `ResizeTrack (slot, 0)`. The resize empties stale bits on a reused image (`DiskImage.cpp:810-812`).
   5. `MapReservedTrack` maps `4N + k`, for k from 0 to 3, wherever that position is still unmapped.

Update the `Load` banner (`:530-540`).

#### `CassoEmuCore/Machines/Apple2/Common/Disk2NibbleEngine.h` and `.cpp`

```cpp
private:
    bool  TryCreateTrackUnderHead();
```

The helper returns `false` when `m_disk` is null. Otherwise:

- `created = m_disk->TryCreateTrack (m_currentTrack, kUnformattedTrackBits);`
- `if (created) ResolveSlot();`
- `return m_slot >= 0;`

In `StepLss`, inside `if (readClock)` and before `writing` is computed at `:672`:

```cpp
if (m_writeMode && !hasTrack)
{
    hasTrack = TryCreateTrackUnderHead();
}
```

Also:

- Add the `static_assert` that ties `kUnformattedTrackBits` to `NibblizationLayer::kTrackBitCapacity` (include `NibblizationLayer.h`).
- Fix the comments at `:457-462` and `:702-705`.

#### Comment-only changes

- `Ui/DriveWidgetState.h:66`: "0 to `Disk2Controller::kMaxQuarterTrack` (158)".
- `Shell/EmulatorShellPresent.cpp:672`: "runs to 158".
- `Ui/Chrome/DriveWidget.cpp:317-322`: positions past the rail now come from the controller's stop at 158.
- `UnitTest/EmuTests/Disk2Tests.cpp:384`.
- `spec.md` FR-020 and FR-021 wording:
  - FR-020: the head stop is quarter track 158.
  - FR-021: limited to WOZ images, to whole tracks whose 4N position holds no data, mapping 4N..4N+3.

#### Unchanged

- `WozLoader::Serialize`
- all three `SaveState` functions and every `kStateVersion`
- `Disk2AudioSource`, `IDriveAudioSink`, `IDisk2EventSink`
- `DiskHeadView`
- `DriveWidget::kMaxQuarterTrack`
- `Disk2NibbleEngine::kMaxTrack`
- the flush and hold code

### Test plan (which UnitTest files and new tests)

"Fails before" marks a regression test for an audited defect (SC-001). All tests use in-memory images only.

#### FR-020

**`UnitTest/EmuTests/Disk2Tests.cpp`**

- `HeadReachesTrack39AndReadsIt` (fails before):
  - Setup: `EnsureTrackSlots (40)`, then size slot 39.
  - Walk out 78 half-steps.
  - Assert the head is at qt 156, the engine is at 156, and `ResolveQuarterTrack (156) == 39`.
  - Lay down D5 AA 96 and read 0xD5.
- `HeadRestsOnAnEvenDetentAtTheOuterStop` (fails before):
  - Walk 300 steps.
  - Assert the head is at `DiskImage::kQuarterTrackCount - 2` and even.
  - Take one inward half-step and assert 156.
- Strengthen `HeadStepWrapsAtTrackBoundaries` (`:377-399`) to assert equality and evenness.

**`UnitTest/Devices/Disk2ControllerEventTests.cpp`**

- `PhaseChange_pastOuterStop_firesHeadBumpNotHeadStep`: the `atQt` must equal the literal `kQuarterTrackCount - 2`. Comparing against the constant itself would pass before the fix.

**`UnitTest/Devices/Disk2ControllerAudioTests.cpp`**

- `PhaseChange_pastOuterStop_firesOnHeadBump_notOnHeadStep`
- `PhaseChange_fromTrack34ToTrack36_firesOnlyHeadSteps` (fails before: today's stop bumps at 140)

**`UnitTest/Devices/Disk2StateTests.cpp`**

- `ControllerHeadAtTheOuterStopRoundTrips`: head at 158.

#### FR-021: reservation

**`UnitTest/EmuTests/WozLoaderTests.cpp`**

- `Load_BlankWholeTracks_ReservedAtTheirOwnIndex`:
  - Image: tracks 0-34 mapped Applesauce-style through `BuildSyntheticV21`.
  - Asserts: `GetTrackCount() == 40`; `GetMappedSlot (4N..4N+3) == N` for N from 35 to 39; `ResolveQuarterTrack == -1`; mapped tracks' 4N+2 untouched.
- `Load_BlankTrackWhoseIndexIsTaken_TakesTheLowestFreeIndex`: track 5 maps to TRK 20, and track 20 is unmapped.
- `Load_EmptyBitRecord_IsReservedLikeAnUnmappedTrack`: two cases, an all-zero record, built by zeroing the start block and block count of the record `BuildSyntheticV2` writes for 0 bits, and that record as written (start block 3, block count 1), both empty under 040's rule.
- `Load_DamagedImage_KeepsItsMapAndReservesNothing`: a WOZ with one damaged record and one all-zero record keeps every `GetMappedSlot` and its slot count after `Load`, and stays write-protected; the `DamagedMountReport` quarter tracks are unchanged. In Phase 7 the damage is a record past the end of the file (`OutsideFile`), damage under both loaders; tasks.md T136 adds `RecordLocationMissing`, `RecordInHeader` and a `DamagedQuarterTrack` (a TMAP entry of 200) once `51ad867e8` is in.
- `Load_ReusedImage_EmptiesReservedSlots`: stale bits from an earlier load are cleared.
- `Serialize_UnwrittenImage_IsUnchangedByTheReservation`: TMAP and TRKS bytes are identical.
- `Serialize_ReservationSurvivesARoundTrip`: create and write one reserved track, serialize, load into a fresh image; the slot count and every `GetMappedSlot` are equal. Cover three cases: identity, fallback index, and an empty bit record (all zero).

**`UnitTest/EmuTests/DiskImageTests.cpp`**

- `TryCreateTrack_OnAReservedBlankTrack_SizesItWithoutMarkingDirty`: 51,200 bits, `!IsDirty()`, layout generation changed.
- `TryCreateTrack_DeclinesEveryOtherCase`: write-protected, not loaded, `.dsk` track 37, flux slot, slot already holding data, unmapped position.

#### FR-021: write path

**`UnitTest/EmuTests/DiskWritePathTests.cpp`**

- `LssWrite_OverATrackTheWozLeavesBlank_LandsAndReadsBack` (fails before). Also asserts the slot count is unchanged and that the bit cursor is the same before and after the creation.
- `LssWrite_OverAnEmptyBitRecord_LandsAndReadsBack` (fails before), over an all-zero record.
- `LssWrite_OverABlankTrackOfAProtectedWoz_LeavesItBlank`.
- `LssWrite_IntoAnEmptyDrive_CreatesNothing`: the controller's unloaded disk. It would fail under the audit's version.
- `LssWrite_OverTrack37OfASectorImage_CreatesNothing`.
- `LssWrite_OverABlankTrackOfAFluxWoz_CreatesABitTrack`: the TMAP has the entry and FLUX keeps only the flux track.
- `StoreFlush_OfACreatedTrack_MapsAllFourPositionsInTheSavedFile` (fails before). Follows the `SetFlushSink` pattern at `:392-435`.
- Scenario `GuestWriteRoutine_OverAnUnmappedWoz_SavedFileHoldsThePayload` (fails before). Uses `TestMachine`, the `kWriteSource` routine and an all-$FF `BuildSyntheticV21({})`, then ejects and reloads the sink bytes.
- Scenario `Guest_SeeksToTrack39OfAFortyTrackWoz_ReadsIt` (fails before). Drives the soft switches through the machine bus.

#### FR-021: state and keyframes

**`UnitTest/Devices/Disk2StateTests.cpp`**

- `DiskStepBackAcrossTheFirstWriteToABlankTrack`:
  - `LoadFrom` returns `S_OK`.
  - Then `ResolveQuarterTrack == -1` and `IsDirty`.
  - The engine re-resolves to -1.
- `ControllerAndDiskRunTheSameAcrossATrackCreation`: load the keyframe, rerun, and the media is byte-identical.
- `DiskLoadsAWozStateWithFewerBlankSlots`.
- `DiskRejectsFewerSlotsWhenTheSurplusHoldsData`.
- `DiskRejectsFewerSlotsOnASectorImage`.
- `SnapshotRestoreAcrossATrackCreation_RefreshesTheEngine`.

#### Host tools

**`UnitTest/EmuTests/NibblizationTests.cpp`**

- `TrackWritability_GuestCreatedTracks_KeepTheImageWritable`: pins the qt/4 choice.

**`VolumeImage` tests**

- `SaveBitStream_IntoAnUnmappedWozTrack_IsKept`. This is a side effect of the reservation:
  - `RenibblizeTracks` sizes slot N but never maps it (`NibblizationLayer.cpp:452-466`).
  - So today a host-side edit into an unmapped WOZ track serializes as $FF and is lost.

#### Unchanged by design

- `DiagnosticsProviderTests.cpp:153`
- the debugger view tests with a literal 139
- `DriveWidgetHitTests.cpp:332-378`
- `DriveWidgetStateTests.cpp:56`

#### Gate

The full suite in Debug and Release, including `GameBootTests` and `DiskReadbackTests`, to catch any title that relied on the stop at track 35.

### Risks

- **035 defect (reported to the 035 session, not fixed here).** At `811a6f727`, `MachineStateFile::Apply` replaced the disks in the bays before `LoadStateOverMountedMedia` (`MachineStateFile.cpp:232-236`), so any state load that failed after that point left the bays changed, which contradicts the banner at `:205-210`. The compatibility change keeps this decision from triggering it. Reported on 2026-10-08 as 041 audit defect e; 035 fixed it in `7e6c7bf8d` and `446df598c`, which tasks.md T008 brings. The compatibility change does not depend on that fix and stays.
- **The track-record rule is spec 040's.** 040 built it in `51ad867e8`, and this branch takes that commit, through `master` when 040 merged first or by cherry-pick at tasks.md T136, and builds no loader change of its own (040 session, 2026-10-09). The question the second revision left open, which case a zero-count record with a start block of 3 or more joins, is settled: it is empty, not damage (owner confirmed 2026-10-09), as 040's code has it. Coordination goes to the session "[relmer-desktop] 040-disk-inspector" (tasks.md T111).
- **Some slot counts can still differ after a round trip.** If an empty or damaged TRK record has the highest index in the file, and that index is 40 or above, a reload has fewer slots than the live image. The state then holds *more* tracks than the reload and is still rejected. Today's code has the same failure for damaged records.
- **Half-track aliasing.** Under qt/4 placement:
  - A half-track read of a created track returns track N's data, not noise.
  - A copier writing half-track protection onto a blank WOZ overwrites track N's slot.
  - This matches Casso-authored WOZs and sector images today, but not Applesauce's placement.
- **Existing defect, observed, not in scope.** `TrackWritability` already rejects every image that uses Applesauce placement (qt 4N-1 → N, e.g. `Apple2/Demos/Karateka.woz`), so the `disk` tools will not edit third-party WOZs.
- **Write cost over an uncreatable position.** While the guest writes over a position that cannot be created (protected disk, `.dsk` tracks 35-39, a half track of a mapped track), `TryCreateTrack` runs on every bit cell. That is about six comparisons per cell, only during the write burst. Reads cost nothing extra.
- **Keyframe size.** Each created track adds 6,400 bytes, shared between keyframes until it changes, plus 13 header bytes per reserved slot (`DiskImage.cpp:1115-1117`).
- **Odd head positions from old states.** A state saved before this change with the head at 139 still loads (139 ≤ 158), and the head stays on odd positions until it reaches a stop.
- **2D rail parks for tracks 35-39.** The rail does not show motion beyond track 34. Widening it is the owner's visual decision.
- **Spec text must change.** FR-020 says 159 and FR-021 says "any quarter track". The implementation follows this record, so `spec.md` has to be updated with it.
- **Test churn.** Any test that compares a WOZ's slot count against a hand-built 35-slot image will break. A search found none: `Disk2StateTests` media helpers use `.dsk` (`:256-330`).

## R5. The ownership rule and its Debug assertion (FR-001, SC-003)

Worktree `041-disk-integrity` at `fb05ee2ab`, which is 035 at `811a6f727` plus the spec. Every path is relative to the worktree. I changed nothing.

### Decision

1. **One owner token per disk store, shared by its drives.**
   - A new core type, `ThreadOwnership`, records which thread holds a `DiskImageStore`.
   - Every `Disk2Controller` that `MachineBuilder` wires into a machine checks the token of that machine's store, not a token of its own.
   - So the store, the images it owns, the controller and its nibble engines always change hands together. That is the single owner FR-001 requires.
2. **The thread that constructs a store holds it.** No call is needed to own one. This is why tests and scenario tests are owners by default:
   - `MachineHost` creates its store in its constructor (`CassoEmuCore/Shell/MachineHost.cpp:41`).
   - Every test, `TestMachine`, `HeadlessMachineFactory`, `DebugBatchRunner` (`Cli/DebugBatchRunner.cpp:181`) and every `EmulatorShell` built in a test constructs that store on the thread that then uses it.
   - A controller built outside a machine (for example `Disk2Controller ctrl (6)` in `UnitTest/EmuTests/DiskImageStoreTests.cpp:732`) falls back to a token of its own, also held by the thread that built it.
3. **Handover takes two steps, each on its own thread.** The holder calls `Release`, then the taking thread calls `Claim`. No call can make some other thread the owner.
4. **Owner in each phase:**

| Phase | Owner | Where the change happens |
|---|---|---|
| Live machine constructed | UI thread | `EmulatorShell::m_machine` (`Shell/EmulatorShell.h:1706`) builds the store at `MachineHost.cpp:41` |
| Startup, before the CPU thread exists | UI thread | `Initialize`: `BuildMachineDevices` at `EmulatorShell.cpp:424`, `PowerCycle` at `:605` (which calls `FlushAllUnlessHeld` and `BindDiskDrives`, `MachineHost.cpp:984`, `:992`), `MountCommandLineDisks` at `:626`. The wiring setters run at `:703` and `:865`, and `:1140` (`InstallChangeReporting`) |
| Handover to the CPU thread | UI releases, CPU claims | The UI releases immediately before `m_cpuManager.Start` (`Shell/Window/EmulatorWindow.cpp:1346`). `OnCpuThreadStart` (`Shell/EmulatorShellCpuThread.cpp:200`) claims in the first statement after its declaration block (`HRESULT  hr = S_OK;` and the three blank lines CS0016 requires), ahead of `m_wasapiAudio.Initialize()` and `StartReverseRecording` (`:219`), which reaches `SetFlushHold`/`SetMediaRetention` (`Debugger/Reverse/ReverseController.cpp:98-99`) |
| Running, **paused or not** | CPU thread | Drained commands (`CpuManager.cpp:583`), the service (`:585-588`), Tick callbacks (`MachineBuilder.cpp:1519-1545`), keyframes, replays, `HeatHistory::CopyDisks`. A pause transfers nothing: it has no handshake and the queue still drains (threading-035 §1) |
| Machine switch | CPU thread throughout | `SwitchMachine` runs on the CPU thread (`CpuCommandDispatcher.cpp:53-54`), and the store survives it. The rebuilt controllers are wired to the store's token at `MachineBuilder.cpp:613-625`, on that thread. Nothing is handed over |
| CPU thread stop | CPU releases | The last statement of `OnCpuThreadStop` (`EmulatorShellCpuThread.cpp:319-326`) releases, after `StopReverseRecording` and `CloseDebugger` |
| Join until the shell takes the disks back | Nobody | Any access in this window asserts |
| Shutdown flush | UI thread | `~EmulatorShell` claims right after `m_cpuManager.Stop()` (`EmulatorShell.cpp:236`). That comes before `controller->SetEventSink (nullptr)` at `:250` and `FlushAllForShutdown` at `:299` |
| `ScratchHeatReplayer` | Whichever thread runs the job | `Run` (`Shell/ScratchHeatReplayer.cpp:360-393`) claims at entry and releases at `Error:`. The machine is built inside `Run` (`:419`, `:480`) on a pool thread, or on the CPU thread for `FindAccess`/`Rebuild` |
| `ScratchMachineRenderer` | Whichever thread renders | `Render` (`Shell/ScratchMachineRenderer.cpp:76-130`) claims and releases the same way |
| CLI debug batch, headless runs, unit and scenario tests | The constructing thread | Never handed over |

5. **The check.** `ASSERT_THREAD_OWNERSHIP (ownership)`:
   - It is a Debug-only macro that expands to EHM `ASSERT` (`Ehm/Ehm.h:135-147`) at the call site, so the report gives the entry point's file, line and function.
   - In Release it is `((void) 0)`, gated by the same condition as `EHM_BREAKPOINT` (`Ehm.h:126-131`).
   - It is the first statement after the declaration block of every guarded entry point (after the three blank lines CS0016 requires), or the first statement of an inline one-liner's body. Putting it ahead of the declarations would push them below a statement, which the declarations-at-the-top rule forbids and no gate catches.
   - Its argument is a member, a dereference or a hoisted local reference, never a call.
   - It detects; it does not lock. A violation reports through the installed host hook, and the call continues.
6. **Exempt from the check:**
   - `NoteExternalChange` (`DiskImageStore.cpp:2692`), the any-thread inbox for FR-005.
   - `GetThreadOwnership`.
   - No published-status reader: R1 puts the publisher outside the store, and the store's own status members (`GetBayStatus`, `GetStatusGeneration`) are guarded like any other.
   - Constructors and destructors.
   - All statics, which hold no instance state and are called from many threads: `GetSourceFormatByExtension`, both `IsMountableImageExtension` overloads, `MakeRecoveryPath`, `WriteFileAtomically`, `GetCommitTemporaryPath`, `ReadFileBytes`, `FormatMountFailureMessage`, `ClassifyLoadFailure`.
   - On `Disk2Controller`: `Read`, `Write` and `Tick` (the per-instruction path), `GetStart`/`GetEnd`, `GetDiagnosticsId`/`GetDiagnosticsTitle` (constants) and `Create` (static).

### Rationale

- **Tests hold the store without any setup, so the check runs in every test.** A store with no owner that accepts any thread (the `GamePortInputMixer` precedent, `Controllers/GamePortInputMixer.cpp:317-318`) would let every single-threaded test pass while checking nothing. With construct-claims, each existing test exercises the check, and any test that reaches a store from a second thread fails.
- **Two steps are the only handover that works at CPU start.**
  - The CPU thread's id does not exist until `std::thread` is constructed (`CpuManager.cpp:86`).
  - `ThreadProc` calls `m_onThreadEnter` right away (`:541-544`), and that touches the store.
  - So the UI cannot set the owner to "the CPU thread" before that thread is already using the store. Release-then-claim makes the gap explicit and checked: anything that touches the store in it asserts.
- **One shared token keeps the store and the drive from disagreeing.**
  - FR-001 covers the bays, the images, and the controller with its engines.
  - A token per object would need a handover for every `Disk2Controller` in the device list, not just `GetRefs().diskController` (`MachineBuilder.cpp:615-625` caches only the first). A missed one would assert on `GetDiagnostics` during the CPU-thread panel build (`Ui/Debugger/DebuggerViewState.cpp:2750`).
- **The macro reports per entry point.** The GUI assertion host shows each assertion site once per run (`Gui/GuiMain.cpp:491-506`, `:514-543`). If the check lived in one shared function, every violation would share one site, and only the first offender would ever be reported. Expanding `ASSERT` at the call site gives `DiskImageStore::GetImage`, `Disk2Controller::GetEngine` and so on a site each.
- **It uses the existing assert facility.** EHM `ASSERT` goes through `EhmBreakpoint`. In the app that is the dialog in `GuiMain.cpp:679`; in unit tests it is the handler that fails the test (`UnitTest/EhmTestHelper.cpp:31-48`). The Debug/Release gate follows the `DXUI_ASSERT_UI_THREAD` precedent (`Dxui/Dxui.h:58-66`).
- **The hot path stays out.** `Tick` runs on every instruction (`MachineHost.cpp:844-847`), and `Read`/`Write` on every `$C0Ex` access. Checking them would add a `this_thread::get_id` to each one in a Debug suite that already takes about 15 minutes. They are reached only through the machine's own step. A step on the wrong thread is still caught: its Tick callbacks call `FlushAllUnlessHeld` and `ApplyPendingReload`, which are guarded.
- **This changes several audit fixes.**
  - FR-002 has the UI read a published copy, so no off-thread reader of `IsMounted`, `GetSourcePath` or `GetImage` survives. The audit's proposals to make those callable off-thread are dropped: the atomics in entry-flags-race Part A, and the locked path copy in entry-path-string-race. Those functions stay owner-only.
  - With FR-005 turning `NoteExternalChange` into an inbox, no bay writer needs `m_pendingMutex` either. note-external-change-reads-entry-unlocked's lock-every-writer fix becomes unnecessary.

### Alternatives considered

| Alternative | Why not |
|---|---|
| Unowned means any thread may call (`GamePortInputMixer.cpp:317-318`) | Silent in every test and in any store nobody wires. "Full suite passing" (SC-003) would then prove nothing |
| First caller claims (`Dxui/Core/DxuiThread.cpp:26-51`) | The live store's first caller is the UI thread during `Initialize`, so the CPU thread could never take over. A process-wide static also cannot serve the per-store scratch machines |
| A bare `SetOwnerThread (std::thread::id)` | Cannot point at the CPU thread before it exists. A setter that can point at any thread cannot itself be checked. `Claim()` is `SetOwnerThread (this_thread)` that only succeeds on an unowned token, and `Release()` is `SetOwnerThread ({})` that only the holder can do |
| Separate tokens on the store and on each controller | The tokens could disagree, and every controller would need its own handover, including those outside `refs` |
| A mutex on every entry point | FR-001 requires one owner, not serialization. A mutex would block the UI frame on CPU slices, and the UI could still keep a `DiskImage *` after unlocking. It also adds lock-order hazards with the watcher joins (note-external-change: never hold a lock across `EndWatching`/`BeginWatching`) |
| Checking against the CPU manager's thread id | It would tie `Devices/Disk` to `Shell`, and it is wrong for startup, shutdown, scratch machines, the CLI and tests |
| Guarding `Read`/`Write`/`Tick` too | Debug cost on every instruction. The store guards already catch a wrong-thread step through the Tick callbacks |
| Checking in Release as well | The spec requires Debug only (SC-003), and it would cost the shipped build |

### Design detail

#### New type: `CassoEmuCore/Core/ThreadOwnership.h` / `.cpp`

One class per `.h`/`.cpp` pair. Add both files to `CassoEmuCore.vcxproj` (the repo has no `.filters` files). `<atomic>` and `<thread>` are already in `CassoEmuCore/Pch.h:31`, `:59`.

```cpp
class ThreadOwnership
{
public:
                       ThreadOwnership       () = default;
                       ThreadOwnership       (const ThreadOwnership &) = delete;
    ThreadOwnership &  operator=             (const ThreadOwnership &) = delete;

    void               Claim                 ();
    void               Release               ();
    bool               IsHeldByCurrentThread () const;

private:
    std::atomic<std::thread::id>  m_holder { std::this_thread::get_id() };
};

#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
    #define ASSERT_THREAD_OWNERSHIP(__ownership)                                \
        {                                                                       \
            bool  __isOwnershipHeld = (__ownership).IsHeldByCurrentThread();    \
            ASSERT (__isOwnershipHeld);                                         \
        }
#else
    #define ASSERT_THREAD_OWNERSHIP(__ownership)  ((void) 0)
#endif
```

- **`Claim`**: `compare_exchange_strong` from `std::thread::id()` to this thread (acq_rel).
  - If the token is unowned, this thread now holds it.
  - If this thread already holds it, nothing changes. The shell's destructor claims whether or not the CPU thread ever ran.
  - If another thread holds it, `CBRA` fires and the holder is unchanged.
  - Written in EHM style with a vestigial `hr`, and the CAS result is hoisted to a local before the macro, as CS0011's rule requires.
- **`Release`**: if this thread holds it, the token becomes unowned (release store). If it is already unowned, nothing changes. This covers `OnCpuThreadStop` when `CoInitializeEx` failed (`CpuManager.cpp:536-537` skips `m_onThreadEnter` but runs `m_onThreadExit` at `:663-666`). If another thread holds it, `CBRA` fires and the holder is unchanged.
- **`IsHeldByCurrentThread`**: an acquire load compared with `std::this_thread::get_id()`. It is compiled in both configurations, so the handover tests mean something in Release too.

#### `DiskImageStore` (`Devices/Disk/DiskImageStore.h`, `.cpp`)

- Add `#include "Core/ThreadOwnership.h"` and two public accessors: `ThreadOwnership & GetThreadOwnership () { return m_ownership; }` and `const ThreadOwnership & GetThreadOwnership () const { return m_ownership; }`, the second for code that holds a `const DiskImageStore &` (the publisher). They are exempt from the check, since they return the token itself.
- Add a private member `ThreadOwnership m_ownership;`.
- Put `ASSERT_THREAD_OWNERSHIP (m_ownership);` as the first statement after the declaration block in every public instance member except `NoteExternalChange`. That is 54 signatures with 51 distinct names at `811a6f727`, and 56 with 53 on 035's tip `d3c15b55c`, which adds `IsRetainingMedia` (an inline one-liner, `DiskImageStore.h:165` there) and `ReportSeatedMedia` for its defect e fix; tasks.md T008 brings both and T012 and T013 count them:
  - **Mounting:** `Mount` (×2), `MountFromBytes` (×2), `Eject`, `MountRestored`.
  - **Flushing:** `Flush`, `FlushAll`, `FlushAllForShutdown`, `FlushAllUnlessHeld`.
  - **Flush hold and replay:** `SetFlushHold`, `IsFlushHeld`, `HasUnsavedWrites`, `CountUnsavedDisks`, `CommitHeldWrites`, `DiscardHeldWrites`, `SetReplaying`, `IsReplaying`.
  - **Media retention:** `GetMediaId`, `SetMediaRetention`, `GetRetainedMediaCount`, `CanSeatMedia`, `SeatMedia`, `PruneRetainedMedia`, `SetPositionSource`, `SetMediaChangeListener`, and on 035's tip `IsRetainingMedia` and `ReportSeatedMedia`.
  - **Write protection and salvage:** `SetImageWriteProtect`, `AssessSalvage`, `IsSalvageOffered`, `SalvageToFile`.
  - **Machine lifecycle:** `SoftReset`, `PowerCycle`.
  - **Bay accessors:** `GetImage`, `IsMounted`, `GetSourcePath`, `GetMountedSourcePaths`.
  - **Wiring setters:** `SetFlushSink`, `SetImageReader`, `SetIdentityReader`, `SetImageWatcher`, `SetFileIo`, `SetMachineName`, `SetMachineRestartCallback`, `SetChangeReportSink`, `SetAskSink`, `SetRescueSink`, `SetBayChangeSink`, `SetDecisionSink`, `SetClock`, `SetTimestampSource`.
  - **External changes:** `ResolvePendingChange`, `ApplyPendingReload`.
  - **State:** `GetSharedState` (×2).
- Entry points other 041 decisions add are guarded too, each with a row in `EveryStoreEntryPointAssertsOffItsOwner` added by the task that adds it (tasks.md, Rules for every task): `GetBayStatus`, `GetStatusGeneration`, `MarkStatusChanged`, `SetUserWriteProtectFlag` and `SetFileWriteProtect` (R1), `SetFileBacked` and `AreAllWritesSaved` (R2), and `SetUserWriteProtect` (settings-wp-drops-dirty). `SalvageToFile`'s row changes with its signature. On the controller, R1's `const` overload of `GetEngine` is guarded and gets a row in `EveryControllerEntryPointAssertsOffItsOwner`. The publisher checks through the store's token (R1).
- The inline one-liners stay inline, with the macro as their first statement. They are at `DiskImageStore.h:143`, `144`, `153`, `154`, `165`, `179`, `185`, `242`, `248`, `252`, `259`, `264`, `269`, `276`, `289`, `309`, `323`, `340`, `354`, `371` and `379`. Keeping them inline limits churn in a header 035 also edits.
- Correct the comments that describe ownership:
  - the class banner (`:19-57`): add the rule;
  - `:335-337` and `:350`: "the thread that owns disk writes" becomes "the thread holding the store";
  - `:383-385`;
  - `:388-389`: "Called on the CPU thread" becomes "On the thread holding the store";
  - `:715-718`.

#### `Disk2Controller` (`Machines/Apple2/Common/Disk2Controller.h`, `.cpp`)

```cpp
    //  Which thread may use this controller: the disk store's, once the
    //  builder wires it, so the drive and its disks change hands together.
    void   SetThreadOwnership (const ThreadOwnership & ownership);
private:
    ThreadOwnership            m_ownOwnership;
    const ThreadOwnership *    m_ownership = &m_ownOwnership;
```

- `SetThreadOwnership` asserts that the current token and the new one are both held by the calling thread, then repoints `m_ownership`. That check takes the place of `ASSERT_THREAD_OWNERSHIP`, and `EveryControllerEntryPointAssertsOffItsOwner` gets a row for it that passes the store's own token, so its repoint changes nothing for the rows after it (tasks.md T014).
- The self-pointer is safe: `ThreadOwnership` cannot be copied, so `Disk2Controller` cannot be copied either.
- Put `ASSERT_THREAD_OWNERSHIP (*m_ownership);` first in:
  - **Disk access:** `MountDisk`, `EjectDisk`, `GetDisk`, `SetExternalDisk`, `HasExternalDisk`, `NotifyDiskInserted`, `NotifyDiskEjected`.
  - **Wiring setters:** `SetAudioSink`, `GetAudioSink`, `SetEventSink`, `SetIwmMode`, `SetMotorOffFlushCallback`, `SetIdleCallback`, `SetCpuCycleSource`.
  - **Inspectors:** `GetActiveDrive`, `IsMotorOn`, `IsMotorAtSpeed`, `GetMotorSpinupRemaining`, `GetQuarterTrack`, `GetCurrentTrack`, `IsQ6`, `IsQ7`, `GetPhases`.
  - **Engine and diagnostics:** `GetEngine`, `GetDiagnostics`.
  - **Lifecycle:** `Reset`, `SoftReset`, `PowerCycle`, `SaveState`, `LoadState`.
- **Drop `noexcept`** from `SetEventSink` (`Disk2Controller.h:104`, `.cpp:1023`) and `SetCpuCycleSource` (`.h:162`). The unit-test assertion handler reports by throwing (`EhmTestHelper.cpp:47`), and a throw out of a `noexcept` function calls `std::terminate`.
- Comments: `Disk2Controller.cpp:1018-1019` becomes "Only the thread holding the store; the shell attaches the panel through a posted command." Also update `Disk2Controller.h:116-122`.

#### `MachineBuilder` (`Shell/MachineBuilder.cpp:613-625`)

Replace the loop that stops at the first controller with one that wires every controller and keeps the first in `refs`:

```cpp
        if (dc == nullptr) { continue; }   // braces on their own lines per style
        dc->SetThreadOwnership (m_host.GetDiskStore().GetThreadOwnership());
        if (m_host.GetRefs().diskController == nullptr) { m_host.GetRefs().diskController = dc; }
```

Also fix the comment at `:1515-1516` ("races nothing").

#### `MachineHost` (`Shell/MachineHost.h`, `.cpp`)

```cpp
    //  The disk store and every drive wired to it, between threads: the
    //  holder releases, then the next thread claims.
    void  ClaimDiskOwnership   ();
    void  ReleaseDiskOwnership ();
```

These forward to `m_diskStore->GetThreadOwnership()`.

#### Shell handover: four statements

- `Shell/Window/EmulatorWindow.cpp`, in `RunMessageLoop`, immediately before `:1346`: `m_machine.ReleaseDiskOwnership();`.
  - Move `m_diskManager->SetColdBootMountWindow (false);` from `:1364` to just above it.
  - Today it writes a plain `bool` (`DiskManager.h:204`) on the UI thread after the CPU thread is running, and `OnBayChange` reads it on the CPU thread (`DiskManager.cpp:558`).
- `EmulatorShellCpuThread.cpp:200`: the first statement after `OnCpuThreadStart`'s declaration block (`HRESULT  hr = S_OK;`, then the three blank lines CS0016 requires) is `m_machine.ClaimDiskOwnership();`, ahead of `m_wasapiAudio.Initialize()`.
- `EmulatorShellCpuThread.cpp:319-326`: the last statement of `OnCpuThreadStop` is `m_machine.ReleaseDiskOwnership();`.
- `EmulatorShell.cpp:236`: right after `m_cpuManager.Stop();` comes `m_machine.ClaimDiskOwnership();`. Update the comment at `:294-298` to state that the shell has taken the disks back.
- The claim is deliberately **not** placed in `OnDestroy` (`EmulatorWindow.cpp:1657`). Any frame that runs between that `Stop` and the destructor must not touch the store, and the assertion should catch one that does.

#### Scratch machines

- `ScratchHeatReplayer::Run` (`ScratchHeatReplayer.cpp:360`): after the declarations, `if (m_machine != nullptr) { m_machine->ClaimDiskOwnership(); }`. At `Error:`, the same test followed by `ReleaseDiskOwnership()`.
  - A machine that `BuildMachine` (`:480`) creates mid-run is owned by its builder and released at the end.
- `ScratchMachineRenderer::Render` (`ScratchMachineRenderer.cpp:76`): the same bracket around `Build`/`LoadStateForPicture`/`Render`.
  - `LoadStateForPicture` reaches the store through `GetStateParts` (`MachineHost.cpp:1192` → `:1427`).
- On 035's tip `d3c15b55c` the heat replayer's machine is built by `ScratchReplayMachine` (`Build`, `ScratchReplayMachine.cpp:63`), and 035's new `ScratchCallReplayer` builds and replays one the same way, on a pool thread of its own or in `Rebuild` (`ScratchCallReplayer.cpp:515`, `:713`). Its `Run` gets the same bracket, with `m_scratch.GetMachine()` in place of `m_machine` (tasks.md T015).
- Why the brackets are needed: the scratch machines are cached across jobs, and `ThreadPoolWorkQueue` runs items "whichever pool threads run them" (`Core/ThreadPoolWorkQueue.h` banner). The one-thread pools created with a name (`ThreadPoolWorkQueue.cpp:64-69`) happen to keep one thread, but tests pass a default-pool queue with no name (`UnitTest/EmuTests/HistoryThumbnailCostTests.cpp:50-51`).
- Update the threading paragraphs at `ScratchHeatReplayer.h:41-43` and `ScratchMachineRenderer.h:25-27`.

#### Callers off the owning thread that must move

| Caller (UI thread unless stated) | Entry point | Moves to |
|---|---|---|
| `DiskManager::UpdateDriveWidgets` every frame (`EmulatorShellPresent.cpp:637`) | `GetSourcePath` `DiskManager.cpp:803`, `GetImage` `:895`, `GetEngine` `:867` | FR-002 published status |
| `EmulatorShellPresent.cpp:780` | `GetSourcePath` | FR-002 |
| `EmulatorShellScene.cpp:1009` | `GetSourcePath` | FR-002 |
| `Window/EmulatorWindowInput.cpp:505`, `:525`, `:543` (tooltips) | `GetSourcePath` | FR-002 |
| Menu enable query (`EmulatorWindow.cpp:755-758`) | `IsWriteProtectToggleOffered` (`EmulatorShellDisks.cpp:771-772`); `IsSalvageOffered` (`:578`) | FR-002 |
| Menu label query (`EmulatorWindow.cpp:777`, `:786`) | `GetImage`, `GetSourcePath` | FR-002 |
| Create disk (`WindowCommandManager.cpp:1221`, `:1281`) | `GetMountedSourcePaths`, `IsMounted` | FR-002 |
| `RunSalvageFlow` from `WindowCommandManager.cpp:1501` and `EmulatorShellDisks.cpp:751` | `AssessSalvage` `:610`, `GetSourcePath` `:621`, `SalvageToFile` `:637`, `MountDiskInSlot6` `:674` (store `Mount`, controller `SetExternalDisk`/`NotifyDiskInserted`) | FR-004 posted commands (salvage-decode-on-ui-thread, salvage-mount-on-ui-thread) |
| `ReportDamagedMount` via `WM_APP_REPORT_DAMAGE` and `HandleMountCompletion` (`EmulatorShellDisks.cpp:244`) | `GetImage` `:696`, `GetSourcePath` `:734`, `AssessSalvage` `:737` | Damage report computed on the mounting thread and returned in `MountCompletion`, as the drive's `BayStatus` copied at P3, as R1's row also gives it |
| `AskAboutChange` | `GetSourcePath` `EmulatorShellDisks.cpp:1157` | The prompt's own image path (entry-path-string-race fix 4) |
| `OpenDisk2DebugDialog` | `controller->SetEventSink` `EmulatorShellDebug.cpp:563` | Posted attach command (debug-sink-attach-race) |
| `OnCopyData` (`EmulatorWindow.cpp:2789`); watcher threads (`DiskImageStore.cpp:3623-3627`) | `NoteExternalChange` | Stays; exempt. FR-005 must make it read no `Entry` |
| After the master merge: update deploy flush (`EmulatorShellUpdate.cpp:686`, master) | `FlushAllForShutdown` | FR-019 deploy hold, with ownership handed over (see Risks) |
| After the master merge: drive context menu (`EmulatorShellStorage.cpp:241-244`, master) | via `IsWriteProtectToggleOffered`/`IsSalvageOffered` | FR-002 |

The rest of the callers already run on the owning thread: every CPU-thread command, `MachineManager`, `MachineStateFile`, `ReverseController`/`Replayer`/`ReverseHost`, `HeatHistory`, `MachineDebugTarget`, the destructor after its claim, and `DebugBatchRunner`.

### Test plan

**Existing suite.** No changes. Every test builds its store on its own thread, and there are no shared fixtures: the only `TEST_MODULE_INITIALIZE` is `UnitTest/ModuleSetup.cpp:21`. No test starts an `EmulatorShell` CPU thread; `cpu.Start` appears only in `UiTests/CpuManagerCommandTests.cpp`, with counters. Two existing tests change thread mid-life, and the scratch brackets cover both: `HeatHistoryTests.cpp:959` and `:1002` (the replayer's own pool) and `HistoryThumbnailCostTests.cpp:50-51` (default pool).

**New helper: `UnitTest/EmuTests/OwnerThreadStandIn.h` / `.cpp`.** It stands in for the CPU thread:
- `HRESULT Take (ThreadOwnership &)`: the test thread releases, a `std::thread` claims and parks.
- `HRESULT RunOnOwner (const std::function<void ()> &)`: runs work on the owning thread. Any exception is caught there into a `std::exception_ptr` and rethrown on the test thread, because `Assert::Fail` on any other thread terminates the host.
- `HRESULT GiveBack ()`: the stand-in releases and exits, and the test thread claims again.
- Every wait is bounded at 5 s and returns `HRESULT_FROM_WIN32 (ERROR_TIMEOUT)`.
- The destructor gives the token back without asserting.
- Code is flat EHM, single exit.

**New: `UnitTest/EmuTests/ThreadOwnershipTests.cpp`.**
- `AnOwnershipIsHeldByTheThreadThatMadeIt`
- `ReleaseThenClaimOnAnotherThreadMovesIt`
- `ClaimingWhileAnotherThreadHoldsItAssertsAndChangesNothing`
- `ReleasingFromAThreadThatDoesNotHoldItAssertsAndChangesNothing`
- `ClaimingTwiceAndReleasingTwiceAreHarmless`
- `TheCheckAssertsOffTheHolder`: uses the macro inside `ExpectedEhmAssert` with `RequireCount (1)`.

**New: `UnitTest/EmuTests/DiskOwnershipTests.cpp`.**
- `EveryStoreEntryPointAssertsOffItsOwner`:
  - A table of `{ L"GetImage", lambda }`, one row per guarded signature (54 at `811a6f727`, 56 on 035's tip), with the stand-in holding the store.
  - One `ExpectedEhmAssert` scope surrounds the whole table. Each row reads `Count()` before and after its call and records the row as missing when the count did not change.
  - Debug only, `Assert::IsTrue (missing.empty(), names)`.
  - Mutation check: stub the macro to `((void) 0)` and the test goes red.
- `ExemptStoreEntryPointsDoNotAssert`: `NoteExternalChange`, `GetThreadOwnership` and the statics. Called off the owner; any assertion fails the test.
- `EveryControllerEntryPointAssertsOffItsOwner`: the same sweep over the controller's guarded list.
- `BusAndTickPathsAreNotChecked`: `Read`, `Write` and `Tick` called off the owner do not assert. This pins the hot-path exemption.
- `ABuiltControllerUsesItsStoresToken`: on a `TestMachine ("Apple2e")`, the stand-in takes the store, and `GetRefs().diskController->GetDisk (0)` on the test thread asserts. This fails if the builder wiring at `MachineBuilder.cpp:613-625` is missing.
- `ARebuildOnTheOwnerWiresTheNewController`: `RunOnOwner ([&] { machine.GetBuilder().Build (machine.GetConfig()); })`, standing in for a machine switch. Then `GiveBack`, and the new controller accepts calls on the test thread.
- `DiskOwnershipFollowsTheCpuThreadFromStartToStop`: a real `CpuManager`.
  - Release before `Start`; `onThreadEnter` claims and `onThreadExit` releases.
  - The command callback runs `store.Eject` and records `IsHeldByCurrentThread` in an atomic. `PostCommand (IDM_DISK_EJECT1)` triggers it.
  - After a bounded wait, `Stop`, then claim.
  - All asserts are on the test thread. This is meaningful in both configurations.

**Extensions** (each fails in Debug until the FR-002/FR-004 change it covers lands):
- `UnitTest/EmuTests/DiskResetRemountHoldTests.cpp`, `UpdateDriveWidgetsTouchesNoDiskEntryPoint`: the existing `Shell` rig; mount; the stand-in takes the store; `manager.UpdateDriveWidgets()` on the test thread. Today it asserts at `DiskManager.cpp:803`.
- Each FR-002 reader that replaces a menu, tooltip or scene query gets the same stand-in test in that decision's test file. The `EmulatorShell` queries are private (`EmulatorShell.h:1469`, `:1473`), so the test goes through the public reader FR-002 adds. The create-disk replace prompt (`WindowCommandManager.cpp:1281`) gets one through `DiskManager::IsReplacePromptNeeded`, and the salvage insert one through the static `EmulatorShell::InsertSalvagedCopy (DiskManager &, drive, path)`, called with the `DiskManager` rig's manager, so no shell is built (a shell built in a test has no `DiskManager`).
- Window code that runs only around a modal dialog or a window a unit test cannot create (`ShowSalvageOffer`, `ShowSalvageOutcome`, `AskAboutChange` after its modal, `OpenDisk2DebugDialog`, `OpenInputDebugDialog`) gets no stand-in test. Its proof is the regression that pins the data it now reads (the salvage offer's assessment, the prompt's `imagePath`, the posted attach command) plus the step of the Debug app run that drives it; tasks.md's Ownership move list gives each row's proof.
- `UnitTest/EmuTests/HeatHistoryTests.cpp`, `AReplayerReusedOnAnotherThreadTakesItsDisksWithIt`: a `Rebuild` on a `std::thread` (joined), then a `Rebuild` on the test thread, with no assertion. It fails without the `Run` bracket.
- `UnitTest/EmuTests/HistoryScratchPictureTests.cpp`, `ARendererReusedOnAnotherThreadTakesItsDisksWithIt`: the same check for the `Render` bracket.
- `UnitTest/DebuggerTests/CallStackHistoryTests.cpp` (035's), `ACallReplayerReusedOnAnotherThreadTakesItsDisksWithIt`: the same check for `ScratchCallReplayer::Run`'s bracket, through its `Rebuild`.
- The audit's other multi-thread tests must hand ownership over through the helper: note-external-change's blocking-clock tests, the SC-002 stress test, and the FR-019 deploy-flush tests. The mutating role has to hold the store, and off-owner reads go through the published status.

**Configurations.** The new tests are not `#ifdef`'d out, so they count in both Debug and Release (SC-004). Assertion-count and must-not-assert checks verify only in Debug, as `ExpectedEhmAssert` documents (`EhmTestHelper.h:36-40`). Handover tests using `IsHeldByCurrentThread` verify in both.

**Scenario suite.** No changes; it is single-threaded. Also run a Debug `Casso` (launched with `--title`) through startup, mount, eject, hover, the menus, a machine switch and quit. Any violation the unit suite cannot reach shows one dialog per entry point.

### Risks

- **Single-threaded tests cannot see UI-path violations.** In existing tests the test thread is both "UI" and owner, so the suite passing (SC-003) shows the check never misfires, not that the UI is clean. That is the job of the stand-in tests and the Debug app run. Each FR-002/FR-004 change must add its stand-in test.
- **Assertions off the test thread end the test run.** The test handler throws (`EhmTestHelper.cpp:47`). On a worker thread, or inside a `noexcept` function or a destructor, that throw becomes `std::terminate`. Mitigations:
  - drop `noexcept` from the two guarded controller methods;
  - send all off-thread work in tests through `RunOnOwner`, which brings exceptions back to the test thread;
  - never write a "must not assert" test whose only failure point is a destructor (for example `~EmulatorShell`).
- **ScenarioTests already installs the breakpoint handler.** `ScenarioTests/ScenarioTests.vcxproj:175-176` compiles `UnitTest/ModuleSetup.cpp` and `UnitTest/EhmTestHelper.cpp`, so its module initializer calls `SetupForUnitTests`, which installs `EhmBreakpointHandler` (`UnitTest/EhmTestHelper.cpp:86-89`), and a violation in a Debug scenario run fails the test rather than reaching `__debugbreak` (`Ehm.cpp:91-93`). An earlier draft of this record said otherwise. tasks.md T004 records the check; no file is added, because a second module initializer in that project would duplicate the first.
- **Order of landing on the branch.** Token, handover, scratch brackets, controller wiring and guards land together; the brackets are required in the same commit or `HeatHistoryTests` and `HistoryThumbnailCostTests` assert. Until the FR-002 readers move:
  - the unit suite stays green;
  - a Debug `Casso` asserts on its first frame, with one dialog per entry-point site thanks to the per-site check (`GuiMain.cpp:491-506`);
  - use Release for manual work meanwhile.
  - Nothing merges until every row of the off-thread table is gone ("no MVPs").
- **FR-019 after the master merge.**
  - The deploy flush runs on the UI thread so the rescue picker has an STA; the CPU thread is `COINIT_MULTITHREADED`, `CpuManager.cpp:536`. That makes it a violation by design unless the deploy hold hands ownership over.
  - The hold must call a CPU-thread hook that releases before signaling "held" and claims after the hold is released. The UI claims after `HoldForDeploy` returns and releases before `ReleaseDeployHold`.
  - A hold without this handover will assert. That is the check catching the FR-019 defect class.
  - The hold's names differ from those of 035's pause park (`CpuManager::IsParked`, defect b's fix), which a paused machine reaches while it still runs posted commands; the hold stops those too, and its request is part of 035's `WaitWhilePaused` wake predicate so a paused thread acts on it at once.
  - Behind live, the deploy first returns the machine to the live end (owner decision, 2026-10-08). That return runs on the CPU thread, which holds the disks, through a posted `IDM_DEPLOY_GO_LIVE` the UI waits on with a deadline, before the hold; it is not run by the UI while the CPU thread is held, because the return to live is CPU-thread code beyond what the disk token covers (plan.md section 8). So the ownership claim the hold hands over covers only the save.
- **The check detects; it does not prevent.** In Debug a violation reports and continues, and Release is unchanged. FR-001's data races are fixed by moving the callers, not by the assertion.
- **New public methods.** A method added without the macro and without a sweep-table row goes unchecked; C++ has no reflection to enumerate members. The class banner gets a rule line, and review has to apply it.
- **`NoteExternalChange` stays exempt and still reads `Entry`** under `m_pendingMutex`, while no writer takes that lock (`DiskImageStore.cpp:2692-2715`). The ownership check does not cover it, so FR-005's inbox change has to land.
- **`DiskImage` itself is unchecked.** A pointer taken legitimately on the owner and passed to another thread is not caught. Today only the controller (owner) and copies (`HeatHistory`, keyframes) hold them.
- **Destruction order.** `m_ownedDevices` (`MachineHost.h:441`) is destroyed after `m_diskStore` (`:464`), so a controller's ownership pointer dangles while devices are torn down. That is harmless while no destructor is guarded, so the check must never go into `~Disk2Controller`.
- **Ownership does not replace the lifetime lock.** UI layout still reads `GetRefs().diskController` as a pointer during a switch (`DiskManager.cpp:296-299`), which is FR-006's job. Other `DiskManager` state crossing threads is outside the token too: `m_coldBootMountWindow` is fixed by the move above, and `m_programmaticRemount` is written and read only on the owning thread.

## R6. Proving the races are gone (SC-002)

### Decision

1. **Add an opt-in AddressSanitizer (ASan) build of Debug x64.** It is turned on by a build property, `CassoSanitize=Address`, set in `Directory.Build.props`. It is not a new solution configuration. You build it with `scripts\Build.ps1 -Sanitize` and run it with `scripts\RunTests.ps1 -Build -Sanitize`. Its output goes to a separate tree, `x64\DebugAsan\`.
2. **SC-002 rests on three checks.** MSVC has no race detector, and no single check available here covers the whole claim:
   - **The SC-003 ownership assertion.** This stands in for a race detector. It is deterministic: it fires on the first call into the store from a thread other than the owner, whether or not two threads actually collide.
   - **A new drive-status stress test** that runs in all three builds (Debug, Release, DebugAsan). It compares every status it reads against the states the owner thread published.
   - **The ASan build.** When a race frees memory or resizes a string mid-copy, ASan turns that into a process abort that fails the run.
3. **Application Verifier and page heap are not gates. ThreadSanitizer does not exist for this toolchain.**
4. **Replace SC-002 with:**

> **SC-002**: The drive-status stress test passes in the Debug, Release and AddressSanitizer builds (`scripts/RunTests.ps1 -Build -Sanitize` for the last). It performs 10,000 mount, eject, swap and reload operations on a thread holding the disk store's ownership, while the test thread reads the published drive status throughout. It passes only when all of these hold:
> - every status read equals a state the owner published;
> - each of those states is read at least once;
> - the published status equals the live bay after every operation;
> - in the AddressSanitizer build, the run ends with no AddressSanitizer report.
>
> Run once against a status read from the live bays instead of the published copy, the same test fails in the AddressSanitizer build. The commit that adds the test records that result.

SC-003 could optionally add "and in the AddressSanitizer build" after "with the full suite passing".

### Rationale

#### What the tree has today
- **No sanitizer setup exists.**
  - `Casso.sln:27-32` and `UnitTest.vcxproj:3-20` define only Debug and Release, for x64 and ARM64.
  - No project and no props file sets `EnableASAN` or `/fsanitize`.
  - `scripts/RunTests.ps1:69-70` and `scripts/Build.ps1:57-58` accept only Debug and Release.
  - The audit says the same: "this repo builds with neither TSan nor ASan" (`audit.md:1632`).
- **The toolset supports ASan but not TSan.** VS 18.10 with MSVC 14.51.36231 ships `clang_rt.asan_dynamic-x86_64.{lib,dll}` (and aarch64 builds) plus `include\sanitizer\asan_interface.h`. That header provides `__asan_address_is_poisoned` (:97) and `__asan_report_present` (:123). There is no TSan runtime: `lib\x64` holds only asan, ubsan, fuzzer, profile, stats and builtins. `VC\Tools\Llvm` contains no `clang-cl.exe`, and LLVM's TSan does not support Windows targets anyway.
- **US2 already calls for a sanitizer.** Its Independent Test says "under the address sanitizer" (`spec.md:92-95`). SC-002 as written cannot be measured.

#### Measured
These runs used a `git archive` copy of `fb05ee2ab` in the scratchpad, on RELMER-DESKTOP at BelowNormal priority. Other sessions were running vstest at times, so the timings are only approximate. Nothing in the worktree changed.

**Build setup.** I added three settings to the copy's `Directory.Build.props`: `EnableASAN=true`, `LinkIncremental=false` and `BasicRuntimeChecks=Default`.
- `cl` then compiles with `/fsanitize=address /MDd` and no `/RTC`.
- `UnitTest.dll` imports `clang_rt.asan_dynamic-x86_64.dll`.
- MSBuild copied that DLL next to the test DLL, and vstest's own `testhost.exe`, which has no ASan, loaded it with no PATH change.
- A rebuild of the UnitTest chain took 1:19, against 1:18 for plain Debug.

| Probe | Plain Debug | DebugAsan |
|---|---|---|
| Planted use-after-free on a worker thread, read through an alias | n/a | `heap-use-after-free` with allocation and free stacks; testhost aborts; vstest reports "Test host process crashed" and "Test Run Aborted", exit 1 |
| The same read through the deleted variable itself | n/a | `/sdl` sets the deleted pointer to 0x8123, so ASan reports "access-violation on unknown address 0x8127" (still a crash, different text) |
| Data race, no free: two threads, a plain `int`, 2,000,000 increments | n/a | 613,363 lost updates, **no report**, test passes |
| Today's store: writer runs 5,000 `MountFromBytes`/`Eject` cycles; reader calls `GetSourcePath` + `GetImage()->GetWriteProtectInfo()` | **3/3 pass** (~180 ms, ~550k reads each) | **11/11 fail**: `container-overflow` / `unknown-crash` in the `std::string` copy on the reader thread, or `abort()` from the STL's hardened checks |
| `ASAN_OPTIONS=continue_on_error=2` on that probe | n/a | Summary printed, but the **test passes, exit 0**, and `__asan_report_present()` returns 0 |
| Whole suite | 9,525/9,525 pass, 4.86 min wall, 288 s summed | 9,529 run (9,525 + 4 probes), 9,528 pass, **0 ASan reports**, 15.1 min wall, 895 s summed (about 3.1×). The one failure is `CassoCli_DeclaresTheUtf8CodePage`, because the scratch copy did not build CassoCli.exe |
| Cost per operation (µs): NIB mount+eject / NIB swap / DSK mount+eject / blank WOZ mount+eject | 32 / 50 / 12,023 / not measured | 235-281 / 352-354 / 26,351-27,237 / 2,220 |
| `ThreadAllocationCounter` over 11 known allocations | n/a | **counts 0** |

A build of only the UnitTest target failed 586 tests with "Casso.exe not found next to the test DLL". The suite needs `Casso.exe` and `CassoCli.exe` built beside `UnitTest.dll`.

#### Why three checks
- **ASan catches use-after-free and container overflow, not races.** The plain-counter probe shows it misses a pure race. Against today's code it was the only thing that made the race fail reliably: 11/11 failures, where plain Debug passed 3/3. That matches the audit's warning that a plain stress test "would pass on broken code" (`audit.md:906`).
- **The ownership assertion is the race-freedom proof.** It does not depend on timing.
- **The stress test's checks catch logic errors no sanitizer reports.** These are a mixed status ("never a mixture", US2 acceptance scenario 2; FR-002 "consistent within itself") and a missed publish.
- **The instrument itself must be shown working.** Commit `6893681ee` records a clean ASan run that detected nothing, and `ContainerIntegrityTests.cpp:144-152` explains why: an overflow past a page-aligned 143,360-byte volume has no redzone. Hence the canary test and the recorded mutation run.
- **Continue-on-error mode must stay off.** It produced a passing test with a report present.

#### Run-time budget
- `docs/testing.md:289-290` says the Debug suite takes "under four" minutes (`copilot-instructions.md:591` says ~15, which is stale). Measured today: 4.86 min for 9,525 tests. The CI test job has a 30-minute limit (`ci.yml:35`).
- **Estimated stress-test cost**, from the per-operation numbers with one WOZ mount per 10 operations and no DSK: about 4-6 s in DebugAsan, about 1.5 s in Debug, under 0.5 s in Release. That is under 1% of the Debug suite.
- The full DebugAsan suite (15.1 min) runs once per merge, before merging, not on every iteration.

### Alternatives considered
- **A third solution configuration "Asan".** Rejected. Every vcxproj repeats four blocks per configuration, so this means duplicating them across 10 projects, plus `Casso.sln`, the scripts' accepted configuration lists and `-Configuration` consumers. The property switch gives the same separation for two lines per project.
- **The ASan flavor sharing `x64\Debug`.** Rejected. Every switch between flavors would be a full rebuild, and instrumented and plain objects could mix silently. RunTests' staleness guard only detects source newer than the binary (`copilot-instructions.md:563-571`).
- **Making ASan the default for Debug.** Rejected. It costs 3.1× suite time, drops the `/RTC1` checks that are kept deliberately (`Directory.Build.props:108-110`, `docs/testing.md:374`), and disables the allocation counter.
- **ThreadSanitizer.** Not available (see above).
- **Application Verifier or gflags page heap.** Rejected as a gate:
  - Both are configured per image name under HKLM Image File Execution Options and need elevation.
  - They apply to every `testhost.exe` on the machine, and other sessions' vstest runs were active during these measurements.
  - Neither can be reproduced on CI.
  - They put one page per allocation across 9,525 tests.
  - They catch the same use-after-free class as ASan.

  They remain a manual tool for Casso.exe only, with your go-ahead. `Build.ps1 -Sanitize` already produces an ASan build of Casso.exe for the manual checks the audit suggests (`audit.md:1149`, `:1248`).
- **The debug CRT's 0xDD fill or `_CRTDBG_DELAY_FREE_MEM_DF`.** Rejected: detection is stochastic (`audit.md:647`).
- **A plain stress test as the SC-002 instrument.** Rejected: it passed 3/3 on today's racy code.
- **SAL `_Guarded_by_` checked by `/analyze`.** Rejected for now: `<mutex>` in 14.51.36231 has no SAL lock annotations, so `std::lock_guard` is invisible to the checker.
- **Continue-on-error mode.** Rejected: measured exit 0 with a report present.
- **Deterministic interleaving seams only** (`audit.md:906`, `:1632`). These stay as the SC-001 regression tests for each defect. They cannot cover interleavings nobody predicted.

### Design detail

#### Build flavor
- **`Directory.Build.props`**, appended before `</Project>`:
  ```xml
  <PropertyGroup Condition="'$(CassoSanitize)' == 'Address'">
    <CassoBuildFlavor>Asan</CassoBuildFlavor>
    <EnableASAN>true</EnableASAN>
    <LinkIncremental>false</LinkIncremental>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(CassoSanitize)' == 'Address'">
    <ClCompile>
      <BasicRuntimeChecks>Default</BasicRuntimeChecks>
      <PreprocessorDefinitions>CASSO_SANITIZE_ADDRESS=1;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    </ClCompile>
  </ItemDefinitionGroup>
  ```
  `/RTC` is off in this flavor because Microsoft documents it as incompatible with `/fsanitize=address`; I did not test that separately.
- **All 10 vcxproj files** change `<OutDir>$(SolutionDir)$(Platform)\$(Configuration)$(CassoBuildFlavor)\</OutDir>` and `<IntDir>$(Platform)\$(Configuration)$(CassoBuildFlavor)\</IntDir>`:
  - `Casso`, `CassoCli`, `CassoCore`, `CassoEmuCore`, `Cassque`, `Dxui`, `Ehm` at lines 74-75
  - `MeshCreator` 51-52, `ScenarioTests` 88-89, `UnitTest` 75-76

  `x64/` is already ignored (`.gitignore:24`). This suffix change is a design proposal; the experiment built into `x64\Debug`.
- **`scripts/Build.ps1`**: add `[switch]$Sanitize`. Throw unless the build is Debug x64; there is no ARM64 device to run an ARM64 build. Append `-p:CassoSanitize=Address` to the single-configuration arguments (:292-313). Build the whole solution.
- **`scripts/RunTests.ps1`**:
  - Add `[switch]$Sanitize`, passed through to Build.ps1.
  - Point the assembly at `x64\DebugAsan\UnitTest.dll` (:210).
  - Require `clang_rt.asan_dynamic-x86_64.dll` beside it.
  - Print a "SANITIZER BUILD" banner.
  - Remove `ASAN_OPTIONS` for the vstest child and restore it afterward.
  - Tee vstest's output (:332), and fail when it contains `ERROR: AddressSanitizer`, even if the exit code is 0.
- **`UnitTest/Pch.h`**: `#ifdef __SANITIZE_ADDRESS__` / `#include <sanitizer/asan_interface.h>`. Angle-bracket includes are allowed only there (`copilot-instructions.md:29`).
- **`UnitTest/EmuTests/ThreadAllocationCounter.h:23-30`**: `IsAvailable()` returns false under `__SANITIZE_ADDRESS__`.
- **`UnitTest/EmuTests/RecordingBufferTests.cpp:41`**: change the guard to `#if defined(_DEBUG) && !defined(__SANITIZE_ADDRESS__)`. Under ASan this test passes vacuously.
- **`docs/testing.md`**: a new section on the sanitizer build covering what it catches, what it misses (races, overflow past page-aligned buffers), the continue-on-error hazard, its run time, and when to run it.

#### Ownership assertion (SC-003, the race-detector substitute)

**Superseded by R5.** The plan uses R5's `ThreadOwnership` (the constructing
thread holds the store, and handover is `Release` then `Claim`), not the
`ThreadOwner` below, which accepts every thread while unbound and leaves
scratch stores unbound. This subsection and the `ThreadOwnerTests` and
`DiskStoreOwnershipTests` entries in the test plan are kept as the record of
the alternative; R5's `ThreadOwnershipTests` and `DiskOwnershipTests` replace
them.

- **New `CassoEmuCore/Core/ThreadOwner.h/.cpp`**, following `DxuiThread.cpp:26-48` and `GamePortInputMixer.cpp:317-318`:
  ```cpp
  class ThreadOwner
  {
  public:
      void  Bind          ();
      void  Release       ();
      bool  IsCallerOwner () const;   // true when unbound or bound to the caller

  private:
      std::atomic<DWORD>  m_threadId { 0 };
  };
  ```
- **`DiskImageStore.h`**:
  - public: `void BindOwnerThread ();` and `void ReleaseOwnerThread ();`
  - private: `void AssertOnOwnerThread () const;` and `ThreadOwner m_owner;`

  `AssertOnOwnerThread` follows the EHM pattern: `bool isOwner = m_owner.IsCallerOwner(); CBRA (isOwner);` then `Error: return;`. In Release the breakpoint compiles out.
- **Which entry points assert.** It is called first in every public non-static entry point. `NoteExternalChange` (callable from any thread, `DiskImageStore.h:383-386`) is exempt; R5's list, which supersedes this one, adds `GetThreadOwnership`, the statics, constructors and destructors, and no published-status reader, because the publisher lives outside the store. Accessors defined inline in the header (:143-185, :242-379) move out of line, or call it inline.
- **Wiring.** Bind as the first statement of `EmulatorShell::OnCpuThreadStart` (`EmulatorShellCpuThread.cpp:200`), which runs on the CPU thread from `CpuManager.cpp:541-544`. Release as the last statement of `OnCpuThreadStop` (:319-326), after `StopReverseRecording`. That keeps the shutdown flush legal: it runs after the CPU thread stops (`EmulatorShell.cpp:294-299`). The store lives as long as `EmulatorShell::m_machine` (`EmulatorShell.h:1706`, `MachineHost.cpp:41`), so the binding holds across machine switches. Scratch stores (`ScratchHeatReplayer`) stay unbound.

#### Status the test reads
The test reads through whatever any-thread accessor the publish decision settles. The audit proposes `BayView DiskImageStore::GetBayView (int slot, int drive) const` (`audit.md:591-607`). I suggest adding a `uint64_t generation` field so the reader can also assert that statuses never go backward.

### Test plan
- **New `UnitTest/EmuTests/ThreadOwnerTests.cpp`**, class `ThreadOwnerTests` (superseded by R5's `ThreadOwnershipTests`):
  - `AnUnboundOwnerAcceptsEveryThread`
  - `ABoundOwnerAcceptsOnlyItsThread`
  - `ReleaseAcceptsEveryThreadAgain`
- **New `UnitTest/EmuTests/DiskStoreOwnershipTests.cpp`**, class `DiskStoreOwnershipTests` (superseded by R5's `DiskOwnershipTests`):
  - `EveryEntryPointOffTheOwnerThreadAsserts`: the owner is a helper thread parked on a latch. A table of lambdas covers every owner-only entry point, run under `ExpectedEhmAssert` with `RequireCount (tableSize)`. This verifies only in Debug (`EhmTestHelper.cpp:120`).
  - `NoteExternalChangeAndThePublishedStatusDoNotAssert`
  - `AnUnboundStoreAcceptsTheTestThread`: the premise every existing test relies on.
  - `TheOwnerThreadDoesNotAssert`
- **New `UnitTest/EmuTests/DriveStatusStressTests.cpp`**, class `DriveStatusStressTests`, test method `TenThousandDiskChangesPublishOnlyWholeStatuses`:
  - **Rig.** The store lives on the heap (as in `DiskFlushHoldTests.cpp:48`). The seams follow the rig in `SharedImageTests.cpp:78-274`, with a `FakeImageWatcher` and a `std::atomic<int64_t>` clock. Images:
    - A and A': NIB images at two long paths, longer than the small-string buffer.
    - B: a WOZ broken with `DamagedDisk::BreakSector`, so its published status reports damage.
    - C: mounted in drive (6,1) for the whole run.
  - **Owner thread.** Takes the store through R5's `OwnerThreadStandIn::Take` (not `BindOwnerThread()`, which R5 supersedes), then runs 1,000 cycles of 10 operations each:
    1. mount A into the empty drive
    2. reload A (bump its identity, `NoteExternalChange`, advance the clock by `MountedImageState::kQuietPeriodMs`, `ApplyPendingReload`)
    3. swap to A'
    4. eject
    5. mount A
    6. swap to B
    7. eject
    8. mount A'
    9. reload A'
    10. eject

    After each operation it checks that the published status equals the live bay (`IsMounted`, `GetSourcePath`, `GetImage()->GetWriteProtectInfo()`). Every 500 operations it waits at most 2 s for the reader's count to advance; a timeout is a failure, never an unbounded wait. Exceptions go to a `std::exception_ptr`. `EhmBreakpointHandler` throws through `Assert::Fail` (`EhmTestHelper.cpp:31-47`), so an exception left loose on a worker thread would abort the test process.
  - **Watcher thread.** Calls `NoteExternalChange` for A's path in a loop until stopped. This exercises the race in `audit.md:829-909`.
  - **Test thread (reader).** Reads both bays until the owner finishes, then classifies each status:
    - empty means not mounted, an empty path and a default `WriteProtectInfo`;
    - A or A' means not damaged;
    - B means damaged;
    - drive (6,1) is always C;
    - anything else fails the test.

    It yields between reads (yield-heavy tests slow badly under ASan: 3.3 s plain against 21.0 s DebugAsan for `TwoThreadStressNoDropsNoReorder`).
  - **After join.** Rethrow any captured exception, then assert:
    - exactly 10,000 operations ran;
    - at least 1,000 reads were taken;
    - each status class was read at least once;
    - the watcher sent at least one note;
    - the final status is empty for (6,0) and C for (6,1);
    - after ejecting C, the fake watcher holds no watches (FR-011).

    Log the elapsed time.
- **New `UnitTest/EmuTests/SanitizerCanaryTests.cpp`**, test method `TheSanitizerBuildPoisonsFreedMemory`:
  - A `static_assert` checks that `CASSO_SANITIZE_ADDRESS` and `__SANITIZE_ADDRESS__` are defined together or not at all.
  - Under ASan: free a 64-byte block, keep an alias, and assert `__asan_address_is_poisoned (alias) == 1` while a live block is not poisoned.
- The canary passes in every build: outside the sanitizer build its body is the `static_assert` alone. Nothing committed fails under `-Sanitize`, because an ASan report aborts the whole test process.
- **Mutation checks, made locally, run once, reverted, and recorded in the commit message (never committed):**
  1. The reader calls `IsMounted (6, 0)` once: Debug fails with the ownership assertion.
  2. The stress test's reader reads the live `Entry` instead of the published status: DebugAsan fails. The scratch probe of today's code is the measured analog (11/11 failures).
  3. Eject's publish is removed: the owner's published-equals-live check fails in every build.
- **When to run.**
  - Edit loop: `RunTests.ps1 -Build -Filter DriveStatusStress`, plus the same with `-Sanitize`.
  - Before merging: full Debug and full Release (SC-004), plus one full `-Sanitize` run.
  - Counts: the DebugAsan count equals the Debug count minus the one allocation test.

### Risks
- **ASan misses data races.** On its own, SC-002 does not prove race freedom; SC-003 and review of the publish lock cover that part.
- **Load can cause flaky failures.** The bounded handshake and minimum-count assertions only, with no timing assertions, make it robust; CI runners have fewer cores.
- **An ASan report aborts the test process,** so the rest of that run is lost (exit 1). That is right for a gate; rerun after fixing.
- **`/sdl` changes the report text** for a use-after-free through the deleted variable itself, so no check may match on report text.
- **The full ASan suite takes 15.1 min, against 4.86 min plain.** It runs only before merging. Adding a CI job is your decision and is not part of 041.
- **The ownership binding is not armed until `OnCpuThreadStart`** (`CpuManager.cpp:534-544`). Before that the store is unbound; the window is small. (R5's construct-claims token, which the plan uses, has no unbound window: the window thread holds the store until it releases it just before the CPU thread starts.)
- **Unfixed UI-thread callers will assert in a Debug app** once the binding is armed. Arm it after the publish work lands, or use it deliberately to find the remaining callers. Each 035 merge can bring in new callers that also assert, which is what the check is for.
- **Merge conflicts.** The 10 vcxproj edits can conflict with 035's `UnitTest.vcxproj` additions; the conflicts would be in `ClCompile` lists, not the `OutDir` lines.

The scratch copy with the probe code and logs is at `C:\Users\relmer\AppData\Local\Temp\claude\C--Users-relmer-source-repos-relmer-Casso\76c7343d-9eb8-42be-850a-498f31e14796\scratchpad\asan-src`. It takes 5.1 GB and is safe to delete; the probe source is `UnitTest\EmuTests\AsanProbeTests.cpp`. The worktree is unchanged.

