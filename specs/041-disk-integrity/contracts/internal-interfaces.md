# Contract: internal interfaces

The interfaces other code depends on. Names and parameters here are binding;
bodies, private helpers and file placement inside the listed files are not.
Paths are under `CassoEmuCore/`.

## Ownership (`Core/ThreadOwnership.h`)

```cpp
class ThreadOwnership
{
public:
    ThreadOwnership();                       // owned by the constructing thread

    bool  IsHeldByCurrentThread() const;
    void  Release();                         // on the owner; leaves it unowned
    void  Claim();                           // on the taker; it must be unowned
};

// First statement of a guarded entry point. Debug: an EHM ASSERT naming the
// entry point. Release: nothing.
#define ASSERT_THREAD_OWNERSHIP(ownership)   ...
```

- `DiskImageStore::GetThreadOwnership()` returns the store's token.
- `Disk2Controller::SetThreadOwnership (ThreadOwnership &)` points a controller
  at its machine's store token; a controller with none uses its own.

## Published drive status (`Devices/Disk/DriveStatus.h`, `Shell/DriveStatusPublisher.h`)

```cpp
struct BayStatus
{
    bool              mounted        = false;
    std::string       path;
    DiskFormat        format         = DiskFormat::Dsk;
    WriteProtectInfo  writeProtect;
    bool              salvageOffered = false;
};

struct DriveActivity
{
    bool      motorOn          = false;
    int       headQuarterTrack = -1;
    uint64_t  readNibbles      = 0;
    uint64_t  writeNibbles     = 0;
};

using BayTable = std::array<std::array<BayStatus, kDriveCount>, kSlotCount>;

struct DriveStatus
{
    std::shared_ptr<const BayTable>             bays;
    uint64_t                                    bayGeneration      = 0;
    std::array<DriveActivity, kDriveCount>      slot6Activity;
    bool                                        hasSlot6Controller = false;
    uint64_t                                    sequence           = 0;
};

class DriveStatusPublisher
{
public:
    void                                 Publish      (std::shared_ptr<const DriveStatus> status, bool bayChanged);
    std::shared_ptr<const DriveStatus>   Take() const;
    void                                 SetWakeEvent (HANDLE frameReadyEvent);
};
```

- `Publish` and `Take` take only the publisher's own leaf mutex. Nothing else is
  locked while it is held.
- `DiskImageStore::GetStatusGeneration()` and
  `DiskImageStore::CaptureBayTable (BayTable &)` are owner-only.

## Store operations that move to the owner (`Devices/Disk/DiskImageStore.h`)

```cpp
HRESULT  SetUserWriteProtect (int slot, int drive, bool writeProtected);   // saves first when protecting
void     NoteExternalChange  (const std::string & path, ExternalChangeIntent intent); // any thread; inbox only
```

`AssessSalvage` and `SalvageToFile` keep their signatures and become owner-only.

## File I/O seam additions (`Devices/Disk/IDiskFileIo.h`)

```cpp
virtual HRESULT  CopyFileMetadata       (const std::string & fromPath, const std::string & toPath) = 0;
virtual HRESULT  FlushToStorage         (const std::string & path) = 0;
virtual HRESULT  RenameWithoutReplacing (const std::string & fromPath, const std::string & toPath) = 0;
```

## Durable commit (`Devices/Disk/DurableCommit.h`)

```cpp
enum class CommitMode { Replace, CreateNew };

class DurableCommit
{
public:
    static HRESULT  Commit (IDiskFileIo & io, const std::string & targetPath,
                            const std::vector<Byte> & bytes, CommitMode mode);
};
```

Order: write the temporary, copy metadata from the target when it exists,
flush the temporary to storage, then replace (or rename without replacing).
On any failure the temporary is removed and the target is untouched.

## Commands (`resource.h`, `Shell/CpuCommandDispatcher.h`)

| Id | Posted by | Payload | Runs |
|---|---|---|---|
| `IDM_DISK_SET_USER_WP` | Settings, Disk menu | `"<drive> <0\|1>"` | `DiskImageStore::SetUserWriteProtect` on slot 6 |
| `IDM_DISK_SALVAGE_ASSESS` | Disk menu | `"<drive>"` | `AssessSalvage`, then posts `WM_APP_SALVAGE_ASSESSED` with the counts |
| `IDM_DISK_SALVAGE_WRITE` | the salvage dialog | `"<drive>"` | `SalvageToFile`, then the ordinary insert of the copy |
| `IDM_DEBUG_ATTACH_SINKS` | opening or closing a debug panel | none | points the open panels' sinks at the machine's devices |

None of these is journaled for reverse execution except the salvage insert,
which is an ordinary insert.

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

    static bool   TryCapture (MachineHost & host, bool externalDriveConnected, MachineChromeFacts & facts);
};
```

`TryCapture` takes the machine lifetime lock shared with `try_to_lock` and
leaves `facts` unchanged when it does not get it.

## After the `master` merge (`Shell/CpuManager.h`)

```cpp
HRESULT  Park (DWORD timeoutMs);    // returns once the emulation thread is parked
void     Unpark();
bool     IsParked() const;
```

While parked, the emulation thread runs no frame, no command drain and no
service pass. The update installer parks before its final save.
