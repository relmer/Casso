#include "Pch.h"

#include "DiskManager.h"

#include "Core/MemoryBus.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Win32ImageWatcher.h"
#include "Cli/Win32DiskFileIo.h"
#include "Audio/DriveAudioMixer.h"
#include "Audio/Disk2AudioSource.h"
#include "../Config/IFileSystem.h"
#include "../DiskSettings.h"
#include "../WasapiAudio.h"
#include "../Ui/Chrome/DriveWidget.h"
#include "../Ui/DriveWidgetController.h"
#include "../Ui/DriveWidgetState.h"
#include "../resource.h"
#include "CpuManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManager
//
////////////////////////////////////////////////////////////////////////////////

DiskManager::DiskManager (
    std::vector<std::unique_ptr<MemoryDevice>>      & ownedDevices,
    DiskImageStore                                  & diskStore,
    std::vector<std::unique_ptr<Disk2AudioSource>> & diskAudioSources,
    WasapiAudio                                     & wasapiAudio,
    DriveWidgetController                           & driveWidgets,
    std::array<DriveWidgetState, 2>                 & driveWidgetState,
    std::array<DriveWidget, 2>                      & driveChrome,
    CpuManager                                      & cpuManager,
    const std::wstring                              & currentMachineName,
    UserConfigStore                                 & userConfigStore,
    IFileSystem                                     & fileSystem,
    std::array<bool, 2>                             & userWriteProtect)
    : m_ownedDevices       (ownedDevices),
      m_diskStore          (diskStore),
      m_diskAudioSources   (diskAudioSources),
      m_wasapiAudio        (wasapiAudio),
      m_driveWidgets       (driveWidgets),
      m_driveWidgetState   (driveWidgetState),
      m_driveChrome        (driveChrome),
      m_cpuManager         (cpuManager),
      m_currentMachineName (currentMachineName),
      m_userConfigStore    (userConfigStore),
      m_fileSystem         (fileSystem),
      m_userWriteProtect   (userWriteProtect)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNowMs
//
//  Steady-clock millisecond timestamp used by the drive-widget door
//  animation FSM. Monotonic so an NTP step doesn't strand a half-open
//  door.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DiskManager::GetNowMs()
{
    auto  duration = std::chrono::steady_clock::now().time_since_epoch();



    return std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProbeFileWritability
//
//  Determines whether the backing host file can be written back to.
//  Prefers the read-only attribute (surfaced via the owner_write perms
//  bit on Windows) so a plain read-only file reports its true cause;
//  otherwise probes with a non-truncating read+write open to catch ACL
//  denials / exclusive locks. A missing / empty path is writable --
//  there is nothing to protect.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::ProbeFileWritability (
    const std::string & path,
    bool              & outReadOnly,
    bool              & outNoPermission)
{
    std::error_code  ec;
    fs::file_status  st;
    fs::path         p (path);



    outReadOnly     = false;
    outNoPermission = false;

    // A missing / empty path is writable -- there is nothing to protect.
    if (!path.empty() && fs::exists (p, ec))
    {
        st = fs::status (p, ec);

        // The read-only ATTRIBUTE is preferred over the open probe so a plain
        // read-only file reports its true cause; only when the attribute says
        // writable do we probe for an ACL denial or exclusive lock.
        if (!ec && (st.permissions() & fs::perms::owner_write) == fs::perms::none)
        {
            outReadOnly = true;
        }
        else
        {
            std::fstream  probe (p, std::ios::in | std::ios::out | std::ios::binary);

            outNoPermission = !probe.good();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyExternalWriteProtect
//
//  Re-asserts the user's per-drive write-protect preference and the
//  backing file's writability onto a freshly mounted image. The image's
//  own embedded flag (WOZ INFO chunk) is set by its loader and left
//  alone here.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::ApplyExternalWriteProtect (
    int                 drive,
    DiskImage         * image,
    const std::string & path)
{
    bool  readOnly     = false;
    bool  noPermission = false;
    bool  userWp       = false;



    if (image == nullptr)
    {
        return;
    }

    if (drive >= 0 && drive < static_cast<int> (m_userWriteProtect.size()))
    {
        userWp = m_userWriteProtect[drive];
    }

    ProbeFileWritability (path, readOnly, noPermission);

    image->SetUserWriteProtected (userWp);
    image->SetFileWriteProtect   (readOnly, noPermission);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleImageWriteProtect
//
//  WOZ carries its own write-protect flag (INFO byte 2), so the toggle has
//  to reach the file. SetImageWriteProtect does the whole operation --
//  flush pending guest writes, patch the one flag byte, recompute the
//  header CRC, write it back atomically, then move the live image's flag to
//  match. That ordering used to live here, split across a Flush, a flag
//  assignment and a ForceFlush, and getting it wrong lost data; it belongs
//  with the operation, not with the menu handler. Sector-image formats have
//  no in-image flag, so the toggle sets or clears the backing file's
//  read-only attribute instead.
//
//  Either way the function ends by re-probing the backing file and
//  re-applying the external state: the padlock, tooltip, and menu check
//  reflect what actually happened, never what was merely attempted.
//  Failures report through the shared EHM notifier.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::ToggleImageWriteProtect (int drive)
{
    HRESULT       hr         = S_OK;
    DiskImage   * image      = nullptr;
    DiskFormat    fmt        = DiskFormat::Dsk;
    bool          protecting = false;
    std::string   path;
    std::wstring  wide;



    CBRAEx (drive == 0 || drive == 1, E_INVALIDARG);

    image = m_diskStore.GetImage (6, drive);
    path  = m_diskStore.GetSourcePath (6, drive);

    CBREx (image != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    hr = DiskImageStore::GetSourceFormatByExtension (path, fmt);
    CHR (hr);

    if (fmt == DiskFormat::Woz)
    {
        protecting = !image->GetWriteProtectInfo().imageFlag;

        hr = m_diskStore.SetImageWriteProtect (6, drive, protecting);
        CHR (hr);
    }
    else
    {
        bool  readOnly = false;

        wide = fs::path (path).wstring();

        hr = m_fileSystem.GetReadOnlyAttribute (wide, readOnly);
        CHRN (hr, L"The disk file's attributes could not be read.");

        protecting = !readOnly;

        if (protecting)
        {
            hr = m_diskStore.Flush (6, drive);
            CHR (hr);
        }

        hr = m_fileSystem.SetReadOnlyAttribute (wide, protecting);
        CHRN (hr, L"The disk file's read-only attribute could not be changed.");
    }

Error:
    // Truth, not intent: whatever happened above, the indicators re-read
    // the file and the effective state (FR-015 / FR-016).
    if (image != nullptr)
    {
        ApplyExternalWriteProtect (drive, image, path);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSlot6Controller
//
//  Scans the owned-device list for the Disk II controller. Returns
//  nullptr if none is wired (e.g., a machine config without a disk
//  slot).
//
////////////////////////////////////////////////////////////////////////////////

Disk2Controller * DiskManager::FindSlot6Controller()
{
    Disk2Controller *  result = nullptr;



    for (auto & dev : m_ownedDevices)
    {
        result = dynamic_cast<Disk2Controller *> (dev.get());

        if (result != nullptr)
        {
            break;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountCommandLineDisks
//
//  Resolves the per-drive image to mount: explicit command-line path
//  wins, otherwise the last-session registry remembers it. A missing
//  remembered file is cleared on the spot (FR-047) so a moved or
//  deleted image stops resurrecting every launch.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::MountCommandLineDisks (
    const std::string & disk1Path,
    const std::string & disk2Path)
{
    HRESULT      hr            = S_OK;
    std::string  resolvedDisk1 = disk1Path;
    std::string  resolvedDisk2 = disk2Path;



    if (resolvedDisk1.empty() && !m_currentMachineName.empty())
    {
        std::wstring  saved;
        HRESULT       hrRead = DiskSettings::ReadSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                                0, m_currentMachineName, saved);

        if (hrRead == S_OK && !saved.empty())
        {
            if (fs::exists (fs::path (saved)))
            {
                resolvedDisk1 = fs::path (saved).string();
            }
            else
            {
                OutputDebugStringW (L"[DiskManager] FR-047: drive 0 last-mounted image missing; clearing.\n");
                HRESULT  hrClear = DiskSettings::WriteSavedDiskPath (
                    m_userConfigStore, m_fileSystem, 0, m_currentMachineName, std::wstring());
                IGNORE_RETURN_VALUE (hrClear, S_OK);
            }
        }
    }

    if (resolvedDisk2.empty() && !m_currentMachineName.empty())
    {
        std::wstring  saved;
        HRESULT       hrRead = DiskSettings::ReadSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                                1, m_currentMachineName, saved);

        if (hrRead == S_OK && !saved.empty())
        {
            if (fs::exists (fs::path (saved)))
            {
                resolvedDisk2 = fs::path (saved).string();
            }
            else
            {
                OutputDebugStringW (L"[DiskManager] FR-047: drive 1 last-mounted image missing; clearing.\n");
                HRESULT  hrClear = DiskSettings::WriteSavedDiskPath (
                    m_userConfigStore, m_fileSystem, 1, m_currentMachineName, std::wstring());
                IGNORE_RETURN_VALUE (hrClear, S_OK);
            }
        }
    }

    if (resolvedDisk1.empty() && resolvedDisk2.empty())
    {
        return;
    }

    // Drive 2 goes in first so drive 1's disk -- the one the machine boots
    // from -- is the later of the two mounts and therefore the top entry in
    // the recent-disks picker, which the mount completion feeds in the order
    // the mounts finish. The two bays are independent, so the order costs
    // nothing else.
    if (!resolvedDisk2.empty())
    {
        hr = MountDiskInSlot6 (1, resolvedDisk2);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (!resolvedDisk1.empty())
    {
        hr = MountDiskInSlot6 (0, resolvedDisk1);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiskInSlot6
//
//  Routes the mount through the DiskImageStore so dirty writes
//  auto-flush back to the host filesystem on Eject, SwitchMachine,
//  PowerCycle, and Shutdown. The controller's nibble engine is then
//  re-pointed at the store-owned DiskImage via SetExternalDisk so the
//  controller drives the same image bytes the store will eventually
//  serialize.
//
//  Whatever the outcome, it leaves through the completion callback: this
//  runs on the CPU thread for every user-initiated mount, so the HRESULT
//  returned here reaches nobody who can act on it. A mount that failed used
//  to end here in silence, with the machine sitting at a bare text screen and
//  nothing said about why.
//
//  The missing-controller bail is the one exception, and deliberately so.
//  There is no drive to mount into on a machine without a Disk II, so no
//  mount was attempted and there is no outcome to report; the only caller
//  that can reach it already checks HasSlot6Controller first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::MountDiskInSlot6 (int drive, const std::string & path)
{
    HRESULT              hr         = S_OK;
    Disk2Controller  *   controller = FindSlot6Controller();
    bool                 attempted  = false;
    MountDiagnosis       diagnosis;



    CBR (controller != nullptr);

    // Past here a mount was genuinely attempted, so its outcome is reported
    // whichever way it goes.
    attempted = true;

    // The diagnosis rides out with the outcome. Without it the shell would be
    // left re-deriving a reason from the file name, which can only ever tell
    // an unreadable extension from everything else.
    // The store announces the insert through its bay-change sink, so
    // re-pointing the controller, re-applying write protection, the debug
    // event, and the door and its sound all happen in OnBayChange -- fired
    // from inside this call, before it returns. Cold-boot suppression of the
    // sound lives there too.
    hr = m_diskStore.Mount (6, drive, path, diagnosis);
    CHR (hr);

    // Persist this drive's mount path so the next launch / next time
    // this machine is selected auto-mounts the same disk. Don't
    // pollute hr with the registry result -- a missing key is
    // non-fatal.
    if (!m_currentMachineName.empty())
    {
        std::wstring  wPath = fs::path (path).wstring();
        HRESULT       hrReg = DiskSettings::WriteSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                                drive, m_currentMachineName, wPath);
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

Error:
    // Last, and on the way out of both paths, so anything the callback does
    // (raising a dialog, say) sees the drive in its settled state -- fully
    // mounted, or left as the failure left it.
    if (attempted && m_onMountCompleted)
    {
        m_onMountCompleted (drive, path, hr, diagnosis);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDiskInSlot6
//
//  Auto-flushes dirty bits via the store, which announces the eject through
//  its bay-change sink -- so detaching the controller, the debug event, the
//  door and its sound all happen in OnBayChange, not here.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::EjectDiskInSlot6 (int drive)
{
    m_diskStore.Eject (6, drive);

    // Clear the per-machine remembered path so the next launch comes
    // up empty in this slot.
    if (!m_currentMachineName.empty())
    {
        HRESULT  hrReg = DiskSettings::WriteSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                           drive, m_currentMachineName, L"");
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnBayChange
//
//  The store's bay-change sink. One reaction to a disk changing in a bay,
//  whoever moved it.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::OnBayChange (int slot, int drive, BayChange change)
{
    Disk2Controller *  controller = nullptr;
    DiskImage *        image      = nullptr;
    bool               inserted   = false;
    bool               suppressFx = false;
    int64_t            nowMs      = 0;



    //  Slot 6 is the only bay with a drive on screen; the store has eight.
    if (slot != 6 || drive < 0 || drive >= DiskImageStore::kDriveCount)
    {
        return;
    }

    controller = FindSlot6Controller();
    image      = m_diskStore.GetImage (6, drive);
    inserted   = (change != BayChange::Ejected);

    //  Insert and swap sounds are held during the cold-boot mount window and a
    //  programmatic remount (reset / power-cycle rebuild); an eject sound
    //  always plays. This is the pre-central rule of MountDiskInSlot6 and
    //  EjectDiskInSlot6, kept in one place.
    suppressFx = m_coldBootMountWindow || m_programmaticRemount;

    //  Re-point the controller at what the store now holds -- the new image on
    //  an insert or a swap, null on an eject, which restores the internal disk.
    if (controller != nullptr)
    {
        controller->SetExternalDisk (drive, image);
    }

    //  Re-assert the effective write protection on a disk that just arrived:
    //  the user's per-drive preference plus the backing file's read-only state.
    if (inserted && image != nullptr)
    {
        ApplyExternalWriteProtect (drive, image, m_diskStore.GetSourcePath (6, drive));
    }

    //  The debug event fires whichever way the disk went, and is not held at
    //  cold boot: the debug log is not something a launch should hide.
    if (controller != nullptr)
    {
        if (inserted)
        {
            controller->NotifyDiskInserted (drive);
        }
        else
        {
            controller->NotifyDiskEjected (drive);
        }
    }

    //  Door and sound. The visual door for a plain insert or eject rides the
    //  source-path poll in UpdateDriveWidgets, which sees the file in the bay
    //  change; a swap keeps the same file, so its open-then-close door is
    //  published here as the event the poll cannot infer.
    if (drive >= static_cast<int> (m_diskAudioSources.size()) ||
        m_diskAudioSources[drive] == nullptr)
    {
        return;
    }

    nowMs = GetNowMs();

    switch (change)
    {
        case BayChange::Ejected:
            m_wasapiAudio.RecordDriveDoorSyncEvent (drive, nowMs);
            m_driveWidgets.PublishSyncEvent (drive,
                                             DriveWidgetController::SyncAction::DoorOpen, nowMs);
            m_diskAudioSources[drive]->OnDiskEjected();
            break;

        case BayChange::Inserted:
            if (!suppressFx)
            {
                m_wasapiAudio.RecordDriveDoorSyncEvent (drive, nowMs);
                m_driveWidgets.PublishSyncEvent (drive,
                                                 DriveWidgetController::SyncAction::DoorClose, nowMs);
                m_diskAudioSources[drive]->OnDiskInserted();
            }

            break;

        case BayChange::Swapped:
            if (!suppressFx)
            {
                m_wasapiAudio.RecordDriveDoorSyncEvent (drive, nowMs);
                m_driveWidgets.PublishSyncEvent (drive,
                                                 DriveWidgetController::SyncAction::DoorReinsert, nowMs);
                m_diskAudioSources[drive]->OnDiskSwapped();
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemountSlot6Disks
//
//  Re-reads every currently mounted slot-6 disk image from the host
//  filesystem so that an external regeneration of the .dsk file (e.g.
//  a developer iterating on a demo image) is picked up by the next
//  boot. Used by the Reset and Power Cycle menu commands. Snapshots
//  paths first because the re-mount path goes through Eject + Mount
//  internally, which transiently blanks the source-path slot.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::RemountSlot6Disks()
{
    std::string  savedDisk[DiskImageStore::kDriveCount];
    HRESULT      hrMount = S_OK;
    int          drive   = 0;



    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        savedDisk[drive] = m_diskStore.GetSourcePath (6, drive);
    }

    m_programmaticRemount = true;
    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        if (!savedDisk[drive].empty())
        {
            hrMount = MountDiskInSlot6 (drive, savedDisk[drive]);
            IGNORE_RETURN_VALUE (hrMount, S_OK);
        }
    }

    m_programmaticRemount = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mount  (IDriveCommandSink-style)
//
//  UI-thread entry point. Routes through the existing IDM_DISK_INSERT*
//  command queue so the actual mount runs on the CPU thread, mirroring
//  the menu-driven path. Only slot 6 is supported today (the
//  integrated Disk II); unknown slots are E_INVALIDARG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::Mount (int slot, int drive, const std::wstring & path)
{
    HRESULT  hr      = S_OK;
    WORD     command = 0;



    CBRAEx (slot == 6, E_INVALIDARG);

    CBRAEx (drive == 0 || drive == 1, E_INVALIDARG);

    command = (drive == 0) ? IDM_DISK_INSERT1 : IDM_DISK_INSERT2;

    m_cpuManager.PostCommand (command, fs::path (path).string());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject
//
//  Ejects a drive: posts the command to the CPU thread and starts the door
//  animation.
//
//  The command is POSTED rather than performed, because ejecting detaches a
//  disk the drive engine may be actively reading -- it has to land between
//  instructions.
//
//  The door animation is started HERE rather than being left to the
//  path-change watcher, and that is the point of this function. The watcher in
//  UpdateDriveWidgets only calls BeginEject when the mounted path actually
//  transitions to empty, so clicking eject on an already-empty drive would be
//  a visual no-op -- the user would press the button and see nothing happen.
//
//  Only slot 6 drives 1 and 2 have an eject affordance; anything else leaves
//  the command at 0, which is the nothing-to-do signal for both halves.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::Eject (int slot, int drive)
{
    WORD  command = 0;



    // Only slot 6 drives 1 and 2 have an eject affordance; command stays 0
    // for anything else, which is the "nothing to do" signal below.
    if (slot == 6)
    {
        if      (drive == 0) { command = IDM_DISK_EJECT1; }
        else if (drive == 1) { command = IDM_DISK_EJECT2; }
    }

    if (command != 0)
    {
        m_cpuManager.PostCommand (command);

        // Open the door as immediate visual feedback, but leave the mount
        // bookkeeping alone: the path-change watcher in UpdateDriveWidgets
        // runs the real BeginEject once the store actually empties. The
        // old immediate BeginEject cleared mountedImagePath here, so the
        // watcher's very next tick read the still-mounted store as a fresh
        // INSERT (door flaps closed), then as an eject once the CPU thread
        // caught up (door reopens) -- one click, two open animations. The
        // flap predates the modal keep-alive; it just never PRESENTED
        // before, because the old pre-picker pump stalled.
        m_driveWidgetState[drive].StartDoorTransition (DriveWidgetState::Door::Opening, GetNowMs());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDriveWidgets
//
//  Per-UI-frame sync from the (CPU-thread-owned) Disk II controller
//  state into the (UI-thread-only) DriveWidgetState. Reads the
//  engine's lifetime nibble counters and treats any forward movement
//  since the previous frame as "disk active" (FR-025 active LED).
//  Motor-on tracks the controller's IsMotorOn() directly.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::UpdateDriveWidgets()
{
    Disk2Controller                                     * controller = FindSlot6Controller();
    int64_t                                               nowMs      = GetNowMs();
    std::vector<DriveWidgetController::DriveSyncEvent>    syncEvents = m_driveWidgets.ConsumeSyncEvents();
    int                                                   drive      = 0;



    for (drive = 0; drive < static_cast<int> (m_driveWidgetState.size()); drive++)
    {
        DriveWidgetState   & st               = m_driveWidgetState[drive];
        const std::string  & src              = m_diskStore.GetSourcePath (6, drive);
        std::wstring         wPath;
        bool                 motorOn          = false;
        int                  headQuarterTrack = -1;   // -1 == no controller, position unknown
        bool                 active           = false;
        uint64_t             reads            = 0;
        uint64_t             writes           = 0;

        for (const auto & evt : syncEvents)
        {
            if (evt.driveId == drive)
            {
                st.lastSyncEventId = evt.eventId;

                // A swap keeps the same file in the bay, so the path diff
                // below never sees it. OnBayChange publishes this instead, and
                // it is the one door transition that comes from an event
                // rather than from the store's path. Open, then close.
                if (evt.action == DriveWidgetController::SyncAction::DoorReinsert)
                {
                    st.BeginReinsert (nowMs);
                }
            }
        }

        // mountedImagePath -- single writer (UI thread), source of
        // truth is the DiskImageStore. Reflect door FSM transitions
        // when mount state changes.
        //
        // THE NARROW PATH IS DIFFED FIRST, so the wide conversion below runs
        // only on the frame a disk actually goes in or out, not on every one
        // of the ~60 frames a second this loop runs to sample drive activity.
        // A mount or eject is rare; the string it changes is not worth
        // rebuilding continuously.
        if (src != m_lastDriveSourcePath[drive])
        {
            m_lastDriveSourcePath[drive] = src;

            // Decode the store's native-narrow source path back to wide via
            // fs::path (the inverse of the fs::path(...).string() narrowing the
            // mount path used). A manual wstring(begin,end) widen would
            // sign-extend a high byte like 0xF8 ('o' with stroke) into U+FFF8
            // and render as a tofu box in the drive label.
            wPath = fs::path (src).wstring();

            if (wPath.empty())
            {
                st.BeginEject (nowMs);
            }
            else
            {
                st.BeginInsert (wPath, nowMs);
            }
        }

        st.TickDoorAnimation (nowMs);

        // motorOn + diskActive sampling. The controller's engine is
        // owned by the device, which the CPU thread mutates; we read
        // the bool + monotonic counters with relaxed atomics
        // semantics (existing audio-system pattern).

        if (controller != nullptr)
        {
            auto &  engine = controller->GetEngine (drive);
            motorOn = engine.IsMotorOn();
            reads   = engine.GetReadNibbles();
            writes  = engine.GetWriteNibbles();
            headQuarterTrack = engine.GetCurrentTrack();

            if (reads != m_lastReadNibbles[drive] ||
                writes != m_lastWriteNibbles[drive])
            {
                active = true;
            }
        }

        m_lastReadNibbles[drive]  = reads;
        m_lastWriteNibbles[drive] = writes;

        st.motorOn.store    (motorOn,   std::memory_order_relaxed);
        st.diskActive.store (active,    std::memory_order_relaxed);
        st.headQuarterTrack.store  (headQuarterTrack, std::memory_order_relaxed);

        // Sample the mounted image's write-protect breakdown for the
        // padlock cue + hover tooltip. Empty drive -> no protection.
        {
            DiskImage *  image = m_diskStore.GetImage (6, drive);

            st.writeProtect = (image != nullptr) ? image->GetWriteProtectInfo()
                                                 : WriteProtectInfo();
        }
    }

    m_driveWidgets.SyncFromStates (m_driveWidgetState);
    m_driveChrome[0].SyncFromState (m_driveWidgetState[0]);
    m_driveChrome[1].SyncFromState (m_driveWidgetState[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManager::InstallSharedImageSupport
//
//  Builds the platform pieces a shared image needs and hands them over.
//
//  THE HAND-OVER IS THE WHOLE JOB. Registering a watch at mount and dropping it
//  at eject is orchestration, and orchestration lives in the store where a fake
//  watcher can assert it; choosing which implementation to build is what an
//  executable is for.
//
//  `disabled` INSTALLS ONE THAT REFUSES EVERY WATCH rather than installing
//  none. That is the measurement seam, and the distinction matters: the store
//  still asks, still records that it is not watching, and still refuses to
//  write over a change it never saw -- which is precisely the guarantee being
//  measured.
//
////////////////////////////////////////////////////////////////////////////////

//
//  A watcher that cannot watch anything, standing in for a location that
//  cannot be watched. Behind the seam so the degraded path is reachable
//  without a network share.
//
class RefusingImageWatcher : public IImageWatcher
{
public:
    bool  Watch (const std::string &, Callback) override { return false; }
    void  Unwatch (const std::string &) override {}
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManager::InstallSharedImageSupport
//
//  Hands the store the watcher and the file probe it was built with.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::InstallSharedImageSupport (bool watchDisabled)
{
    if (watchDisabled)
    {
        m_imageWatcher = std::make_unique<RefusingImageWatcher> ();
    }
    else
    {
        m_imageWatcher = std::make_unique<Win32ImageWatcher> ();
    }

    //  The same platform seam the command line writes disk images through, so
    //  "is somebody else holding this file" is answered one way in one place.
    m_imageFileIo = std::make_unique<Win32DiskFileIo> ();

    m_diskStore.SetImageWatcher (m_imageWatcher.get());
    m_diskStore.SetFileIo       (m_imageFileIo.get());

    //  Every path that moves a disk ends at OnBayChange, so the door, its
    //  sounds and the debug event are lit from one place. Set here, before the
    //  command-line disks mount, so a cold-boot mount reaches the same handler
    //  (which suppresses its launch-time sound).
    m_diskStore.SetBayChangeSink ([this] (int slot, int drive, BayChange change)
    {
        OnBayChange (slot, drive, change);
    });
}
