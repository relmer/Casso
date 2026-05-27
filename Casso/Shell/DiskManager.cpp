#include "Pch.h"

#include "DiskManager.h"

#include "Core/MemoryBus.h"
#include "Devices/DiskIIController.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Audio/DriveAudioMixer.h"
#include "Audio/DiskIIAudioSource.h"
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
    std::vector<std::unique_ptr<DiskIIAudioSource>> & diskAudioSources,
    WasapiAudio                                     & wasapiAudio,
    DriveWidgetController                           & driveWidgets,
    std::array<DriveWidgetState, 2>                 & driveWidgetState,
    std::array<DriveWidget, 2>                      & driveChrome,
    CpuManager                                      & cpuManager,
    const std::wstring                              & currentMachineName,
    UserConfigStore                                 & userConfigStore,
    IFileSystem                                     & fileSystem)
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
      m_fileSystem         (fileSystem)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
//  Steady-clock millisecond timestamp used by the drive-widget door
//  animation FSM. Monotonic so an NTP step doesn't strand a half-open
//  door.
//
////////////////////////////////////////////////////////////////////////////////

int64_t DiskManager::NowMs ()
{
    auto  duration = std::chrono::steady_clock::now().time_since_epoch();

    return std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();
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

DiskIIController * DiskManager::FindSlot6Controller ()
{
    DiskIIController *  result = nullptr;


    for (auto & dev : m_ownedDevices)
    {
        result = dynamic_cast<DiskIIController *> (dev.get());

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

    if (!resolvedDisk1.empty())
    {
        hr = MountDiskInSlot6 (0, resolvedDisk1);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (!resolvedDisk2.empty())
    {
        hr = MountDiskInSlot6 (1, resolvedDisk2);
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
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::MountDiskInSlot6 (int drive, const std::string & path)
{
    HRESULT              hr         = S_OK;
    DiskIIController  *  controller = FindSlot6Controller();
    DiskImage         *  external   = nullptr;


    CBR (controller != nullptr);

    hr = m_diskStore.Mount (6, drive, path);
    CHR (hr);

    external = m_diskStore.GetImage (6, drive);
    controller->SetExternalDisk (drive, external);

    // The store-based mount path bypasses the controller's own
    // MountDisk method, so fire the IDiskIIEventSink hook explicitly
    // here so the debug window sees the insert. Cold boot mounts
    // still fire on the controller side (the debug window is rarely
    // open at app launch and the user wants to see the mount that
    // ran without their click); the cold-boot suppression below is
    // audio-only (FR-013).
    controller->NotifyDiskInserted (drive);

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

    // Drive-audio door-close (FR-013). Cold-boot mounts (command-line,
    // last-session restoration, autoload) MUST be suppressed -- they
    // happen before the user has interacted with the running //e and
    // shouldn't audibly slam the drive door at app launch. Post-
    // startup mounts (user-initiated mid-session) always fire.
    if (!m_coldBootMountWindow &&
        drive >= 0 &&
        static_cast<size_t> (drive) < m_diskAudioSources.size() &&
        m_diskAudioSources[drive] != nullptr)
    {
        m_wasapiAudio.RecordDriveDoorSyncEvent (drive, NowMs());
        m_driveWidgets.PublishSyncEvent (drive,
                                         DriveWidgetController::SyncAction::DoorClose,
                                         NowMs());
        m_diskAudioSources[drive]->OnDiskInserted();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDiskInSlot6
//
//  Auto-flushes dirty bits via the store and detaches the controller's
//  external disk.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::EjectDiskInSlot6 (int drive)
{
    DiskIIController *  controller = FindSlot6Controller();


    m_diskStore.Eject (6, drive);

    if (controller != nullptr)
    {
        controller->SetExternalDisk (drive, nullptr);

        // Mirror the insert path: fire the controller-level event
        // sink so the debug window logs the eject. Fires AFTER the
        // external disk is detached so any sink that inspects the
        // controller sees the post-eject state.
        controller->NotifyDiskEjected (drive);
    }

    // Clear the per-machine remembered path so the next launch comes
    // up empty in this slot.
    if (!m_currentMachineName.empty())
    {
        HRESULT  hrReg = DiskSettings::WriteSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                           drive, m_currentMachineName, L"");
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

    // Drive-audio door-open (FR-014). Eject events always fire (no
    // cold-boot eject case in practice -- the app launches with the
    // drive bay closed).
    if (drive >= 0 &&
        static_cast<size_t> (drive) < m_diskAudioSources.size() &&
        m_diskAudioSources[drive] != nullptr)
    {
        m_wasapiAudio.RecordDriveDoorSyncEvent (drive, NowMs());
        m_driveWidgets.PublishSyncEvent (drive,
                                         DriveWidgetController::SyncAction::DoorOpen,
                                         NowMs());
        m_diskAudioSources[drive]->OnDiskEjected();
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

void DiskManager::RemountSlot6Disks ()
{
    std::string  savedDisk[DiskImageStore::kDriveCount];
    HRESULT      hrMount = S_OK;
    int          drive   = 0;


    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        savedDisk[drive] = m_diskStore.GetSourcePath (6, drive);
    }

    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        if (!savedDisk[drive].empty())
        {
            hrMount = MountDiskInSlot6 (drive, savedDisk[drive]);
            IGNORE_RETURN_VALUE (hrMount, S_OK);
        }
    }
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


    if (slot != 6)
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    if (drive == 0)
    {
        command = IDM_DISK_INSERT1;
    }
    else if (drive == 1)
    {
        command = IDM_DISK_INSERT2;
    }
    else
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    m_cpuManager.PostCommand (command, fs::path (path).string());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject  (IDriveCommandSink-style)
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::Eject (int slot, int drive)
{
    WORD  command = 0;


    if (slot != 6)
    {
        return;
    }

    if (drive == 0)
    {
        command = IDM_DISK_EJECT1;
    }
    else if (drive == 1)
    {
        command = IDM_DISK_EJECT2;
    }
    else
    {
        return;
    }

    m_cpuManager.PostCommand (command);

    // Animate the door open even if no disk is currently mounted.
    // The path-change watcher in UpdateDriveWidgets only triggers
    // BeginEject when the mounted path actually transitions to empty,
    // so an eject click on an already-empty drive would otherwise be
    // a visual no-op.
    m_driveWidgetState[drive].BeginEject (NowMs());
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

void DiskManager::UpdateDriveWidgets ()
{
    DiskIIController *  controller = FindSlot6Controller();
    int64_t             nowMs      = NowMs();
    std::vector<DriveWidgetController::DriveSyncEvent>  syncEvents = m_driveWidgets.ConsumeSyncEvents();
    int                 drive      = 0;


    for (drive = 0; drive < static_cast<int> (m_driveWidgetState.size()); drive++)
    {
        DriveWidgetState &  st = m_driveWidgetState[drive];

        for (const auto & evt : syncEvents)
        {
            if (evt.driveId == drive)
            {
                st.lastSyncEventId = evt.eventId;
            }
        }

        // mountedImagePath -- single writer (UI thread), source of
        // truth is the DiskImageStore. Reflect door FSM transitions
        // when mount state changes.
        const std::string &  src   = m_diskStore.GetSourcePath (6, drive);
        std::wstring         wPath (src.begin(), src.end());

        if (wPath != st.mountedImagePath)
        {
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
        bool      motorOn  = false;
        bool      active   = false;
        uint64_t  reads    = 0;
        uint64_t  writes   = 0;

        if (controller != nullptr)
        {
            auto &  engine = controller->GetEngine (drive);
            motorOn = engine.IsMotorOn();
            reads   = engine.GetReadNibbles();
            writes  = engine.GetWriteNibbles();

            if (reads != m_lastReadNibbles[drive] ||
                writes != m_lastWriteNibbles[drive])
            {
                active = true;
            }
        }

        m_lastReadNibbles[drive]  = reads;
        m_lastWriteNibbles[drive] = writes;

        st.motorOn.store    (motorOn, std::memory_order_relaxed);
        st.diskActive.store (active,  std::memory_order_relaxed);
    }

    m_driveWidgets.SyncFromStates (m_driveWidgetState);
    m_driveChrome[0].SyncFromState (m_driveWidgetState[0]);
    m_driveChrome[1].SyncFromState (m_driveWidgetState[1]);
}
