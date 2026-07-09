#pragma once

#include "Pch.h"
#include "Devices/AppleKeyboard.h"

class AppleSpeaker;




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eKeyboard
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

class Apple2eKeyboard : public AppleKeyboard
{
public:
    Apple2eKeyboard (MemoryBus * bus = nullptr);

    Byte Read    (Word address) override;
    void Write   (Word address, Byte value) override;
    Word GetEnd  () const override { return 0xC063; }
    void Reset   () override;
    void SoftReset () override;

    // Modifier-key state (set from UI thread via HostShell key events).
    // Open Apple <-> left Alt, Closed Apple <-> right Alt, Shift <-> Shift
    // (Phase 6 / T063 / FR-013).
    void SetOpenApple   (bool pressed);
    void SetClosedApple (bool pressed);
    void SetShift       (bool pressed) { m_shift.store       (pressed, memory_order_release); }

    // Soft-switch sibling — owns $C00C-$C00F, $C011-$C01F status reads,
    // and $C050-$C05F display switches (T061 ownership split).
    void SetSoftSwitchSibling (class Apple2eSoftSwitchBank * sibling) { m_softSwitchSibling = sibling; }

    // Speaker sibling — owns $C030-$C03F. The keyboard's bus range now
    // covers this so we forward through.
    void SetSpeakerSibling (AppleSpeaker * spk) { m_speakerSibling = spk; }

    // MMU sibling — used by the soft-switch bank for $C013-$C018 status
    // reads; cached here for legacy callers (Phase 2 wiring).
    void SetMmu (class Apple2eMmu * mmu) { m_mmu = mmu; }

    // LanguageCard sibling — used by the soft-switch bank for
    // $C011/$C012 status reads (Phase 3 wiring).
    void SetLanguageCard (class LanguageCard * lc) { m_lc = lc; }

    // VideoTiming sibling — used by the soft-switch bank for $C019
    // (RDVBLBAR) (Phase 5 wiring).
    void SetVideoTiming (class IVideoTiming * vt) { m_videoTiming = vt; }

    // Apple //c only: the IOU mouse (US4). When attached, $C063 reads the
    // mouse button (ACTIVE LOW — the //c wires the button where the //e had
    // its shift-key mod) and $C048 (RSTXY, any access) clears the movement
    // interrupt latches. Null on the //e and earlier, where the legacy
    // shift-read / no-op behavior stands.
    void SetMouse (class AppleMouse * mouse) { m_mouse = mouse; }

    // Override key press to allow lowercase
    void KeyPressRaw (Byte asciiChar);

    // Phase 6 / FR-013 read-only modifier accessors used by the soft-switch
    // bank when sourcing bit 7 of $C061/$C062/$C063 status reads.
    bool IsOpenApplePressed   () const { return m_openApple.load   (memory_order_acquire); }
    bool IsClosedApplePressed () const { return m_closedApple.load (memory_order_acquire); }
    bool IsShiftPressed       () const { return m_shift.load       (memory_order_acquire); }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    static constexpr Word kFirstButtonAddress = 0xC061;
    static constexpr int  kButtonCount        = 3;     // $C061-$C063
    static constexpr int  kHostButtonCount    = 2;     // Open / Closed Apple

    // CPU thread. Coalesced emit for a guest read of a $C061-$C063
    // button: fires only when that button's returned byte changed.
    void EmitButtonRead (Word address, Byte value);
    void EmitHostButton (int index, bool pressed);

    MemoryBus *                    m_bus               = nullptr;
    class Apple2eSoftSwitchBank *  m_softSwitchSibling = nullptr;
    AppleSpeaker *                 m_speakerSibling    = nullptr;
    class Apple2eMmu *             m_mmu               = nullptr;
    class LanguageCard *           m_lc                = nullptr;
    class IVideoTiming *           m_videoTiming       = nullptr;
    class AppleMouse *             m_mouse             = nullptr;
    atomic<bool>                   m_openApple   {false};
    atomic<bool>                   m_closedApple {false};
    atomic<bool>                   m_shift       {false};

    // Last-emitted byte per button for coalescing (CPU thread only).
    // -1 means "nothing emitted yet"; a Byte never matches it.
    int                            m_lastEmittedButton[kButtonCount]        = { -1, -1, -1 };
    int                            m_lastEmittedHostButton[kHostButtonCount] = { -1, -1 };
};
