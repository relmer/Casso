#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ISoftSwitchBank
//
//  Layered soft-switch surface. Each layer registers its own handlers;
//  resolution is by address ownership at machine-build time, not by
//  per-access fallthrough.
//
//  Layers:
//    1. AppleSoftSwitchBank — ][ base ($C030 speaker, $C000/$C010 keyboard,
//       $C050-$C057 display switches).
//    2. (][+ additions composed on top.)
//    3. AppleIIeSoftSwitchBank — //e additions:
//         - Write switches $C000-$C00F (80STORE / RAMRD / RAMWRT / INTCXROM /
//           ALTZP / SLOTC3ROM / 80COL / ALTCHARSET) — forwarded to
//           AppleIIeMmu (and own state for 80COL/ALTCHARSET).
//         - Status reads $C011-$C01F: bit 7 from the owning subsystem
//           (LC for $C011/$C012; MMU for $C013-$C018; video timing for
//           $C019; bank for $C01A-$C01F). Bits 0-6 from keyboard latch
//           (floating-bus stand-in).
//         - Display switches $C050-$C05F.
//
//  Invariants:
//    - $C011-$C01F reads MUST NOT clear the keyboard strobe (FR-014).
//    - Every write to a //e write switch triggers the MMU's
//      OnSoftSwitchChanged() so display-driven routing is re-resolved.
//    - Layering is composition: ][ and ][+ machine configs do not
//      register the //e bank; their behavior is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

class ISoftSwitchBank
{
public:
    virtual              ~ISoftSwitchBank () = default;

    virtual uint8_t       Read  (uint16_t address) = 0;
    virtual void          Write (uint16_t address, uint8_t value) = 0;
    virtual uint16_t      GetStart () const = 0;
    virtual uint16_t      GetEnd   () const = 0;
};
