#pragma once

#include "Pch.h"

#include "Core/ComponentRegistry.h"
#include "Core/EmuCpu.h"
#include "Core/IMachineState.h"
#include "Core/InterruptController.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"
#include "Core/StateWriter.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/IAciaEndpoint.h"
#include "Machines/Apple2/Common/CharacterRomData.h"
#include "Machines/Apple2/Common/VideoScanner.h"
#include "Machines/Apple2/Common/VideoTiming.h"
#include "Shell/HostInputGate.h"
#include "Shell/MachineRefs.h"
#include "Ui/UiCommandTypes.h"
#include "Video/VideoOutput.h"


class Apple2cRomBank;
class Apple2eMmu;
class AppleMouse;
class DebugHook;
struct DebugHookFilter;
class HeldInputWatch;
class HistoryRecorder;
class IDriveAudioSink;
class SiriusJoyport;
class IDisk2EventSink;
class IInputEventSink;
class Prng;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineObservers
//
//  Who is watching the machine run.
//
//  The debug panels implement these two interfaces, and the devices know
//  nothing about them beyond the interface -- so a test can watch a machine
//  with the same seam the panels use, and a machine switch can re-attach
//  whoever was watching without the machine knowing what a panel is.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineObservers
{
    IDisk2EventSink  *  disk  = nullptr;
    IInputEventSink  *  input = nullptr;
};





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

class MachineHost : public IDiagnosticsProvider
{
public:
    MachineHost  ();
    ~MachineHost () override;

    MachineHost                (const MachineHost &) = delete;
    MachineHost & operator=    (const MachineHost &) = delete;

    //  The bus and the fixed plumbing every machine has. Always present: a
    //  machine without a bus is not a state this class can be in. (The bus
    //  sits on the heap for its size, not because it can be absent.)
    MemoryBus            &  GetMemoryBus           () noexcept { return *m_memoryBus; }
    ComponentRegistry    &  GetRegistry            () noexcept { return m_registry; }
    InterruptController  &  GetInterruptController () noexcept { return m_interruptController; }

    const MemoryBus            &  GetMemoryBus           () const noexcept { return *m_memoryBus; }
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
    SiriusJoyport   *  GetJoyport         () noexcept { return m_joyport.get(); }
    VideoTiming     *  GetVideoTiming     () noexcept { return m_videoTiming.get(); }

    const EmuCpu          *  GetCpu            () const noexcept { return m_cpu.get(); }
    const Prng            *  GetPrng           () const noexcept { return m_prng.get(); }
    const Apple2eMmu      *  GetMmu            () const noexcept { return m_mmu.get(); }
    const Apple2cRomBank  *  GetApple2cRomBank () const noexcept { return m_apple2cRomBank.get(); }
    const AppleMouse      *  GetMouse          () const noexcept { return m_mouse.get(); }
    const SiriusJoyport   *  GetJoyport        () const noexcept { return m_joyport.get(); }
    const VideoTiming     *  GetVideoTiming    () const noexcept { return m_videoTiming.get(); }

    void  SetCpu            (std::unique_ptr<EmuCpu>          cpu);
    void  SetPrng           (std::unique_ptr<Prng>            prng);
    void  SetMmu            (std::unique_ptr<Apple2eMmu>      mmu);
    void  SetApple2cRomBank (std::unique_ptr<Apple2cRomBank>  romBank);
    void  SetMouse          (std::unique_ptr<AppleMouse>      mouse);
    void  SetJoyport        (std::unique_ptr<SiriusJoyport>   joyport);
    void  SetVideoTiming    (std::unique_ptr<VideoTiming>     videoTiming);

    //  The video scanner the floating bus reads. Held by value: it owns
    //  nothing, and MachineBuilder points it at each new machine's parts.
    VideoScanner        &  GetVideoScanner()       noexcept { return m_videoScanner; }
    const VideoScanner  &  GetVideoScanner() const noexcept { return m_videoScanner; }

    //  Everything the machine owns and destroys as a unit. The ACIA
    //  endpoints are held apart from the devices because an IAciaEndpoint
    //  is not a MemoryDevice.
    std::vector<std::unique_ptr<MemoryDevice>>   &  GetOwnedDevices       () noexcept { return m_ownedDevices; }
    std::vector<std::unique_ptr<IAciaEndpoint>>  &  GetOwnedAciaEndpoints () noexcept { return m_ownedAciaEndpoints; }
    std::vector<std::unique_ptr<VideoOutput>>    &  GetVideoModes         () noexcept { return m_videoModes; }

    const std::vector<std::unique_ptr<MemoryDevice>>   &  GetOwnedDevices       () const noexcept { return m_ownedDevices; }
    const std::vector<std::unique_ptr<IAciaEndpoint>>  &  GetOwnedAciaEndpoints () const noexcept { return m_ownedAciaEndpoints; }
    const std::vector<std::unique_ptr<VideoOutput>>    &  GetVideoModes         () const noexcept { return m_videoModes; }

    CharacterRomData  &  GetCharacterRom () noexcept { return *m_charRom; }
    DiskImageStore    &  GetDiskStore    () noexcept { return *m_diskStore; }
    MachineConfig     &  GetConfig       () noexcept { return *m_config; }

    const CharacterRomData  &  GetCharacterRom () const noexcept { return *m_charRom; }
    const DiskImageStore    &  GetDiskStore    () const noexcept { return *m_diskStore; }
    const MachineConfig     &  GetConfig       () const noexcept { return *m_config; }

    //  Raw pointers into the two collections above, reset whenever either is
    //  rebuilt. See MachineRefs.
    MachineRefs  &  GetRefs() noexcept { return m_refs; }

    const MachineRefs  &  GetRefs() const noexcept { return m_refs; }

    // The devices' lifetime, between the two threads that touch them. A
    // machine switch on the CPU thread destroys and rebuilds every device;
    // it holds this exclusively for the whole of that. The UI thread reads
    // devices through the references every frame, and holds it shared for
    // the frame, so a frame never sees the list half-built.
    std::shared_mutex  &  GetLifetimeLock() noexcept { return m_lifetimeLock; }

    SoftSwitchMirror  &  GetSoftSwitchMirror() noexcept { return m_softSwitches; }

    const SoftSwitchMirror  &  GetSoftSwitchMirror() const noexcept { return m_softSwitches; }

    //  Which machine this is: the directory name the config was loaded from
    //  ("apple2e", "apple2c"), and the directory its ROMs and other assets
    //  are read from. The name doubles as the per-machine suffix on user
    //  state, so one machine's last-mounted disks cannot clobber another's.
    const std::wstring  &  GetCurrentMachineName () const noexcept { return m_currentMachineName; }
    const std::wstring  &  GetAssetBaseDir       () const noexcept { return m_assetBaseDir; }

    //  Retire exactly one instruction -- or, when a line is asserted, the
    //  interrupt vector that takes an opcode fetch's place -- and advance
    //  every device that counts cycles with it. Returns what it cost.
    Byte  StepOne();

    //  Retire instructions until at least `cycleBudget` cycles are spent,
    //  and return what was actually spent. The overshoot is at most the
    //  length of the last instruction: the machine cannot stop mid-opcode,
    //  so a caller pacing against a budget carries the remainder forward
    //  rather than expecting an exact count.
    uint64_t  RunCycles (uint64_t cycleBudget);

    //  The debugger's per-instruction hook, or null. While set, StepOne asks
    //  it before each instruction its filter names and returns 0 without
    //  executing when it says stop; RunCycles returns early on that, or on a
    //  stop raised during the instruction it just ran. Unset, each instruction
    //  costs one pointer test.
    void         SetDebugHook (DebugHook * hook) noexcept { m_debugHook = hook; }
    DebugHook *  GetDebugHook () const noexcept           { return m_debugHook; }

    //  Reverse execution's recorder, or null. While set, StepOne tells it of
    //  each instruction about to run; unset, each instruction costs one
    //  pointer test.
    void               SetHistoryRecorder (HistoryRecorder * recorder) noexcept { m_historyRecorder = recorder; }
    HistoryRecorder *  GetHistoryRecorder () const noexcept                    { return m_historyRecorder; }

    //  The machine's position: the instructions it has retired, an interrupt
    //  dispatch counting as one. It never restarts on its own, not even on a
    //  power cycle, so it orders history where the cycle counter cannot.
    //  Reverse execution sets it when it restores a snapshot.
    uint64_t         GetPosition    () const noexcept    { return m_position; }
    void             SetPosition    (uint64_t position)  { m_position = position; }
    const uint64_t * GetPositionPtr () const noexcept    { return &m_position; }

    //  The host inputs applied on the CPU thread, in order. RecordInput
    //  stamps one with the current cycle; with the journal off it records
    //  nothing. The journal outlives a machine switch.
    InputJournal        &  GetInputJournal()       noexcept { return m_inputJournal; }
    const InputJournal  &  GetInputJournal() const noexcept { return m_inputJournal; }

    void  RecordInput (InputKind kind, Byte value, uint16_t detail, std::string_view payload);

    //  Turn the journal on or off and point the devices whose reads see
    //  host input at it (see InputJournal::RecordObserved). The builder
    //  calls AttachInputJournal again after a machine switch. For a replay,
    //  ApplyDeviceInput hands one such record back to its device; it
    //  returns false for a record no device here holds.
    void  SetInputJournalOn  (bool isOn);
    void  AttachInputJournal ();
    bool  ApplyDeviceInput   (const InputRecord & record);

    //  CPU thread, at the start of an execution slice: with the journal on,
    //  journals each host-written input that changed since it was last seen,
    //  at this position, so a replay landing anywhere after it holds it.
    //  With the journal off it does nothing.
    void  SampleHostInputs();

    //  CPU thread: the watch on the input held back behind live, or null. Set,
    //  the devices whose reads see host input report a read the held input
    //  would change, and RunCycles ends after the instruction that made it;
    //  unset, each instruction costs one pointer test.
    void              SetHeldInputWatch (HeldInputWatch * watch);
    HeldInputWatch *  GetHeldInputWatch () const noexcept { return m_heldInputWatch; }

    //  The saved state of every device whose state another thread writes
    //  (keyboard, game port, //e paddles and buttons, mouse, Joyport), as
    //  one blob, and back; see InputKind::HostState.
    HRESULT  SaveHostInputState (StateWriter & writer) const;
    HRESULT  LoadHostInputState (std::string_view blob);

    //  Whether the host may write its input into the devices; held while
    //  reverse execution has the machine behind live (see HostInputGate).
    HostInputGate  &  GetHostInputGate() noexcept { return m_hostInputGate; }

    //  The debugger changed memory, registers or I/O state from outside the
    //  program: reverse execution's recorded future no longer follows, and
    //  the history recorder is told so.
    void  NoteDebuggerEdit();

    //  Silences what the machine sends to the host while it runs: speaker
    //  clicks and the Disk II drive sounds, and printer bytes, together or
    //  apart. Reverse execution mutes a replay that reaches a point, since
    //  what it replays was heard and printed live; one that plays on at the
    //  machine's own pace is heard again but not printed again.
    void  SetOutputMuted (bool isMuted);
    void  SetOutputMuted (bool isSoundMuted, bool isPrinterMuted);
    bool  IsOutputMuted  () const noexcept { return m_isSoundMuted || m_isPrinterMuted; }
    bool  IsSoundMuted   () const noexcept { return m_isSoundMuted; }
    bool  IsPrinterMuted () const noexcept { return m_isPrinterMuted; }

    //  The opcodes, a 256-entry table read in place, whose fetches the CPU
    //  tells the watcher of (see IOpcodeWatcher); null for none. It survives
    //  the CPU being replaced.
    void         SetOpcodeWatch (const bool * opcodes, IOpcodeWatcher * watcher);

    //  Point the machine's devices at whoever is watching, or at nobody.
    //  Every device is attached independently: the input panel is useful on
    //  its own, and it used to receive nothing unless the disk panel
    //  happened to be open as well.
    //
    //  A machine switch rebuilds every device, so this has to run again
    //  afterwards or an open panel goes quiet against the new machine.
    void  AttachObservers (const MachineObservers & observers);

    //  Ctrl+Reset. User RAM survives; every device takes its reset line.
    void  SoftReset();

    //  Power off and on. Dirty disks are flushed first -- the mounts
    //  themselves persist -- and then every DRAM-owning device is refilled
    //  with the power-on pattern, so the machine comes up with what a real
    //  one would hold rather than the contents it just had.
    void  PowerCycle();

    //  The whole machine's state as one stream, and back. The stream opens
    //  with the machine's name and ROM identity, so a state from another
    //  machine or another ROM set fails to load with ERROR_INVALID_DATA. The
    //  machine must already be built with the same devices and the same disks
    //  mounted; a failed load leaves it unusable. Call both on the thread
    //  that runs the machine.
    HRESULT   SaveState      (StateWriter & writer) const;
    HRESULT   LoadState      (StateReader & reader);

    //  The same load for a state read back from a file, in a session that
    //  never held the saved disks: the caller has mounted them, and each bay
    //  that held a disk at the save must hold one now, and each empty bay
    //  must be empty.
    HRESULT   LoadStateOverMountedMedia (StateReader & reader);

    //  Enough of a state to draw its screen, into a machine built from the
    //  same configuration that holds no disks: the disks the state holds, and
    //  any part this machine has no counterpart for, are passed over. The
    //  machine and ROM set must still match. The machine is for drawing only
    //  afterwards; running it is undefined.
    HRESULT   LoadStateForPicture       (StateReader & reader);

    //  Debug builds only: asserts that every run a sharing save kept by
    //  reference still holds the bytes of the buffer it stands for, which
    //  catches a RAM write no page marking saw and a track change no
    //  generation saw. Call it right after the save.
    void      CheckSharedSave (const StateWriter & writer) const;

    //  A hash of every ROM image the machine was built with. It is kept until a
    //  ROM image is patched or replaced, and GetRomIdentityHashCount says how
    //  many times it was worked out.
    uint64_t  GetRomIdentity          () const;
    uint64_t  GetRomIdentityHashCount () const noexcept { return m_romIdentityHashes; }

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('M', 'A', 'C', 'H');
    static constexpr uint16_t  kStateVersion = 2;

    //  Where this machine's pending printer strip persists across a switch
    //  or a shutdown: <assetBase>/Machines/<machine>/PendingPrint.
    std::filesystem::path  GetPendingPrintDir() const;

    void  SetCurrentMachineName (const std::wstring & name) { m_currentMachineName = name; }

    //  The devices of this machine that publish debugger panels, the clock
    //  first. Only what the machine has: a ][ lists no MMU, and an empty slot
    //  lists no card.
    std::vector<const IDiagnosticsProvider *>  GetDiagnosticsProviders () const;

    //  The clock panel: the cycle counters, the video beam, and the speed the
    //  CPU thread runs at, which it sets here since the machine does not pace
    //  itself.
    std::string  GetDiagnosticsId    () const override { return "clock"; }
    std::string  GetDiagnosticsTitle () const override { return "Clock"; }
    void         GetDiagnostics      (DiagnosticsSnapshot & snapshot) const override;
    void         SetSpeedMode        (SpeedMode mode) noexcept { m_speedMode = mode; }
    void  SetAssetBaseDir       (const std::wstring & dir)  { m_assetBaseDir = dir; }

private:

    Byte  StepOneWithHook  ();
    Byte  FinishStep       ();
    Byte  StepOneAsked     (const DebugHookFilter & filter, Word pc);

    //  The disk each drive bay holds, slot-major, as a snapshot records it.
    using MediaIds = std::array<uint64_t, DiskImageStore::kSlotCount * DiskImageStore::kDriveCount>;

    //  The slot whose Disk II the refs hold, which the drive bays feed.
    static constexpr int  kDiskControllerSlot = 6;

    void                          GetStateParts       (std::vector<IMachineState *> & outParts);
    void                          GetHostInputParts   (std::vector<IMachineState *> & outParts);
    void                          WriteStateHeader    (StateWriter & writer, size_t partCount) const;
    HRESULT                       LoadStateSeating    (StateReader & reader, bool isOverMounted);
    HRESULT                       CheckStateHeader    (StateReader & reader, bool isOverMounted, uint32_t & outParts);
    HRESULT                       ReadPictureHeader   (StateReader & reader, uint32_t & outParts) const;
    static HRESULT                GetPartTag          (const IMachineState & part, uint32_t & outTag);
    bool                          CanSeatMedia        (const MediaIds & mediaIds) const;
    bool                          AreBaysAsSaved      (const MediaIds & mediaIds) const;
    HRESULT                       SeatMedia           (const MediaIds & mediaIds);
    void                          BindDiskDrives      ();
    void                          OnMediaChanged      ();

    static uint64_t  HashBytes (uint64_t hash, const Byte * data, size_t size);
    static uint64_t  HashBytes (uint64_t hash, const std::vector<Byte> & bytes);

    //  Power-on bytes the fill must not decide: the power-up byte and the
    //  monitor's random seed.
    void  ApplyPowerOnOverrides();

    // 4K of page tables; on the heap, see m_diskStore.
    std::unique_ptr<MemoryBus>  m_memoryBus;

    ComponentRegistry    m_registry;
    InterruptController  m_interruptController;

    std::unique_ptr<EmuCpu>  m_cpu;
    std::unique_ptr<Prng>    m_prng;

    DebugHook         *  m_debugHook        = nullptr;
    const bool        *  m_watchOpcodes     = nullptr;
    IOpcodeWatcher    *  m_watcher          = nullptr;
    HistoryRecorder   *  m_historyRecorder  = nullptr;
    HeldInputWatch    *  m_heldInputWatch   = nullptr;
    IDriveAudioSink   *  m_mutedDiskAudio   = nullptr;  // the Disk II's sound sink, held while muted
    uint64_t             m_position         = 0;
    bool                 m_isSoundMuted     = false;
    bool                 m_isPrinterMuted   = false;
    bool                 m_wasMotorOnAtMute = false;

    mutable uint64_t  m_romIdentity           = 0;
    mutable uint64_t  m_romIdentityGeneration = 0;
    mutable uint64_t  m_romIdentityHashes     = 0;      // times GetRomIdentity hashed the ROMs

    // The part list a save or load fills; kept so it keeps its capacity.
    mutable std::vector<IMachineState *>  m_stateParts;

    std::vector<std::unique_ptr<MemoryDevice>>   m_ownedDevices;
    std::vector<std::unique_ptr<IAciaEndpoint>>  m_ownedAciaEndpoints;
    std::vector<std::unique_ptr<VideoOutput>>    m_videoModes;

    // 4K of glyphs; on the heap, see m_diskStore.
    std::unique_ptr<CharacterRomData>  m_charRom;

    SoftSwitchMirror  m_softSwitches;

    MachineRefs  m_refs;

    std::shared_mutex  m_lifetimeLock;

    std::unique_ptr<Apple2eMmu>      m_mmu;
    std::unique_ptr<Apple2cRomBank>  m_apple2cRomBank;
    std::unique_ptr<AppleMouse>      m_mouse;
    std::unique_ptr<SiriusJoyport>   m_joyport;
    std::unique_ptr<VideoTiming>     m_videoTiming;
    VideoScanner                     m_videoScanner;

    // On the heap: together they are most of the host's size, and a test
    // that builds two machines on the stack must stay under the analyzer's
    // frame budget.
    std::unique_ptr<DiskImageStore>  m_diskStore;
    std::unique_ptr<MachineConfig>   m_config;

    std::wstring  m_currentMachineName;
    std::wstring  m_assetBaseDir;

    SpeedMode     m_speedMode = SpeedMode::Authentic;

    InputJournal  m_inputJournal;

    HostInputGate  m_hostInputGate;
};
