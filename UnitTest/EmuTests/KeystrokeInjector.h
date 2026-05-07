#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeystrokeInjector
//
//  Phase 7 (T068). Injects ASCII keystrokes into an //e EmulatorCore via
//  AppleIIeKeyboard::KeyPressRaw, pumping CPU cycles between strokes
//  until the ROM consumes the strobe. Apple //e accepts lowercase from
//  the full keyboard so KeyPressRaw is used directly (the base
//  KeyPress translates to uppercase, which would silently break tests
//  that mean to inject `]` or other non-letter glyphs).
//
//  Caller responsibilities:
//    - The machine must already be powered on with the ROM idling at the
//      keyboard polling loop. Call core.RunCycles (...) for cold-boot
//      first, then InjectString.
//    - All injected characters use 7-bit ASCII; KeyPressRaw sets bit 7
//      (the strobe) internally.
//
////////////////////////////////////////////////////////////////////////////////

class KeystrokeInjector
{
public:
    static constexpr uint64_t   kPerKeyCycleBudget    = 50000;     // ~50 ms wall
    static constexpr uint64_t   kAfterReturnCycles    = 500000;    // half-second
    static constexpr Byte       kAppleReturn          = 0x0D;

    // Injects each character of `text`, then waits `keyCycles` between
    // strokes for the ROM to clear the strobe. Returns the number of
    // characters successfully consumed (== text.size () on success).
    static size_t   InjectString (
        EmulatorCore  &  core,
        const std::string &  text,
        uint64_t           keyCycles = kPerKeyCycleBudget);

    // Convenience: types `text` then a Return ($0D), then runs an
    // additional cycle budget for the ROM to act on the line.
    static size_t   InjectLine (
        EmulatorCore  &  core,
        const std::string &  text,
        uint64_t           settleCycles = kAfterReturnCycles);

    // Single key. Returns true if the strobe was consumed within the
    // allotted cycle budget.
    static bool     InjectKey (
        EmulatorCore  &  core,
        Byte               ch,
        uint64_t           cycleBudget = kPerKeyCycleBudget);
};
