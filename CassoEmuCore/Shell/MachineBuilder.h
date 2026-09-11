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
//  What machine construction connects the machine TO.
//
//  A Disk ][ still reads sectors with nothing listening to its head, and a
//  Mockingboard still answers its registers with no mixer attached -- but
//  when the emulator is running, both want connecting the moment the device
//  exists, while the build still knows which device it just made. So they
//  come in here rather than being reached for through a back-pointer to the
//  shell.
//
//  Every one of them is optional, and a default-constructed instance leaves
//  all of them out. That is what a machine built inside a test is: the same
//  devices, wired the same way, with nothing on the other end of the audio
//  and no printer thread -- which is the difference between running a real
//  machine in a test and maintaining a second copy of this wiring that
//  drifts from the first.
//
//  The pan and the three drive-audio volumes are pointers rather than values
//  because the user can move them while a machine is running, and the two
//  command-line options because the builder is constructed before the
//  command line has been parsed.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineBuildServices
{
    std::vector<std::unique_ptr<Disk2AudioSource>>  *  diskAudioSources       = nullptr;
    DriveAudioMixer                                 *  driveAudioMixer        = nullptr;
    DriveAudioMixer                                 *  mockingboardAudioMixer = nullptr;
    PrinterAudioSource                              *  printerAudio           = nullptr;
    WasapiAudio                                     *  wasapiAudio            = nullptr;

    PrinterWorker  *  printerWorker           = nullptr;
    uint64_t       *  printerAutoOpenActivity = nullptr;

    const float  *  drivePan          = nullptr;    // two entries
    const float  *  driveMotorVolume  = nullptr;
    const float  *  driveHeadVolume   = nullptr;
    const float  *  driveDoorVolume   = nullptr;

    //  Command-line options that reach machine construction: the CPU trace
    //  ring size (0 = off) and whether mounted images are watched for
    //  changes on disk.
    const size_t  *  traceCapacity      = nullptr;
    const bool    *  imageWatchDisabled = nullptr;

    //  Restarting the machine after the image store picks up a file that
    //  changed on disk. The store decides; the emulator acts. Held by value
    //  because the builder hands it to the store, which calls it long after
    //  the build has returned.
    std::function<void()>  requestPowerCycle;

    size_t  GetTraceCapacity      () const { return traceCapacity != nullptr ? *traceCapacity : 0; }
    bool    IsImageWatchDisabled  () const { return imageWatchDisabled != nullptr && *imageWatchDisabled; }
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

    //  The whole build: devices, the language-card and //c ROM-bank splits,
    //  the video-mode renderers, the CPU, and the page table the MMU banks
    //  through. Both the initial launch and a machine switch run this, and
    //  they used to run separate copies of it that drifted.
    HRESULT  Build                (const MachineConfig & config);

    HRESULT  CreateMemoryDevices  (const MachineConfig & config);
    void     WireLanguageCard     ();
    void     WireBankedRom        ();
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
