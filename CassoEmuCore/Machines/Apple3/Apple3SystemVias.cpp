#include "Pch.h"

#include "Apple3SystemVias.h"
#include "Apple3Mmu.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





static constexpr int  s_kSlotCount = 4;
static constexpr int  s_kSlot3     = 3;
static constexpr int  s_kSlot4     = 4;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SystemVias
//
////////////////////////////////////////////////////////////////////////////////

Apple3SystemVias::Apple3SystemVias (Apple3Mmu & mmu)
    : m_mmu (mmu)
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3SystemVias::AttachInterruptController (IInterruptController * ic)
{
    HRESULT  hr = S_OK;



    hr = m_d.AttachInterruptController (ic);
    CHR (hr);

    hr = m_e.AttachInterruptController (ic);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  A reset returns every port pin to an input, which the pull-ups read as
//  high. That deselects the Apple II lockout, which only a reset can do.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::Reset()
{
    m_d.Reset();
    m_e.Reset();

    m_eInputA = kPulledUp;

    m_d.SetPortAInput (kPulledUp);
    m_d.SetPortBInput (kPulledUp);
    m_e.SetPortAInput (m_eInputA);
    m_e.SetPortBInput (kPulledUp);

    UpdateOutputs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
////////////////////////////////////////////////////////////////////////////////

Byte Apple3SystemVias::Read (Word address)
{
    m_accesses++;

    return GetVia (address).ReadRegister ((Byte) address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::Write (Word address, Byte value)
{
    m_accesses++;

    GetVia (address).WriteRegister ((Byte) address, value);
    UpdateOutputs();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPeekIo
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3SystemVias::TryPeekIo (Word address, Byte & value)
{
    value = GetVia (address).PeekRegister ((Byte) address);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::Tick (uint32_t cycles)
{
    m_d.Tick (cycles);
    m_e.Tick (cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateOutputs
//
//  Passes the three registers and the lockout to the memory map.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::UpdateOutputs()
{
    m_mmu.SetEnvironment (m_d.GetPortA());
    m_mmu.SetZeroPage    (m_d.GetPortB());
    m_mmu.SetBank        ((Byte) (m_e.GetPortA() & kBankBits));
    m_mmu.SetLockout     (IsLockedOut());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsLockedOut
//
////////////////////////////////////////////////////////////////////////////////

bool Apple3SystemVias::IsLockedOut() const
{
    bool  isDriven = (m_e.GetDdra() & kLockout) != 0;



    return isDriven && (m_e.GetPortA() & kLockout) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPortAInputBit
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::SetPortAInputBit (Byte bit, bool high)
{
    m_eInputA = high ? (Byte) (m_eInputA | bit) : (Byte) (m_eInputA & ~bit);
    m_e.SetPortAInput (m_eInputA);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSlotInterrupt
//
//  Slots 3 and 4 each have an E VIA port A input, low while requesting.
//  The OR of every slot's request is the D VIA's CA1, the "any slot" line,
//  also low while any slot requests.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::SetSlotInterrupt (int slot, bool asserted)
{
    Byte  bit = 0;



    if (slot < 1 || slot > s_kSlotCount)
    {
        return;
    }

    bit = (Byte) (1u << (slot - 1));
    m_slotRequests = asserted ? (Byte) (m_slotRequests | bit) : (Byte) (m_slotRequests & ~bit);

    if (slot == s_kSlot4)
    {
        SetPortAInputBit (kSlot4Irq, !asserted);
    }
    else if (slot == s_kSlot3)
    {
        SetPortAInputBit (kSlot3Irq, !asserted);
    }

    m_d.SetCa1 (m_slotRequests == 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetInterruptLine
//
//  The CPU's interrupt line, low while asserted, wired back to E VIA port A
//  bit 7 for the diagnostics.
//
////////////////////////////////////////////////////////////////////////////////

void Apple3SystemVias::SetInterruptLine (bool asserted)
{
    SetPortAInputBit (kIrqLine, !asserted);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3SystemVias::SaveState (StateWriter & writer) const
{
    HRESULT  hr = S_OK;



    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte (m_eInputA);
    writer.WriteByte (m_slotRequests);

    hr = m_d.SaveState (writer);
    CHR (hr);

    hr = m_e.SaveState (writer);
    CHR (hr);

    hr = writer.EndSection();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Apple3SystemVias::LoadState (StateReader & reader)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte (m_eInputA);
    reader.ReadByte (m_slotRequests);

    hr = m_d.LoadState (reader);
    CHR (hr);

    hr = m_e.LoadState (reader);
    CHR (hr);

    hr = reader.EndSection();
    CHR (hr);

    UpdateOutputs();

Error:
    return hr;
}
