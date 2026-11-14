#include "Pch.h"

#include "Devices/BlockDevice/IScsiTarget.h"
#include "Devices/BlockDevice/ScsiBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AttachTarget
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::AttachTarget (IScsiTarget * target)
{
    int  id = 0;



    if (target == nullptr)
    {
        return;
    }

    id = target->GetScsiId();

    if (id >= 0 && id < kMaxIds)
    {
        m_targets[(size_t) id] = target;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DetachTargets
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::DetachTargets()
{
    Release();
    m_targets.fill (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInitiatorLines
//
//  The initiator's lines changed. RST resets every target and frees the bus;
//  otherwise the change is a selection, the end of one, or an edge of ACK.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::SetInitiatorLines (const ScsiInitiatorLines & lines)
{
    bool  wasAck = m_initiator.ack;



    m_initiator = lines;

    if (lines.rst)
    {
        for (IScsiTarget * target : m_targets)
        {
            if (target != nullptr)
            {
                target->ResetTarget();
            }
        }

        Release();
        return;
    }

    switch (m_phase)
    {
        case ScsiPhase::BusFree:
            if (lines.sel && !lines.bsy)
            {
                TrySelect();
            }

            break;

        case ScsiPhase::Selection:
            if (!lines.sel)
            {
                EnterPhase (lines.atn ? ScsiPhase::MessageOut : ScsiPhase::Command, 0);
            }

            break;

        default:
            if (!wasAck && lines.ack)
            {
                OnAckAsserted();
            }
            else if (wasAck && !lines.ack)
            {
                OnAckReleased();
            }

            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetData
//
////////////////////////////////////////////////////////////////////////////////

Byte ScsiBus::GetData() const
{
    Byte  initiator = m_initiator.driveData ? m_initiator.data : (Byte) 0;
    Byte  target    = (m_targetBsy && m_io) ? m_targetData : (Byte) 0;



    return (Byte) (initiator | target);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsReq
//
////////////////////////////////////////////////////////////////////////////////

bool ScsiBus::IsReq() const
{
    return m_req && GetNow() >= m_reqAtCycle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCommandLength
//
//  The length of a command from its group code, the opcode's top three bits.
//
////////////////////////////////////////////////////////////////////////////////

size_t ScsiBus::GetCommandLength (Byte opcode)
{
    constexpr int     kGroupShift = 5;
    constexpr size_t  kGroup0     = 6;
    constexpr size_t  kGroup1     = 10;
    constexpr size_t  kGroup5     = 12;
    int               group       = opcode >> kGroupShift;



    switch (group)
    {
        case 1:
        case 2:
            return kGroup1;

        case 5:
            return kGroup5;

        default:
            return kGroup0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Release
//
//  The target lets go of the bus.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::Release()
{
    m_selected   = nullptr;
    m_phase      = ScsiPhase::BusFree;
    m_targetBsy  = false;
    m_req        = false;
    m_msg        = false;
    m_cd         = false;
    m_io         = false;
    m_targetData = 0;
    m_index      = 0;
    m_expected   = 0;
    m_buffer.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrySelect
//
//  SEL with BSY released: a selectable target whose ID bit is on the data
//  lines asserts BSY.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::TrySelect()
{
    Byte  data = GetData();



    for (int id = 0; id < kMaxIds; id++)
    {
        IScsiTarget  * target   = m_targets[(size_t) id];
        bool           isCalled = (data & (1 << id)) != 0;



        if (target == nullptr || !isCalled || !target->IsSelectable())
        {
            continue;
        }

        m_selected  = target;
        m_targetBsy = true;
        m_phase     = ScsiPhase::Selection;
        return;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnterPhase
//
//  Sets MSG, C/D and I/O for the phase, puts the first byte on the bus in an
//  input phase, and asserts REQ after the delay.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::EnterPhase (ScsiPhase phase, uint64_t delayCycles)
{
    constexpr Byte  kCommandComplete = 0x00;



    m_phase      = phase;
    m_msg        = phase == ScsiPhase::MessageOut || phase == ScsiPhase::MessageIn;
    m_cd         = phase == ScsiPhase::MessageOut || phase == ScsiPhase::Command || phase == ScsiPhase::Status || phase == ScsiPhase::MessageIn;
    m_io         = phase == ScsiPhase::DataIn     || phase == ScsiPhase::Status  || phase == ScsiPhase::MessageIn;
    m_index      = 0;
    m_req        = true;
    m_reqAtCycle = GetNow() + delayCycles;

    switch (phase)
    {
        case ScsiPhase::MessageOut:
        case ScsiPhase::Command:
            m_buffer.clear();
            m_expected = 0;
            break;

        case ScsiPhase::DataOut:
            m_buffer.clear();
            break;

        case ScsiPhase::DataIn:
            m_targetData = m_buffer.empty() ? (Byte) 0 : m_buffer[0];
            break;

        case ScsiPhase::Status:
            m_targetData = m_status;
            break;

        case ScsiPhase::MessageIn:
            m_targetData = kCommandComplete;
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAckAsserted
//
//  The initiator took the byte the target offered, or offers one: the target
//  latches an output byte and releases REQ.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::OnAckAsserted()
{
    bool  isOutput = m_phase == ScsiPhase::MessageOut || m_phase == ScsiPhase::Command || m_phase == ScsiPhase::DataOut;
    bool  isReq    = IsReq();



    if (!isReq)
    {
        return;
    }

    if (isOutput)
    {
        m_buffer.push_back (GetData());
    }

    m_req = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnAckReleased
//
//  The end of one byte's handshake: the target offers the next byte, or
//  moves to the next phase.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::OnAckReleased()
{
    if (m_req)
    {
        return;
    }

    switch (m_phase)
    {
        case ScsiPhase::MessageOut:
            if (m_initiator.atn)
            {
                m_req        = true;
                m_reqAtCycle = GetNow();
            }
            else
            {
                EnterPhase (ScsiPhase::Command, 0);
            }

            break;

        case ScsiPhase::Command:
            if (m_buffer.size() == 1)
            {
                m_expected = GetCommandLength (m_buffer[0]);
            }

            if (m_buffer.size() < m_expected)
            {
                m_req        = true;
                m_reqAtCycle = GetNow();
            }
            else
            {
                RunCommand();
            }

            break;

        case ScsiPhase::DataOut:
            if (m_buffer.size() < m_expected)
            {
                m_req        = true;
                m_reqAtCycle = GetNow();
            }
            else
            {
                FinishDataOut();
            }

            break;

        case ScsiPhase::DataIn:
            m_index++;

            if (m_index < m_buffer.size())
            {
                m_targetData = m_buffer[m_index];
                m_req        = true;
                m_reqAtCycle = GetNow();
            }
            else
            {
                EnterPhase (ScsiPhase::Status, 0);
            }

            break;

        case ScsiPhase::Status:
            EnterPhase (ScsiPhase::MessageIn, 0);
            break;

        case ScsiPhase::MessageIn:
            Release();
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RunCommand
//
//  All the command's bytes are in: the target executes it, and the bus moves
//  to data out, data in, or status.
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::RunCommand()
{
    ScsiCommandResult  result;



    m_lastCommand = m_buffer;
    m_selected->BeginCommand (m_buffer, GetNow(), result);
    m_status = (Byte) result.status;

    if (result.dataOutLength > 0)
    {
        m_expected = result.dataOutLength;
        EnterPhase (ScsiPhase::DataOut, result.delayCycles);
    }
    else if (!result.dataIn.empty())
    {
        m_buffer = std::move (result.dataIn);
        EnterPhase (ScsiPhase::DataIn, result.delayCycles);
    }
    else
    {
        EnterPhase (ScsiPhase::Status, result.delayCycles);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FinishDataOut
//
////////////////////////////////////////////////////////////////////////////////

void ScsiBus::FinishDataOut()
{
    uint64_t  delay = 0;



    m_status = (Byte) m_selected->FinishDataOut (m_buffer, GetNow(), delay);
    EnterPhase (ScsiPhase::Status, delay);
}
