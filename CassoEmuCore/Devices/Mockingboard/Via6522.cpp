#include "Pch.h"

#include "Via6522.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
//  Reads that touch a timer's low counter clear that timer's interrupt
//  flag; every other read is side-effect free. Port reads combine the
//  output register (for output pins) with the external input latch (for
//  input pins).
//
////////////////////////////////////////////////////////////////////////////////

Byte Via6522::ReadRegister (Byte reg)
{
    Byte   result = 0;

    switch (reg & kRegisterMask)
    {
    case kRegOrb:
        result = GetPortB ();
        break;

    case kRegOra:
    case kRegOraNh:
        result = GetPortA ();
        break;

    case kRegDdrb:
        result = m_ddrb;
        break;

    case kRegDdra:
        result = m_ddra;
        break;

    case kRegT1CL:
        ClearFlag (kIrqTimer1);
        result = static_cast<Byte> (m_t1Counter & 0xFF);
        break;

    case kRegT1CH:
        result = static_cast<Byte> ((m_t1Counter >> 8) & 0xFF);
        break;

    case kRegT1LL:
        result = m_t1LatchLo;
        break;

    case kRegT1LH:
        result = m_t1LatchHi;
        break;

    case kRegT2CL:
        ClearFlag (kIrqTimer2);
        result = static_cast<Byte> (m_t2Counter & 0xFF);
        break;

    case kRegT2CH:
        result = static_cast<Byte> ((m_t2Counter >> 8) & 0xFF);
        break;

    case kRegSr:
        result = m_sr;
        break;

    case kRegAcr:
        result = m_acr;
        break;

    case kRegPcr:
        result = m_pcr;
        break;

    case kRegIfr:
        result = GetIfr ();
        break;

    case kRegIer:
        result = GetIer ();
        break;

    default:
        break;
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  WriteRegister
//
//  Loading T1C-H transfers the latch to the counter, arms the timer, and
//  clears the T1 flag; writing T1L-H or T2C-H clears the respective flag.
//  IFR writes clear the flagged bits (write-1-to-clear); IER writes set or
//  clear the flagged enables depending on bit 7.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::WriteRegister (Byte reg, Byte value)
{
    switch (reg & kRegisterMask)
    {
    case kRegOrb:
        m_orb = value;
        break;

    case kRegOra:
    case kRegOraNh:
        m_ora = value;
        break;

    case kRegDdrb:
        m_ddrb = value;
        break;

    case kRegDdra:
        m_ddra = value;
        break;

    case kRegT1CL:
        m_t1LatchLo = value;
        break;

    case kRegT1CH:
        m_t1LatchHi = value;
        m_t1Counter = (static_cast<int32_t> (m_t1LatchHi) << 8) | m_t1LatchLo;
        m_t1Armed   = true;
        ClearFlag (kIrqTimer1);
        break;

    case kRegT1LL:
        m_t1LatchLo = value;
        break;

    case kRegT1LH:
        m_t1LatchHi = value;
        ClearFlag (kIrqTimer1);
        break;

    case kRegT2CL:
        m_t2LatchLo = value;
        break;

    case kRegT2CH:
        m_t2Counter = (static_cast<int32_t> (value) << 8) | m_t2LatchLo;
        m_t2Armed   = true;
        ClearFlag (kIrqTimer2);
        break;

    case kRegSr:
        m_sr = value;
        break;

    case kRegAcr:
        m_acr = value;
        break;

    case kRegPcr:
        m_pcr = value;
        break;

    case kRegIfr:
        // Write-1-to-clear the flag bits; bit 7 is not a real flag.
        m_ifr &= static_cast<Byte> (~(value & 0x7F));
        UpdateIrq ();
        break;

    case kRegIer:
        if (value & kIerSetClear)
        {
            m_ier |= static_cast<Byte> (value & 0x7F);
        }
        else
        {
            m_ier &= static_cast<Byte> (~(value & 0x7F));
        }
        UpdateIrq ();
        break;

    default:
        break;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::Tick (uint32_t cycles)
{
    if (cycles == 0)
    {
        return;
    }

    TickTimer1 (cycles);
    TickTimer2 (cycles);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::Reset ()
{
    m_ora   = 0;
    m_orb   = 0;
    m_ddra  = 0;
    m_ddrb  = 0;
    m_portAIn = 0;
    m_portBIn = 0;

    m_sr    = 0;
    m_acr   = 0;
    m_pcr   = 0;

    m_ifr   = 0;
    m_ier   = 0;

    m_t1LatchLo = 0;
    m_t1LatchHi = 0;
    m_t1Counter = 0;
    m_t1Armed   = false;

    m_t2LatchLo = 0;
    m_t2Counter = 0;
    m_t2Armed   = false;

    UpdateIrq ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Via6522::AttachInterruptController (IInterruptController * ic)
{
    HRESULT   hr = S_OK;

    CBRAEx (ic, E_INVALIDARG);

    hr = ic->RegisterSource (m_irqSource);
    CHR (hr);

    m_ic       = ic;
    m_irqBound = true;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetIfr
//
//  IFR bit 7 is a computed summary: set whenever any enabled flag is
//  pending. The stored m_ifr holds only bits 0..6.
//
////////////////////////////////////////////////////////////////////////////////

Byte Via6522::GetIfr () const
{
    Byte   ifr = static_cast<Byte> (m_ifr & 0x7F);

    if ((m_ifr & m_ier & 0x7F) != 0)
    {
        ifr |= kIrqAny;
    }

    return ifr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  TickTimer1
//
//  16-bit down-counter. The counter reaches its first underflow counter+1
//  cycles after a load. In continuous mode it reloads from the latch and
//  keeps firing every latch+1 cycles; in one-shot mode it fires once and
//  then free-runs through 0xFFFF without setting the flag again. A single
//  batched Tick may cross several periods, so the counter position is
//  restored with modular arithmetic.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::TickTimer1 (uint32_t cycles)
{
    bool      freeRun     = (m_acr & kAcrT1Continuous) != 0;
    int64_t   counter     = m_t1Counter;
    int64_t   toUnderflow = counter + 1;
    int64_t   remaining   = 0;
    int64_t   latch       = (static_cast<int64_t> (m_t1LatchHi) << 8) | m_t1LatchLo;
    int64_t   period      = latch + 1;
    int64_t   into        = 0;

    if (static_cast<int64_t> (cycles) < toUnderflow)
    {
        m_t1Counter = static_cast<int32_t> (counter - cycles);
        return;
    }

    if (m_t1Armed)
    {
        SetFlag (kIrqTimer1);

        if (!freeRun)
        {
            m_t1Armed = false;
        }
    }

    remaining = static_cast<int64_t> (cycles) - toUnderflow;

    if (freeRun)
    {
        into        = remaining % period;
        m_t1Counter = static_cast<int32_t> (latch - into);
    }
    else
    {
        m_t1Counter = static_cast<int32_t> (0xFFFF - (remaining & 0xFFFF));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  TickTimer2
//
//  One-shot timed mode only. Fires once on underflow, then free-runs
//  through 0xFFFF. PB6 pulse counting (ACR bit 5) is not modelled.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::TickTimer2 (uint32_t cycles)
{
    int64_t   counter     = m_t2Counter;
    int64_t   toUnderflow = counter + 1;
    int64_t   remaining   = 0;

    if (static_cast<int64_t> (cycles) < toUnderflow)
    {
        m_t2Counter = static_cast<int32_t> (counter - cycles);
        return;
    }

    if (m_t2Armed)
    {
        SetFlag (kIrqTimer2);
        m_t2Armed = false;
    }

    remaining   = static_cast<int64_t> (cycles) - toUnderflow;
    m_t2Counter = static_cast<int32_t> (0xFFFF - (remaining & 0xFFFF));
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetFlag / ClearFlag
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::SetFlag (Byte flag)
{
    m_ifr |= static_cast<Byte> (flag & 0x7F);

    UpdateIrq ();
}


void Via6522::ClearFlag (Byte flag)
{
    m_ifr &= static_cast<Byte> (~(flag & 0x7F));

    UpdateIrq ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIrq
//
//  Level-sensitive: the line follows (IFR & IER) continuously.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::UpdateIrq ()
{
    if (!m_irqBound || (m_ic == nullptr))
    {
        return;
    }

    if ((m_ifr & m_ier & 0x7F) != 0)
    {
        m_ic->Assert (m_irqSource);
    }
    else
    {
        m_ic->Clear (m_irqSource);
    }
}
