#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeystrokeInjector
//
//  Injects ASCII keystrokes into an //e EmulatorCore via
//  Apple2eKeyboard::PressKey, pumping CPU cycles between strokes until the
//  ROM consumes the strobe. The //e keyboard passes lowercase through, so
//  what is injected is what the guest latches; the ][ / ][+ keyboard would
//  fold it to uppercase, which is why this injector is //e-only.
//
//  Caller responsibilities:
//    - The machine must already be powered on with the ROM idling at the
//      keyboard polling loop. Call core.RunCycles (...) for cold-boot
//      first, then InjectString.
//    - All injected characters use 7-bit ASCII; PressKey sets bit 7
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

    // Convenience: types `text` then a Return ($0D), then lets the ROM act
    // on the line. `settleCycles` is a ceiling, not a target: the machine
    // runs until MachineIdle says it has finished, or until the ceiling is
    // reached, whichever comes first.
    static size_t   InjectLine (
        EmulatorCore  &  core,
        const std::string &  text,
        uint64_t           settleCycles = kAfterReturnCycles);

    // Single key. Succeeds iff the strobe was consumed within the
    // allotted cycle budget.
    static HRESULT  InjectKey (
        EmulatorCore  &  core,
        Byte               ch,
        uint64_t           cycleBudget = kPerKeyCycleBudget);

private:
    // Pumps CPU cycles in small batches until the keyboard strobe is
    // consumed by the ROM polling loop; fails if the budget is
    // exhausted first.
    static HRESULT  WaitForStrobeClear (EmulatorCore & core, uint64_t cycleBudget);
};
