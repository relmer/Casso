#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "HeadlessHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineIdle
//
//  Runs the emulated machine until it has finished whatever it was doing,
//  rather than until a fixed cycle budget runs out.
//
//  The tests that drive a real DOS 3.3 or ProDOS disk used to spend a flat
//  budget on every step -- 200M cycles for a cold boot, 60M for each typed
//  line. A cold boot actually settles in 12.2M and a line like HOME or NEW
//  in 1.2M, so those budgets ran 16x to 50x longer than the work needed and
//  the emulator-bound tests dominated the suite.
//
//  The budget still exists, as a ceiling. RunUntilIdle stops early when the
//  machine goes quiet and otherwise spends the whole cap, so a caller whose
//  cap is smaller than the quiet window behaves exactly as it did before.
//
//  Idle means all three of:
//
//    - the drive motor is off. The Disk II spins down kMotorSpindownCycles
//      (1M) after the last access, so this alone costs about a megacycle,
//      but it is the only honest way to know RWTS is done.
//    - the text screen has stopped changing except for one cell. The cursor
//      flashes about three times a second, so an exact compare never
//      settles.
//    - the last non-blank row is a bare prompt. Without this a BASIC program
//      computing silently with the motor off looks identical to a machine
//      waiting for input.
//
//  All three are needed. The prompt appears on the way into Applesoft before
//  DOS goes back to the disk for HELLO, so a screen-only check stops
//  mid-boot; and the motor is off for the whole of a non-disk command.
//
////////////////////////////////////////////////////////////////////////////////

class MachineIdle
{
public:
    // Cycles run between idle samples. Small enough to cut the waste
    // finely, large enough that scraping the screen is not the cost.
    static constexpr uint64_t   kSampleSlice    = 200'000ULL;

    // Consecutive quiet samples required. One megacycle of calm, which
    // covers several cursor blinks and the motor spindown.
    static constexpr int        kQuietSamples   = 5;

    // Runs until the machine is idle or `cycleCap` cycles have been spent,
    // whichever comes first. Returns the cycles actually spent.
    static uint64_t   RunUntilIdle (EmulatorCore & core, uint64_t cycleCap);

    // True when the machine looks finished: drive stopped, screen quiet
    // versus `previous`, and a bare prompt showing. `previous` empty means
    // there is no earlier sample to compare against, which is never idle.
    static bool       IsIdle (
        EmulatorCore                    &  core,
        const std::vector<std::string>  &  previous,
        const std::vector<std::string>  &  current);
};
