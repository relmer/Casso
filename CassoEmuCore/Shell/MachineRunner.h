#pragma once

#include "Pch.h"

class DebugHook;
struct DebugHookFilter;
class HistoryRecorder;
class HeldInputWatch;
class IOpcodeWatcher;
class MachineConnectors;
struct MachineParts;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRunner
//
//  Runs the machine an instruction at a time: the debugger's hook before an
//  instruction, reverse execution's recorder around it, and after it the
//  devices that count cycles. The machine's position, the instructions it
//  has retired with an interrupt dispatch counting as one, is here: only
//  a restored snapshot sets it, not even a power cycle restarts it, so it
//  orders history where the cycle counter cannot.
//
////////////////////////////////////////////////////////////////////////////////

class MachineRunner
{
public:
    MachineRunner (MachineParts & parts, MachineConnectors & connectors);

    MachineRunner (const MachineRunner &)             = delete;
    MachineRunner & operator= (const MachineRunner &) = delete;

    //  Retire one instruction, or the interrupt vector that takes an opcode
    //  fetch's place, and advance every device that counts cycles with it.
    //  Returns what it cost; 0 when the hook stopped it.
    Byte  StepOne();

    //  Retire instructions until at least `cycleBudget` cycles are spent, and
    //  return what was spent. The overshoot is at most the last instruction:
    //  the machine cannot stop mid-opcode, so a caller pacing against a budget
    //  adds the remainder to the next one.
    uint64_t  RunCycles (uint64_t cycleBudget);

    //  The debugger's per-instruction hook, or null. While set, StepOne calls
    //  it before each instruction its filter marks, and RunCycles returns
    //  early on a stop. Unset, each instruction costs one pointer test.
    void         SetDebugHook (DebugHook * hook) noexcept { m_debugHook = hook; }
    DebugHook  * GetDebugHook () const noexcept           { return m_debugHook; }

    //  A flag another thread raises to stop the machine, or null. RunCycles
    //  tests it before each instruction, so a pause lands on the next
    //  instruction boundary rather than at the end of the slice.
    void  SetStopFlag (const std::atomic<bool> * stopFlag) noexcept { m_stopFlag = stopFlag; }

    //  Reverse execution's recorder, or null. While set, StepOne reports each
    //  instruction to it.
    void               SetHistoryRecorder (HistoryRecorder * recorder) noexcept { m_historyRecorder = recorder; }
    HistoryRecorder  * GetHistoryRecorder () const noexcept                    { return m_historyRecorder; }

    //  Reverse execution sets the position when it restores a snapshot.
    uint64_t          GetPosition    () const noexcept   { return m_position; }
    void              SetPosition    (uint64_t position) { m_position = position; }
    const uint64_t  * GetPositionPtr () const noexcept   { return &m_position; }

    //  The opcodes, a 256-entry table read in place, whose fetches the CPU
    //  reports to the watcher; null for none. Applied to each CPU the machine
    //  is rebuilt with (ApplyOpcodeWatch).
    void  SetOpcodeWatch   (const bool * opcodes, IOpcodeWatcher * watcher);
    void  ApplyOpcodeWatch ();

    //  The watch on the input behind live, or null. Set, RunCycles ends after
    //  an instruction that read a line that input would change.
    void  SetHeldInputWatch (const HeldInputWatch * watch) noexcept { m_heldInputWatch = watch; }

private:
    //  The cards with separate processors catch up every 16,384 cycles.
    static constexpr int  kCatchUpShift = 14;

    Byte  StepOneWithHook ();
    Byte  StepOneAsked    (const DebugHookFilter & filter, Word pc);
    Byte  FinishStep      ();
    void  CatchUpCards    ();

    MachineParts               & m_parts;
    MachineConnectors          & m_connectors;

    DebugHook                  * m_debugHook       = nullptr;
    const std::atomic<bool>    * m_stopFlag        = nullptr;
    HistoryRecorder            * m_historyRecorder = nullptr;
    const bool                 * m_watchOpcodes    = nullptr;
    IOpcodeWatcher             * m_watcher         = nullptr;
    const HeldInputWatch       * m_heldInputWatch  = nullptr;
    uint64_t                     m_position        = 0;
};
