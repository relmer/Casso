# Contract: internal interfaces

The interfaces other code depends on. Names and parameters here are binding;
bodies, private helpers and file placement inside the listed files are not.
Paths are under `CassoEmuCore/`. The code below is design: when it is written,
every EHM macro condition is a hoisted local, never a call (CS0011).

## Ownership (`Core/ThreadOwnership.h`)

```cpp
class ThreadOwnership
{
public:
                       ThreadOwnership       ();                                    // held by the constructing thread
                       ThreadOwnership       (const ThreadOwnership &) = delete;
    ThreadOwnership &  operator=             (const ThreadOwnership &) = delete;

    void               Claim                 ();          // unowned: this thread takes it; held by this thread: no change
    void               Release               ();          // held by this thread: unowned afterward; unowned: no change
    bool               IsHeldByCurrentThread () const;
};

// Debug: an EHM ASSERT at the call site, so each entry point reports as its
// own site. Release: nothing.
#define ASSERT_THREAD_OWNERSHIP(ownership)   ...
```

- `Claim` asserts (`CBRA`) only when another thread holds the token, and
  `Release` only when a thread other than the caller holds it; the holder is
  unchanged either way. A claim by the holder changes nothing, because
  `~EmulatorShell` claims after `Stop` whether or not the CPU thread ever ran,
  and many tests build a shell whose CPU thread never starts.
- `ASSERT_THREAD_OWNERSHIP` is the first statement after the function's
  declaration block (after the three blank lines CS0016 requires); in an
  inline one-liner it is the first statement of the body. Its argument is a
  member (`m_ownership`), a dereference (`*m_ownership`) or a local reference
  hoisted among the declarations, never a call. It never goes in a destructor,
  and a function that gains it loses any `noexcept`, because the unit-test
  handler reports by throwing.
- `DiskImageStore::GetThreadOwnership()` returns the store's token, as
  `ThreadOwnership &` and, on a const store, as `const ThreadOwnership &`.
  Code that checks ownership through a reference parameter hoists the token
  first: `const ThreadOwnership & ownership = store.GetThreadOwnership();`
  among the declarations, then `ASSERT_THREAD_OWNERSHIP (ownership);`.
- `Disk2Controller::SetThreadOwnership (const ThreadOwnership &)` points a
  controller at its machine's store token; a controller with none uses its
  own. In place of `ASSERT_THREAD_OWNERSHIP` it checks that both the current
  token and the new one are held by the calling thread, and it has a row in
  `EveryControllerEntryPointAssertsOffItsOwner` like any guarded member.
- `MachineHost::ClaimDiskOwnership()` and `ReleaseDiskOwnership()` forward to
  the store's token, for the shell and the scratch machines.

## Published drive status (`Devices/Disk/DriveStatus.h`, `Shell/DriveStatusPublisher.h`)

```cpp
struct BayStatus
{
    bool              isMounted        = false;
    std::string       path;
    DiskFormat        format           = DiskFormat::Dsk;
    WriteProtectInfo  writeProtect;
    bool              isSalvageOffered = false;
    WozRequirements   wozRequirements;          // after the master merge (tasks.md T136)

    bool  operator== (const BayStatus &) const = default;
};

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

    uint64_t               GetSequence       () const;
    uint64_t               GetBayGeneration  () const;
    bool                   HasController     () const;
    const BayStatus      & GetBay            (int slot, int drive) const;   // an empty BayStatus for a bad bay
    const DriveActivity  & GetActivity       (int drive) const;             // slot 6
    std::vector<DiskImageStore::MountedSource>  GetMountedSources () const;   // isMounted && !path.empty()
};

enum class DriveStatusChange
{
    None,
    Activity,
    Bays,
};

class DriveStatusPublisher
{
public:
    DriveStatusChange                   Publish (const DiskImageStore  & store,
                                                 const Disk2Controller * controller,
                                                 bool                    isActivityDue);
    std::shared_ptr<const DriveStatus>  Take    () const;                   // never null
};
```

- `Publish` runs on the store's owning thread. It returns `Bays` when the
  store's status generation moved, `Activity` when only activity changed and
  activity was due, and `None` otherwise. It takes only the publisher's own
  leaf mutex, to swap the pointer. The publisher holds no event.
- A Debug publish with an unmoved generation rebuilds the table and `ASSERT`s
  it is unchanged, so a missed generation bump fails the suite.
- `DriveStatusChange DiskManager::PublishDriveStatus (bool isActivityDue)`
  (owning thread) returns the publish's result and calls the wake installed
  with `void DiskManager::SetStatusWake (std::function<void ()> wake)` exactly
  when the result is `Bays`; the shell installs a wake that sets
  `m_frameReadyEvent`, and a test installs one that counts, so the paused
  path's publish and wake are both asserted through the `DiskManager` rig
  (tasks.md T030), with no kernel object in the test. `DiskManager` also has
  `TakeLatestDriveStatus()` (any thread) and `GetShownDriveStatus()` (window
  thread: the copy taken once per window frame). `EmulatorShell` adds `GetShownDriveStatus()`
  and `void ServiceCpuThread ()`, the service function, which calls
  `ServiceDebugger`, `ServiceHistoryThumbnails` and then
  `PublishDriveStatus (isPaused)` on every pass (P1), a fixed sequence the
  Debug app run confirms on screen (T051).
- The window-thread readers and their replacements are R1's table, with one
  addition: the create-disk replace prompt calls
  `DiskManager::IsReplacePromptNeeded` (Drive widgets, below).
- `wozRequirements` arrives with the `master` merge (tasks.md T136), which
  brings `WozCompatibility` and the WOZ info icon: `GetBayStatus` fills it
  with `WozCompatibility::ReadRequirements (image->GetWozMetadata())` on the
  owning thread, so the window's `UpdateDriveWidgets` takes the requirements
  from the shown bay instead of reading the image every frame. The conflict
  test and the machine facts (`WozCompatibility::GetMachineFacts` and the
  machine name) stay machine configuration, read as `master` reads them,
  under the machine lifetime lock. `WozRequirements` has a defaulted
  `operator==`, so `BayStatus`'s stays defaulted.
- `MountCompletion` gains `BayStatus bay`, copied with `GetBayStatus` on the
  owning thread just before the completion is posted, so the damage report
  describes the medium that mount produced.

## Store operations (`Devices/Disk/DiskImageStore.h`)

```cpp
// Published status, owning thread
BayStatus  GetBayStatus            (int slot, int drive) const;
uint64_t   GetStatusGeneration     () const;
void       MarkStatusChanged       ();

// Write protection
void       SetUserWriteProtectFlag (int slot, int drive, bool isProtected);                       // flag and generation only; never saves
void       SetFileWriteProtect     (int slot, int drive, bool isReadOnly, bool hasNoPermission);  // flag and generation only
HRESULT    SetUserWriteProtect     (int slot, int drive, bool isProtected);                       // FR-015: saves first when protecting

// External changes
void       NoteExternalChange      (const std::string & path, ExternalChangeIntent intent);       // any thread; inbox only

// Saving
bool       AreAllWritesSaved       () const;                  // every bay clean, or its recovery copy current
void       SetFileBacked           (bool isFileBacked);       // false: scratch stores write nothing

// Salvage
HRESULT    SalvageToFile           (int slot, int drive, uint64_t mediaId,
                                    const std::string & path, DenibblizeReport & report);
```

- `SetUserWriteProtect` saves at `FlushMoment::Running` before the flag turns
  on. When that save fails, the disk stays writable with its writes, the save
  reports the failure, and the store calls `NotifyMediaChanged`, so the
  boundary keyframe at that position records the unprotected drive. (The
  `DriveWriteProtect` record journaled before dispatch holds the requested
  value; in a replay the boundary keyframe wins, `Replayer.cpp:175-179`.)
  Turning the flag off never saves. Only `EmulatorShell::SetDriveUserWriteProtect`,
  the `IDM_DISK_WRITEPROTECT1/2` handler, calls it; `Replayer::ApplyInput` and
  `DiskManager::ApplyExternalWriteProtect` call `SetUserWriteProtectFlag`, so
  a replay never saves.
- `NoteExternalChange` records the intent and time under `m_pendingMutex` in an
  inbox keyed by path, keeping only the latest per path, and reads no `Entry`.
  `ApplyPendingReload` drains the inbox on the owning thread after its
  `m_isReplaying` return and matches paths against `m_entries` only.
- `SalvageAssessment` gains `uint64_t mediaId` and `std::string sourcePath`.
  `SalvageToFile` writes nothing and fails with
  `HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED)` when the bay's media id is not
  `mediaId`.
- Every other public instance member may run only on the owning thread, and is
  checked: R5's list, the two 035 adds for its defect e fix
  (`IsRetainingMedia`, `ReportSeatedMedia`, which tasks.md T008 brings), and
  every member this contract adds to the store (`GetBayStatus`,
  `GetStatusGeneration`, `MarkStatusChanged`, `SetUserWriteProtectFlag`,
  `SetFileWriteProtect`, `SetUserWriteProtect`, `AreAllWritesSaved`,
  `SetFileBacked`) and the `const` overload of `Disk2Controller::GetEngine`.
  Each has a row in `DiskOwnershipTests`' sweep, added by the task that adds
  the member (T013 for the two 035 members). `NoteExternalChange`,
  `GetThreadOwnership`, the statics, constructors and destructors are exempt.

## Mount outcome (`Devices/Disk/MountDiagnosis.h`)

- `MountFailure` gains `UnsavedWrites` (the change was declined because the
  disk in the drive has writes in no file) and `BehindLive` (the insert
  reached the emulation thread while the machine was behind live).
- `MountDiagnosis` gains `keptPath`, `saveError`, `isSameFile`, `wasReported`
  and `bool ShouldReport () const` (false only for a same-file decline already
  reported), as R2 gives them. `wasReported` is read from
  `MountedImageState::IsSaveFailureReported()`, which is true when either of
  FR-016's report fields is set (the recovery copy path last reported, or the
  plain-failure flag), so a same-file decline right after its own save
  reported a first recovery copy is silent; the store keeps no other report
  state.

## Disk image (`Devices/Disk/DiskImage.h`)

```cpp
void                       MarkSaved           (const vector<Byte> & savedBytes);   // the file now holds these bytes; clears dirty
const vector<uint64_t> &   GetTrackGenerations () const;
bool                       TryCreateTrack      (int quarterTrack, size_t bitCount);
```

## File I/O seam additions (`Devices/Disk/IDiskFileIo.h`)

```cpp
virtual HRESULT      FlushToStorage         (const std::string & path) = 0;
virtual HRESULT      CopyFileMetadata       (const std::string & fromPath, const std::string & toPath) = 0;
virtual HRESULT      RenameWithoutReplacing (const std::string & tempPath, const std::string & targetPath) = 0;
virtual std::string  GetWorkingDirectory    () const = 0;
```

- The first three, with `DurableCommit` below, are built by spec 040 to this
  contract in one self-contained commit (GH #170) with their `Win32DiskFileIo`
  and fake implementations, fake-based unit tests and real-file scenario
  cases; tasks.md T054 cherry-picks it with `-x`, and builds the same thing
  to this contract only when the commit is not available.
- `GetWorkingDirectory` is this branch's own addition (tasks.md T073), not
  part of 040's commit: `Win32DiskFileIo` returns
  `PathResolver::GetWorkingDirectory()`, and every test implementation
  returns a synthetic base (`"C:\\work"` in `FakeDiskFileIo` unless a test
  sets another). It is the base the CLI senders resolve a relative path
  against (Paths, below).

## Durable commit (`Devices/Disk/DurableCommit.h`, `Devices/Disk/CommitPlan.h`)

```cpp
enum class CommitMode
{
    Replace,        // replace the target when it exists
    CreateNew,      // fail with ERROR_ALREADY_EXISTS rather than overwrite anything
};

class DurableCommit
{
public:
    static HRESULT  Commit (IDiskFileIo              & fileIo,
                            const std::string        & targetPath,
                            const std::vector<Byte>  & bytes,
                            uint64_t                   invocationTag,
                            CommitMode                 mode,
                            CommitPlan::Progress     & progress);
};
```

- Order: write the temporary, copy metadata from the target when it exists and
  the mode is `Replace`, flush the temporary to storage, then replace (or
  rename without replacing). `progress.furthestAttempted` records the step
  reached, so `DiskImageSession` chooses between its temporary-write and
  replace messages. The metadata copy is best-effort (owner decision,
  2026-10-09): when `CopyFileMetadata` fails, the commit goes ahead with the
  flush and the replace, nothing is shown, and the new version may lose
  hidden or system flags or custom permissions. This holds for every caller,
  the emulator's store saves and the command line alike. On a failure of the
  write, the flush or the replace, the temporary is removed and the target is
  untouched.
- `CommitPlan::Step` gains `CopyMetadata` and `FlushTemporary` between
  `WriteTemporary` and `Replace`, and `ShouldRemoveTemporary` treats all four
  as steps after which a temporary may exist.
- `CommitMode` is a free type in `DurableCommit.h`, because every caller
  passes it in.

## Commands (`resource.h`, `Shell/CpuCommandDispatcher.h`)

| Id | Posted by | Payload | Runs on the emulation thread |
|---|---|---|---|
| `IDM_DISK_WRITEPROTECT1/2` (existing, unchanged id) | Settings apply | `"0"` or `"1"` | `SetDriveUserWriteProtect`, which calls `DiskImageStore::SetUserWriteProtect (6, drive, wp)`; journaled as `DriveWriteProtect` and state-changing, as today |
| `IDM_DISK_SALVAGE1/2` (existing ids, now posted) | Disk menu; the damage report's salvage button | none | `AssessSalvage`; when salvage is offered, posts `WM_APP_SALVAGE_OFFER` |
| `IDM_DISK_SALVAGE_WRITE` (new) | the salvage dialog | `"<drive> <mediaId> <path>"`, path last so it may hold spaces | `SalvageToFile`, then posts `WM_APP_SALVAGE_DONE` |
| `IDM_DEBUG_ATTACH_SINKS` (new) | opening a debug panel | none | points the open panels' sinks and cycle counters at the machine's devices |
| `IDM_DEPLOY_GO_LIVE` (new) | the update installer, behind live | the request's serial | `GoLiveForDeploy`: nothing once the serial is no longer awaited; otherwise `RunReverseCommand (ReverseCommand::GoLive, 0)` when behind live, then `DeploySave::CompleteLiveReturn` (The update installer's save, below) |

- The salvage ids, `IDM_DEBUG_ATTACH_SINKS` and `IDM_DEPLOY_GO_LIVE` are
  neither journaled (`CpuCommandDispatcher::TryGetJournalInput`) nor
  state-changing (`DivergenceGate::IsStateChangingCommand`), so the window's
  divergence gate passes them behind live.
- The salvaged copy goes into the drive through the window's ordinary
  `IDM_DISK_INSERTn` post: `ShowSalvageOutcome` calls
  `static HRESULT EmulatorShell::InsertSalvagedCopy (DiskManager & disks, int drive, const std::string & path)`,
  which calls `DiskManager::Mount`, so the insert is journaled and
  divergence-gated like any other, and a test calls the helper with a
  `DiskManager` rig and no shell.
- `ICpuCommandTarget` gains `AssessSalvage (int drive)`,
  `WriteSalvagedCopy (int drive, uint64_t mediaId, const std::string & path)`,
  `AttachDebugSinks ()` and `GoLiveForDeploy (uint64_t serial)`.
- `CpuCommandDispatcher::Dispatch` is split first (tasks.md T017): the disk
  commands go through `DispatchDiskCommand` and the debugger commands,
  `IDM_DEBUG_ATTACH_SINKS` and `IDM_DEPLOY_GO_LIVE` among them, through
  `DispatchDebugCommand`.

## Window messages (`Shell/EmulatorShellInternal.h`)

| Message | Payload | Window-thread handler |
|---|---|---|
| `WM_APP_SALVAGE_OFFER` (new) | `SalvageOffer { drive, assessment }` | `ShowSalvageOffer` |
| `WM_APP_SALVAGE_DONE` (new) | `SalvageOutcome { drive, hr, report, path }` | `ShowSalvageOutcome`: the result, the failure dialog, or the Insert / Not now question |
| `WM_APP_MOUNT_COMPLETED` (existing) | `MountCompletion`, now with the drive's `BayStatus` | `HandleMountCompletion` |
| `WM_APP_REPORT_DAMAGE` | removed | |

## Commands behind live

The entry points that only the disk commands reach change nothing when the
store's `IsFlushHeld()` or `IsReplaying()` is true: a command that reached the
emulation thread behind live slipped past the window's divergence gate, whose
check runs on the posting thread.

```cpp
HRESULT  DiskManager::MountDiskForCommand (int drive, const std::string & path);   // the IDM_DISK_INSERTn mount
```

- The insert: `EmulatorShell::MountDisk`, the `IDM_DISK_INSERTn` override,
  calls `MountDiskForCommand`, which declines behind live and otherwise calls
  `MountDiskInSlot6`. The check is not in `MountDiskInSlot6`, because
  `MountCommandLineDisks` and `RemountSlot6Disks` also call it, and a machine
  switch's remount runs after recording stops. A declined insert reports
  `MountFailure::BehindLive` through the mount completion, after
  `RestoreDoor` and the P3 publish, as `MountDiskInSlot6` reports a failure.
- The eject and the Disk menu's toggle: `DiskManager::EjectDiskInSlot6` and
  `ToggleImageWriteProtect`, whose only callers are their command overrides
  (`Shell/EmulatorShellCpuThread.cpp`). A declined eject calls `RestoreDoor`
  and raises a notice; a declined toggle raises a notice.
- Settings' write protection: `EmulatorShell::SetDriveUserWriteProtect`, the
  `IDM_DISK_WRITEPROTECT1/2` handler, raises a notice. A command whose value
  equals the drive's current setting (`m_userWriteProtect[drive]`) changes
  nothing and raises no notice, behind live or not, because Settings sends the
  command for both drives on every apply, changed or not
  (`Ui/Settings/SettingsPanelState.cpp:1054-1057`). The handler stores that
  setting only when the drive is empty or the store's `SetUserWriteProtect`
  succeeds, so after a protect FR-015 declined the setting is still
  unprotected, the next apply that sends `"1"` runs the save again, and the
  next mount applies no protection the save never got.
- The store's own explicit flushes keep writing while held.

## Paths (`Core/PathResolver.h`)

```cpp
static std::string  MakeAbsolutePath (const std::string & path, const std::string & baseDirectory);
```

Empty input returns empty; an absolute input comes back normalized; anything
else is resolved against `baseDirectory` and normalized: a relative path
against the directory, a rooted path (`\a.dsk`) against the base's drive
(`C:\a.dsk`), and
a drive-relative path on the base's own drive (`C:a.dsk` with base `C:\work`)
against the directory. A drive-relative path on another drive (`D:a.dsk` with
base `C:\work`) comes back unchanged, because resolving it needs that drive's
working directory, which is process state the function does not read; it is
then handled as it is today. Where the base comes from:

- The emulator: `DiskManager`'s constructor takes it, and
  `EmulatorShell::InitAssetPathsAndStores` (`Shell/EmulatorShell.cpp:678-704`)
  passes `PathResolver::GetWorkingDirectory()` where it creates
  `m_diskManager`; no unit test reaches that function, and the tests'
  `DiskManager` rig passes `"C:\\work"`.
- The CLI senders: `DiskCommandRunner::AnnounceIntent` and
  `ImageArtifactSink` resolve against `m_fileIo.GetWorkingDirectory()`, so
  the one real read is `Win32DiskFileIo::GetWorkingDirectory`, behind a seam
  every unit test replaces. `DiskCommand::Run`, `AssemblerMode::Run` and
  Cassque's `DiskOperations::RunCommand`, which build the runner or the sink,
  pass the `IDiskFileIo` they already pass; `DiskOperations`' tests pass
  `FakeDiskFileIo`.

## Layout facts (`Shell/MachineChromeFacts.h`)

```cpp
struct MachineChromeFacts
{
    bool          hasDiskController      = false;
    bool          hasCaseSwitches        = false;
    bool          hasBuiltInDrive        = false;
    bool          hasBankedSystemRom     = false;
    bool          isSlotless             = false;
    int           attachedDiskIiDrives   = 0;
    bool          externalDriveConnected = false;
    std::wstring  displayName;

    static MachineChromeFacts  Capture    (const MachineConfig & config, const MachineRefs & refs, bool externalDriveConnected);
    static bool                TryCapture (MachineHost & host, const bool & externalDriveConnected, MachineChromeFacts & facts);

    bool  ShouldShowExternalDrive () const;
    int   GetDeskSceneDriveCount  () const;
};
```

`TryCapture` takes the machine lifetime lock shared with `try_to_lock` and
leaves `facts` unchanged when it does not get it.

## Drive widgets (`Ui/DriveWidgetController.h`, `Ui/DriveWidgetState.h`, `Shell/DiskManager.h`)

- `SyncAction::DoorRestore`: a declined insert or eject puts the door back.
  `void DiskManager::RestoreDoor (int drive)` (owning thread) publishes it
  when the bay is still mounted.
- `DriveWidgetState::isEjectPosted` (window thread): set by `DiskManager::Eject`
  when it posts the eject, cleared when the door sync handles the bay's path
  change and when a `DoorRestore` arrives, so a declined eject leaves no flag
  behind.
- `bool DiskManager::IsReplacePromptNeeded (int drive) const` (window thread):
  the shown bay is mounted and no eject is posted for it.

## User preferences (`Config/UserConfigStore.h`)

```cpp
HRESULT  UpdateUiPrefs (const std::string & machineName, const JsonValue & defaultJson,
                        const std::vector<std::pair<std::string, JsonValue>> & values,
                        IFileSystem & fs) const;
```

One hold of the store's lock covers the load, the splice and the save, so two
threads updating different keys of one machine both land.

## Deploy hold (`Shell/CpuManager.h`)

```cpp
HRESULT  HoldForDeploy         (DWORD timeoutMs);   // returns once the emulation thread is held between passes
void     ReleaseDeployHold     ();
bool     IsHeldForDeploy       () const noexcept;
bool     IsDeployHoldRequested () const noexcept;   // a hold is requested and not yet withdrawn or released
```

- It is built on 035's `CpuManager` after tasks.md T008, which brings 035's
  pause acknowledgement for defect b (`IsParked`, `TryWaitUntilParked`,
  `TryPark`, `ParkForExit`, `WaitWhilePaused`). The names differ because
  the meanings do. 035's state is weaker: a paused thread still drains posted
  commands and runs the service function every `kServiceIntervalMs` (20 ms),
  it never moves disk ownership, and `IsParked` is true with no CPU thread at
  all (`m_isParked` starts true). The deploy hold stops the command drain and
  the service pass too, and moves disk ownership (below). No 041 code uses
  035's names for the deploy hold.
- The hold check sits between `WaitWhilePaused` and `DrainCommandQueue`, so
  commands posted before the hold stay queued while it is held, on a paused
  machine as on a running one.
- `HoldForDeploy` sets the request under `m_pauseMutex` and notifies
  `m_pauseCV`; the request is part of `WaitWhilePaused`'s wake predicate, so a
  paused thread acts on it at once rather than at its next service interval,
  and acts on it with no service function at all.
- While held, the emulation thread runs no frame, no command drain and no
  service pass. A hold that times out is withdrawn under the same mutex.
  `Stop` ends a hold and joins the thread.
- Ownership moves with the hold: the emulation thread releases the disk store
  before it signals held and claims it again after the hold is released; the
  window thread claims after `HoldForDeploy` returns and releases before
  `ReleaseDeployHold`.

## The update installer's save (`Shell/DeploySave.h`)

An update installed while the machine is behind live saves the disks at the
live end (owner decision, 2026-10-08), on every install path (owner confirmed
2026-10-09): the deploy `HandleUpdateApplyResult` runs for a bundle ready to
deploy, an MSIX update installed now (also from `ApplyPendingUpdateNow`),
which returns to live and then holds the thread and saves; a zip update
installed now, which returns to live just before it posts `WM_CLOSE` and
then saves through quit; and an update left until Casso closes, which returns
to live in `OnDestroy` just before the emulation thread stops and then saves
through the exit flush. A quit with no update pending is unchanged: it stops
recording first and saves the disks where the machine stands.

```cpp
enum class LiveReturnOutcome
{
    NotBehindLive,      // the machine was live; nothing ran
    ReachedLiveEnd,
    HistoryCut,         // the replay diverged: live at the last good keyframe, the recorded future dropped
    NotReached,         // still behind live: a gap, a failed Execute, or a later command moved it back
    TimedOut,           // no outcome for this request before the deadline
};

struct DeployFlushResult
{
    bool  wasBehindLive = false;    // the hold or the replay flag was set once held
    bool  wasReplaying  = false;    // the replay flag's value before the save cleared it
};

class DeploySave
{
public:
    LiveReturnOutcome  RequestLiveReturn  (CpuManager & cpu, DWORD timeoutMs);           // window thread
    bool               IsAwaited          (uint64_t serial) const;                          // emulation thread, first
    void               CompleteLiveReturn (uint64_t serial, LiveReturnOutcome outcome);    // emulation thread
    LiveReturnOutcome  GetLastOutcome     () const;                                         // window thread, once held

    static HRESULT            Flush               (CpuManager & cpu, DiskImageStore & store, DWORD timeoutMs, DeployFlushResult & result);
    static void               Resume              (CpuManager & cpu, DiskImageStore & store, const DeployFlushResult & result);
    static LiveReturnOutcome  ToLiveReturnOutcome (const std::optional<ReverseOutcome> & outcome, bool isBehindLive);
    static bool               IsSavedBehindLive   (LiveReturnOutcome outcome, const DeployFlushResult & result);
};
```

- `RequestLiveReturn` posts `IDM_DEPLOY_GO_LIVE` with a new serial and waits
  on a condition variable, without pumping messages, until that serial's
  outcome arrives or the deadline passes. At the deadline it stops awaiting
  the serial and returns `TimedOut`.
- `IsAwaited` is false once the request for that serial has stopped waiting,
  so a `GoLiveForDeploy` that starts after the deadline does nothing, and a
  failed deploy that releases the hold is not followed by a return to live.
- `CompleteLiveReturn` records the outcome for its serial under the same
  mutex, whether or not a request still waits on it, and notifies.
  `GetLastOutcome` gives the newest request's outcome, a late one included, or
  `TimedOut` while none has arrived. Read once held, it gives every outcome
  that can still arrive, because the hold lands only between passes, after a
  return to live already running has finished.
- `EmulatorShell` holds the `DeploySave` instance, which both threads reach
  only through these members.
- The emulation thread's `GoLiveForDeploy` does nothing when `IsAwaited` is
  false. Otherwise, when `m_reverseHost != nullptr && m_reverseHost->IsBehindLive()`
  (`m_reverseHost` exists only once recording has started,
  `EmulatorShellReverse.cpp:53-55`), it clears
  `m_lastReverseOutcome`, runs the call `ApplyReverseOptions` makes,
  `RunReverseCommand (ReverseCommand::GoLive, 0)`, and reports
  `ToLiveReturnOutcome (m_lastReverseOutcome, m_reverseHost->IsBehindLive())`;
  when that condition is false, a shell that never recorded included, it
  reports `NotBehindLive`.
- `ToLiveReturnOutcome`: no outcome (a failed `Execute` leaves
  `m_lastReverseOutcome` empty, since `RunReverseCommand` sets it only after a
  successful one) gives `NotReached`; `HistoryCut` gives `HistoryCut`;
  `Moved` with the machine live gives `ReachedLiveEnd`; anything else gives
  `NotReached`.
- `Flush` calls `cpu.HoldForDeploy (timeoutMs)`, claims the store's token on
  the calling thread, hoists `isBehindLive` (`IsFlushHeld()` or
  `IsReplaying()`) into `result.wasBehindLive`, records `IsReplaying()` in
  `result.wasReplaying`, clears the replay flag when it is set, so
  `FlushEntry`'s replay check does not skip the save, and runs
  `FlushAllForShutdown`, which writes held writes as quit's does. It returns
  with the machine held and the token held. When `HoldForDeploy` fails it
  claims nothing and returns the failure.
- `Resume`, for a deploy that did not go ahead: when the calling thread holds
  the store's token (`Flush` held the thread and claimed), it sets the replay
  flag back to `result.wasReplaying` and releases the token; then it calls
  `ReleaseDeployHold`. After a failed `HoldForDeploy` it touches no store
  state.
- `IsSavedBehindLive` is true when `result.wasBehindLive` is true or the
  outcome is `HistoryCut`, and false otherwise, so a `NotBehindLive` answer
  (a queued Go live had already run) and a `TimedOut` return that finished
  before the hold, both of which leave the disks saved at the live end, give
  no notice.
- The callers, after the `master` merge (tasks.md T139), share one helper,
  `ReturnToLiveForUpdate`: `StopReplay`, then, when `IsBehindLiveForUi()`,
  `RequestLiveReturn` with a 5 s deadline, giving the outcome or
  `NotBehindLive`. The zip update installed now calls it before its
  `WM_CLOSE` post and raises section 13's notice there when
  `IsSavedBehindLive (outcome, { IsBehindLiveForUi(), false })` is true;
  `OnDestroy` calls it before `m_cpuManager.Stop()` when an update is
  pending and raises no notice, because the window is being destroyed. The
  MSIX deploy, in a helper `SaveDisksForDeploy`, calls it and then `Flush`
  with a 5 s deadline for the hold. When `IsSavedBehindLive (outcome, result)` is true,
  with `outcome` from `GetLastOutcome()` when a request was made and
  `NotBehindLive` otherwise, it raises contracts/user-messages.md section 13's
  notice before the deploy starts. On every failure branch, a hold that
  missed its deadline included, it calls `Resume`.
