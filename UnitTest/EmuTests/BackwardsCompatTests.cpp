#include "Pch.h"
#include "TestMachine.h"
#include "Core/MachineConfig.h"
#include "Machines/MachineDefinitions.h"
#include "HResultAssert.h"
#include "Machines/IMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BackwardsCompatTests — Phase 14 / User Story 5.
//
//  Consolidates the explicit ][/][+ regression gate. The whole point of
//  the //e fidelity feature is that it MUST NOT regress original Apple
//  ][ or Apple ][+ behavior (FR-039) and MUST be implemented through
//  composition rather than branching the existing ][/][+ machine
//  configurations or the existing devices (FR-040).
//
//  Two distinct surfaces are pinned here:
//
//    1. JSON pin — `Machines/Apple2.json` and `Machines/Apple2Plus.json`
//       remain byte-identical to their pre-feature shape; their device
//       lists contain ONLY the original ][/][+ device types and explicitly
//       NONE of the //e-specific types (apple2e-keyboard,
//       apple2e-softswitches, apple2e-mmu, language-card). Their RAM map
//       is single-bank ($0000-$BFFF) with no `aux` bank. Their video-mode
//       list contains exactly text40/lores/hires (no text80/dhgr).
//
//    2. Composition pin — the ][ and ][+ build from
//       BuildAppleIIPlus continue to compose only the deterministic
//       harness primitives (Prng, FixtureProvider) and
//       MUST NOT pull in the //e wiring (no Apple2eMmu, no EmuCpu,
//       no aux RAM, no LanguageCardBank). This is the architectural
//       proof that the //e build path is a *separate* composition, not
//       a branch of the ][/][+ build path.
//
//  This file has surface 2. Surface 1's loads resolve their ROMs through a
//  resolver that writes them to the system temp folder, so they are in the
//  scenario suite's BackwardsCompatTests.
//
//  Together, these tests serve as the "must never change byte-exactly"
//  baseline. If any assertion in this file would have to change for a
//  later-phase change to compile or pass, that's a regression — the fix
//  belongs in the production code, never in this file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BackwardsCompatTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  What the ][ and ][+ ARE, now that a test can build one.
    //
    //  These replace three tests that asserted the old harness had composed
    //  itself -- that it had wired a mock host, a fixture provider and a
    //  pinned Prng, and that its per-machine enum still held three distinct
    //  values. None of that was the machine. The harness built no ][ or ][+
    //  at all, so the strongest thing those tests could say about either
    //  model was that asking for one did not fail.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (TheAppleIIAndIIPlusAreDifferentMachines)
    {
        TestMachine  machineII     ("Apple2");
        TestMachine  machineIIPlus ("Apple2Plus");

        //  Integer BASIC and Applesoft: same size image, different ROM.
        std::vector<Byte>  romII     = GuestBytesAt (machineII,     0xE000, 64);
        std::vector<Byte>  romIIPlus = GuestBytesAt (machineIIPlus, 0xE000, 64);

        Assert::IsFalse (romII == romIIPlus,
            L"the ][ boots Integer BASIC and the ][+ Applesoft; their ROMs differ");
    }


    TEST_METHOD (NeitherHasTheIIeMemoryManager)
    {
        //  The composition pin, said about the machine rather than about
        //  which builder function ran: an MMU is //e-and-later hardware, and
        //  the earlier models must not acquire one.
        for (const char * id : { "Apple2", "Apple2Plus" })
        {
            TestMachine  machine (id);

            Assert::IsNull (machine.GetMmu(),
                L"a ][ or ][+ has no memory management unit");
            Assert::IsNull (machine.GetRefs().iieSoftSwitches,
                L"nor the //e soft-switch bank");
            Assert::IsNull (machine.GetRefs().iieKeyboard,
                L"nor the //e keyboard");
        }
    }


    TEST_METHOD (TwoBuildsOfOneMachineAgree)
    {
        TestMachine  a ("Apple2Plus");
        TestMachine  b ("Apple2Plus");

        a.PowerCycle();
        b.PowerCycle();

        //  The Prng is pinned, so the arbitrary contents a real machine
        //  powers on with are the same arbitrary contents twice running --
        //  which is what lets any test downstream assert on memory at all.
        Assert::IsTrue (GuestBytesAt (a, 0x2000, 256) == GuestBytesAt (b, 0x2000, 256),
            L"two builds of one machine power on identically");
    }


private:

    //  Straight off the bus, without running the guest.
    static std::vector<Byte> GuestBytesAt (MachineHost & machine, Word address, size_t count)
    {
        std::vector<Byte>  bytes;

        for (size_t i = 0; i < count; i++)
        {
            bytes.push_back (machine.GetMemoryBus().ReadByte (static_cast<Word> (address + i)));
        }

        return (bytes);
    }
};

