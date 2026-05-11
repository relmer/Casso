#pragma once

#include "Pch.h"
#include "Devices/AppleKeyboard.h"

class AppleSpeaker;




////////////////////////////////////////////////////////////////////////////////
//
//  AppleIIeKeyboard
//
//  Apple //e keyboard: full lowercase, modifier keys (Open Apple, Closed
//  Apple, Shift). Phase 6 / T060 / T061 — bus range extended from
//  $C000-$C01F to $C000-$C063 so reads of $C061/$C062/$C063 land here.
//
//  Logical ownership (Phase 6 / T061):
//    - Keyboard owns:  $C000-$C00B (data), $C010 (strobe-clear),
//                      $C061-$C063 (modifier reads).
//    - Soft-switch bank owns: $C011-$C01F (status reads — bit 7 from
//                      MMU/LC/VideoTiming/SoftSwitchBank, bits 0-6 from
//                      the keyboard latch via GetLatchedKeyDataBits()),
//                      $C00C-$C00F (80COL/ALTCHARSET), $C050-$C05F
//                      (display switches).
//    - Speaker owns:   $C030-$C03F (toggle).
//
//  The bus first-match-wins ordering means this device wins for every
//  address in $C000-$C063; we forward to the canonical sibling device
//  for ranges we don't logically own.
//
//  Strobe-clear isolation (audit §1.2, §4): ONLY $C010 (read or write)
//  clears the strobe. $C011-$C01F MUST NOT clear it. Forwarding the
//  status reads to the bank's read-only path enforces this.
//
////////////////////////////////////////////////////////////////////////////////

class AppleIIeKeyboard : public AppleKeyboard
{
public:
    AppleIIeKeyboard (MemoryBus * bus = nullptr);

    Byte Read    (Word address) override;
    void Write   (Word address, Byte value) override;
    Word GetEnd  () const override { return 0xC063; }
    void Reset   () override;
    void SoftReset () override;

    // Modifier-key state (set from UI thread via HostShell key events).
    // Open Apple <-> left Alt, Closed Apple <-> right Alt, Shift <-> Shift
    // (Phase 6 / T063 / FR-013).
    void SetOpenApple   (bool pressed) { m_openApple.store   (pressed, memory_order_release); }
    void SetClosedApple (bool pressed) { m_closedApple.store (pressed, memory_order_release); }
    void SetShift       (bool pressed) { m_shift.store       (pressed, memory_order_release); }

    // Soft-switch sibling — owns $C00C-$C00F, $C011-$C01F status reads,
    // and $C050-$C05F display switches (T061 ownership split).
    void SetSoftSwitchSibling (class AppleIIeSoftSwitchBank * sibling) { m_softSwitchSibling = sibling; }

    // Speaker sibling — owns $C030-$C03F. The keyboard's bus range now
    // covers this so we forward through.
    void SetSpeakerSibling (AppleSpeaker * spk) { m_speakerSibling = spk; }

    // MMU sibling — used by the soft-switch bank for $C013-$C018 status
    // reads; cached here for legacy callers (Phase 2 wiring).
    void SetMmu (class AppleIIeMmu * mmu) { m_mmu = mmu; }

    // LanguageCard sibling — used by the soft-switch bank for
    // $C011/$C012 status reads (Phase 3 wiring).
    void SetLanguageCard (class LanguageCard * lc) { m_lc = lc; }

    // VideoTiming sibling — used by the soft-switch bank for $C019
    // (RDVBLBAR) (Phase 5 wiring).
    void SetVideoTiming (class IVideoTiming * vt) { m_videoTiming = vt; }

    // Override key press to allow lowercase
    void KeyPressRaw (Byte asciiChar);

    // Phase 6 / FR-013 read-only modifier accessors used by the soft-switch
    // bank when sourcing bit 7 of $C061/$C062/$C063 status reads.
    bool IsOpenApplePressed   () const { return m_openApple.load   (memory_order_acquire); }
    bool IsClosedApplePressed () const { return m_closedApple.load (memory_order_acquire); }
    bool IsShiftPressed       () const { return m_shift.load       (memory_order_acquire); }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    MemoryBus *                    m_bus               = nullptr;
    class AppleIIeSoftSwitchBank * m_softSwitchSibling = nullptr;
    AppleSpeaker *                 m_speakerSibling    = nullptr;
    class AppleIIeMmu *            m_mmu               = nullptr;
    class LanguageCard *           m_lc                = nullptr;
    class IVideoTiming *           m_videoTiming       = nullptr;
    atomic<bool>                   m_openApple   {false};
    atomic<bool>                   m_closedApple {false};
    atomic<bool>                   m_shift       {false};
};
