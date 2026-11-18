#include "Pch.h"

#include "Debugger/CallRecordKeeper.h"
#include "Debugger/DebugSessionView.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/SymbolTable.h"
#include "Debugger/WatchpointTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::CallRecordKeeper
//
////////////////////////////////////////////////////////////////////////////////

CallRecordKeeper::CallRecordKeeper (IDebugTarget & target, WatchpointTable & watchpoints) :
    m_target      (target),
    m_watchpoints (watchpoints)
{
    m_recorder.SetPeek             ([this] (Word address) { return PeekByte (address); });
    m_recorder.SetWriterLocator    ([this] { return FindStoreInProgress(); });
    m_recorder.SetClock            ([this] { return m_target.GetHistoryCycleCount(); });
    CallStackRecorder::MarkOpcodes (m_opcodes.data());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::OnWatchedFetch
//
//  The CPU reports only the opcodes that can change the call record, the
//  instruction after each and every interrupt; any other instruction leaves
//  the record as it was, so the record is what it would be shown them all.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::OnWatchedFetch (Word pc, Byte sp, Byte opcode)
{
    m_recorder.OnInstruction (pc, sp, opcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::SetRecording
//
//  Turning recording off during a step out that ends on the record waits for
//  that step to stop: the step would otherwise end at once, on a record
//  that no longer has its frame.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::SetRecording (bool isOn)
{
    Word  pc = m_target.GetRegisters().pc;



    m_isRecordingEndDeferred = !isOn && m_isStepOutOnRecord && m_recorder.IsActive();

    if (isOn == m_recorder.IsActive() || m_isRecordingEndDeferred)
    {
        return;
    }

    m_generation++;

    if (isOn)
    {
        m_recorder.Begin (pc, PeekByte (pc), m_target.GetCycleCount() == 0);
        m_stopPc = pc;
        m_target.SetOpcodeWatch (m_opcodes.data(), this);
        m_watchpoints.SetStackWriteSink ([this] (Word address, Byte value, std::optional<Byte> previous)
        {
            m_recorder.OnStackWrite (address, value, previous);
        });
    }
    else
    {
        m_target.SetOpcodeWatch (nullptr, nullptr);
        m_watchpoints.SetStackWriteSink (nullptr);
        m_recorder.End();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::Restart
//
//  For a machine that moved through history, so the bottom is not where the
//  debugger attached: the calls before here are in history, and until a
//  rebuild from it replaces this record, none of them is in it.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::Restart()
{
    Word  pc = m_target.GetRegisters().pc;



    if (!m_recorder.IsActive())
    {
        return;
    }

    m_generation++;
    m_recorder.Begin (pc, PeekByte (pc), (m_target.GetCycleCount() == 0) ? CallBreakKind::PowerOn : CallBreakKind::HistoryBegan);
    m_stopPc = pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::MarkUnrebuilt
//
//  History was turned off, could not reach where the machine stands, or the
//  rebuild failed or was not taken: the record keeps its calls, and its
//  bottom marks only that the calls before it are not available, not where
//  history starts. Any other bottom is left as it is.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::MarkUnrebuilt()
{
    m_recorder.RelabelBottom (CallBreakKind::HistoryBegan, CallBreakKind::TrackingRestarted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::GetRecord
//
////////////////////////////////////////////////////////////////////////////////

CallRecord CallRecordKeeper::GetRecord()
{
    Settle();

    return m_recorder.GetRecord();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::Adopt
//
//  The record replaces the one kept since recording began, and the next
//  instruction the CPU reports goes on from it. A record that is off takes
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::Adopt (const CallRecord & record)
{
    if (!m_recorder.IsActive() || !record.isActive)
    {
        return;
    }

    m_recorder.SetRecord (record);
    m_stopPc = m_target.GetRegisters().pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::GetCallStack
//
////////////////////////////////////////////////////////////////////////////////

CallStackData CallRecordKeeper::GetCallStack (const SymbolTable & symbols, const std::set<Word> * entries)
{
    CallStackData      data;
    SymbolTableId      table = SymbolTableId::User;
    Cpu6502Registers   registers;



    Settle();

    registers = m_target.GetRegisters();
    data      = CallStack::Build (m_mechanism, m_recorder, registers.sp,
                                  [this] (Word address) { return PeekByte (address); },
                                  entries);

    for (CallStackRow & row : data.rows)
    {
        if (row.frame.has_value())
        {
            symbols.TryFindName (row.frame->target, row.frame->symbol, table);
        }
    }

    if (data.lastReturn.has_value())
    {
        symbols.TryFindName (data.lastReturn->target, data.lastReturn->symbol, table);
    }

    if (m_recorder.IsActive())
    {
        data.rebuildProgress = m_rebuildProgress;
    }

    return data;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::HasNoCallSince
//
////////////////////////////////////////////////////////////////////////////////

bool CallRecordKeeper::HasNoCallSince (uint64_t cycle)
{
    Settle();

    return m_recorder.HasNoCallSince (cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::GetCallerLinks
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::GetCallerLinks (std::vector<CallerLink> & outLinks)
{
    Settle();

    m_recorder.GetCallerLinks (outLinks);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::Settle
//
//  The record takes in the last instruction executed, from the registers
//  after it; called only between instructions.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::Settle()
{
    Cpu6502Registers  registers;



    if (!m_recorder.IsActive())
    {
        return;
    }

    registers = m_target.GetRegisters();
    m_recorder.Settle (registers.pc, registers.sp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::SetStepOutFrame
//
//  A step out leaves the innermost recorded frame: a call by its return, an
//  interrupt by the RTI that goes back to the instruction it stopped. The
//  stack pointer alone cannot tell that when code between the handler and
//  the interrupted instruction keeps a return frame of its own, as the
//  enhanced //e's ROM does around an interrupt, or pulls bytes and jumps.
//
//  The record takes in each instruction at the next one's fetch, which is
//  after the stop hook asks about it, so it is settled first, from the
//  registers the next instruction will run with.
//
//  With no frame on record, or with the PC moved since the stop so that the
//  innermost frame is the stopped code's, the step out ends at the first RTS
//  or RTI that takes the stack above where it began. That frame may never
//  end, and a step out waiting for it would run on with no indication. The
//  console and the status bar have a line about it.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::SetStepOutFrame (RunRequest & request, std::string & outNotice)
{
    Word            pc      = m_target.GetRegisters().pc;
    bool            isMoved = m_stopPc.has_value() && *m_stopPc != pc;
    CallStackFrame  frame;



    Settle();

    if (!m_recorder.IsActive())
    {
        return;
    }

    if (m_recorder.GetFrames().empty() || isMoved)
    {
        outNotice = std::format ("No call on record was made by the code at ${:04X}, so the step out ends at the first RTS or RTI that raises the stack.", pc);
        return;
    }

    frame = m_recorder.GetFrames().back();
    m_isStepOutOnRecord = true;

    request.hasLeftFrame = [this, frame] (Word pc, Byte sp)
    {
        const std::vector<CallStackFrame> & frames = m_recorder.GetFrames();



        m_recorder.Settle (pc, sp);

        return std::none_of (frames.begin(), frames.end(), [&frame] (const CallStackFrame & each)
        {
            return each.callSite == frame.callSite && each.target == frame.target &&
                   each.kind == frame.kind && each.stackLevel == frame.stackLevel;
        });
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::OnMachineChanged
//
//  The record starts over at the new machine's PC, which is not where the
//  debugger opened.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::OnMachineChanged()
{
    m_isStepOutOnRecord = false;

    if (m_isRecordingEndDeferred)
    {
        SetRecording (false);
    }

    if (m_recorder.IsActive())
    {
        m_recorder.End();
        SetRecording (true);
        m_recorder.RelabelBottom (CallBreakKind::TrackingBegan, CallBreakKind::TrackingRestarted);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::OnReset
//
//  The record starts over from the reset, so a rebuild of the old one from
//  history is no longer of use.
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::OnReset (bool isPowerCycle)
{
    Word  pc = m_target.GetRegisters().pc;



    if (m_recorder.IsActive())
    {
        m_generation++;
    }

    m_recorder.OnReset (pc, PeekByte (pc), isPowerCycle);
    m_stopPc = pc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::OnStopped
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::OnStopped()
{
    Settle();
    m_isStepOutOnRecord = false;

    if (m_isRecordingEndDeferred)
    {
        SetRecording (false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::TakeView
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::TakeView (DebugSessionView & out) const
{
    out.callMechanism = m_mechanism;
    out.callRecord    = m_recorder.GetRecord();
    out.callRebuild   = m_rebuildProgress;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::InstallView
//
////////////////////////////////////////////////////////////////////////////////

void CallRecordKeeper::InstallView (const DebugSessionView & view)
{
    m_mechanism = view.callMechanism;
    m_recorder.SetRecord (view.callRecord);
    m_rebuildProgress = view.callRebuild;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::PeekByte
//
//  A byte as the CPU sees it, or zero where the debugger cannot read one
//  without disturbing the machine.
//
////////////////////////////////////////////////////////////////////////////////

Byte CallRecordKeeper::PeekByte (Word address) const
{
    Byte  value = 0;



    m_target.TryPeek (address, value);
    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordKeeper::FindStoreInProgress
//
//  Called from inside a bus write; see CallStackRecorder::FindStoreInProgress.
//
////////////////////////////////////////////////////////////////////////////////

Word CallRecordKeeper::FindStoreInProgress() const
{
    return CallStackRecorder::FindStoreInProgress (m_target.GetInstructionSet(), m_target.GetRegisters().pc,
                                                   [this] (Word address) { return PeekByte (address); });
}
