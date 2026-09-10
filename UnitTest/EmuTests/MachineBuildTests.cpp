#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/MachineDefinitions.h"

#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//  Long enough for a machine to clear its ROM's power-on self test and reach
//  the point where it is running its monitor, and short enough to stay well
//  inside a unit test's budget: roughly a tenth of a second of guest time.
static constexpr uint32_t  s_kBootCycles = 100000;

//  Everything from $C000 up is ROM or I/O on every Apple II. A machine that
//  survived its self test is executing there; one that fell into RAM is not.
static constexpr Word  s_kRomSpaceStart = 0xC000;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineBuildTests
//
//  A real machine, built by the code that builds the real one.
//
//  The test tree used to carry its own //e, //c, ][ and ][+ builders --
//  hundreds of lines repeating the production wiring in the production
//  order, because the production builder was a method on a class that could
//  not be constructed without a window. Two copies of a wiring order is one
//  copy too many: the power-cycle sequences had already drifted apart, and
//  the one under test was the one nobody ships.
//
//  What makes this reachable is that every service the builder connects a
//  machine TO is optional. A machine with no mixer attached to its speaker
//  and no thread draining its printer is still the same machine, wired the
//  same way, by the same code.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineBuildTests)
{
public:

    TEST_METHOD (TheProductionBuilderBuildsAnAppleIIe)
    {
        TestMachine  machine ("Apple2e");

        //  The devices a //e has, found through the same cached pointers the
        //  renderer and the input path read.
        Assert::IsNotNull (machine.GetCpu(),                  L"CPU");
        Assert::IsNotNull (machine.GetMmu(),                  L"//e MMU");
        Assert::IsNotNull (machine.GetRefs().keyboard,        L"keyboard");
        Assert::IsNotNull (machine.GetRefs().softSwitches,    L"soft switches");
        Assert::IsNotNull (machine.GetRefs().iieSoftSwitches, L"//e soft switches");
        Assert::IsNotNull (machine.GetRefs().iieKeyboard,     L"//e keyboard");
        Assert::IsNotNull (machine.GetRefs().speaker,         L"speaker");
        Assert::IsNotNull (machine.GetRefs().mainRamDev,      L"main RAM");
        Assert::IsNotNull (machine.GetRefs().languageCard,    L"language card");
        Assert::IsNotNull (machine.GetRefs().diskController,  L"slot 6 Disk ][");

        //  All five renderers exist on every machine, because the per-frame
        //  mode selection switches between them and cannot afford to build
        //  one mid-render.
        Assert::IsNotNull (machine.GetRefs().text40,      L"40-column text");
        Assert::IsNotNull (machine.GetRefs().text80,      L"80-column text");
        Assert::IsNotNull (machine.GetRefs().loRes,       L"lo-res");
        Assert::IsNotNull (machine.GetRefs().hiRes,       L"hi-res");
        Assert::IsNotNull (machine.GetRefs().doubleHiRes, L"double hi-res");
    }


    TEST_METHOD (ABuiltMachineBootsItsRom)
    {
        TestMachine  machine ("Apple2e");
        uint32_t     spent = 0;

        machine.PowerCycle();
        spent = machine.RunCycles (s_kBootCycles);

        Assert::IsTrue (spent >= s_kBootCycles, L"the cycles asked for were spent");

        //  The one assertion here that depends on the wiring being RIGHT
        //  rather than merely present.
        Assert::IsTrue (machine.GetCpu()->GetPC() >= s_kRomSpaceStart,
            std::format (L"a booted //e runs from ROM; PC was ${:04X}",
                         machine.GetCpu()->GetPC()).c_str());
    }


    TEST_METHOD (TheSoftSwitchesComeUpInTextMode)
    {
        TestMachine  machine ("Apple2e");

        machine.PowerCycle();
        machine.RunCycles (s_kBootCycles);

        //  The reset routine puts the display back to 40-column text. These
        //  read the live bank rather than the latched mirror, which only the
        //  frame loop writes.
        Assert::IsFalse (machine.GetRefs().softSwitches->IsGraphicsMode(),
            L"a machine that has just reset shows text, not graphics");
        Assert::IsFalse (machine.GetRefs().iieSoftSwitches->Is80ColMode(),
            L"and 40 columns of it");
    }


    TEST_METHOD (MainRamIsWritableAcrossItsWholeRange)
    {
        TestMachine  machine ("Apple2e");

        //  The page table is what routes these, so a wrong one shows up as a
        //  read that does not answer with what was written.
        for (Word addr : { Word (0x0000), Word (0x0400), Word (0x2000),
                           Word (0x6000), Word (0xBFFF) })
        {
            machine.GetMemoryBus().WriteByte (addr, 0x5A);
            Assert::AreEqual<Byte> (0x5A, machine.GetMemoryBus().ReadByte (addr),
                std::format (L"main RAM must answer at ${:04X}", addr).c_str());
        }
    }


    TEST_METHOD (OnlyASlotlessMachineGivesUpItsCxxxRange)
    {
        //  The banked-ROM wiring hands $C100-$CFFF entirely to the internal
        //  firmware when the machine has no card slots, and it asks the
        //  machine rather than assuming -- it used to assume, which made
        //  "banked ROM" and "no slots" the same fact for anyone who added a
        //  banked machine later.
        Assert::AreEqual (0, MachineDefinitions::Find ("Apple2c")->slotCount,
            L"the //c has no card slots");

        for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
        {
            Assert::AreEqual (7, MachineDefinitions::Find (id)->slotCount,
                L"every other model has seven");
        }
    }


    TEST_METHOD (EveryShippedMachineBuildsAndBoots)
    {
        //  All five, through the production builder, from the shipped JSON,
        //  with the ROMs scripts/FetchRoms.ps1 -Fixtures provides. A machine
        //  whose config asks for a device the builder cannot make, or whose
        //  ROM is not where the config says, fails here rather than on
        //  someone's desk.
        //
        //  The ][ and ][+ had never been booted by a test before this: the
        //  headless harness composed a Prng and a mock host for them and no
        //  machine at all.
        for (const char * id : { "Apple2", "Apple2Plus", "Apple2e",
                                 "Apple2eEnhanced", "Apple2c" })
        {
            TestMachine   machine (id);
            std::wstring  name (id, id + strlen (id));

            Assert::IsNotNull (machine.GetCpu(),
                std::format (L"{} must build a CPU", name).c_str());
            Assert::IsNotNull (machine.GetRefs().keyboard,
                std::format (L"{} must build a keyboard", name).c_str());
            Assert::IsNotNull (machine.GetRefs().speaker,
                std::format (L"{} must build a speaker", name).c_str());

            machine.PowerCycle();
            machine.RunCycles (s_kBootCycles);

            Assert::IsTrue (machine.GetCpu()->GetPC() >= s_kRomSpaceStart,
                std::format (L"{} must boot into ROM; PC was ${:04X}",
                             name, machine.GetCpu()->GetPC()).c_str());
        }
    }
};
