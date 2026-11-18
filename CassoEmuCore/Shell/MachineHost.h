#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Printer/PrinterByteRing.h"
#include "Shell/MachineClockPanel.h"
#include "Shell/MachineConnectors.h"
#include "Shell/MachineHostInput.h"
#include "Shell/MachineObserverWiring.h"
#include "Shell/MachineOutputMute.h"
#include "Shell/MachineParts.h"
#include "Shell/MachineRomIdentity.h"
#include "Shell/MachineRunner.h"
#include "Shell/MachineStateStream.h"
#include "Shell/MachineStorage.h"
#include "Shell/MachineTape.h"

class Apple2cRomBank;
class Apple2eMmu;
class Apple3System;
class AppleMouse;
class CharacterRomData;
class HeldInputWatch;
class IAciaEndpoint;
class Prng;
class SiriusJoyport;





////////////////////////////////////////////////////////////////////////////////
//
//  SoftSwitchMirror
//
//  The video-relevant soft switches, latched once per frame.
//
//  The renderer reads these rather than the soft-switch bank itself so that
//  one frame is drawn from one consistent set of switches: the guest is free
//  to flip HIRES or PAGE2 partway through the frame the CPU thread is about
//  to pass to the renderer, and a renderer that re-read the live bank per
//  scanline would draw half of each. SelectVideoMode latches them between the CPU
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
//  The emulated machine: its parts, the configuration and identity of which
//  machine it is, and the components that run it, take the host's input,
//  attach its storage, mute its output, wire its observers, drive its tape
//  and publish its clock panel.
//
//  It is a machine, not a window. Nothing here uses an HWND, a swap chain,
//  an audio endpoint or a message pump, so a test can build a machine, run
//  it and assert on it. Everything the emulator wraps around a machine (the
//  window, the chrome, the audio device, the debug panels, the user's
//  preferences) is on EmulatorShell and accesses the machine through this
//  class.
//
//  MachineBuilder constructs the devices and wires them: the wiring is long,
//  it is specific to the Apple II, and it changes on a separate schedule.
//
////////////////////////////////////////////////////////////////////////////////

class MachineHost : private IMachineStateParts
{
public:
    MachineHost  ();
    ~MachineHost () override;

    MachineHost             (const MachineHost &) = delete;
    MachineHost & operator= (const MachineHost &) = delete;

    //  The bus and the fixed plumbing every machine has. Always present.
    MemoryBus                  &  GetMemoryBus           () noexcept       { return *m_parts.memoryBus; }
    ComponentRegistry          &  GetRegistry            () noexcept       { return m_parts.registry; }
    InterruptController        &  GetInterruptController () noexcept       { return m_parts.interruptController; }
    const MemoryBus            &  GetMemoryBus           () const noexcept { return *m_parts.memoryBus; }
    const ComponentRegistry    &  GetRegistry            () const noexcept { return m_parts.registry; }
    const InterruptController  &  GetInterruptController () const noexcept { return m_parts.interruptController; }

    //  The parts a machine may or may not have, rebuilt on every machine
    //  switch. Null is a real answer for all but the CPU and the Prng.
    EmuCpu                  * GetCpu            () noexcept { return m_parts.cpu.get(); }
    Prng                    * GetPrng           () noexcept { return m_parts.prng.get(); }
    Apple2eMmu              * GetMmu            () noexcept { return m_parts.mmu.get(); }
    Apple2cRomBank          * GetApple2cRomBank () noexcept { return m_parts.apple2cRomBank.get(); }
    AppleMouse              * GetMouse          () noexcept { return m_parts.mouse.get(); }
    Apple2cPlusAccelerator  * GetAccelerator    () noexcept { return m_parts.accelerator.get(); }
    SiriusJoyport           * GetJoyport        () noexcept { return m_parts.joyport.get(); }
    VideoTiming             * GetVideoTiming    () noexcept { return m_parts.videoTiming.get(); }
    Apple3System            * GetApple3         () noexcept { return m_parts.apple3.get(); }

    const EmuCpu          *  GetCpu            () const noexcept { return m_parts.cpu.get(); }
    const Prng            *  GetPrng           () const noexcept { return m_parts.prng.get(); }
    const Apple2eMmu      *  GetMmu            () const noexcept { return m_parts.mmu.get(); }
    const Apple2cRomBank  *  GetApple2cRomBank () const noexcept { return m_parts.apple2cRomBank.get(); }
    const AppleMouse      *  GetMouse          () const noexcept { return m_parts.mouse.get(); }
    const SiriusJoyport   *  GetJoyport        () const noexcept { return m_parts.joyport.get(); }
    const VideoTiming     *  GetVideoTiming    () const noexcept { return m_parts.videoTiming.get(); }

    void  SetCpu            (std::unique_ptr<EmuCpu>                 cpu);
    void  SetPrng           (std::unique_ptr<Prng>                   prng);
    void  SetMmu            (std::unique_ptr<Apple2eMmu>             mmu);
    void  SetApple2cRomBank (std::unique_ptr<Apple2cRomBank>         romBank);
    void  SetMouse          (std::unique_ptr<AppleMouse>             mouse);
    void  SetAccelerator    (std::unique_ptr<Apple2cPlusAccelerator> accelerator);
    void  SetJoyport        (std::unique_ptr<SiriusJoyport>          joyport);
    void  SetVideoTiming    (std::unique_ptr<VideoTiming>            videoTiming);
    void  SetApple3         (std::unique_ptr<Apple3System>           apple3);

    VideoScanner        &  GetVideoScanner()       noexcept { return m_parts.videoScanner; }
    const VideoScanner  &  GetVideoScanner() const noexcept { return m_parts.videoScanner; }

    //  Everything the machine owns and destroys as a unit.
    std::vector<std::unique_ptr<MemoryDevice>>         &  GetOwnedDevices       ()       noexcept { return m_parts.ownedDevices; }
    std::vector<std::unique_ptr<IAciaEndpoint>>        &  GetOwnedAciaEndpoints ()       noexcept { return m_parts.ownedAciaEndpoints; }
    std::vector<std::unique_ptr<VideoOutput>>          &  GetVideoModes         ()       noexcept { return m_parts.videoModes; }
    const std::vector<std::unique_ptr<MemoryDevice>>   &  GetOwnedDevices       () const noexcept { return m_parts.ownedDevices; }
    const std::vector<std::unique_ptr<IAciaEndpoint>>  &  GetOwnedAciaEndpoints () const noexcept { return m_parts.ownedAciaEndpoints; }
    const std::vector<std::unique_ptr<VideoOutput>>    &  GetVideoModes         () const noexcept { return m_parts.videoModes; }

    //  Raw pointers into the collections above, reset whenever one is
    //  rebuilt. See MachineRefs.
    MachineRefs        &  GetRefs()       noexcept { return m_parts.refs; }
    const MachineRefs  &  GetRefs() const noexcept { return m_parts.refs; }

    CharacterRomData        &  GetCharacterRom ()       noexcept { return *m_parts.charRom; }
    DiskImageStore          &  GetDiskStore    ()       noexcept { return *m_diskStore; }
    MachineConfig           &  GetConfig       ()       noexcept { return *m_config; }
    PrinterByteRing         &  GetPrinterRing  ()       noexcept { return *m_printerRing; }
    const CharacterRomData  &  GetCharacterRom () const noexcept { return *m_parts.charRom; }
    const DiskImageStore    &  GetDiskStore    () const noexcept { return *m_diskStore; }
    const MachineConfig     &  GetConfig       () const noexcept { return *m_config; }

    //  The components.
    MachineConnectors         & GetConnectors       ()       noexcept { return m_connectors; }
    MachineStateStream        & GetStateStream      ()       noexcept { return m_stateStream; }
    MachineRunner             & GetRunner           ()       noexcept { return m_runner; }
    MachineHostInput          & GetHostInput        ()       noexcept { return m_hostInput; }
    MachineStorage            & GetStorage          ()       noexcept { return m_storage; }
    MachineOutputMute         & GetOutputMute       ()       noexcept { return m_outputMute; }
    MachineObserverWiring     & GetObserverWiring   ()       noexcept { return m_observerWiring; }
    MachineTape               & GetTape             ()       noexcept { return m_tape; }
    MachineClockPanel         & GetClockPanel       ()       noexcept { return m_clockPanel; }
    const MachineConnectors   & GetConnectors       () const noexcept { return m_connectors; }
    const MachineStateStream  & GetStateStream      () const noexcept { return m_stateStream; }
    const MachineRunner       & GetRunner           () const noexcept { return m_runner; }
    const MachineHostInput    & GetHostInput        () const noexcept { return m_hostInput; }
    const MachineStorage      & GetStorage          () const noexcept { return m_storage; }
    const MachineOutputMute   & GetOutputMute       () const noexcept { return m_outputMute; }
    const MachineTape         & GetTape             () const noexcept { return m_tape; }
    const MachineClockPanel   & GetClockPanel       () const noexcept { return m_clockPanel; }
    const MachineRomIdentity  & GetRomIdentityCache () const noexcept { return m_romIdentity; }

    // The devices' lifetime, between the two threads that use them. A
    // machine switch on the CPU thread destroys and rebuilds every device,
    // with this locked exclusively for the whole of that. The UI thread
    // reads devices through the references every frame, with this locked
    // shared for the frame, so a frame never reads the list half-built.
    std::shared_mutex  &  GetLifetimeLock() noexcept { return m_lifetimeLock; }

    SoftSwitchMirror        &  GetSoftSwitchMirror()       noexcept { return m_softSwitches; }
    const SoftSwitchMirror  &  GetSoftSwitchMirror() const noexcept { return m_softSwitches; }

    //  Which machine this is: the directory name the config was loaded from
    //  ("apple2e", "apple2c"), and the directory its ROMs and other assets
    //  are read from. The name is also the per-machine suffix on user state,
    //  so one machine's last-mounted disks cannot clobber another's.
    const std::wstring  &  GetCurrentMachineName () const noexcept { return m_currentMachineName; }
    const std::wstring  &  GetAssetBaseDir       () const noexcept { return m_assetBaseDir; }
    void                   SetCurrentMachineName (const std::wstring & name) { m_currentMachineName = name; }
    void                   SetAssetBaseDir       (const std::wstring & dir)  { m_assetBaseDir = dir; }

    //  Where this machine's pending printer strip persists across a switch
    //  or a shutdown: <assetBase>/Machines/<machine>/PendingPrint.
    std::filesystem::path  GetPendingPrintDir() const;

    //  After the builder wired a machine's devices: the input journal and
    //  the comm sink attached to them again, so a journal that was on before
    //  a machine switch is filled from the new devices too.
    void  AttachDevices();

    //  The watch on the input behind live, or null: the devices report a
    //  read that input would change, and the run ends after it.
    void  SetHeldInputWatch (HeldInputWatch * watch);

    //  The debugger changed memory, registers or I/O state from outside the
    //  program: the history after this point no longer follows, and the
    //  history recorder gets the edit.
    void  NoteDebuggerEdit();

    //  The connectors let go of their endpoints, then the machine destroys
    //  the endpoints it owns.
    void  DetachSerialPorts();

    //  Ctrl+Reset. User RAM survives; every device takes its reset line.
    void  SoftReset();

    //  Power off and on. Dirty disks are flushed first, and the mounts
    //  persist; then every DRAM-owning device is refilled with the power-on
    //  pattern, so the machine comes up with what a real one would have.
    void  PowerCycle();

    //  A hash of every ROM image the machine was built with, computed again
    //  after a ROM image is patched or replaced.
    uint64_t  GetRomIdentity() const override;

private:
    //  IMachineStateParts: what the state stream reads from the machine.
    void    GetStateParts  (std::vector<IMachineState *> & outParts) override;
    Prng  * GetStatePrng   () const override { return m_parts.prng.get(); }
    void    BindDiskDrives () override       { m_storage.BindDiskDrives(); }

    void  OnMediaChanged        ();
    void  ApplyPowerOnOverrides ();

    MachineParts                       m_parts;
    SoftSwitchMirror                   m_softSwitches;
    std::shared_mutex                  m_lifetimeLock;

    // On the heap: together they are most of the host's size, and a test
    // that builds two machines on the stack must stay under the analyzer's
    // frame budget.
    std::unique_ptr<DiskImageStore>    m_diskStore;
    std::unique_ptr<MachineConfig>     m_config;

    // The ImageWriter II's input, fed by whichever connector it is attached
    // to. The printer's drain thread reads it, so it outlives every rebuild
    // and every change of connector. 64 KB, so on the heap.
    std::unique_ptr<PrinterByteRing>   m_printerRing;

    std::wstring                       m_currentMachineName;
    std::wstring                       m_assetBaseDir;
    MachineRomIdentity                 m_romIdentity;

    //  Each component after the members it refers to.
    MachineConnectors                  m_connectors;
    MachineStateStream                 m_stateStream;
    MachineRunner                      m_runner;
    MachineHostInput                   m_hostInput;
    MachineStorage                     m_storage;
    MachineOutputMute                  m_outputMute;
    MachineObserverWiring              m_observerWiring;
    MachineTape                        m_tape;
    MachineClockPanel                  m_clockPanel;
};
