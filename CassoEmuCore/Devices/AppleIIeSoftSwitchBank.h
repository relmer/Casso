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
//      are the keyboard latch (floating-bus low 7 bits per AppleWin
//      convention; FR-001, FR-003, audit §1.2). Status reads MUST NOT
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

    Byte Read  (Word address) override;
    void Write (Word address, Byte value) override;
    Word GetEnd () const override { return 0xC07F; }
    void Reset () override;
    void SoftReset () override;

    bool Is80ColMode    () const { return m_80colMode; }
    bool IsDoubleHiRes  () const { return m_doubleHiRes; }
    bool IsAltCharSet   () const { return m_altCharSet; }
    bool Is80Store      () const;

    void SetMmu          (AppleIIeMmu * mmu)        { m_mmu          = mmu; }
    void SetKeyboard     (AppleIIeKeyboard * kbd)   { m_keyboard     = kbd; }
    void SetLanguageCard (LanguageCard * lc)        { m_lc           = lc; }
    void SetVideoTiming  (IVideoTiming * vt)        { m_videoTiming  = vt; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    Byte ReadStatusRegister (Word address);

    MemoryBus *          m_bus         = nullptr;
    AppleIIeMmu *        m_mmu         = nullptr;
    AppleIIeKeyboard *   m_keyboard    = nullptr;
    LanguageCard *       m_lc          = nullptr;
    IVideoTiming *       m_videoTiming = nullptr;
    bool                 m_80colMode   = false;
    bool                 m_doubleHiRes = false;
    bool                 m_altCharSet  = false;
};
