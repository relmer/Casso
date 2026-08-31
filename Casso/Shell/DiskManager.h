#pragma once

#include "Pch.h"

#include "Devices/Disk/MountDiagnosis.h"
#include "Devices/Disk/IImageWatcher.h"
#include "Devices/Disk/IDiskFileIo.h"


class CpuManager;
class Disk2AudioSource;
class Disk2Controller;
class DiskImage;
class DiskImageStore;
class DriveWidget;
class DriveWidgetController;
struct DriveWidgetState;
class MemoryDevice;
class WasapiAudio;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskManager
//
//  Owner of the slot-6 Disk II glue: scans the shell's owned-device
//  list for the active controller, mounts/ejects through the
//  DiskImageStore (which auto-flushes dirty images on Eject /
//  SwitchMachine / PowerCycle / Shutdown), persists the per-machine
//  last-mounted disk path, fires the drive-audio door FX, and pumps
//  the per-frame DriveWidget sync from the CPU-thread controller
//  state into the UI-thread chrome.
//
//  Holds back-references to every shared collection it operates on
//  (owned-device vector, audio sources, chrome widgets, image store)
//  plus the CpuManager (for IDriveCommandSink-routed PostCommand into
//  the CPU-thread mount/eject path) and the current machine name
//  (for DiskSettings registry keys). No new global state is added.
//
////////////////////////////////////////////////////////////////////////////////

class UserConfigStore;
class IFileSystem;


class DiskManager
{
public:
    DiskManager (std::vector<std::unique_ptr<MemoryDevice>>      & ownedDevices,
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
                 std::array<bool, 2>                             & userWriteProtect);

    Disk2Controller *   FindSlot6Controller    ();
    bool                HasSlot6Controller     () { return FindSlot6Controller() != nullptr; }

    HRESULT  MountDiskInSlot6       (int drive, const std::string & path);

    //  Called after any attempted mount, whatever started it: the command
    //  line, the picker, a machine switch, session restore. Every mount funnels
    //  through MountDiskInSlot6, so hooking it here is what makes "report a
    //  damaged image" cover all of them rather than the one path someone
    //  remembered. DiskManager owns no UI, so the shell supplies the reaction.
    //
    //  The mount's own HRESULT rides along, because the mount runs on the CPU
    //  thread and this callback is the only way its outcome gets back to the
    //  shell that asked for it. Fires on failure too: a mount that could not
    //  happen is exactly what the user needs told about.
    //
    //  The diagnosis rides with it for the same reason and one more: by the
    //  time the shell has the outcome, the bytes and the loader that judged
    //  them are gone, so a reason not carried here is a reason nothing can
    //  reconstruct.
    void     SetMountCompletedCallback (
                 std::function<void (int, const std::string &, HRESULT, const MountDiagnosis &)> cb)
    {
        m_onMountCompleted = std::move (cb);
    }

    //  Builds the platform pieces a shared image needs and hands them to the
    //  image store: the directory watcher, and the file probe that answers
    //  whether something else is writing the image right now.
    //
    //  HANDING IT OVER IS ALL THIS DOES. Which directory to watch, when to
    //  take a watch up and when to drop it are orchestration, and orchestration
    //  is the store's, where a fake watcher can assert it. Everything here is
    //  the choice of implementation.
    //
    //  `disabled` INSTALLS A WATCHER THAT REFUSES EVERY WATCH rather than
    //  installing none. That is the measurement seam: the emulator runs with
    //  notification broken exactly as an unwatchable share leaves it, and the
    //  check made before every write is what has to carry the guarantee.
    void     InstallSharedImageSupport (bool watchDisabled);

    void     EjectDiskInSlot6       (int drive);
    void     RemountSlot6Disks      ();
    void     MountCommandLineDisks  (const std::string & disk1Path,
                                     const std::string & disk2Path);

    // IDriveCommandSink-style entry points routed through the CPU
    // command queue so the actual mount/eject runs on the CPU thread.
    HRESULT  Mount (int slot, int drive, const std::wstring & path);
    void     Eject (int slot, int drive);

    void     UpdateDriveWidgets ();

    // Cold-boot mount window (FR-013): suppress door-close FX while
    // command-line / autoload mounts are running at app launch.
    void     SetColdBootMountWindow (bool value) noexcept { m_coldBootMountWindow = value; }

    // Steady-clock millisecond timestamp used by the drive-widget door
    // animation FSM and the audio drive-door sync recorder.
    static int64_t  GetNowMs ();

    // Applies the effective external write-protect state to a freshly
    // mounted image: the user's per-drive preference plus the backing
    // file's read-only / no-write-permission status (probed from the
    // host filesystem). The image's own embedded flag is left untouched.
    void     ApplyExternalWriteProtect (int drive, DiskImage * image, const std::string & path);

    // Flips the mounted image's own write-protection: the WOZ INFO flag
    // (flushed so it travels with the file) or, for sector-image formats
    // with no in-image flag, the backing file's read-only attribute.
    // Pending guest writes are persisted BEFORE protecting -- a protected
    // image drops dirty content at flush. Ends by re-probing the file and
    // re-applying the external state, so every indicator reflects reality
    // whether the change stuck or failed.
    HRESULT  ToggleImageWriteProtect (int drive);

    std::function<void (int, const std::string &, HRESULT, const MountDiagnosis &)>  m_onMountCompleted;

    // Probes whether the host file at `path` can be written back. Sets
    // outReadOnly when the file carries the read-only attribute and
    // outNoPermission when it cannot be opened for writing for another
    // reason (ACL denial, exclusive lock). A missing / empty path is
    // treated as writable (nothing to protect).
    static void  ProbeFileWritability (const std::string & path,
                                       bool              & outReadOnly,
                                       bool              & outNoPermission);

private:
    std::vector<std::unique_ptr<MemoryDevice>>      & m_ownedDevices;
    DiskImageStore                                  & m_diskStore;
    std::vector<std::unique_ptr<Disk2AudioSource>>  & m_diskAudioSources;
    WasapiAudio                                     & m_wasapiAudio;
    DriveWidgetController                           & m_driveWidgets;
    std::array<DriveWidgetState, 2>                 & m_driveWidgetState;
    std::array<DriveWidget, 2>                      & m_driveChrome;
    CpuManager                                      & m_cpuManager;
    const std::wstring                              & m_currentMachineName;
    UserConfigStore                                 & m_userConfigStore;
    IFileSystem                                     & m_fileSystem;
    std::array<bool, 2>                             & m_userWriteProtect;

    //  Both outlive every mount, because the store holds bare pointers to them
    //  and a watch may be taken up at any mount.
    std::unique_ptr<IImageWatcher>  m_imageWatcher;
    std::unique_ptr<IDiskFileIo>    m_imageFileIo;

    std::array<uint64_t, 2>  m_lastReadNibbles      {};
    std::array<uint64_t, 2>  m_lastWriteNibbles     {};
    bool                     m_coldBootMountWindow  = true;
    bool                     m_programmaticRemount  = false;
};
