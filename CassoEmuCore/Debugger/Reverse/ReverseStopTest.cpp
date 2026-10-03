#include "Pch.h"

#include "Debugger/Reverse/ReverseStopTest.h"
#include "Debugger/DebugSession.h"
#include "Debugger/EffectiveAddress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStopTest::ShouldStopBefore
//
//  Breakpoints first, then before-mode watchpoints against what the
//  instruction would touch. The instruction's own reads are noted first, so
//  the accesses it makes when it runs are judged as a forward run judges
//  them.
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



    (void) machine;

    m_session.GetCpuOwnReads (pc, length, m_storeTarget);
    m_fetchPc     = pc;
    m_fetchesLeft = (Byte) ((1u << length) - 1);

    isPeeked = target.TryPeek (pc, opcode);

    if (m_session.GetBreakpoints().IsStopBefore (pc, isPeeked ? std::optional<Byte> (opcode) : std::nullopt, m_session))
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





