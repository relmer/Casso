#pragma once

#include "Pch.h"


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
    static int64_t  NowMs ();

    // Applies the effective external write-protect state to a freshly
    // mounted image: the user's per-drive preference plus the backing
    // file's read-only / no-write-permission status (probed from the
    // host filesystem). The image's own embedded flag is left untouched.
    void     ApplyExternalWriteProtect (int drive, DiskImage * image, const std::string & path);

    // Probes whether the host file at `path` can be written back. Sets
    // outReadOnly when the file carries the read-only attribute and
    // outNoPermission when it cannot be opened for writing for another
    // reason (ACL denial, exclusive lock). A missing / empty path is
    // treated as writable (nothing to protect).
    static void  ProbeFileWritability (const std::string & path,
                                       bool              & outReadOnly,
                                       bool              & outNoPermission);

private:
    std::vector<std::unique_ptr<MemoryDevice>>       & m_ownedDevices;
    DiskImageStore                                   & m_diskStore;
    std::vector<std::unique_ptr<Disk2AudioSource>>  & m_diskAudioSources;
    WasapiAudio                                      & m_wasapiAudio;
    DriveWidgetController                            & m_driveWidgets;
    std::array<DriveWidgetState, 2>                  & m_driveWidgetState;
    std::array<DriveWidget, 2>                       & m_driveChrome;
    CpuManager                                       & m_cpuManager;
    const std::wstring                               & m_currentMachineName;
    UserConfigStore                                  & m_userConfigStore;
    IFileSystem                                      & m_fileSystem;
    std::array<bool, 2>                              & m_userWriteProtect;

    std::array<uint64_t, 2>  m_lastReadNibbles      {};
    std::array<uint64_t, 2>  m_lastWriteNibbles     {};
    bool                     m_coldBootMountWindow  = true;
    bool                     m_programmaticRemount  = false;
};
