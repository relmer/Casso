#pragma once

#include "Pch.h"
#include "Devices/AppleSoftSwitchBank.h"

class Apple2eMmu;
class Apple2eKeyboard;
class LanguageCard;
class IVideoTiming;
class IInputEventSink;
class IRomBankSwitch;




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eSoftSwitchBank
//
//  Extended IIe soft switches including 80-column and double hi-res.
//  $C000-$C00B write-switches are forwarded to the Apple2eMmu (set via
//  SetMmu); the MMU owns the canonical RAMRD/RAMWRT/ALTZP/80STORE/INTCXROM/
//  SLOTC3ROM flag state. Display-only flags (80COL, ALTCHARSET, PAGE2,
//  HIRES, TEXT, MIXED, DHIRES) remain owned here.
//
//  Phase 6 / T061 ownership split:
//    - This bank owns $C011-$C01F status reads. Bit 7 is sourced from
//      the canonical state device (LanguageCard / Apple2eMmu /
//      VideoTiming / this bank itself for the display flags). Bits 0-6
//      are the keyboard latch (floating-bus low 7 bits per the documented
//      Apple //e convention; FR-001, FR-003, audit §1.2). Status reads MUST NOT
//      clear the keyboard strobe — the data is fetched via the
//      keyboard's read-only GetLatchedKeyDataBits() accessor.
//    - The Apple2eKeyboard claims $C000-$C063 on the bus and forwards
//      $C011-$C01F reads here (strobe-clear isolation, audit §1.2, §4).
//
////////////////////////////////////////////////////////////////////////////////

class Apple2eSoftSwitchBank : public AppleSoftSwitchBank
{
public:
    Apple2eSoftSwitchBank (MemoryBus * bus = nullptr);

    Byte Read      (Word address) override;
    void Write     (Word address, Byte value) override;
    Word GetEnd    () const override { return 0xC07F; }
    void Reset     () override;
    void SoftReset () override;

    bool Is80ColMode    () const { return m_80colMode; }
    bool IsDoubleHiRes  () const { return m_doubleHiRes; }
    bool IsAltCharSet   () const { return m_altCharSet; }
    bool Is80Store      () const;

    void SetMmu          (Apple2eMmu * mmu)         { m_mmu          = mmu; }
    void SetKeyboard     (Apple2eKeyboard * kbd)    { m_keyboard     = kbd; }
    void SetLanguageCard (LanguageCard * lc)        { m_lc           = lc; }
    void SetVideoTiming  (IVideoTiming * vt)        { m_videoTiming  = vt; }

    // Apple //c only: the $C028 ROM-bank flip-flop. Null on the //e and
    // earlier, where $C028 has no effect.
    void SetRomBankSwitch (IRomBankSwitch * rb)     { m_romBank      = rb; }

    // Apple //c only: the IOU mouse (US4). When attached this bank serves
    // the mouse's soft-switch surface — $C015/$C017 (X0/Y0 interrupt
    // status), $C019 (VBL interrupt latch, replacing the //e RDVBLBAR),
    // $C066/$C067 (MOUX1/MOUY1 direction lines, replacing PADDL2/3),
    // $C070's VBL-latch clear side effect, $C078/$C079 (IOU access gate),
    // and $C058-$C05F as the IOU programming bank while access is enabled.
    // Null on the //e and earlier, where all legacy behavior stands.
    void SetMouse (class AppleMouse * mouse)        { m_mouse        = mouse; }

    // Wire the CPU bus-cycle accumulator that drives the PREAD paddle timer.
    void SetCpuCycleSource (const uint64_t * src) { m_cpuCycleSource = src; }

    // Attach the input-debug notification sink.
    void SetInputEventSink (IInputEventSink * sink) noexcept { m_inputSink = sink; }

    // Stage an analog axis position (0-255, s_knPaddleCenter = neutral).
    void SetPaddle (int axis, Byte position);

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    static constexpr Byte s_knPaddleCenter = 127;

private:
    static constexpr int      s_knPaddleAxisCount   = 4;
    static constexpr Word     s_kwPaddle0Address    = 0xC064;
    static constexpr Word     s_kwPaddleTimerStrobe = 0xC070;

    // PREAD's poll loop advances its counter once per ~11 CPU cycles, so an
    // axis holds bit 7 for position*11 cycles to yield a returned count equal
    // to the position. The //e game-port resistor-capacitor full-scale read
    // (~2.82 ms) lands at 255*11 cycles.
    static constexpr uint64_t s_knPaddleCyclesPerUnit = 11;

    Byte ReadStatusRegister (Word address);
    Byte ReadPaddle         (Word address) const;
    void EmitHostPaddle     (int axis, Byte value);
    void EmitPaddleTrigger  ();
    void EmitPaddleRead     (Word address, Byte value);

    MemoryBus *          m_bus                = nullptr;
    Apple2eMmu *         m_mmu                = nullptr;
    IRomBankSwitch *     m_romBank            = nullptr;
    class AppleMouse *   m_mouse              = nullptr;
    Apple2eKeyboard *    m_keyboard           = nullptr;
    LanguageCard *       m_lc                 = nullptr;
    IVideoTiming *       m_videoTiming        = nullptr;
    IInputEventSink *    m_inputSink          = nullptr;
    const uint64_t *     m_cpuCycleSource     = nullptr;
    uint64_t             m_paddleTriggerCycle = 0;
    bool                 m_80colMode          = false;
    bool                 m_doubleHiRes        = false;
    bool                 m_altCharSet         = false;
    atomic<Byte>         m_paddlePosition[s_knPaddleAxisCount];
    int                  m_lastEmittedPaddle[s_knPaddleAxisCount]     = { -1, -1, -1, -1 };
    int                  m_lastEmittedHostPaddle[s_knPaddleAxisCount] = { -1, -1, -1, -1 };
};
