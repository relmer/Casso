#pragma once

#include "Pch.h"

#include "Core/IInterruptController.h"
#include "Core/IMachineState.h"

struct DiagnosticsMeters;
struct DiagnosticsSoundVia;
struct DiagnosticsSnapshot;





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
//    $2 DDRB       $A SR    (shift register)
//    $3 DDRA       $B ACR   (T1, T2 and shift register modes; latch bits stored)
//    $4 T1C-L      $C PCR   (CA1, CB1 edges; CA2, CB2 modes)
//    $5 T1C-H      $D IFR   (bit7 = IRQ summary, bit6 = T1, bit5 = T2, ...)
//    $6 T1L-L      $E IER   (bit7 = set/clear control, per-source enables)
//    $7 T1L-H      $F ORA/IRA (same as $1, no CA handshake)
//
//  Timer 1 is a 16-bit down-counter clocked at the CPU phi2 rate. It is
//  loaded from the latch on a T1C-H write and underflows (setting IFR
//  bit 6) latch+1 cycles later. In one-shot mode (ACR bit 6 = 0) it fires
//  once; in continuous mode (ACR bit 6 = 1) it reads $FFFF for one cycle,
//  reloads from the latch and re-fires every latch+2 cycles -- the periodic
//  IRQ that Mockingboard music players use for timing. Timer 2 is one-shot
//  timed only; PB6 pulse counting (ACR bit 5 = 1) is not modeled.
//
//  CA2 and CB2 are interrupt inputs on either edge, or outputs: handshake,
//  pulse, or a fixed level. The shift register shifts under timer 2, the
//  system clock or CB1, in or out through CB2, and sets its flag after
//  eight bits. Shifting out under CB1 counts on past eight and sets the flag
//  every eight bits; shifting out free-running under timer 2 never sets it.
//
//  Modeled: full register file, ports A/B + DDRs, Timer 1 (one-shot and
//  continuous) and Timer 2 (one-shot), the control lines, the shift
//  register, IFR/IER and the level-sensitive IRQ line. NOT modeled
//  (registers stored, behavior absent): PB6 pulse counting, PB7 timer
//  output and the port input latches.
//
////////////////////////////////////////////////////////////////////////////////

class Via6522 : public IMachineState
{
public:
    static constexpr Byte  kRegOrb        = 0x0;
    static constexpr Byte  kRegOra        = 0x1;
    static constexpr Byte  kRegDdrb       = 0x2;
    static constexpr Byte  kRegDdra       = 0x3;
    static constexpr Byte  kRegT1CL       = 0x4;
    static constexpr Byte  kRegT1CH       = 0x5;
    static constexpr Byte  kRegT1LL       = 0x6;
    static constexpr Byte  kRegT1LH       = 0x7;
    static constexpr Byte  kRegT2CL       = 0x8;
    static constexpr Byte  kRegT2CH       = 0x9;
    static constexpr Byte  kRegSr         = 0xA;
    static constexpr Byte  kRegAcr        = 0xB;
    static constexpr Byte  kRegPcr        = 0xC;
    static constexpr Byte  kRegIfr        = 0xD;
    static constexpr Byte  kRegIer        = 0xE;
    static constexpr Byte  kRegOraNh      = 0xF;
    static constexpr Byte  kRegisterCount = 16;
    static constexpr Byte  kRegisterMask  = 0x0F;

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

    // ACR bits 2-4: the shift register mode.
    static constexpr Byte    kAcrSrShift      = 2;
    static constexpr Byte    kAcrSrMask       = 0x07;
    static constexpr Byte    kSrDisabled      = 0;
    static constexpr Byte    kSrInT2          = 1;
    static constexpr Byte    kSrInPhi2        = 2;
    static constexpr Byte    kSrInCb1         = 3;
    static constexpr Byte    kSrOutFreeT2     = 4;
    static constexpr Byte    kSrOutT2         = 5;
    static constexpr Byte    kSrOutPhi2       = 6;
    static constexpr Byte    kSrOutCb1        = 7;
    static constexpr Byte    kSrBits          = 8;

    // PCR: bit 0 selects the CA1 active edge, bit 4 the CB1 active edge
    // (0 = falling, 1 = rising). Bits 1-3 are the CA2 mode and bits 5-7 the
    // CB2 mode.
    static constexpr Byte    kPcrCa1Rising = 0x01;
    static constexpr Byte    kPcrCb1Rising = 0x10;
    static constexpr Byte    kPcrCa2Shift  = 1;
    static constexpr Byte    kPcrCb2Shift  = 5;
    static constexpr Byte    kPcrModeMask  = 0x07;

    // CA2 and CB2 modes.
    static constexpr Byte    kC2InFalling            = 0;
    static constexpr Byte    kC2InFallingIndependent = 1;
    static constexpr Byte    kC2InRising             = 2;
    static constexpr Byte    kC2InRisingIndependent  = 3;
    static constexpr Byte    kC2OutHandshake         = 4;
    static constexpr Byte    kC2OutPulse             = 5;
    static constexpr Byte    kC2OutLow               = 6;
    static constexpr Byte    kC2OutHigh              = 7;

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

    // Port A with `input` on its input pins in place of the input latch.
    Byte    GetPortAWith  (Byte input) const { return static_cast<Byte> ((m_ora & m_ddra) | (input & ~m_ddra)); }

    // ReadRegister's byte with no flag cleared: the debugger's peek.
    Byte    PeekRegister  (Byte reg) const;
    Byte    GetPortB      () const { return static_cast<Byte> ((m_orb & m_ddrb) | (m_portBIn & ~m_ddrb)); }

    // Drive the external input pins (used by the card to hand the AY data
    // bus back to the CPU during a PSG read).
    void    SetPortAInput (Byte value) { m_portAIn = value; }
    void    SetPortBInput (Byte value) { m_portBIn = value; }

    void    SetCa1        (bool level);
    void    SetCb1        (bool level);
    void    SetCa2        (bool level);
    void    SetCb2        (bool level);

    bool    GetCa1        () const { return m_ca1; }
    bool    GetCb1        () const { return m_cb1; }

    // The CA2 and CB2 pins: the output level in an output mode (CB2 also
    // while the shift register shifts out), the input level otherwise.
    bool    GetCa2        () const;
    bool    GetCb2        () const;
    Byte    GetShiftMode  () const { return static_cast<Byte> ((m_acr >> kAcrSrShift) & kAcrSrMask); }

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

    // A debugger panel's rows for this chip, as one group, and each timer's
    // remaining count from 0 to 1 for the panel's meters, or "running" for a
    // timer a once-a-frame sample cannot follow.
    void     AppendDiagnostics (const std::string & title, DiagnosticsSnapshot & snapshot) const;
    void     AppendTimerLevels (const std::string & title, DiagnosticsMeters & meters) const;

    //  A sound board's view of this chip, whose T1 rate is at this clock.
    void     AppendSoundBoard  (double clockHz, DiagnosticsSoundVia & out) const;

    // IMachineState: the register file, port input latches, control lines and
    // both timers. The IRQ source token and controller are wiring; the
    // interrupt controller saves whether the line is held, so a load does not
    // drive it.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('V', 'I', 'A', ' ');
    static constexpr uint16_t  kStateVersion = 2;

private:
    static constexpr uint16_t  kFirstWithShiftRegister = 2;

    void      TickTimer1     (uint32_t cycles);
    void      TickTimer2     (uint32_t cycles);
    void      TickShift      (uint32_t cycles);
    void      StartShift     ();
    void      ShiftOne       ();
    void      OnPortAAccess  ();
    void      OnPortBAccess  (bool isWrite);
    Byte      GetCa2Mode     () const { return static_cast<Byte> ((m_pcr >> kPcrCa2Shift) & kPcrModeMask); }
    Byte      GetCb2Mode     () const { return static_cast<Byte> ((m_pcr >> kPcrCb2Shift) & kPcrModeMask); }
    uint32_t  GetShiftPeriod () const;
    void      SetFlag        (Byte flag);
    void      ClearFlag      (Byte flag);
    void      UpdateIrq      ();

    static bool  IsActiveEdge (Byte mode, bool before, bool after);

protected:
    Byte  m_ora     = 0;
    Byte  m_orb     = 0;
    Byte  m_ddra    = 0;
    Byte  m_ddrb    = 0;
    Byte  m_portAIn = 0;
    Byte  m_portBIn = 0;

    // Control-line inputs. Idle high, matching an undriven open-collector
    // peripheral output held up by its pull-up -- so a VIA whose lines are
    // never driven sees no edge and behaves exactly as it did before these
    // existed.
    bool  m_ca1 = true;
    bool  m_cb1 = true;
    bool  m_ca2 = true;
    bool  m_cb2 = true;

    // CA2's handshake output and CB2's handshake or shifted-out bit.
    bool  m_ca2Out = true;
    bool  m_cb2Out = true;

    Byte      m_sr        = 0;
    Byte      m_srCount   = kSrBits;   // bits shifted since the start; kSrBits when stopped
    uint32_t  m_srDivider = 0;         // cycles to the next timed shift
    Byte      m_acr       = 0;
    Byte      m_pcr       = 0;

    // IFR holds flag bits 0..6 only; bit 7 (IRQ summary) is computed on read.
    Byte     m_ifr   = 0;
    Byte     m_ier   = 0;

    Byte     m_t1LatchLo = 0;
    Byte     m_t1LatchHi = 0;
    int32_t  m_t1Counter = 0;     // -1 in the $FFFF cycle before a continuous reload
    bool     m_t1Armed   = false;

    Byte     m_t2LatchLo = 0;
    int32_t  m_t2Start   = 0;     // the count T2 was last started from
    int32_t  m_t2Counter = 0;
    bool     m_t2Armed   = false;

private:
    IInterruptController *   m_ic        = nullptr;
    IrqSourceId              m_irqSource = 0;
    bool                     m_irqBound  = false;
};
