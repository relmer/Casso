#pragma once

#include "Debugger/Reply.h"

class DebugHook;
class IRunObserver;
class Microcode;





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
    virtual bool                TryPoke           (Word address, Byte value) = 0;
    virtual MemoryRegion        GetRegion         (Word address) const = 0;
    virtual Byte                ReadIo            (Word address) = 0;
    virtual void                WriteIo           (Word address, Byte value) = 0;
    virtual void                GetSoftSwitches   (std::vector<SoftSwitch> & switches) const = 0;

    virtual void                SetRunObserver    (IRunObserver * observer) = 0;
    virtual HRESULT             StartRun          (const RunRequest & request) = 0;
    virtual void                RequestPause      () = 0;
    virtual void                SetHookInstalled  (bool installed) = 0;
    virtual void                SetStopConditions (DebugHook * conditions) = 0;
    virtual void                SetWatchedPages   (const WatchedPages & pages) = 0;

    virtual VideoPosition       GetVideoPosition  () const = 0;
    virtual uint64_t            GetCycleCount     () const = 0;
    virtual DebugCpuKind        GetCpuKind        () const = 0;
    virtual const Microcode   * GetInstructionSet () const = 0;
    virtual DebugMachineInfo    GetMachineInfo    () const = 0;

    // The keyboard: a key is pending until the guest clears the strobe.
    virtual void                InjectKey         (Byte key) = 0;
    virtual bool                IsKeyPending      () const = 0;
};
