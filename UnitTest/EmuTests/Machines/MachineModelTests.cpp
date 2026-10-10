#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/MachineDefinitions.h"

#include "../TestMachine.h"
#include "../TextScreenScraper.h"
#include "Machines/MachineDefinition.h"
#include "Machines/Apple2/Apple2c/Apple2c.h"
#include "Machines/IMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//  Enough for any of the five to clear its self test and put its prompt up.
static constexpr uint32_t  s_kColdBootCycles = 5000000;

//  What each machine's ROM greets you with. Integer BASIC prompts with `>`
//  and Applesoft with `]`, which is the most visible difference between a ][
//  and everything after it. The ]['s monitor prompts with `*`.
static constexpr char  s_kIntegerPrompt   = '>';
static constexpr char  s_kApplesoftPrompt = ']';
static constexpr char  s_kMonitorPrompt   = '*';

//  The 65C02 added STZ. A 6502 reads $64 as an undocumented two-byte NOP, so
//  the two answer differently about what they did with it.
static constexpr Byte  s_kStzZeroPage = 0x64;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineModelTests
//
//  What each model does that the others do not.
//
//  Everything the five machines share is tested once, on one machine, in the
//  files that own those components -- the Disk ][ nibble engine does not
//  become five tests by being reachable from five machines. What belongs
//  here is the difference: which BASIC answers, which memory the machine
//  has, which processor is in it.
//
//  These became possible when the suite stopped building its own machines.
//  Two of the five had never been booted by a test at all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineModelTests)
{
public:

    //  The ][ has no autostart ROM. Power-on lands in the monitor, and
    //  Ctrl+B is how its owner got to Integer BASIC.
    TEST_METHOD (TheAppleIIComesUpInTheMonitorAndCtrlBEntersIntegerBasic)
    {
        constexpr Byte      kCtrlB       = 0x02;
        constexpr Byte      kReturn      = 0x0D;
        constexpr uint64_t  kKeyCycles   = 100'000ULL;

        TestMachine  machine ("Apple2", TestMachine::Slots::Empty);



        BootToPrompt (machine);

        Assert::IsTrue (FindPrompt (machine, s_kMonitorPrompt) >= 0,
            ScreenDump (machine, L"a ][ powers on to the monitor's * prompt").c_str());

        machine.GetRefs().keyboard->PressKey (kCtrlB);
        machine.RunCycles (kKeyCycles);
        machine.GetRefs().keyboard->PressKey (kReturn);
        machine.RunCycles (kKeyCycles);

        Assert::IsTrue (FindPrompt (machine, s_kIntegerPrompt) >= 0,
            ScreenDump (machine, L"Ctrl+B from the monitor reaches Integer BASIC's > prompt").c_str());
    }


    TEST_METHOD (TheAppleIIPlusComesUpInApplesoft)
    {
        TestMachine  machine ("Apple2Plus", TestMachine::Slots::Empty);

        BootToPrompt (machine);

        //  The ][+ put Applesoft in ROM, which is the whole reason it is a
        //  different machine from the ][.
        Assert::IsTrue (FindPrompt (machine, s_kApplesoftPrompt) >= 0,
            ScreenDump (machine, L"a ][+ greets you with Applesoft's ] prompt").c_str());
    }


    TEST_METHOD (TheIIePutsUpApplesoftInFortyColumns)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);

        Assert::IsTrue (FindPrompt (machine, s_kApplesoftPrompt) >= 0,
            ScreenDump (machine, L"a //e greets you with Applesoft").c_str());
        Assert::IsFalse (machine.GetRefs().iieSoftSwitches->Is80ColMode(),
            L"and does it in 40 columns until something asks for 80");
    }


    TEST_METHOD (OnlyTheIIeAndLaterHaveEightyColumnHardware)
    {
        //  80COL, 80STORE, ALTCHARSET and the auxiliary bank arrived together
        //  on the //e and are the same piece of hardware. A ][ or ][+ has
        //  none of it, and the absence is what the earlier models ARE.
        for (const char * id : { "Apple2", "Apple2Plus" })
        {
            TestMachine  machine (id, TestMachine::Slots::Empty);

            Assert::IsNull (machine.GetRefs().iieSoftSwitches,
                L"a ][ or ][+ has no //e soft-switch bank");
            Assert::IsNull (machine.GetMmu(),
                L"and no memory management unit to bank an aux 64K with");
        }

        for (const char * id : { "Apple2e", "Apple2eEnhanced", "Apple2c" })
        {
            TestMachine  machine (id, TestMachine::Slots::Empty);

            Assert::IsNotNull (machine.GetRefs().iieSoftSwitches,
                L"a //e and later has the //e soft-switch bank");
            Assert::IsNotNull (machine.GetMmu(),
                L"and the MMU that banks its auxiliary 64K");
        }
    }


    TEST_METHOD (OnlyTheEnhancedIIeAndTheIICarryA65C02)
    {
        //  STZ $nn is a 65C02 instruction. On a 6502 the same opcode is an
        //  undocumented two-byte NOP, so running it from RAM and looking at
        //  where the program counter landed says which processor answered.
        struct Expected
        {
            const char *  machineId;
            Word          pcAdvance;   // STZ is 2 bytes; the NMOS NOP is 2 as well
            bool          stores;
        };

        const Expected  machines[] =
        {
            { "Apple2",          2, false },
            { "Apple2Plus",      2, false },
            { "Apple2e",         2, false },
            { "Apple2eEnhanced", 2, true  },
            { "Apple2c",         2, true  },
        };

        for (const Expected & expected : machines)
        {
            TestMachine   machine (expected.machineId, TestMachine::Slots::Empty);
            std::wstring  name (expected.machineId,
                                expected.machineId + strlen (expected.machineId));

            machine.PowerCycle();

            //  A byte the instruction would zero, and the instruction itself
            //  somewhere the machine can execute from.
            machine.GetMemoryBus().WriteByte (0x0080, 0xFF);
            machine.GetMemoryBus().WriteByte (0x0300, s_kStzZeroPage);
            machine.GetMemoryBus().WriteByte (0x0301, 0x80);
            machine.GetCpu()->SetPC (0x0300);

            machine.StepOne();

            Assert::AreEqual<Word> (static_cast<Word> (0x0300 + expected.pcAdvance),
                machine.GetCpu()->GetPC(),
                std::format (L"{}: STZ $80 is two bytes wide on either processor",
                             name).c_str());

            Assert::AreEqual<Byte> (expected.stores ? 0x00 : 0xFF,
                machine.GetMemoryBus().ReadByte (0x0080),
                std::format (L"{}: only a 65C02 stores zero here; a 6502 reads "
                             L"$64 as an undocumented NOP and leaves it alone",
                             name).c_str());
        }
    }


    TEST_METHOD (OnlyTheIICHasNoSlotsAndABuiltInDrive)
    {
        //  The //c's whole shape: nothing to plug a card into, and a drive
        //  soldered in rather than attached. Both are declared by the machine
        //  class, and both are read by code that has to behave differently
        //  for it -- the banked-ROM wiring asks about the slots, the chrome
        //  asks about the drive.
        const IMachine *  apple2c = MachineDefinitions::FindMachine ("Apple2c");

        Assert::IsNotNull (apple2c, L"the //c is a machine Casso ships");
        Assert::AreEqual (0, apple2c->GetSlotCount(), L"the //c has no card slots");
        Assert::IsTrue (apple2c->HasBuiltInDrive(),   L"and a drive built into it");
        Assert::IsTrue (apple2c->HasCaseSwitches(),   L"and switches on the case");

        for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
        {
            const IMachine *  machine = MachineDefinitions::FindMachine (id);

            Assert::AreEqual (7, machine->GetSlotCount(), L"every other model has seven slots");
            Assert::IsFalse (machine->HasBuiltInDrive(),  L"and no drive of its own");
        }
    }


    TEST_METHOD (TheGamePortLeavesWithTheIIe)
    {
        //  PREAD moved into the //e's soft-switch bank, so the separate game
        //  port device stops being a thing the machine has. This is the one
        //  device the //e genuinely REMOVED rather than replaced, and it is
        //  easy to re-add by accident when adding a model.
        for (const char * id : { "Apple2", "Apple2Plus" })
        {
            Assert::IsTrue (MachineDefinitions::FindMachine (id)->HasGamePortDevice(),
                L"a ][ or ][+ has a separate game port device");
        }

        for (const char * id : { "Apple2e", "Apple2eEnhanced", "Apple2c" })
        {
            Assert::IsFalse (MachineDefinitions::FindMachine (id)->HasGamePortDevice(),
                L"the //e absorbed PREAD into its soft-switch bank");
        }
    }


    TEST_METHOD (TheIIcHasTwoGamePortAxes)
    {
        //  Four paddles or two joysticks on every model but the //c, whose
        //  PDL2 and PDL3 lines carry its mouse.
        for (const char * id : { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced" })
        {
            Assert::AreEqual (4, MachineDefinitions::FindMachine (id)->GetGamePortAxisCount(),
                L"a ][, ][+ or //e has four axes");
            Assert::AreEqual (4, MachineDefinitions::Find (id)->gamePortAxisCount,
                L"and its definition carries the same count");
        }

        Assert::AreEqual (2, MachineDefinitions::FindMachine ("Apple2c")->GetGamePortAxisCount(), L"the //c has two");
        Assert::AreEqual (2, MachineDefinitions::Find ("Apple2c")->gamePortAxisCount);
    }


private:

    static void BootToPrompt (MachineHost & machine)
    {
        machine.PowerCycle();
        machine.RunCycles (s_kColdBootCycles);
    }


    //  What was expected, and what was actually on the screen. A boot that
    //  ends somewhere unexpected says so in one line rather than sending the
    //  reader back to run it themselves.
    static std::wstring ScreenDump (MachineHost & machine, const wchar_t * expected)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape (machine);
        std::wstring              out  = expected;

        out += L" -- screen was:";

        for (const std::string & row : rows)
        {
            out += L"|" + std::wstring (row.begin(), row.end());
        }

        return (out);
    }


    //  The row a prompt starts, or -1. Reads the text screen the way the
    //  renderer does rather than looking for a cursor variable.
    //
    //  COLUMN 0 ONLY. A prompt is printed at the start of a line, and a prompt
    //  character anywhere else is only a character. Searching whole rows let
    //  the ][ test pass for as long as power-on RAM was filled with random
    //  bytes: the ][ never reached BASIC, but 960 random characters nearly
    //  always held a '>'.
    static int FindPrompt (MachineHost & machine, char prompt)
    {
        std::vector<std::string>  rows  = TextScreenScraper::Scrape (machine);
        int                       found = -1;
        int                       row   = 0;

        for (row = 0; found < 0 && row < static_cast<int> (rows.size()); row++)
        {
            if (!rows[row].empty() && rows[row][0] == prompt)
            {
                found = row;
            }
        }

        return (found);
    }
};
