#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"

#include "TestMachine.h"

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


protected:

    static constexpr uint32_t  kBootCycles = 5'000'000;
    static constexpr Word      kPb0        = 0xC061;
    static constexpr Word      kPb1        = 0xC062;
    static constexpr Word      kPb2        = 0xC063;
    static constexpr Byte      kButtonBit  = 0x80;


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
