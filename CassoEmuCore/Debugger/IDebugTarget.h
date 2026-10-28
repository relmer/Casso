#pragma once

#include "Debugger/DebugMemoryImage.h"
#include "Debugger/DiagnosticsSnapshot.h"
#include "Debugger/HeatTypes.h"
#include "Debugger/Reply.h"

class AccessHeatMap;
class Disk2NibbleRecord;
class IDisk2EventSink;
class DebugHook;
class IDiagnosticsProvider;
class IHeatAccessFinder;
class IHeatRebuilder;
class IHistoryObserver;
class IOpcodeWatcher;
class IRunObserver;
class IWatchSink;
class KeyframeStore;
class Microcode;
class TapeDeck;





////////////////////////////////////////////////////////////////////////////////
//
//  Target types
//
////////////////////////////////////////////////////////////////////////////////

enum class DebugCpuKind
{
    M6502,
    M65C02,
};

struct VideoPosition
{
    uint32_t  scanline    = 0;
    uint32_t  cycleInLine = 0;
};

struct DebugMachineInfo
{
    std::string               name;
    std::string               title;
    std::vector<std::string>  disks;

    //  The cards' and ports' bus registers, for the operand tips.
    std::vector<DiagnosticsIoRegister>  ioRegisters;
};

//  The Disk II controller's state, read from its getters, never through the
//  bus: the selected drive, each drive's head, the motor, write mode, the
//  magnets on, and each drive's disk.
struct DebugDiskState
{
    int                          slot          = 0;
    int                          selectedDrive = 0;
    std::array<int, 2>           quarterTrack  = {};
    bool                         isMotorOn     = false;
    bool                         isWriteMode   = false;
    Byte                         phases        = 0;
    std::array<bool, 2>          isProtected   = {};
    std::array<bool, 2>          hasDisk       = {};
    std::array<std::string, 2>   imageName;
};

static constexpr size_t  s_kDebugPageCount = 256;

using WatchedPages = std::array<bool, s_kDebugPageCount>;





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugTarget
//
//  The machine as the debugger sees it. Peek and Poke never disturb the
//  machine; ReadIo and WriteIo are real bus access and exist only for IN and
//  OUT. A run's stop is delivered to the observer set with SetRunObserver.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugTarget
{
public:
    virtual ~IDebugTarget() = default;

    virtual Cpu6502Registers    GetRegisters      () const = 0;
    virtual void                SetRegisters      (const Cpu6502Registers & registers) = 0;

    virtual bool                TryPeek           (Word address, Byte & value) const = 0;

    //  A $C000-$C0FF switch's byte from its device's side-effect-free peek
    //  (MemoryBus::TryPeekIo). TryPeek, and so every memory pane, never
    //  reads I/O; only the trial run of the PC's instruction calls this.
    virtual bool                TryPeekIo         (Word address, Byte & value) const { (void) address; value = 0; return false; }
    virtual bool                TryPoke           (Word address, Byte value) = 0;

    //  Everything TryPeek and GetRegion would answer, all at once. A target
    //  that can copy whole pages overrides this; the default peeks each byte.
    virtual void                TakeMemoryImage   (DebugMemoryImage & image) const
    {
        for (size_t address = 0; address < DebugMemoryImage::kBytes; address++)
        {
            image.readable[address] = TryPeek ((Word) address, image.bytes[address]);
        }

        for (size_t page = 0; page < DebugMemoryImage::kPages; page++)
        {
            image.regions[page] = GetRegion ((Word) (page * DebugMemoryImage::kPageBytes));
        }
    }

    //  A memory window's edit: RAM as a write, ROM into the image the CPU
    //  reads from, I/O refused.
    virtual bool                TryPatch          (Word address, Byte value) = 0;
    virtual MemoryRegion        GetRegion         (Word address) const = 0;
    virtual Byte                ReadIo            (Word address) = 0;
    virtual void                WriteIo           (Word address, Byte value) = 0;
    virtual void                GetSoftSwitches   (std::vector<SoftSwitch> & switches) const = 0;

    // The slot of the card whose $C800 expansion ROM is selected, 0 for
    // none; false where that is not known.
    virtual bool                TryGetExpansionRomSlot (int & slot) const { slot = 0; return false; }

    virtual void                SetRunObserver    (IRunObserver * observer) = 0;
    virtual HRESULT             StartRun          (const RunRequest & request) = 0;
    virtual void                RequestPause      () = 0;
    virtual bool                IsPausePending    () const { return false; }  // a pause still on its way to the stop
    virtual void                SetHookInstalled  (bool installed) = 0;
    virtual void                SetStopConditions (DebugHook * conditions) = 0;
    virtual void                SetWatchedPages   (const WatchedPages & pages) = 0;

    // The opcodes, a 256-entry table read in place, whose fetches the CPU
    // tells the watcher of (see IOpcodeWatcher); null for none. A target with
    // no CPU of its own tells nobody.
    virtual void                SetOpcodeWatch    (const bool *, IOpcodeWatcher *) {}

    // Where an access to a watched page is reported.
    virtual void                SetWatchSink      (IWatchSink * sink) = 0;

    virtual VideoPosition       GetVideoPosition  () const = 0;
    virtual uint64_t            GetCycleCount     () const = 0;

    // The cycle count on history's clock, which a power cycle does not set
    // back; the CPU's count for a target with no history.
    virtual uint64_t            GetHistoryCycleCount () const { return GetCycleCount(); }

    // The Cpu::kPenalty bits the last instruction paid.
    virtual Byte                GetLastPenalties  () const = 0;

    // The address of the last instruction that transferred control; false
    // if none has since the CPU was reset.
    virtual bool                TryGetLastBranch  (Word & from) const = 0;

    // Where that instruction sent PC; false when it is not known.
    virtual bool                TryGetLastBranchTarget (Word & to) const   { (void) to; return false; }

    virtual DebugCpuKind        GetCpuKind        () const = 0;
    virtual const Microcode   * GetInstructionSet () const = 0;
    virtual DebugMachineInfo    GetMachineInfo    () const = 0;

    // What owns the instruction set, when the target has a copy of its own
    // rather than the live CPU's; null for the live CPU's.
    virtual std::shared_ptr<const std::vector<Microcode>>  GetInstructionTable () const { return nullptr; }

    // The Disk II controller for disk breakpoints: its Breakpoints listener
    // slot, its state, its recent nibbles, a look-ahead of a drive's next
    // nibbles that moves nothing, and the address of the instruction
    // running. A target with no controller has none of them.
    virtual void                        SetDiskEventSink        (IDisk2EventSink *) {}
    virtual bool                        TryGetDiskState         (DebugDiskState &) const { return false; }
    virtual const Disk2NibbleRecord   * GetDiskRecord           () const { return nullptr; }
    virtual bool                        PeekDiskNibbles         (int, int, std::vector<Byte> &, bool &) const { return false; }
    virtual Word                        GetInstructionAddress   () const { return 0; }

    // The cassette deck and the bus cycle its transport runs by; null on a
    // target with no deck.
    virtual TapeDeck          * GetTapeDeck       ()           { return nullptr; }
    virtual uint64_t            GetBusCycle       () const     { return GetCycleCount(); }

    // The keyboard: a key is pending until the guest clears the strobe.
    virtual void                InjectKey         (Byte key) = 0;
    virtual bool                IsKeyPending      () const = 0;

    // The instruction trace. Turning it on starts a new one; off keeps what
    // it holds until it is turned on again or cleared. The window is up to
    // count entries from first, oldest first, raw fields only.
    virtual void                SetTraceOn        (bool on) = 0;
    virtual bool                IsTraceOn         () const = 0;
    virtual void                ClearTrace        () = 0;
    virtual size_t              GetTraceSize      () const = 0;
    virtual void                GetTraceWindow    (size_t first, size_t count, std::vector<TraceRecord> & entries) const = 0;

    //  The index of the trace's first entry at or after cycle, when the
    //  target can find it faster than a search through its entries; false
    //  otherwise.
    virtual bool                TryFindTraceCycle (uint64_t cycle, size_t & outIndex) const { (void) cycle; (void) outIndex; return false; }

    // The access heat map. On, every bus access and opcode fetch is counted;
    // off frees it, and the machine does no work for it. Fold brings the heat
    // up to the current cycle and gives the map, or null while it is off.
    // Clear forgets it without touching the machine, for a machine switch;
    // Reset zeroes what it has counted and keeps it on. The fade time is how
    // long a single access stays on it. A target with no machine has none.
    virtual void                    SetHeatMapOn   (bool on)        { (void) on; }
    virtual void                    ClearHeatMap   ()               {}
    virtual void                    ResetHeatMap   ()               {}
    virtual void                    SetHeatMapFade (double seconds) { (void) seconds; }
    virtual const AccessHeatMap   * FoldHeatMap    ()               { return nullptr; }

    // The heat map follows the machine through reverse execution's history:
    // the history it keeps its counts in and the rebuilder of its heat, or
    // none; the observer the history is to tell of what it does; a move
    // through history, isInterim while a drag of the timeline goes on;
    // whether it accumulates rather than fades; and whether its heat is
    // still being rebuilt.
    virtual void                    AttachHistory         (KeyframeStore * keyframes, IHeatRebuilder * rebuilder) { (void) keyframes; (void) rebuilder; }
    virtual IHistoryObserver      * GetHistoryObserver    ()                    { return nullptr; }
    virtual void                    NoteHistoryMoved      (bool isInterim)      { (void) isInterim; }
    virtual void                    SetHeatMapCumulative  (bool isCumulative)   { (void) isCumulative; }
    virtual bool                    IsHeatMapRebuilding   () const              { return false; }

    // The instruction that last wrote or read an address in one of the heat
    // map's spaces, as of where the machine stands, looked up in history
    // where the map does not hold it; and what looks it up there. A target
    // with no machine has none.
    virtual HeatAccessState  LookUpHeatMapAccess (HeatSpace space, bool isWrite, Word address, HeatLastAccess & outAccess)
    {
        (void) space;
        (void) isWrite;
        (void) address;

        outAccess = HeatLastAccess();
        return HeatAccessState::None;
    }

    virtual void             SetHeatAccessFinder (IHeatAccessFinder * finder) { (void) finder; }

    // The break on a read of RAM nothing has written since power-on, which
    // only the heat map sees, so arming it turns the map on; the read it is
    // holding for a stop, until cleared; the CPU addresses it leaves out,
    // as first and last of each span; and what the map can say about such
    // reads. A target with no machine has none.
    virtual void                 SetUnwrittenBreak   (bool isArmed)                                    { (void) isArmed; }
    virtual bool                 TryGetUnwrittenStop (HeatUnwrittenRead & outRead) const               { (void) outRead; return false; }
    virtual void                 ClearUnwrittenStop  ()                                                {}
    virtual void                 SetUnwrittenIgnore  (const std::vector<std::pair<Word, Word>> & spans) { (void) spans; }
    virtual HeatUnwrittenStatus  GetUnwrittenStatus  () const                                          { return {}; }

    // The devices that publish debugger panels. A target with no devices has
    // none, which is the default.
    virtual std::vector<const IDiagnosticsProvider *>  GetDiagnosticsProviders () const { return {}; }

    // The address the Mockingboard's page starts at; false where the machine
    // has none, which is the default.
    virtual bool  TryGetMockingboardBase (Word &) const { return false; }
};
