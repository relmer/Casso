#include "Pch.h"

#include "Devices/Via6522.h"
#include "Debugger/IDiagnosticsProvider.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Debugger/DiagnosticsSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReadRegister
//
//  Reads that touch a port with handshaking or a timer's low counter clear
//  that interrupt flag; every other read is side-effect free.
//
////////////////////////////////////////////////////////////////////////////////

Byte Via6522::ReadRegister (Byte reg)
{
    Byte  result = PeekRegister (reg);



    switch (reg & kRegisterMask)
    {
    case kRegOrb:   ClearFlag (kIrqCb1);    OnPortBAccess (false); break;
    case kRegOra:   ClearFlag (kIrqCa1);    OnPortAAccess();       break;   // $F, the no-handshake alias, does not
    case kRegT1CL:  ClearFlag (kIrqTimer1);                        break;
    case kRegT2CL:  ClearFlag (kIrqTimer2);                        break;
    case kRegSr:    StartShift();                                  break;
    default:                                                       break;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PeekRegister
//
//  Port reads combine the output register (for output pins) with the
//  external input latch (for input pins).
//
////////////////////////////////////////////////////////////////////////////////

Byte Via6522::PeekRegister (Byte reg) const
{
    Byte  result = 0;



    switch (reg & kRegisterMask)
    {
    case kRegOrb:   result = GetPortB();                                    break;
    case kRegOra:
    case kRegOraNh: result = GetPortA();                                    break;
    case kRegDdrb:  result = m_ddrb;                                        break;
    case kRegDdra:  result = m_ddra;                                        break;
    case kRegT1CL:  result = static_cast<Byte> (m_t1Counter & 0xFF);        break;
    case kRegT1CH:  result = static_cast<Byte> ((m_t1Counter >> 8) & 0xFF); break;
    case kRegT1LL:  result = m_t1LatchLo;                                   break;
    case kRegT1LH:  result = m_t1LatchHi;                                   break;
    case kRegT2CL:  result = static_cast<Byte> (m_t2Counter & 0xFF);        break;
    case kRegT2CH:  result = static_cast<Byte> ((m_t2Counter >> 8) & 0xFF); break;
    case kRegSr:    result = m_sr;                                          break;
    case kRegAcr:   result = m_acr;                                         break;
    case kRegPcr:   result = m_pcr;                                         break;
    case kRegIfr:   result = GetIfr();                                      break;
    case kRegIer:   result = GetIer();                                      break;
    default:                                                                break;
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
        ClearFlag (kIrqCb1);
        OnPortBAccess (true);
        m_orb = value;
        break;

    case kRegOra:
        ClearFlag (kIrqCa1);
        OnPortAAccess();
        m_ora = value;
        break;

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
        m_t2Start   = m_t2Counter;
        m_t2Armed   = true;
        ClearFlag (kIrqTimer2);
        break;

    case kRegSr:
        m_sr = value;
        StartShift();
        break;

    case kRegAcr:
        // The Timer1 continuous-mode bit and the shift register mode change
        // behavior here. The rest -- T2 pulse counting, PB7 output, port
        // input latching -- are stored for read-back and are otherwise inert.
        //
        // Configuring one is NOT a fault. `LDA #$C0 / STA ACR` is the ordinary
        // way to arm a Mockingboard's timer, T1 continuous with PB7 output
        // enabled in a single store, and PB7 drives nothing on this card. This
        // once asserted on any bit but $40, which stopped the machine with a
        // dialog on a title doing something entirely routine.
        m_acr = value;
        break;

    case kRegPcr:
        // The CA1/CB1 active-edge selects and the CA2/CB2 modes. A speech
        // card drives its ready line into CA1 or CB1, and an SSI-263 driver
        // parks CA2 at a fixed output level in the same store that selects
        // the CA1 edge. A handshake output starts high.
        m_pcr    = value;
        m_ca2Out = true;
        m_cb2Out = true;
        break;

    case kRegIfr:
        // Write-1-to-clear the flag bits; bit 7 is not a real flag.
        m_ifr &= static_cast<Byte> (~(value & 0x7F));
        UpdateIrq();
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

        UpdateIrq();
        break;

    default:
        break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
//  Advance both timers by `cycles` phi2 clocks. A zero count is a harmless
//  no-op -- each timer step is a no-op when it cannot underflow.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::Tick (uint32_t cycles)
{
    TickTimer1 (cycles);
    TickTimer2 (cycles);
    TickShift  (cycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Clears every VIA register to its documented power-on state.
//
//  Zeroing the DATA DIRECTION registers is what makes the reset safe: both
//  ports become inputs, so nothing is driven until software deliberately
//  configures a direction. A reset that left an output configured would
//  briefly drive whatever the output register happened to hold.
//
//  Both timers are DISARMED as well as zeroed. A counter at zero is a valid
//  state that would otherwise underflow on the next tick and raise a spurious
//  interrupt immediately after reset.
//
//  UpdateIrq is called at the end rather than relying on the cleared flag and
//  enable registers, because the IRQ line is shared with other sources through
//  the controller -- this is what actually releases it.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::Reset()
{
    m_ora   = 0;
    m_orb   = 0;
    m_ddra  = 0;
    m_ddrb  = 0;
    m_portAIn = 0;
    m_portBIn = 0;

    m_ca1    = true;
    m_cb1    = true;
    m_ca2    = true;
    m_cb2    = true;
    m_ca2Out = true;
    m_cb2Out = true;

    m_sr        = 0;
    m_srCount   = kSrBits;
    m_srDivider = 0;
    m_acr       = 0;
    m_pcr       = 0;

    m_ifr   = 0;
    m_ier   = 0;

    m_t1LatchLo = 0;
    m_t1LatchHi = 0;
    m_t1Counter = 0;
    m_t1Armed   = false;

    m_t2LatchLo = 0;
    m_t2Start   = 0;
    m_t2Counter = 0;
    m_t2Armed   = false;

    UpdateIrq();
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

Byte Via6522::GetIfr() const
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
//  cycles after a load, and reads $FFFF for the cycle after it reaches zero.
//  In continuous mode the latch reloads at the end of that $FFFF cycle, so
//  the timer keeps firing every latch+2 cycles. That is the free-run timing
//  in both the Rockwell R6522 data sheet (Figure 16) and the WDC W65C22 data
//  sheet (Figure 2-4): N+1.5 cycles from the T1C-H write to the first
//  interrupt, then N+2 cycles between interrupts. The first time-out here,
//  counter+1 cycles after the load, is that N+1.5 rounded down to a whole
//  cycle. In one-shot mode it fires once and then free-runs through $FFFF
//  without setting the flag again. A single batched Tick may cross several
//  periods, so the counter position is restored with modular arithmetic.
//
//  The $FFFF cycle of continuous mode is held as a count of -1, which reads
//  as $FFFF. Counting resumes from it as from latch+1, so the next cycle
//  reads the latch; a one-shot (the ACR changed since the time-out) counts
//  on down from $FFFF instead. A tick of no cycles leaves the counter where
//  it is.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::TickTimer1 (uint32_t cycles)
{
    constexpr int32_t  kReloading  = -1;
    constexpr int64_t  kTopCount   = 0xFFFF;
    bool               freeRun     = (m_acr & kAcrT1Continuous) != 0;
    int64_t            elapsed     = cycles;
    int64_t            latch       = (static_cast<int64_t> (m_t1LatchHi) << 8) | m_t1LatchLo;
    int64_t            reloadCount = latch + 1;
    int64_t            period      = reloadCount + 1;
    int64_t            counter     = m_t1Counter;
    int64_t            toUnderflow = 0;
    int64_t            remaining   = 0;
    int64_t            into        = 0;



    if (counter == kReloading)
    {
        counter = freeRun ? reloadCount : kTopCount;
    }

    toUnderflow = counter + 1;

    if (elapsed == 0)
    {
        // Nothing to count, which also keeps a reload cycle reading $FFFF.
    }
    else if (elapsed < toUnderflow)
    {
        m_t1Counter = static_cast<int32_t> (counter - elapsed);
    }
    else
    {
        if (m_t1Armed)
        {
            SetFlag (kIrqTimer1);

            if (!freeRun)
            {
                m_t1Armed = false;
            }
        }

        remaining = elapsed - toUnderflow;

        if (freeRun)
        {
            into        = remaining % period;
            m_t1Counter = (into == 0) ? kReloading : static_cast<int32_t> (reloadCount - into);
        }
        else
        {
            m_t1Counter = static_cast<int32_t> (kTopCount - (remaining & kTopCount));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickTimer2
//
//  One-shot timed mode only. Fires once on underflow, then free-runs
//  through 0xFFFF. PB6 pulse counting (ACR bit 5) is not modeled.
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
    }
    else
    {
        if (m_t2Armed)
        {
            SetFlag (kIrqTimer2);
            m_t2Armed = false;
        }

        remaining   = static_cast<int64_t> (cycles) - toUnderflow;
        m_t2Counter = static_cast<int32_t> (0xFFFF - (remaining & 0xFFFF));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCa1
//
//  Drives the CA1 control-line input from an external peripheral. The active
//  edge is selected by PCR bit 0 (0 = falling, 1 = rising); crossing it latches
//  the CA1 interrupt flag, which software clears by accessing ORA ($1) or by
//  writing the bit back to IFR.
//
//  A device that never calls this leaves the line at its idle-high reset value,
//  so no edge is ever seen and the VIA behaves exactly as it did before control
//  lines were modeled. That is what keeps a card without a handshaking
//  peripheral free of interrupt sources it never had.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::SetCa1 (bool level)
{
    bool   rising = false;
    bool   active = false;



    rising = (!m_ca1 && level);
    active = ((m_pcr & kPcrCa1Rising) != 0) ? rising : (m_ca1 && !level);

    m_ca1 = level;

    if (active)
    {
        SetFlag (kIrqCa1);

        if (GetCa2Mode() == kC2OutHandshake)
        {
            m_ca2Out = true;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCb1
//
//  CB1 counterpart of SetCa1. The active edge is selected by PCR bit 4 and the
//  flag is cleared by accessing ORB ($0). CB1 is also the shift register's
//  external clock: a bit is shifted in on the rising edge and out on the
//  falling edge.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::SetCb1 (bool level)
{
    bool   rising    = false;
    bool   falling   = false;
    bool   active    = false;
    Byte   shiftMode = GetShiftMode();
    bool   isRunning = m_srCount < kSrBits;
    bool   isShiftIn = false;
    bool   isShift   = false;



    rising    = (!m_cb1 && level);
    falling   = (m_cb1 && !level);
    active    = ((m_pcr & kPcrCb1Rising) != 0) ? rising : falling;
    isShiftIn = shiftMode == kSrInCb1 && rising;
    isShift   = isRunning && (isShiftIn || (shiftMode == kSrOutCb1 && falling));

    m_cb1 = level;

    if (active)
    {
        SetFlag (kIrqCb1);

        if (GetCb2Mode() == kC2OutHandshake)
        {
            m_cb2Out = true;
        }
    }

    if (isShift)
    {
        ShiftOne();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCa2
//
//  Drives the CA2 input. In an input mode, the edge the mode selects sets
//  the CA2 flag. An output mode ignores the input.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::SetCa2 (bool level)
{
    bool  active = IsActiveEdge (GetCa2Mode(), m_ca2, level);



    m_ca2 = level;

    if (active)
    {
        SetFlag (kIrqCa2);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCb2
//
//  CB2 counterpart of SetCa2. While the shift register is enabled, CB2 is
//  its data line and doesn't set a flag; a bit shifted in is the level here.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::SetCb2 (bool level)
{
    bool  isShiftData = GetShiftMode() != kSrDisabled;
    bool  active      = !isShiftData && IsActiveEdge (GetCb2Mode(), m_cb2, level);



    m_cb2 = level;

    if (active)
    {
        SetFlag (kIrqCb2);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsActiveEdge
//
//  Whether a change of a CA2 or CB2 input sets its flag in an input mode.
//
////////////////////////////////////////////////////////////////////////////////

bool Via6522::IsActiveEdge (Byte mode, bool before, bool after)
{
    bool  isFallingMode = mode == kC2InFalling || mode == kC2InFallingIndependent;
    bool  isRisingMode  = mode == kC2InRising  || mode == kC2InRisingIndependent;



    return (isFallingMode && before && !after) || (isRisingMode && !before && after);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCa2
//
//  The pulse output returns high: its low pulse lasts one cycle, which is
//  over by the time the next access is made.
//
////////////////////////////////////////////////////////////////////////////////

bool Via6522::GetCa2() const
{
    Byte  mode  = GetCa2Mode();
    bool  level = m_ca2;



    switch (mode)
    {
    case kC2OutHandshake:   level = m_ca2Out; break;
    case kC2OutPulse:       level = true;     break;
    case kC2OutLow:         level = false;    break;
    case kC2OutHigh:        level = true;     break;
    default:                                  break;
    }

    return level;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCb2
//
////////////////////////////////////////////////////////////////////////////////

bool Via6522::GetCb2() const
{
    Byte  shiftMode = GetShiftMode();
    Byte  mode      = GetCb2Mode();
    bool  level     = m_cb2;



    if (shiftMode >= kSrOutFreeT2)
    {
        return m_cb2Out;
    }

    if (shiftMode != kSrDisabled)
    {
        return m_cb2;
    }

    switch (mode)
    {
    case kC2OutHandshake:   level = m_cb2Out; break;
    case kC2OutPulse:       level = true;     break;
    case kC2OutLow:         level = false;    break;
    case kC2OutHigh:        level = true;     break;
    default:                                  break;
    }

    return level;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPortAAccess
//
//  A read or write of ORA ($1) clears the CA2 flag unless CA2 is an
//  independent input, and starts a CA2 handshake.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::OnPortAAccess()
{
    Byte  mode = GetCa2Mode();



    if (mode == kC2InFalling || mode == kC2InRising)
    {
        ClearFlag (kIrqCa2);
    }

    if (mode == kC2OutHandshake)
    {
        m_ca2Out = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPortBAccess
//
//  A read or write of ORB ($0) clears the CB2 flag unless CB2 is an
//  independent input. Only a write starts a CB2 handshake.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::OnPortBAccess (bool isWrite)
{
    Byte  mode         = GetCb2Mode();
    bool  isShiftOwned = GetShiftMode() != kSrDisabled;



    if (mode == kC2InFalling || mode == kC2InRising)
    {
        ClearFlag (kIrqCb2);
    }

    if (isWrite && !isShiftOwned && mode == kC2OutHandshake)
    {
        m_cb2Out = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetShiftPeriod
//
//  Cycles per bit for the timed shift modes, or 0 for the others. Under
//  timer 2 the shift clock toggles each time T2's low counter runs out,
//  every latch + 2 cycles, and a bit takes a full clock period. Under the
//  system clock a bit takes two cycles.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Via6522::GetShiftPeriod() const
{
    constexpr uint32_t  kPhi2Period  = 2;
    constexpr uint32_t  kT2Overhead  = 2;
    constexpr uint32_t  kHalfPeriods = 2;
    Byte                mode         = GetShiftMode();
    uint32_t            period       = 0;



    switch (mode)
    {
    case kSrInT2:
    case kSrOutFreeT2:
    case kSrOutT2:
        period = kHalfPeriods * (m_t2LatchLo + kT2Overhead);
        break;

    case kSrInPhi2:
    case kSrOutPhi2:
        period = kPhi2Period;
        break;

    default:
        break;
    }

    return period;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartShift
//
//  A read or write of SR clears the shift flag and starts eight bits.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::StartShift()
{
    ClearFlag (kIrqShift);

    m_srCount   = 0;
    m_srDivider = GetShiftPeriod();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TickShift
//
//  Shifts the bits a timed mode clocks in `cycles`. Free-running output
//  rotates the register endlessly, so only the count modulo eight matters.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::TickShift (uint32_t cycles)
{
    uint32_t  period    = GetShiftPeriod();
    bool      isFree    = GetShiftMode() == kSrOutFreeT2;
    bool      isRunning = isFree || m_srCount < kSrBits;
    uint64_t  remaining = 0;
    uint64_t  shifts    = 0;



    if (period == 0 || !isRunning || cycles == 0)
    {
        return;
    }

    if (m_srDivider == 0 || m_srDivider > period)
    {
        m_srDivider = period;
    }

    if (cycles < m_srDivider)
    {
        m_srDivider -= cycles;
        return;
    }

    remaining   = cycles - m_srDivider;
    shifts      = 1 + remaining / period;
    m_srDivider = period - (uint32_t) (remaining % period);

    if (isFree)
    {
        for (uint64_t i = 0; i < shifts % kSrBits; i++)
        {
            ShiftOne();
        }

        m_cb2Out = (m_sr & 0x01) != 0;
        return;
    }

    for (uint64_t i = 0; i < shifts && m_srCount < kSrBits; i++)
    {
        ShiftOne();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShiftOne
//
//  Shifts one bit: in from CB2 at bit 0, or out of bit 7 to CB2 and back in
//  at bit 0. The eighth bit sets the shift flag and stops the register,
//  except under CB1 shifting out, where the count starts again.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::ShiftOne()
{
    constexpr Byte  kTopBit = 0x80;
    Byte            mode    = GetShiftMode();
    bool            out     = (m_sr & kTopBit) != 0;



    if (mode <= kSrInCb1)
    {
        m_sr = static_cast<Byte> ((m_sr << 1) | (m_cb2 ? 1 : 0));
    }
    else
    {
        m_sr     = static_cast<Byte> ((m_sr << 1) | (out ? 1 : 0));
        m_cb2Out = out;
    }

    if (mode == kSrOutFreeT2)
    {
        return;
    }

    m_srCount++;

    if (m_srCount >= kSrBits)
    {
        SetFlag (kIrqShift);

        if (mode == kSrOutCb1)
        {
            m_srCount = 0;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFlag
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::SetFlag (Byte flag)
{
    m_ifr |= static_cast<Byte> (flag & 0x7F);

    UpdateIrq();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearFlag
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::ClearFlag (Byte flag)
{
    m_ifr &= static_cast<Byte> (~(flag & 0x7F));

    UpdateIrq();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIrq
//
//  Level-sensitive: the line follows (IFR & IER) continuously. A VIA that
//  has not been bound to an interrupt controller simply drives nothing.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::UpdateIrq()
{
    if (m_irqBound && m_ic != nullptr)
    {
        if ((m_ifr & m_ier & 0x7F) != 0)
        {
            m_ic->Assert (m_irqSource);
        }
        else
        {
            m_ic->Clear (m_irqSource);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Via6522::AppendDiagnostics
//
//  The ports as the pins read, the timers' counts and latches, and IFR and
//  IER with a decode of each interrupt source.
//
//  IER shows the enables alone. Its bit 7 is the set/clear control of a
//  write, not an enable, and a read of the register returns it as 1, so
//  showing the register as read would light it whatever is enabled.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::AppendDiagnostics (const std::string & title, DiagnosticsSnapshot & snapshot) const
{
    using P = IDiagnosticsProvider;

    static constexpr std::array<const char *, 8>  kIrqBits  = { "IRQ", "T1", "T2", "CB1", "CB2", "SR", "CA1", "CA2" };
    static constexpr std::array<const char *, 8>  kIerBits  = { "",    "T1", "T2", "CB1", "CB2", "SR", "CA1", "CA2" };
    constexpr int                                 kByteBits = 8;
    DiagnosticsGroup                              group     { title, {} };
    Word                                          t1Latch   = (Word) ((m_t1LatchHi << kByteBits) | m_t1LatchLo);
    Byte                                          enables   = (Byte) (m_ier & ~kIerSetClear);



    group.rows.push_back (P::MakeHexRow  ("Port A",    GetPortA(), P::kByteDigits));
    group.rows.push_back (P::MakeHexRow  ("DDR A",     m_ddra,     P::kByteDigits));
    group.rows.push_back (P::MakeHexRow  ("Port B",    GetPortB(), P::kByteDigits));
    group.rows.push_back (P::MakeHexRow  ("DDR B",     m_ddrb,     P::kByteDigits));
    group.rows.push_back (P::MakeHexRow  ("T1 count",  GetTimer1(), P::kWordDigits));
    group.rows.push_back (P::MakeHexRow  ("T1 latch",  t1Latch,    P::kWordDigits));
    group.rows.push_back (P::MakeFlagRow ("T1 armed",  m_t1Armed));
    group.rows.push_back (P::MakeHexRow  ("T2 count",  GetTimer2(), P::kWordDigits));
    group.rows.push_back (P::MakeFlagRow ("T2 armed",  m_t2Armed));
    group.rows.push_back (P::MakeHexRow  ("ACR",       m_acr,      P::kByteDigits));
    group.rows.push_back (P::MakeHexRow  ("PCR",       m_pcr,      P::kByteDigits));
    group.rows.push_back (P::MakeByteRow ("IFR",       GetIfr(),   kIrqBits));
    group.rows.push_back (P::MakeByteRow ("IER",       enables,    kIerBits));

    snapshot.groups.push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Via6522::AppendTimerLevels
//
//  A timer is a bar only while it is armed, as the fraction left of the count
//  it started from: timer 1's latch, or what timer 2 was last started with.
//  The panel samples once a frame, so a timer whose count runs out faster
//  than that, or one counting with nothing armed (a fired one-shot wraps
//  through $FFFF about 15.6 times a second), would show a bar at a random
//  place each frame; it shows a steady "running" instead.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::AppendTimerLevels (const std::string & title, DiagnosticsMeters & meters) const
{
    //  The cycles in one 60 Hz frame of the 1.023 MHz Apple II clock.
    constexpr int    kFrameCycles = 17030;
    constexpr int    kByteBits    = 8;
    int              t1Latch      = (m_t1LatchHi << kByteBits) | m_t1LatchLo;
    auto             makeLevel    = [] (const std::string & name, bool armed, int count, int start)
    {
        DiagnosticsMeters::Level  level { name, 0.0f, {} };

        if (armed && start >= kFrameCycles)
        {
            level.level = std::clamp ((float) count / (float) start, 0.0f, 1.0f);
        }
        else
        {
            level.status = "running";
        }

        return level;
    };



    meters.levels.push_back (makeLevel (title + " T1", m_t1Armed, GetTimer1(), t1Latch));
    meters.levels.push_back (makeLevel (title + " T2", m_t2Armed, GetTimer2(), m_t2Start));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Via6522::AppendSoundBoard
//
//  T1 runs free at clock / (latch + 2) with ACR's continuous bit set, the only
//  case with a rate. It is running then or while armed. It fired if its IFR bit
//  is set, which is an interrupt only with its IER bit set too.
//
////////////////////////////////////////////////////////////////////////////////

void Via6522::AppendSoundBoard (double clockHz, DiagnosticsSoundVia & out) const
{
    constexpr int     kByteBits       = 8;
    constexpr double  kFreeRunPeriod  = 2.0;
    bool              isFreeRunning   = (m_acr & kAcrT1Continuous) != 0;



    out.ora       = GetOra();
    out.orb       = GetOrb();
    out.ddra      = GetDdra();
    out.ddrb      = GetDdrb();
    out.acr       = m_acr;
    out.pcr       = m_pcr;
    out.ifr       = GetIfr();
    out.ier       = (Byte) (m_ier & ~kIerSetClear);
    out.t1Count   = GetTimer1();
    out.t1Latch   = (uint16_t) ((m_t1LatchHi << kByteBits) | m_t1LatchLo);
    out.t2Count   = GetTimer2();
    out.t1RateHz  = isFreeRunning ? clockHz / (out.t1Latch + kFreeRunPeriod) : 0.0;
    out.isRunning = isFreeRunning || m_t1Armed;
    out.isFired   = (out.ifr & kIrqTimer1) != 0;
    out.isT1Irq   = (out.ifr & out.ier & kIrqTimer1) != 0;
    out.isIrq     = (out.ifr & kIrqAny) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveState
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Via6522::SaveState (StateWriter & writer) const
{
    writer.BeginSection (kStateTag, kStateVersion);

    writer.WriteByte   (m_ora);
    writer.WriteByte   (m_orb);
    writer.WriteByte   (m_ddra);
    writer.WriteByte   (m_ddrb);
    writer.WriteByte   (m_portAIn);
    writer.WriteByte   (m_portBIn);
    writer.WriteBool   (m_ca1);
    writer.WriteBool   (m_cb1);
    writer.WriteByte   (m_sr);
    writer.WriteByte   (m_acr);
    writer.WriteByte   (m_pcr);
    writer.WriteByte   (m_ifr);
    writer.WriteByte   (m_ier);
    writer.WriteByte   (m_t1LatchLo);
    writer.WriteByte   (m_t1LatchHi);
    writer.WriteUInt32 (static_cast<uint32_t> (m_t1Counter));
    writer.WriteBool   (m_t1Armed);
    writer.WriteByte   (m_t2LatchLo);
    writer.WriteUInt32 (static_cast<uint32_t> (m_t2Start));
    writer.WriteUInt32 (static_cast<uint32_t> (m_t2Counter));
    writer.WriteBool   (m_t2Armed);
    writer.WriteBool   (m_ca2);
    writer.WriteBool   (m_cb2);
    writer.WriteBool   (m_ca2Out);
    writer.WriteBool   (m_cb2Out);
    writer.WriteByte   (m_srCount);
    writer.WriteUInt32 (m_srDivider);

    return writer.EndSection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadState
//
//  IFR and IER hold source bits 0-6 only; a saved bit 7 in either is not
//  something this class ever stores, so it fails the load. A version 1
//  state predates the CA2, CB2 and shift register fields, which load as
//  after a reset.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Via6522::LoadState (StateReader & reader)
{
    HRESULT   hr        = S_OK;
    uint16_t  version   = 0;
    uint32_t  t1Counter = 0;
    uint32_t  t2Start   = 0;
    uint32_t  t2Counter = 0;



    hr = reader.BeginSection (kStateTag, kStateVersion, version);
    CHR (hr);

    reader.ReadByte   (m_ora);
    reader.ReadByte   (m_orb);
    reader.ReadByte   (m_ddra);
    reader.ReadByte   (m_ddrb);
    reader.ReadByte   (m_portAIn);
    reader.ReadByte   (m_portBIn);
    reader.ReadBool   (m_ca1);
    reader.ReadBool   (m_cb1);
    reader.ReadByte   (m_sr);
    reader.ReadByte   (m_acr);
    reader.ReadByte   (m_pcr);
    reader.ReadByte   (m_ifr);
    reader.ReadByte   (m_ier);
    reader.ReadByte   (m_t1LatchLo);
    reader.ReadByte   (m_t1LatchHi);
    reader.ReadUInt32 (t1Counter);
    reader.ReadBool   (m_t1Armed);
    reader.ReadByte   (m_t2LatchLo);
    reader.ReadUInt32 (t2Start);
    reader.ReadUInt32 (t2Counter);
    reader.ReadBool   (m_t2Armed);

    m_ca2       = true;
    m_cb2       = true;
    m_ca2Out    = true;
    m_cb2Out    = true;
    m_srCount   = kSrBits;
    m_srDivider = 0;

    if (version >= kFirstWithShiftRegister)
    {
        reader.ReadBool   (m_ca2);
        reader.ReadBool   (m_cb2);
        reader.ReadBool   (m_ca2Out);
        reader.ReadBool   (m_cb2Out);
        reader.ReadByte   (m_srCount);
        reader.ReadUInt32 (m_srDivider);
    }

    hr = reader.EndSection();
    CHR (hr);

    CBREx ((m_ifr & kIrqAny) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx ((m_ier & kIrqAny) == 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (m_srCount <= kSrBits,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_t1Counter = static_cast<int32_t> (t1Counter);
    m_t2Start   = static_cast<int32_t> (t2Start);
    m_t2Counter = static_cast<int32_t> (t2Counter);

Error:
    return hr;
}
