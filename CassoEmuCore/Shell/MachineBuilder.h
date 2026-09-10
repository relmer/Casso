#pragma once

#include "Pch.h"

#include "Shell/MachineHost.h"


class DriveAudioMixer;
class Disk2AudioSource;
class JsonValue;
class PrinterAudioSource;
class PrinterWorker;
class WasapiAudio;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineBuildServices
//
//  What machine construction needs from the emulator around it.
//
//  A Disk ][ still reads sectors with nothing listening to its head, and a
//  Mockingboard still answers its registers with no mixer attached -- but
//  the emulator wants both connected the moment the device exists, and the
//  connection has to happen inside the build, between creating the device
//  and losing track of which one it was. So these come in as references
//  rather than being reached for through a back-pointer to the shell.
//
//  The three drive-audio volumes and the per-drive pan are references
//  because the user can move them while a machine is running; the builder
//  reads them when it seeds a fresh set of drive audio sources.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineBuildServices
{
    std::vector<std::unique_ptr<Disk2AudioSource>>  &  diskAudioSources;
    DriveAudioMixer                                 &  driveAudioMixer;
    DriveAudioMixer                                 &  mockingboardAudioMixer;
    PrinterAudioSource                              &  printerAudio;
    WasapiAudio                                     &  wasapiAudio;

    PrinterWorker  &  printerWorker;
    uint64_t       &  printerAutoOpenActivity;

    const float (&  drivePan)[2];
    const float  &  driveMotorVolume;
    const float  &  driveHeadVolume;
    const float  &  driveDoorVolume;

    //  Command-line options that reach machine construction: the CPU trace
    //  ring size (0 = off) and whether mounted images are watched for
    //  changes on disk.
    const size_t  &  traceCapacity;
    const bool    &  imageWatchDisabled;

    //  Restarting the machine after the image store picks up a file that
    //  changed on disk. The store decides; the emulator acts. Held by value
    //  because the builder hands it to the store, which calls it long after
    //  the build has returned.
    std::function<void()>  requestPowerCycle;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineBuilder
//
//  Turns a MachineConfig into a working machine on a MachineHost: every
//  MemoryDevice the config calls for, the language-card and //c ROM-bank
//  splits over the same address range, the page table the MMU banks
//  through, the five video-mode renderers, and the CPU.
//
//  It is separate from the host for the reason DiskManager is separate from
//  the disk: the wiring is long, it is specific to the Apple II, and it
//  changes on a different schedule than the state it produces. It is
//  separate from MachineManager because building a machine and switching to
//  a machine are different jobs -- the second saves the user's disks,
//  re-titles the window and re-points the debug panels, and none of that is
//  construction.
//
//  Also owns the per-frame video-mode selection, which is construction read
//  backwards: it picks among the renderers this class made, from the soft
//  switches, and is the only writer of the host's latched mirror.
//
////////////////////////////////////////////////////////////////////////////////

class MachineBuilder
{
public:
    MachineBuilder (MachineHost & host, const MachineBuildServices & services);

    HRESULT  CreateMemoryDevices  (const MachineConfig & config);
    void     WireLanguageCard     ();
    void     WireApple2cRomBank   ();
    static HRESULT ReadRomFileBytes (const std::string & path, std::vector<Byte> & out);
    void     WirePageTable        ();
    void     RebuildBankingPages  ();
    void     CreateVideoModes     ();
    HRESULT  CreateCpu            (const MachineConfig & config);

    Byte *   GetAuxRamBuffer      ();

    void     SelectVideoMode      ();

private:

    MachineHost          &  m_host;
    MachineBuildServices    m_services;
};
