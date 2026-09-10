#pragma once

#include "Pch.h"

#include "Core/ComponentRegistry.h"
#include "Core/EmuCpu.h"
#include "Core/InterruptController.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/IAciaEndpoint.h"
#include "Machines/Apple2/Common/CharacterRomData.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Shell/MachineRefs.h"
#include "Video/VideoOutput.h"


class Apple2cRomBank;
class Apple2eMmu;
class AppleMouse;
class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  SoftSwitchMirror
//
//  The video-relevant soft switches, latched once per frame.
//
//  The renderer reads these rather than the soft-switch bank itself so that
//  one frame is drawn from one consistent set of switches: the guest is free
//  to flip HIRES or PAGE2 partway through the frame the CPU thread is about
//  to hand over, and a renderer that re-read the live bank per scanline
//  would draw half of each. SelectVideoMode latches them between the CPU
//  slice and the render, which is the only point at which they are written.
//
////////////////////////////////////////////////////////////////////////////////

struct SoftSwitchMirror
{
    bool  graphicsMode = false;
    bool  mixedMode    = false;
    bool  page2        = false;
    bool  hiresMode    = false;
    bool  col80Mode    = false;
    bool  doubleHiRes  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHost
//
//  The emulated machine: the bus every device hangs off, the CPU driving it,
//  the devices themselves, the video modes that read their memory, and the
//  configuration and identity that say which machine this is.
//
//  It is a machine, not a window. Nothing here needs an HWND, a swap chain,
//  an audio endpoint or a message pump, and nothing here reads one -- which
//  is what lets a test build a machine, run it and assert on it. Everything
//  the emulator wraps AROUND a machine (the window, the chrome, the audio
//  device, the debug panels, the user's preferences) stays on EmulatorShell
//  and reaches the machine through this class.
//
//  The host owns state and hands it out; it does not build itself.
//  MachineBuilder constructs the devices and wires them, for the same
//  reason DiskManager is not the disk: the wiring is long, it is specific
//  to the Apple II, and it changes on its own schedule.
//
////////////////////////////////////////////////////////////////////////////////

class MachineHost
{
public:
    MachineHost  ();
    ~MachineHost ();

    MachineHost                (const MachineHost &) = delete;
    MachineHost & operator=    (const MachineHost &) = delete;

    //  The bus and the fixed plumbing every machine has. These are values,
    //  not pointers: a machine without a bus is not a state this class can
    //  be in.
    MemoryBus            &  GetMemoryBus           () noexcept { return m_memoryBus; }
    ComponentRegistry    &  GetRegistry            () noexcept { return m_registry; }
    InterruptController  &  GetInterruptController () noexcept { return m_interruptController; }

    const MemoryBus            &  GetMemoryBus           () const noexcept { return m_memoryBus; }
    const ComponentRegistry    &  GetRegistry            () const noexcept { return m_registry; }
    const InterruptController  &  GetInterruptController () const noexcept { return m_interruptController; }

    //  The parts a machine may or may not have, and that are rebuilt from
    //  scratch on every machine switch. Null is a real answer for all but
    //  the CPU and the Prng: a ][ has no MMU, only a //c has the firmware
    //  bank and the IOU mouse.
    EmuCpu          *  GetCpu             () noexcept { return m_cpu.get(); }
    Prng            *  GetPrng            () noexcept { return m_prng.get(); }
    Apple2eMmu      *  GetMmu             () noexcept { return m_mmu.get(); }
    Apple2cRomBank  *  GetApple2cRomBank  () noexcept { return m_apple2cRomBank.get(); }
    AppleMouse      *  GetMouse           () noexcept { return m_mouse.get(); }
    VideoTiming     *  GetVideoTiming     () noexcept { return m_videoTiming.get(); }

    const EmuCpu          *  GetCpu            () const noexcept { return m_cpu.get(); }
    const Prng            *  GetPrng           () const noexcept { return m_prng.get(); }
    const Apple2eMmu      *  GetMmu            () const noexcept { return m_mmu.get(); }
    const Apple2cRomBank  *  GetApple2cRomBank () const noexcept { return m_apple2cRomBank.get(); }
    const AppleMouse      *  GetMouse          () const noexcept { return m_mouse.get(); }
    const VideoTiming     *  GetVideoTiming    () const noexcept { return m_videoTiming.get(); }

    void  SetCpu            (std::unique_ptr<EmuCpu>          cpu);
    void  SetPrng           (std::unique_ptr<Prng>            prng);
    void  SetMmu            (std::unique_ptr<Apple2eMmu>      mmu);
    void  SetApple2cRomBank (std::unique_ptr<Apple2cRomBank>  romBank);
    void  SetMouse          (std::unique_ptr<AppleMouse>      mouse);
    void  SetVideoTiming    (std::unique_ptr<VideoTiming>     videoTiming);

    //  Everything the machine owns and destroys as a unit. The ACIA
    //  endpoints are held apart from the devices because an IAciaEndpoint
    //  is not a MemoryDevice.
    std::vector<std::unique_ptr<MemoryDevice>>   &  GetOwnedDevices       () noexcept { return m_ownedDevices; }
    std::vector<std::unique_ptr<IAciaEndpoint>>  &  GetOwnedAciaEndpoints () noexcept { return m_ownedAciaEndpoints; }
    std::vector<std::unique_ptr<VideoOutput>>    &  GetVideoModes         () noexcept { return m_videoModes; }

    const std::vector<std::unique_ptr<MemoryDevice>>   &  GetOwnedDevices       () const noexcept { return m_ownedDevices; }
    const std::vector<std::unique_ptr<IAciaEndpoint>>  &  GetOwnedAciaEndpoints () const noexcept { return m_ownedAciaEndpoints; }
    const std::vector<std::unique_ptr<VideoOutput>>    &  GetVideoModes         () const noexcept { return m_videoModes; }

    CharacterRomData  &  GetCharacterRom () noexcept { return m_charRom; }
    DiskImageStore    &  GetDiskStore    () noexcept { return m_diskStore; }
    MachineConfig     &  GetConfig       () noexcept { return m_config; }

    const CharacterRomData  &  GetCharacterRom () const noexcept { return m_charRom; }
    const DiskImageStore    &  GetDiskStore    () const noexcept { return m_diskStore; }
    const MachineConfig     &  GetConfig       () const noexcept { return m_config; }

    //  Raw pointers into the two collections above, reset whenever either is
    //  rebuilt. See MachineRefs.
    MachineRefs  &  GetRefs () noexcept { return m_refs; }

    const MachineRefs  &  GetRefs () const noexcept { return m_refs; }

    SoftSwitchMirror  &  GetSoftSwitchMirror () noexcept { return m_softSwitches; }

    const SoftSwitchMirror  &  GetSoftSwitchMirror () const noexcept { return m_softSwitches; }

    //  Which machine this is: the directory name the config was loaded from
    //  ("apple2e", "apple2c"), and the directory its ROMs and other assets
    //  are read from. The name doubles as the per-machine suffix on user
    //  state, so one machine's last-mounted disks cannot clobber another's.
    const std::wstring  &  GetCurrentMachineName () const noexcept { return m_currentMachineName; }
    const std::wstring  &  GetAssetBaseDir       () const noexcept { return m_assetBaseDir; }

    //  Where this machine's pending printer strip persists across a switch
    //  or a shutdown: <assetBase>/Machines/<machine>/PendingPrint.
    std::filesystem::path  GetPendingPrintDir () const;

    void  SetCurrentMachineName (const std::wstring & name) { m_currentMachineName = name; }
    void  SetAssetBaseDir       (const std::wstring & dir)  { m_assetBaseDir = dir; }

private:

    MemoryBus            m_memoryBus;
    ComponentRegistry    m_registry;
    InterruptController  m_interruptController;

    std::unique_ptr<EmuCpu>  m_cpu;
    std::unique_ptr<Prng>    m_prng;

    std::vector<std::unique_ptr<MemoryDevice>>   m_ownedDevices;
    std::vector<std::unique_ptr<IAciaEndpoint>>  m_ownedAciaEndpoints;
    std::vector<std::unique_ptr<VideoOutput>>    m_videoModes;

    CharacterRomData  m_charRom;

    SoftSwitchMirror  m_softSwitches;

    MachineRefs  m_refs;

    std::unique_ptr<Apple2eMmu>      m_mmu;
    std::unique_ptr<Apple2cRomBank>  m_apple2cRomBank;
    std::unique_ptr<AppleMouse>      m_mouse;
    std::unique_ptr<VideoTiming>     m_videoTiming;

    DiskImageStore  m_diskStore;
    MachineConfig   m_config;

    std::wstring  m_currentMachineName;
    std::wstring  m_assetBaseDir;
};
