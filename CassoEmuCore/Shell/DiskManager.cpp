#include "Pch.h"

#include "DiskManager.h"
#include "Core/PathResolver.h"

#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/MachineHost.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Win32ImageWatcher.h"
#include "Devices/Disk/WriteProtectChange.h"
#include "Seams/Win32DiskFileIo.h"
#include "Audio/DriveAudioMixer.h"
#include "Machines/Apple2/Common/Disk2AudioSource.h"
#include "Config/IFileSystem.h"
#include "Config/DiskSettings.h"
#include "../WasapiAudio.h"
#include "../Ui/Chrome/DriveWidget.h"
#include "../Ui/DriveWidgetController.h"
#include "../Ui/DriveWidgetState.h"
#include "../resource.h"
#include "CpuManager.h"
#include "Devices/Disk/MountDiagnosis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManager
//
////////////////////////////////////////////////////////////////////////////////

DiskManager::DiskManager (
    MachineHost                                     & machine,
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
    std::array<bool, 2>                             & userWriteProtect,
    const std::string                               & workingDirectory)
    : m_machine            (machine),
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
      m_userWriteProtect   (userWriteProtect),
      m_workingDirectory   (workingDirectory)
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
//  ApplyExternalWriteProtect
//
//  Re-asserts the user's per-drive write-protect preference onto a freshly
//  mounted image. The backing file's writability is probed by the store's
//  mount and reload jobs, on its I/O thread, and the image's embedded flag
//  (WOZ INFO chunk) is set by its loader.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::ApplyExternalWriteProtect (int drive)
{
    bool  userWp = false;



    if (drive >= 0 && drive < static_cast<int> (m_userWriteProtect.size()))
    {
        userWp = m_userWriteProtect[drive];
    }

    m_diskStore.SetUserWriteProtectFlag (6, drive, userWp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleImageWriteProtect
//
//  Write-protects an unprotected image, or write-enables a protected one.
//  WriteProtectChange::MakePlan decides which mechanisms that touches: a WOZ
//  is protected by its own flag (INFO byte 2), any other format by the
//  backing file's read-only attribute, and write-enabling clears whichever
//  of the two are set -- both, on a WOZ that carries both.
//
//  The attribute is cleared BEFORE the flag is, because the flag lives in
//  the file and patching it needs the file writable. SetImageWriteProtect
//  does the whole flag operation -- flush pending guest writes, patch the
//  one byte, recompute the header CRC, write it back atomically, then move
//  the live image's flag to match. Setting the attribute flushes first for
//  the same reason: a protected image drops dirty content at flush.
//
//  Each step's file work runs on the store's I/O queue, and the next step
//  starts in its completion. Failures report through the shared EHM notifier.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::ToggleImageWriteProtect (int drive)
{
    HRESULT                         hr     = S_OK;
    DiskImage                     * image  = nullptr;
    bool                            isLive = false;
    shared_ptr<WriteProtectToggle>  job    = std::make_shared<WriteProtectToggle>();



    CBRAEx (drive == 0 || drive == 1, E_INVALIDARG);

    //  A command that arrives on this thread behind live changes nothing.
    isLive = !m_diskStore.IsFlushHeld() && !m_diskStore.IsReplaying();
    CBRFEx (isLive, HRESULT_FROM_WIN32 (ERROR_INVALID_STATE),
            EhmNotifyUser (DiskStoreText::FormatBehindLiveWriteProtectMessage (drive).c_str()));

    image = m_diskStore.GetImage (6, drive);
    CBREx (image != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_READY));

    job->drive     = drive;
    job->path      = m_diskStore.GetSourcePath (6, drive);
    job->wide      = fs::path (job->path).wstring();
    job->imageFlag = image->GetWriteProtectInfo().imageFlag;

    hr = DiskImageStore::GetSourceFormatByExtension (job->path, job->format);
    CHR (hr);

    m_diskStore.GetIoQueue().Post ([this, job] { job->hr = m_fileSystem.GetReadOnlyAttribute (job->wide, job->isReadOnly); },
                                   [this, job] { PlanWriteProtectToggle (job); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlanWriteProtectToggle
//
//  After the attribute's read: the plan, and the save before a protect.
//  Setting the attribute saves first, because a protected image drops dirty
//  content at a save.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::PlanWriteProtectToggle (const shared_ptr<WriteProtectToggle> & job)
{
    if (FAILED (job->hr))
    {
        EhmNotifyUser (L"The disk file's attributes could not be read.");
        FinishWriteProtectToggle (job);
    }
    else
    {
        job->plan = WriteProtectChange::MakePlan (job->format == DiskFormat::Woz,
                                                  job->imageFlag,
                                                  job->isReadOnly,
                                                  fs::path (job->path).filename().wstring());

        if (job->plan.changesAttribute && job->plan.protecting)
        {
            m_diskStore.Flush (6, job->drive, [this, job] (HRESULT hrSave)
            {
                job->hr = hrSave;

                if (FAILED (hrSave))
                {
                    FinishWriteProtectToggle (job);
                }
                else
                {
                    SetWriteProtectAttribute (job);
                }
            });
        }
        else
        {
            SetWriteProtectAttribute (job);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteProtectAttribute
//
//  The attribute is set on the I/O queue, and cleared BEFORE the flag is,
//  because the flag is in the file, and the patch writes the file. SetImageWriteProtect then does the whole flag operation.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::SetWriteProtectAttribute (const shared_ptr<WriteProtectToggle> & job)
{
    if (!job->plan.changesAttribute)
    {
        SetWriteProtectFlag (job);
    }
    else
    {
        m_diskStore.GetIoQueue().Post ([this, job] { job->hr = m_fileSystem.SetReadOnlyAttribute (job->wide, job->plan.protecting); },
                                       [this, job]
        {
            if (FAILED (job->hr))
            {
                EhmNotifyUser (L"The disk file's read-only attribute could not be changed.");
                FinishWriteProtectToggle (job);
            }
            else
            {
                SetWriteProtectFlag (job);
            }
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteProtectFlag
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::SetWriteProtectFlag (const shared_ptr<WriteProtectToggle> & job)
{
    if (!job->plan.changesImageFlag)
    {
        job->isCompleted = true;
        FinishWriteProtectToggle (job);
    }
    else
    {
        m_diskStore.SetImageWriteProtect (6, job->drive, job->plan.protecting, [this, job] (HRESULT hrFlag)
        {
            job->hr          = hrFlag;
            job->isCompleted = SUCCEEDED (hrFlag);
            FinishWriteProtectToggle (job);
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishWriteProtectToggle
//
//  Truth, not intent: whatever happened, the file is probed again on the
//  I/O queue and the effective state applied, so the padlock, tooltip and
//  menu label reflect what happened, never what was merely attempted. Only
//  a change carried out in full is reported through the write-protect
//  callback.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::FinishWriteProtectToggle (const shared_ptr<WriteProtectToggle> & job)
{
    m_diskStore.GetIoQueue().Post ([job] { DiskFileOps::ProbeWritability (job->path, job->isReadOnly, job->isDenied); },
                                   [this, job]
    {
        if (m_diskStore.IsMounted (6, job->drive) && m_diskStore.GetSourcePath (6, job->drive) == job->path)
        {
            ApplyExternalWriteProtect (job->drive);
            m_diskStore.SetFileWriteProtect (6, job->drive, job->isReadOnly, job->isDenied);
        }

        if (job->isCompleted && m_onWriteProtectChanged)
        {
            m_onWriteProtectChanged (WriteProtectChange::DescribeResult (job->plan));
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSlot6Controller
//
//  The Disk II controller the builder wired, or nullptr on a machine without
//  one. Read off the machine's references rather than found by scanning its
//  owned-device list: the UI thread asks every frame, and a machine switch on
//  the CPU thread clears and refills that list while it rebuilds. Iterating a
//  vector another thread is emptying dereferenced a freed device and took the
//  process down.
//
////////////////////////////////////////////////////////////////////////////////

Disk2Controller * DiskManager::FindSlot6Controller()
{
    return m_machine.GetRefs().diskController;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishDriveStatus
//
//  Publishes the drive status from the store and the slot 6 controller, on
//  the thread that has the store's token, and calls m_statusWake when a bay
//  changed, so an idle window draws the change at once.
//
//  Code on this thread that posts a message whose handler reads the drive
//  status publishes first, as MountDiskInSlot6 does before its completion.
//
////////////////////////////////////////////////////////////////////////////////

DriveStatusChange DiskManager::PublishDriveStatus (bool isActivityDue)
{
    DriveStatusChange  change = m_statusPublisher.Publish (m_diskStore, FindSlot6Controller(), isActivityDue);
    bool               isWake = change == DriveStatusChange::Bays && m_statusWake;



    if (isWake)
    {
        m_statusWake();
    }

    return change;
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
    std::string  resolvedDisk1 = PathResolver::MakeAbsolutePath (disk1Path, m_workingDirectory);
    std::string  resolvedDisk2 = PathResolver::MakeAbsolutePath (disk2Path, m_workingDirectory);



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

    // ONE FILE CANNOT GO IN BOTH DRIVES, and drive 1 is the one that keeps it:
    // it is the drive the machine boots from. The store refuses the second bay
    // whichever order they arrive in, and drive 2 goes in first, so without
    // this the boot drive would be the one turned away.
    //
    // A DUPLICATE THAT CAME OUT OF THE SAVED PATHS IS CLEARED, not just
    // skipped. Prefs written before the store refused this could hold the same
    // file for both drives, and a launch that only skipped it would raise the
    // same refusal on every launch after.
    if (!resolvedDisk1.empty()
     && MountedImageState::IsSamePath (resolvedDisk1, resolvedDisk2))
    {
        resolvedDisk2.clear();

        if (disk2Path.empty() && !m_currentMachineName.empty())
        {
            HRESULT  hrClear = DiskSettings::WriteSavedDiskPath (
                m_userConfigStore, m_fileSystem, 1, m_currentMachineName, std::wstring());
            IGNORE_RETURN_VALUE (hrClear, S_OK);
        }
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
//  runs on the CPU thread for every user-initiated mount, a salvaged copy's
//  included, so the HRESULT returned here reaches nobody who can act on it. A mount that failed used
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
    MountDiagnosis       diagnosis;



    CBR (controller != nullptr);

    // The diagnosis rides out with the outcome. Without it the shell would be
    // left re-deriving a reason from the file name, which can only ever tell
    // an unreadable extension from everything else.
    // The store announces the insert through its bay-change sink, so
    // re-pointing the controller, re-applying write protection, the debug
    // event, and the door and its sound all happen in OnBayChange -- fired
    // from inside this call, before it returns. Cold-boot suppression of the
    // sound lives there too.
    hr = m_diskStore.Mount (6, drive, path, diagnosis);

    // Past the controller check a mount was genuinely attempted, so its
    // outcome is reported whichever way it goes.
    CompleteMountInSlot6 (drive, path, hr, diagnosis);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartMountInSlot6
//
//  MountDiskInSlot6 for a command: the store's file work runs on its I/O
//  queue, and the outcome is reported from its completion, on the thread
//  with the store's token, so the machine runs while the file is read and
//  the disk in the drive is saved.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::StartMountInSlot6 (int drive, const std::string & path)
{
    HRESULT              hr         = S_OK;
    Disk2Controller  *   controller = FindSlot6Controller();



    CBR (controller != nullptr);

    m_diskStore.Mount (6, drive, path, [this, drive, path] (HRESULT hrMount, const MountDiagnosis & diagnosis)
    {
        CompleteMountInSlot6 (drive, path, hrMount, diagnosis);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CompleteMountInSlot6
//
//  What follows an attempted mount, whichever way it went: the path is saved
//  for the next launch, a declined insert's door closes, the status is
//  published and the completion is reported.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::CompleteMountInSlot6 (int drive, const std::string & path, HRESULT hr, const MountDiagnosis & diagnosis)
{
    // Persist this drive's mount path so the next launch / next time
    // this machine is selected auto-mounts the same disk. Don't
    // pollute hr with the registry result -- a missing key is
    // non-fatal.
    if (SUCCEEDED (hr) && !m_currentMachineName.empty())
    {
        std::wstring  wPath = fs::path (path).wstring();
        HRESULT       hrReg = DiskSettings::WriteSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                                drive, m_currentMachineName, wPath);
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

    //  After a declined insert the disk that was in the drive is there, so the
    //  door the window opened for the insert closes again.
    if (FAILED (hr))
    {
        RestoreDoor (drive);
    }

    // Last, so whatever the callback does, such as raising a dialog, runs
    // with the drive in its settled state: mounted, or as it was after the
    // failure. Published first, so the completion's handler finds the mount.
    (void) PublishDriveStatus (true);

    if (m_onMountCompleted)
    {
        m_onMountCompleted (drive, path, hr, diagnosis);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiskForCommand
//
//  The IDM_DISK_INSERTn mount. A command that arrives on this thread behind
//  live is one the window's divergence gate did not stop, so it changes
//  nothing and is reported as MountDiskInSlot6 reports a failure. The check is not in
//  MountDiskInSlot6, which the command line's mounts and the remounts also
//  call, and a machine switch's remount runs after recording stops.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskManager::MountDiskForCommand (int drive, const std::string & path)
{
    HRESULT  hr     = S_OK;
    bool     isLive = !m_diskStore.IsFlushHeld() && !m_diskStore.IsReplaying();



    CBRFEx (isLive, HRESULT_FROM_WIN32 (ERROR_INVALID_STATE), DeclineBehindLiveInsert (drive, path));

    hr = StartMountInSlot6 (drive, path);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeclineBehindLiveInsert
//
//  The report of an insert declined behind live: the door closes again over
//  a disk in the drive, the status is published, and the completion has
//  MountFailure::BehindLive.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::DeclineBehindLiveInsert (int drive, const std::string & path)
{
    MountDiagnosis  diagnosis;



    diagnosis.failure = MountFailure::BehindLive;

    if (m_diskStore.IsMounted (6, drive))
    {
        diagnosis.keptPath = m_diskStore.GetSourcePath (6, drive);
    }

    RestoreDoor (drive);

    (void) PublishDriveStatus (true);

    if (m_onMountCompleted)
    {
        m_onMountCompleted (drive, path, HRESULT_FROM_WIN32 (ERROR_INVALID_STATE), diagnosis);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EjectDiskInSlot6
//
//  Auto-flushes dirty bits via the store, which announces the eject through
//  its bay-change sink -- so detaching the controller, the debug event, the
//  door, its sound and the saved disk path all happen in OnBayChange, not
//  here. After a declined eject, or one waiting for an answer, the disk is in
//  the drive, and the door closes again.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::EjectDiskInSlot6 (int drive)
{
    bool  isLive = !m_diskStore.IsFlushHeld() && !m_diskStore.IsReplaying();



    //  A command that arrives on this thread behind live changes nothing. The
    //  eject's save runs on the store's I/O queue, and the door closes again
    //  from its completion when the disk is in the drive after it.
    if (isLive)
    {
        m_diskStore.Eject (6, drive, [this, drive] (bool isEjected)
        {
            if (!isEjected)
            {
                RestoreDoor (drive);
            }
        });
    }
    else
    {
        EhmNotifyUser (DiskStoreText::FormatBehindLiveEjectMessage (drive).c_str());

        if (m_diskStore.IsMounted (6, drive))
        {
            RestoreDoor (drive);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreDoor
//
//  Publishes DoorRestore for a drive whose disk is in its bay. A drive with no
//  disk rests with its door open, as a real Disk II does.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::RestoreDoor (int drive)
{
    if (m_diskStore.IsMounted (6, drive))
    {
        m_driveWidgets.PublishSyncEvent (drive, DriveWidgetController::SyncAction::DoorRestore, GetNowMs());
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
    //  This sink replaces MachineHost's, so it makes the same call.
    m_machine.ConnectDiskToController (slot, drive);

    //  Re-assert the effective write protection on a disk that just arrived:
    //  the user's per-drive preference plus the backing file's read-only state.
    if (inserted && image != nullptr)
    {
        ApplyExternalWriteProtect (drive);
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

    //  The disk left, by a user eject, an eject an answer finished or a file
    //  that vanished: the next launch comes up with this drive empty.
    if (!inserted && !m_currentMachineName.empty())
    {
        HRESULT  hrReg = DiskSettings::WriteSavedDiskPath (m_userConfigStore, m_fileSystem,
                                                           drive, m_currentMachineName, L"");
        IGNORE_RETURN_VALUE (hrReg, S_OK);
    }

    PlayBayChangeEffects (drive, change, suppressFx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManager::PlayBayChangeEffects
//
//  The door and the sound of a bay change. The visual door for a plain insert
//  or eject rides the source-path poll in UpdateDriveWidgets, which sees the
//  file in the bay change; a swap keeps the same file, so its open-then-close
//  door is published here as the event the poll cannot infer.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::PlayBayChangeEffects (int drive, BayChange change, bool suppressFx)
{
    int64_t  nowMs = 0;



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
//  Mounts every slot-6 disk again from its file, so a file regenerated
//  outside the emulator (a developer iterating on a demo image) is what
//  the next boot reads. Used by the Reset and Power Cycle menu commands.
//  Each remount is a mount of the file in the bay: the disk is saved first
//  and the file is read after, and when the file is as the bay last wrote
//  it, the disk stays and nothing is read. A disk whose save fails is in
//  the drive afterward, with its writes.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::RemountSlot6Disks()
{
    std::string  savedDisk[DiskImageStore::kDriveCount];
    HRESULT      hrMount = S_OK;
    int          drive   = 0;



    // A remount flushes the disk and reads its file back. While reverse
    // execution holds the disks, a reset and a power cycle write nothing and
    // keep the disks as the machine has them.
    if (m_diskStore.IsFlushHeld())
    {
        return;
    }

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
        m_driveWidgetState[drive].isEjectPosted = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDriveWidgets
//
//  Per-UI-frame sync into the (UI-thread-only) DriveWidgetState from the
//  newest published drive status, which GetShownDriveStatus then returns for
//  the rest of the frame: the door from the bay, the lamp and head from the
//  drive's activity, and the padlock and the WOZ info icon from the bay. It
//  reads no store, image or controller; only the CPU thread may access those
//  while it runs.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::UpdateDriveWidgets()
{
    int64_t                                               nowMs        = GetNowMs();
    std::vector<DriveWidgetController::DriveSyncEvent>    syncEvents   = m_driveWidgets.ConsumeSyncEvents();
    WozMachineFacts                                       machineFacts = WozCompatibility::GetMachineFacts (m_machine.GetConfig());
    bool                                                  isBaysMoved  = false;
    int                                                   drive        = 0;



    m_shownStatus = m_statusPublisher.Take();
    isBaysMoved   = m_shownStatus->GetBayGeneration() != m_shownBayGeneration;

    for (drive = 0; drive < static_cast<int> (m_driveWidgetState.size()); drive++)
    {
        DriveWidgetState  & st  = m_driveWidgetState[drive];
        const BayStatus   & bay = m_shownStatus->GetBay (6, drive);

        ApplySyncEvents   (st, drive, syncEvents, nowMs);
        SyncDoorFromBay   (st, drive, bay, nowMs);
        SyncDriveActivity (st, drive, m_shownStatus->GetActivity (drive), m_shownStatus->HasController(), nowMs);

        // The padlock cue + hover tooltip, and what the image declares about
        // the machine for the info icon, copied when a bay changed. Empty
        // drive -> no protection, nothing declared.
        if (isBaysMoved)
        {
            st.writeProtect    = bay.writeProtect;
            st.wozRequirements = bay.wozRequirements;
        }

        st.wozConflict     = WozCompatibility::HasConflict (st.wozRequirements, machineFacts);
        st.wozMachine      = machineFacts;

        // Copied only when it changes, which is only on a machine switch.
        if (st.wozMachineName != m_machine.GetConfig().name)
        {
            st.wozMachineName = m_machine.GetConfig().name;
        }
    }

    m_shownBayGeneration = m_shownStatus->GetBayGeneration();

    m_driveWidgets.SyncFromStates (m_driveWidgetState);
    m_driveChrome[0].SyncFromState (m_driveWidgetState[0]);
    m_driveChrome[1].SyncFromState (m_driveWidgetState[1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsReplacePromptNeeded
//
//  Whether inserting into a drive replaces a disk, from the window frame's
//  drive status: a disk is in the bay, and no eject the window posted is
//  waiting for the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskManager::IsReplacePromptNeeded (int drive) const
{
    bool  isValid   = drive >= 0 && drive < static_cast<int> (m_driveWidgetState.size());
    bool  isMounted = m_shownStatus->GetBay (6, drive).isMounted;



    return isValid && isMounted && !m_driveWidgetState[drive].isEjectPosted;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplySyncEvents
//
//  The door transitions that come from an event rather than from the bay's
//  path. After a swap the bay has the same file, so the path diff finds no
//  change, and OnBayChange publishes a DoorReinsert instead: open, then
//  close. After a declined insert or eject the bay has the same file too, and
//  DoorRestore closes the door the window opened. It also clears the window's
//  posted eject, so a later insert into the drive raises the replace prompt.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::ApplySyncEvents (
    DriveWidgetState                                            & st,
    int                                                           drive,
    const std::vector<DriveWidgetController::DriveSyncEvent>    & events,
    int64_t                                                       nowMs)
{
    for (const auto & evt : events)
    {
        if (evt.driveId == drive)
        {
            st.lastSyncEventId = evt.eventId;

            if (evt.action == DriveWidgetController::SyncAction::DoorReinsert)
            {
                st.BeginReinsert (nowMs);
            }
            else if (evt.action == DriveWidgetController::SyncAction::DoorRestore)
            {
                st.isEjectPosted = false;

                if (st.IsMounted())
                {
                    st.StartDoorTransition (DriveWidgetState::Door::Closing, nowMs);
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncDoorFromBay
//
//  The door follows the bay's path: an insert when a path appears or
//  changes, an eject when it goes.
//
//  THE NARROW PATH IS DIFFED FIRST, so the wide conversion runs only on the
//  frame a disk actually goes in or out, not on every one of the ~60 frames a
//  second the widgets are synced. The wide path is decoded through fs::path,
//  the inverse of the narrowing the mount used: a manual wstring (begin, end)
//  widen would sign-extend a high byte like 0xF8 into U+FFF8 and render as a
//  tofu box in the drive label.
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::SyncDoorFromBay (DriveWidgetState & st, int drive, const BayStatus & bay, int64_t nowMs)
{
    std::wstring  wPath;



    if (bay.path != m_lastDriveSourcePath[drive])
    {
        m_lastDriveSourcePath[drive] = bay.path;
        st.isEjectPosted             = false;

        wPath = fs::path (bay.path).wstring();

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
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncDriveActivity
//
//  The lamp and the head: the motor as the drive reports it, and the drive
//  active when its nibble counters moved since the previous frame (FR-025
//  active LED).
//
////////////////////////////////////////////////////////////////////////////////

void DiskManager::SyncDriveActivity (
    DriveWidgetState     & st,
    int                    drive,
    const DriveActivity  & activity,
    bool                   hasController,
    int64_t                nowMs)
{
    bool  isActive = hasController &&
                     (activity.readNibbles  != m_lastReadNibbles[drive] ||
                      activity.writeNibbles != m_lastWriteNibbles[drive]);



    m_lastReadNibbles[drive]  = activity.readNibbles;
    m_lastWriteNibbles[drive] = activity.writeNibbles;

    st.motorOn.store          (activity.isMotorOn,        std::memory_order_relaxed);
    st.diskActive.store       (isActive,                  std::memory_order_relaxed);
    st.headQuarterTrack.store (activity.headQuarterTrack, std::memory_order_relaxed);

    if (activity.isMotorOn || isActive)
    {
        st.lastActiveMs = nowMs;
    }
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
//  Sets the watcher, the file probe and the I/O queue on the store.
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

    //  The store's file work runs on a separate thread, and its results are
    //  applied on the thread with the store's token, which drains them from the
    //  CPU thread's service pass.
    m_ioQueue = std::make_unique<ThreadedDiskIoQueue> ();

    m_diskStore.SetImageWatcher (m_imageWatcher.get());
    m_diskStore.SetFileIo       (m_imageFileIo.get());
    m_diskStore.SetIoQueue      (m_ioQueue.get());

    //  Every path that moves a disk ends at OnBayChange, so the door, its
    //  sounds and the debug event are lit from one place. Set here, before the
    //  command-line disks mount, so a cold-boot mount reaches the same handler
    //  (which suppresses its launch-time sound).
    m_diskStore.SetBayChangeSink ([this] (int slot, int drive, BayChange change)
    {
        OnBayChange (slot, drive, change);
    });
}
