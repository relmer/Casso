#pragma once

#include "Pch.h"

#include "Core/IInterruptController.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Via6522
//
//  Clean-room MOS/Rockwell 6522 Versatile Interface Adapter, implemented
//  from the datasheet register map. Generic and reusable: the class knows
//  nothing about the Mockingboard -- it exposes a 16-register file, two
//  8-bit ports with data-direction registers, Timer 1 / Timer 2, and the
//  IFR/IER interrupt logic that drives a single IRQ line. The owning
//  device (MockingboardCard) maps CPU-bus addresses onto ReadRegister /
//  WriteRegister and advances the timers via Tick().
//
//  Register file (offset 0..15):
//
//    $0 ORB/IRB    $8 T2C-L (read clears T2 flag; write -> T2 low latch)
//    $1 ORA/IRA    $9 T2C-H (write loads counter, clears T2 flag, starts)
//    $2 DDRB       $A SR    (shift register -- stored, not clocked)
//    $3 DDRA       $B ACR   (T1/T2 mode select; SR/latch bits stored)
//    $4 T1C-L      $C PCR   (handshake control -- stored, not modelled)
//    $5 T1C-H      $D IFR   (bit7 = IRQ summary, bit6 = T1, bit5 = T2, ...)
//    $6 T1L-L      $E IER   (bit7 = set/clear control, per-source enables)
//    $7 T1L-H      $F ORA/IRA (same as $1, no CA handshake)
//
//  Timer 1 is a 16-bit down-counter clocked at the CPU phi2 rate. It is
//  loaded from the latch on a T1C-H write and underflows (setting IFR
//  bit 6) latch+1 cycles later. In one-shot mode (ACR bit 6 = 0) it fires
//  once; in continuous mode (ACR bit 6 = 1) it reloads from the latch and
//  re-fires every latch+1 cycles -- the periodic IRQ that Mockingboard
//  music players use for timing. Timer 2 is one-shot timed only; PB6
//  pulse counting (ACR bit 5 = 1) is not modelled.
//
//  Modelled: full register file, ports A/B + DDRs, Timer 1 (one-shot and
//  continuous) and Timer 2 (one-shot), IFR/IER and the level-sensitive
//  IRQ line. NOT modelled (registers stored, behaviour absent): the shift
//  register clocking, CA1/CA2/CB1/CB2 handshaking, PB6 pulse counting,
//  and PB7 timer output. The Mockingboard needs none of these.
//
////////////////////////////////////////////////////////////////////////////////

class Via6522
{
public:
    static constexpr Byte    kRegOrb    = 0x0;
    static constexpr Byte    kRegOra    = 0x1;
    static constexpr Byte    kRegDdrb   = 0x2;
    static constexpr Byte    kRegDdra   = 0x3;
    static constexpr Byte    kRegT1CL   = 0x4;
    static constexpr Byte    kRegT1CH   = 0x5;
    static constexpr Byte    kRegT1LL   = 0x6;
    static constexpr Byte    kRegT1LH   = 0x7;
    static constexpr Byte    kRegT2CL   = 0x8;
    static constexpr Byte    kRegT2CH   = 0x9;
    static constexpr Byte    kRegSr     = 0xA;
    static constexpr Byte    kRegAcr    = 0xB;
    static constexpr Byte    kRegPcr    = 0xC;
    static constexpr Byte    kRegIfr    = 0xD;
    static constexpr Byte    kRegIer    = 0xE;
    static constexpr Byte    kRegOraNh  = 0xF;
    static constexpr Byte    kRegisterCount = 16;
    static constexpr Byte    kRegisterMask  = 0x0F;

    // IFR / IER bit assignments (datasheet Table).
    static constexpr Byte    kIrqCa2    = 0x01;
    static constexpr Byte    kIrqCa1    = 0x02;
    static constexpr Byte    kIrqShift  = 0x04;
    static constexpr Byte    kIrqCb2    = 0x08;
    static constexpr Byte    kIrqCb1    = 0x10;
    static constexpr Byte    kIrqTimer2 = 0x20;
    static constexpr Byte    kIrqTimer1 = 0x40;
    static constexpr Byte    kIrqAny    = 0x80;

    // ACR control bits.
    static constexpr Byte    kAcrT2PulseCount = 0x20;   // 0 = timed one-shot
    static constexpr Byte    kAcrT1Continuous = 0x40;   // 0 = one-shot
    static constexpr Byte    kAcrT1Pb7Output  = 0x80;   // PB7 output (stored)

    // IER write control bit: 1 = set the flagged enables, 0 = clear them.
    static constexpr Byte    kIerSetClear = 0x80;

    Via6522 () { Reset (); }

    // Register-file access (reg is masked to 0..15). Reads of the timer
    // low-counter registers and writes of the flag registers have the
    // documented interrupt-flag side effects.
    Byte    ReadRegister  (Byte reg);
    void    WriteRegister (Byte reg, Byte value);

    // Advance both timers by `cycles` phi2 clocks, firing interrupts on
    // underflow.
    void    Tick          (uint32_t cycles);

    void    Reset         ();

    // Register the VIA's IRQ source with a caller-owned controller that
    // outlives the VIA. The line is level-sensitive: it is re-driven from
    // (IFR & IER) on every state change.
    HRESULT AttachInterruptController (IInterruptController * ic);

    // Effective pin state on each port: output bits come from the output
    // register, input bits from the external input latch.
    Byte    GetPortA      () const { return static_cast<Byte> ((m_ora & m_ddra) | (m_portAIn & ~m_ddra)); }
    Byte    GetPortB      () const { return static_cast<Byte> ((m_orb & m_ddrb) | (m_portBIn & ~m_ddrb)); }

    // Drive the external input pins (used by the card to hand the AY data
    // bus back to the CPU during a PSG read).
    void    SetPortAInput (Byte value) { m_portAIn = value; }
    void    SetPortBInput (Byte value) { m_portBIn = value; }

    // Inspectors for tests.
    Byte     GetIfr        () const;
    Byte     GetIer        () const { return static_cast<Byte> (m_ier | kIrqAny); }
    bool     IsIrqAsserted () const { return (m_ifr & m_ier & 0x7F) != 0; }
    uint16_t GetTimer1     () const { return static_cast<uint16_t> (m_t1Counter); }
    uint16_t GetTimer2     () const { return static_cast<uint16_t> (m_t2Counter); }
    Byte     GetOra        () const { return m_ora; }
    Byte     GetOrb        () const { return m_orb; }
    Byte     GetDdra       () const { return m_ddra; }
    Byte     GetDdrb       () const { return m_ddrb; }

private:
    void    TickTimer1  (uint32_t cycles);
    void    TickTimer2  (uint32_t cycles);
    void    SetFlag     (Byte flag);
    void    ClearFlag   (Byte flag);
    void    UpdateIrq   ();

    Byte     m_ora   = 0;
    Byte     m_orb   = 0;
    Byte     m_ddra  = 0;
    Byte     m_ddrb  = 0;
    Byte     m_portAIn = 0;
    Byte     m_portBIn = 0;

    Byte     m_sr    = 0;
    Byte     m_acr   = 0;
    Byte     m_pcr   = 0;

    // IFR holds flag bits 0..6 only; bit 7 (IRQ summary) is computed on read.
    Byte     m_ifr   = 0;
    Byte     m_ier   = 0;

    Byte     m_t1LatchLo = 0;
    Byte     m_t1LatchHi = 0;
    int32_t  m_t1Counter = 0;
    bool     m_t1Armed   = false;

    Byte     m_t2LatchLo = 0;
    int32_t  m_t2Counter = 0;
    bool     m_t2Armed   = false;

    IInterruptController *   m_ic        = nullptr;
    IrqSourceId              m_irqSource = 0;
    bool                     m_irqBound  = false;
};
