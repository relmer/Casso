#pragma once

#include "Pch.h"

#include "I6502DebugInfo.h"

class MemoryBus;
class MemoryBusCpu;
class MemoryDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  CatchUpKind
//
//  Why a device's CPU runs up to the Apple's time. Before an access from the
//  Apple it runs to that time exactly. Between frames, a CPU in its idle loop
//  may stop less than a pass short; the next catch-up, at the latest the one
//  before the Apple's next access, runs it the rest of the way.
//
////////////////////////////////////////////////////////////////////////////////

enum class CatchUpKind
{
    Access,
    BetweenFrames,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeviceCpu
//
//  The 65C02 inside a card or a drive, on a separate bus: the card's
//  memory map is one memory device over the whole 64K. The owner runs it to
//  a cycle count, which follows the Apple's clock.
//
//  The owner runs the CPU up to the Apple's time before each access from
//  the Apple, so while RunUntil runs, the only inputs that change are the
//  device's other hardware. A firmware's idle loop that doesn't read that
//  hardware, or reads only hardware that is idle, makes the same passes up
//  to the target, and the owner's idle loop skip moves the cycle count
//  ahead by whole passes instead of running them.
//
////////////////////////////////////////////////////////////////////////////////

class DeviceCpu
{
public:
    DeviceCpu();
    ~DeviceCpu();

    HRESULT           Initialize      (MemoryDevice & memory);

    //  Serves pages straight from `read` and `write`, page `firstPage` from
    //  their first byte. A null pointer sends the pages' accesses to the
    //  memory device instead.
    void              MapPages        (int firstPage, int pageCount, Byte * read, Byte * write);

    //  The 65C02's RESET: the PC from $FFFC.
    void              Reset           ();

    //  Runs instructions until the cycle count is at least `targetCycle`, or,
    //  between frames, until the CPU is in its idle loop less than a pass
    //  short of it.
    void              RunUntil        (uint64_t targetCycle, CatchUpKind kind = CatchUpKind::Access);

    //  The firmware's idle loop, at `pc`, its shortest pass `passCycles`
    //  long. At the start of each pass short of the target, `skip` is called
    //  with the cycles left. It makes the changes to memory and registers of
    //  the whole passes it skips and returns their cycles, at most the cycles
    //  left, or 0 to run the pass. Between frames, with less than a pass left
    //  after a skip, RunUntil returns with the CPU at `pc` and the cycle
    //  count short of the target. An empty `skip` runs every pass.
    using IdleLoopFn = std::function<uint64_t (uint64_t cyclesLeft)>;
    void              SetIdleLoop     (Word pc, uint64_t passCycles, IdleLoopFn skip);

    Cpu6502Registers  GetRegisters    () const;
    void              SetRegisters    (const Cpu6502Registers & regs);

    //  Moves the cycle count to `cycle` without running instructions, for a
    //  device in reset, or idle for long enough that it doesn't matter.
    void              SkipTo          (uint64_t cycle) { m_cycles = std::max (m_cycles, cycle); }

    uint64_t          GetCycles       () const { return m_cycles; }
    uint64_t          GetInstructions () const { return m_instructions; }
    Word              GetPC           () const;

    const uint64_t  * GetCyclePtr     () const { return &m_cycles; }

private:
    std::unique_ptr<MemoryBus>     m_bus;
    std::unique_ptr<MemoryBusCpu>  m_cpu;
    uint64_t                       m_cycles       = 0;
    uint64_t                       m_instructions = 0;
    Word                           m_idlePc       = 0;
    uint64_t                       m_idlePass     = 0;
    IdleLoopFn                     m_idleLoop;
};
