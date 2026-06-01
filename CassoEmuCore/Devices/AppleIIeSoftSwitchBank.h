#pragma once

#include "Pch.h"
#include "Devices/AppleSoftSwitchBank.h"

class AppleIIeMmu;
class AppleIIeKeyboard;
class LanguageCard;
class IVideoTiming;




////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeSoftSwitchBank
//
//  Extended IIe soft switches including 80-column and double hi-res.
//  $C000-$C00B write-switches are forwarded to the AppleIIeMmu (set via
//  SetMmu); the MMU owns the canonical RAMRD/RAMWRT/ALTZP/80STORE/INTCXROM/
//  SLOTC3ROM flag state. Display-only flags (80COL, ALTCHARSET, PAGE2,
//  HIRES, TEXT, MIXED, DHIRES) remain owned here.
//
//  Phase 6 / T061 ownership split:
//    - This bank owns $C011-$C01F status reads. Bit 7 is sourced from
//      the canonical state device (LanguageCard / AppleIIeMmu /
//      VideoTiming / this bank itself for the display flags). Bits 0-6
//      are the keyboard latch (floating-bus low 7 bits per the documented
//      Apple //e convention; FR-001, FR-003, audit §1.2). Status reads MUST NOT
//      clear the keyboard strobe — the data is fetched via the
//      keyboard's read-only GetLatchedKeyDataBits() accessor.
//    - The AppleIIeKeyboard claims $C000-$C063 on the bus and forwards
//      $C011-$C01F reads here (strobe-clear isolation, audit §1.2, §4).
//
////////////////////////////////////////////////////////////////////////////////

class AppleIIeSoftSwitchBank : public AppleSoftSwitchBank
{
public:
    AppleIIeSoftSwitchBank (MemoryBus * bus = nullptr);

    Byte Read      (Word address) override;
    void Write     (Word address, Byte value) override;
    Word GetEnd    () const override { return 0xC07F; }
    void Reset     () override;
    void SoftReset () override;

    bool Is80ColMode    () const { return m_80colMode; }
    bool IsDoubleHiRes  () const { return m_doubleHiRes; }
    bool IsAltCharSet   () const { return m_altCharSet; }
    bool Is80Store      () const;

    void SetMmu          (AppleIIeMmu * mmu)        { m_mmu          = mmu; }
    void SetKeyboard     (AppleIIeKeyboard * kbd)   { m_keyboard     = kbd; }
    void SetLanguageCard (LanguageCard * lc)        { m_lc           = lc; }
    void SetVideoTiming  (IVideoTiming * vt)        { m_videoTiming  = vt; }

    // Wire the CPU bus-cycle accumulator that drives the PREAD paddle timer.
    void SetCpuCycleSource (const uint64_t * src) { m_cpuCycleSource = src; }

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

    MemoryBus *          m_bus                = nullptr;
    AppleIIeMmu *        m_mmu                = nullptr;
    AppleIIeKeyboard *   m_keyboard           = nullptr;
    LanguageCard *       m_lc                 = nullptr;
    IVideoTiming *       m_videoTiming        = nullptr;
    const uint64_t *     m_cpuCycleSource     = nullptr;
    uint64_t             m_paddleTriggerCycle = 0;
    bool                 m_80colMode          = false;
    bool                 m_doubleHiRes        = false;
    bool                 m_altCharSet         = false;
    atomic<Byte>         m_paddlePosition[s_knPaddleAxisCount];
};
