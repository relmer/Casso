#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"

#include "Shell/MachineGamePortSink.h"

#include "TestMachine.h"
#include "TextScreenScraper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  JoyportMachineTests
//
//  The Joyport on real machines, read through the bus the way a guest reads
//  it: the annunciators written at $C058-$C05B, the buttons read at
//  $C061-$C063. On the ][+ the game port answers the buttons; on the //e the
//  keyboard does, and $C058-$C05F reach the soft-switch bank through the
//  keyboard, so both routes are covered.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (JoyportMachineTests)
{
public:

    TEST_METHOD (TheIIPlusReadsBothJacksThroughTheAnnunciators)
    {
        CheckEveryCombination ("Apple2Plus");
    }


    TEST_METHOD (TheIIeReadsBothJacksThroughTheAnnunciators)
    {
        CheckEveryCombination ("Apple2e");
    }


    TEST_METHOD (TheEnhancedIIeReadsBothJacksThroughTheAnnunciators)
    {
        CheckEveryCombination ("Apple2eEnhanced");
    }


    TEST_METHOD (ThePaddlesReadAsNothingConnected)
    {
        for (const char * id : { "Apple2Plus", "Apple2e" })
        {
            TestMachine  machine (id, TestMachine::Slots::Empty);

            BootToPrompt (machine);

            Assert::IsFalse (IsPaddleStillTiming (machine), L"detached, a centered paddle times out");

            machine.GetJoyport()->SetAttached (true);
            Assert::IsTrue (IsPaddleStillTiming (machine), L"attached, the one-shot never times out: PDL(0) reads 255");
        }
    }


    TEST_METHOD (DetachedTheIIPlusButtonsAreTheGamePortsAgain)
    {
        TestMachine  machine ("Apple2Plus", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack, AllClosed());

        machine.GetRefs().gamePort->SetButton (1, true);

        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb0), L"PB0 released");
        Assert::AreEqual<Byte> (0x80, ReadButton (machine, kPb1), L"PB1 pressed");
        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb2), L"PB2 released");
    }


    TEST_METHOD (DetachedTheIIeButtonsAreTheAppleKeysAgain)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack, AllClosed());

        machine.GetRefs().iieKeyboard->SetOpenApple (true);
        machine.GetRefs().iieKeyboard->SetShift     (true);

        Assert::AreEqual<Byte> (0x80, ReadButton (machine, kPb0), L"Open Apple");
        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb1), L"Closed Apple");
        Assert::AreEqual<Byte> (0x80, ReadButton (machine, kPb2), L"Shift");
    }


    //
    //  Resets. The Joyport's idle lines -- every switch open -- read bit 7
    //  set, which on a //e is Open Apple and Closed Apple both held down: a
    //  reboot or the self-test on every reset, were the lines not released
    //  while the firmware reads them. The witness is Applesoft's end-of-program
    //  pointer, as in OpenAppleResetTests: a warm reset keeps the program, a
    //  cold start clears it.
    //

    TEST_METHOD (TwentyCtrlResetsWithTheJoyportAttachedAreAllWarm)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        machine.GetJoyport()->SetAttached (true);

        for (int i = 0; i < kRepeats; i++)
        {
            PretendAProgramIsLoaded (machine);
            machine.SoftReset();
            machine.RunCycles (kSettleCycles);

            Assert::AreEqual<Word> (kFakeProgramEnd, ProgramEnd (machine),
                std::format (L"reset {} was not a plain warm reset", i + 1).c_str());
        }
    }


    TEST_METHOD (TwentyPowerOnsWithTheJoyportAttachedAllReachBasic)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        machine.GetJoyport()->SetAttached (true);

        for (int i = 0; i < kRepeats; i++)
        {
            BootToPrompt (machine);

            Assert::AreEqual<Word> (kEmptyProgramEnd, ProgramEnd (machine),
                std::format (L"power-on {} did not initialize Applesoft", i + 1).c_str());
            Assert::IsTrue (IsAtBasicPrompt (machine),
                std::format (L"power-on {} did not reach the BASIC prompt", i + 1).c_str());
        }
    }


    TEST_METHOD (OpenAppleHeldThroughResetStillReboots)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);
        machine.GetJoyport()->SetAttached (true);

        //  What the shell does for Ctrl-Open-Apple-Reset.
        machine.GetRefs().iieKeyboard->HoldAppleKeysThroughReset (true, false);
        machine.SoftReset();
        RunTickingTheHold (machine, kSettleCycles);

        Assert::AreEqual<Word> (kEmptyProgramEnd, ProgramEnd (machine), L"a cold start, as with no Joyport");
    }


    TEST_METHOD (AHeldFireCannotTurnAResetIntoAReboot)
    {
        TestMachine       machine ("Apple2e", TestMachine::Slots::Empty);
        JoystickSwitches  fire;

        //  Fire closed pulls PB0 low and leaves PB1 open: Closed Apple alone.
        fire.set (static_cast<size_t> (JoystickSwitch::Fire));

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack, fire);
        machine.GetJoyport()->SetAttached (true);

        machine.SoftReset();
        machine.RunCycles (kSettleCycles);

        Assert::AreEqual<Word> (kFakeProgramEnd, ProgramEnd (machine), L"still a plain warm reset");
    }


    TEST_METHOD (AfterTheWindowTheAppleKeysAndShiftChangeNothing)
    {
        TestMachine       machine ("Apple2e", TestMachine::Slots::Empty);
        JoystickSwitches  closed;

        //  AN0 and AN1 off: PB0 fire, PB1 left, PB2 right.
        closed.set (static_cast<size_t> (JoystickSwitch::Fire));
        closed.set (static_cast<size_t> (JoystickSwitch::Left));
        closed.set (static_cast<size_t> (JoystickSwitch::Right));

        BootToPrompt (machine);
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack, closed);
        machine.GetJoyport()->SetAttached (true);
        SelectAnnunciators (machine, false, false);

        machine.GetRefs().iieKeyboard->SetOpenApple   (true);
        machine.GetRefs().iieKeyboard->SetClosedApple (true);
        machine.GetRefs().iieKeyboard->SetShift       (true);

        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb0), L"Open Apple does not reach PB0");
        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb1), L"Closed Apple does not reach PB1");
        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb2), L"Shift does not reach PB2");
    }


    TEST_METHOD (APowerCycleOpensTheWindowAgain)
    {
        //  A machine switch builds a machine and power-cycles it, so this is
        //  that case as well.
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        machine.GetJoyport()->SetAttached (true);
        machine.PowerCycle();

        Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb0),
            L"right after power-on PB0 is the keyboard's Open Apple, released, not the Joyport's open line");

        machine.RunCycles (SiriusJoyport::kReleaseCycles);
        Assert::AreEqual<Byte> (0x80, ReadButton (machine, kPb0), L"and the Joyport's once the window has run");
    }


    TEST_METHOD (SwitchingToAMachineWithoutTheJoyportLeavesNothingHeld)
    {
        //  The shell's game-port state carries a held fire and up on both jacks
        //  into the next machine, which has none attached (the ][+) or cannot
        //  have one (the //c).
        GamePortState      held;
        std::shared_mutex  lifetime;
        JoystickSwitches   switches;

        switches.set (static_cast<size_t> (JoystickSwitch::Fire));
        switches.set (static_cast<size_t> (JoystickSwitch::Up));
        held.jacks.jack[JoyportJacks::kLeftJack]  = switches;
        held.jacks.jack[JoyportJacks::kRightJack] = switches;

        for (const char * id : { "Apple2Plus", "Apple2c" })
        {
            TestMachine          machine (id, TestMachine::Slots::Empty);
            MachineGamePortSink  sink (lifetime, [&machine]
            {
                GamePortTargets  targets;

                targets.gamePort    = machine.GetRefs().gamePort;
                targets.iieSwitches = machine.GetRefs().iieSoftSwitches;
                targets.iieKeyboard = machine.GetRefs().iieKeyboard;
                targets.joyport     = machine.GetJoyport();
                return targets;
            });

            BootToPrompt (machine);
            sink.TryApply (held, nullptr);

            std::wstring  name (id, id + strlen (id));

            Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb0), (name + L": PB0 is the machine's own, released").c_str());
            Assert::AreEqual<Byte> (0x00, ReadButton (machine, kPb1), (name + L": PB1 too").c_str());

            //  Paddles on the ][+ only. The //c's firmware keeps touching
            //  $C07x while it waits on its drive, and every touch restarts the
            //  one-shot, so a timing read there says nothing about a Joyport.
            if (machine.GetJoyport() != nullptr)
            {
                Assert::IsFalse (IsPaddleStillTiming (machine), (name + L": and the paddles time out as usual").c_str());
            }
        }
    }


protected:

    static constexpr uint32_t  kBootCycles      = 5'000'000;
    static constexpr uint32_t  kSettleCycles    = 2'000'000;
    static constexpr uint32_t  kTickCycles      = 1'000;
    static constexpr int       kRepeats         = 20;
    static constexpr Word      kPb0             = 0xC061;
    static constexpr Word      kPb1             = 0xC062;
    static constexpr Word      kPb2             = 0xC063;
    static constexpr Byte      kButtonBit       = 0x80;
    static constexpr Word      kProgramEndLo    = 0x00AF;      // Applesoft PRGEND
    static constexpr Word      kEmptyProgramEnd = 0x0803;      // right after the empty program at $0801
    static constexpr Word      kFakeProgramEnd  = 0x2000;


    static void PretendAProgramIsLoaded (MachineHost & machine)
    {
        machine.GetMemoryBus().WriteByte (kProgramEndLo,     (Byte) (kFakeProgramEnd & 0xFF));
        machine.GetMemoryBus().WriteByte (kProgramEndLo + 1, (Byte) (kFakeProgramEnd >> 8));
    }


    static Word ProgramEnd (MachineHost & machine)
    {
        return (Word) (machine.GetMemoryBus().ReadByte (kProgramEndLo)
                     | (machine.GetMemoryBus().ReadByte (kProgramEndLo + 1) << 8));
    }


    static bool IsAtBasicPrompt (MachineHost & machine)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), TextScreenScraper::kTextPage1);

        for (const std::string & row : rows)
        {
            if (!row.empty() && row[0] == ']')
            {
                return true;
            }
        }

        return false;
    }


    //  The CPU thread's slice loop, which is what counts a reset hold down.
    static void RunTickingTheHold (MachineHost & machine, uint32_t cycles)
    {
        for (uint32_t ran = 0; ran < cycles; ran += kTickCycles)
        {
            machine.RunCycles (kTickCycles);
            machine.GetRefs().iieKeyboard->TickResetHold (kTickCycles);
        }
    }


    static void BootToPrompt (MachineHost & machine)
    {
        machine.PowerCycle();
        machine.RunCycles (kBootCycles);
    }


    static Byte ReadButton (MachineHost & machine, Word address)
    {
        return static_cast<Byte> (machine.GetMemoryBus().ReadByte (address) & kButtonBit);
    }


    static void SelectAnnunciators (MachineHost & machine, bool an0, bool an1)
    {
        machine.GetMemoryBus().WriteByte (an0 ? 0xC059 : 0xC058, 0);
        machine.GetMemoryBus().WriteByte (an1 ? 0xC05B : 0xC05A, 0);
    }


    static JoystickSwitches AllClosed()
    {
        JoystickSwitches  switches;

        switches.set (static_cast<size_t> (JoystickSwitch::Up));
        switches.set (static_cast<size_t> (JoystickSwitch::Left));
        switches.set (static_cast<size_t> (JoystickSwitch::Fire));
        return switches;
    }


    //  Strobes the one-shot and waits past a full-scale read: a paddle with
    //  a pot has timed out by then, and an unconnected input has not.
    static bool IsPaddleStillTiming (MachineHost & machine)
    {
        constexpr uint32_t  kPastFullScale = 3'000;

        machine.GetMemoryBus().ReadByte (0xC070);
        machine.RunCycles (kPastFullScale);

        return (machine.GetMemoryBus().ReadByte (0xC064) & kButtonBit) != 0;
    }


    //  Closes one distinct switch per jack per button, and reads all twelve
    //  (jack, axis, button) combinations back through the bus.
    static void CheckEveryCombination (const char * machineId)
    {
        TestMachine       machine (machineId, TestMachine::Slots::Empty);
        JoystickSwitches  left;
        JoystickSwitches  right;

        BootToPrompt (machine);

        left.set  (static_cast<size_t> (JoystickSwitch::Left));
        left.set  (static_cast<size_t> (JoystickSwitch::Down));
        right.set (static_cast<size_t> (JoystickSwitch::Right));
        right.set (static_cast<size_t> (JoystickSwitch::Up));
        right.set (static_cast<size_t> (JoystickSwitch::Fire));

        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kLeftJack,  left);
        machine.GetJoyport()->SetJackSwitches (JoyportJacks::kRightJack, right);
        machine.GetJoyport()->SetAttached (true);

        //  Closed reads bit 7 clear.
        //                                     AN0    AN1    PB0    PB1    PB2
        ExpectButtons (machine, machineId, false, false, 0x80,  0x00,  0x80);
        ExpectButtons (machine, machineId, false, true,  0x80,  0x80,  0x00);
        ExpectButtons (machine, machineId, true,  false, 0x00,  0x80,  0x00);
        ExpectButtons (machine, machineId, true,  true,  0x00,  0x00,  0x80);
    }


    static void ExpectButtons (MachineHost & machine, const char * machineId, bool an0, bool an1, Byte pb0, Byte pb1, Byte pb2)
    {
        std::wstring  where = std::format (L"{} with AN0 {} and AN1 {}", TextEncoding::NarrowToWide (machineId),
                                           an0 ? L"on" : L"off", an1 ? L"on" : L"off");

        SelectAnnunciators (machine, an0, an1);

        Assert::AreEqual<Byte> (pb0, ReadButton (machine, kPb0), (where + L": PB0").c_str());
        Assert::AreEqual<Byte> (pb1, ReadButton (machine, kPb1), (where + L": PB1").c_str());
        Assert::AreEqual<Byte> (pb2, ReadButton (machine, kPb2), (where + L": PB2").c_str());
    }
};
