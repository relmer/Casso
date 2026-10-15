#include "Pch.h"

#include "Debugger/Reverse/ReverseStopTest.h"
#include "Debugger/DebugSession.h"
#include "Debugger/DiskCallVector.h"
#include "Debugger/EffectiveAddress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::ReverseStopTest
//
////////////////////////////////////////////////////////////////////////////////

ReverseStopTest::ReverseStopTest (DebugSession & session) :
    m_session (session),
    m_disk    (session.GetBreakpoints(), session.GetTarget())
{
    m_disk.SetReplay();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::GetDiskSink
//
//  The stop test's own listener, while a disk breakpoint other than a call
//  is enabled.
//
////////////////////////////////////////////////////////////////////////////////

IDisk2EventSink * ReverseStopTest::GetDiskSink()
{
    m_disk.Refresh();

    return m_disk.IsArmed() ? &m_disk : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::ShouldStopBefore
//
//  Breakpoints first, then the disk events the last instruction made, then
//  before-mode watchpoints against what the instruction would touch. The
//  instruction's own reads are noted first, so the accesses it makes when it
//  runs are judged as a forward run judges them.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStopTest::ShouldStopBefore (
    MachineHost  & machine,
    Word           pc)
{
    IDebugTarget      & target     = m_session.GetTarget();
    WatchpointTable   & watches    = m_session.GetWatchpoints();
    Word                length     = 0;
    Byte                opcode     = 0;
    bool                isPeeked   = false;
    AccessPrediction    prediction;
    HRESULT             hr         = S_OK;
    uint64_t            cycle      = target.GetCycleCount();
    bool                isJump     = !m_lastCycle.has_value() || cycle < *m_lastCycle || cycle - *m_lastCycle > DiskBreakpointMonitor::kMaxStepCycles;
    bool                isDiskStop = false;



    (void) machine;

    if (isJump)
    {
        ResolveRoutines();
    }

    m_lastCycle = cycle;

    m_session.GetCpuOwnReads (pc, length, m_storeTarget);
    m_fetchPc     = pc;
    m_fetchesLeft = (Byte) ((1u << length) - 1);

    isPeeked   = target.TryPeek (pc, opcode);
    isDiskStop = m_disk.Poll (m_session);
    m_disk.ClearStop();

    if (m_session.GetBreakpoints().IsStopBefore (pc, isPeeked ? std::optional<Byte> (opcode) : std::nullopt, m_session, &m_routines))
    {
        return true;
    }

    if (isDiskStop)
    {
        return true;
    }

    if (!watches.HasEnabledBefore())
    {
        return false;
    }

    hr = EffectiveAddress::Predict (target.GetInstructionSet(), pc, target.GetRegisters(), m_session, prediction);
    return SUCCEEDED (hr) && watches.IsStopBefore (prediction);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::TakePendingStop
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStopTest::TakePendingStop()
{
    bool  isPending = m_isPending;



    m_isPending = false;
    return isPending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::OnWatchedAccess
//
//  An access to a watched page during the replay. One that an after-mode
//  watchpoint or a value breakpoint would stop on raises the pending stop
//  for the next instruction boundary.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStopTest::OnWatchedAccess (
    Word                 address,
    Byte                 value,
    BusAccess            access,
    std::optional<Byte>  previous)
{
    (void) previous;

    if (access == BusAccess::Write && std::ranges::find (m_vectorBytes, address) != m_vectorBytes.end())
    {
        ResolveRoutines();
    }

    if (access == BusAccess::Read && TryConsumeCpuOwnRead (address))
    {
        return;
    }

    if (m_session.GetWatchpoints().IsStopOnAccess (address, value, access))
    {
        m_isPending = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::TryConsumeCpuOwnRead
//
//  Each of the instruction's own bytes is passed over once, and the store
//  target's read once.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStopTest::TryConsumeCpuOwnRead (Word address)
{
    static constexpr Word  kMaxLength = 8;
    Word                   offset     = (Word) (address - m_fetchPc);
    Byte                   bit        = 0;



    if (offset < kMaxLength)
    {
        bit = (Byte) (1u << offset);
    }

    if ((m_fetchesLeft & bit) != 0)
    {
        m_fetchesLeft = (Byte) (m_fetchesLeft & ~bit);
        return true;
    }

    if (m_storeTarget == address)
    {
        m_storeTarget.reset();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::ResolveRoutines
//
//  Each RWTS and driver breakpoint's routine as its vector has it now, read
//  by peeks; none while the vector has no target. A machine with no Disk II
//  resolves a driver breakpoint as slot 6's, as the session does.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStopTest::ResolveRoutines()
{
    static constexpr int   kDefaultSlot = 6;
    IDebugTarget         & target       = m_session.GetTarget();
    DebugDiskState         state;
    int                    slot         = target.TryGetDiskState (state) ? state.slot : kDefaultSlot;



    m_routines.clear();
    m_vectorBytes.clear();

    for (const Breakpoint & entry : m_session.GetBreakpoints().GetAll())
    {
        std::optional<Word>  routine = entry.IsDiskCall() ? DiskCallVector::TryResolve (*entry.disk, slot, m_session) : std::nullopt;



        if (entry.IsDiskCall())
        {
            DiskCallVector::AddVectorBytes (*entry.disk, slot, m_vectorBytes);
        }

        if (routine.has_value())
        {
            m_routines[entry.id] = *routine;
        }
    }
}
