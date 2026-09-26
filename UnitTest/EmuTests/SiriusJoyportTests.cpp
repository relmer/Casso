#include "Pch.h"

#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/SiriusJoyport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SiriusJoyportTests
//
//  The device model on its own, against a real soft-switch bank for the
//  annunciators and a plain counter for the CPU's cycles. The switch table is
//  the owner's manual's: AN0 off is the left jack, AN1 off is left/right; PB0
//  fire, PB1 left or up, PB2 right or down; closed reads bit 7 clear.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SiriusJoyportTests)
{
public:

    TEST_METHOD (EveryButtonReadsTheSwitchTheAnnunciatorsSelect)
    {
        Fixture  fixture;

        //  Each jack gets a pattern the other jack and the other axis cannot
        //  produce, so a swapped AN0 or AN1 shows up as a wrong answer.
        fixture.joyport.SetJackSwitches (JoyportJacks::kLeftJack,  Switches ({ JoystickSwitch::Left, JoystickSwitch::Down }));
        fixture.joyport.SetJackSwitches (JoyportJacks::kRightJack, Switches ({ JoystickSwitch::Right, JoystickSwitch::Up, JoystickSwitch::Fire }));
        fixture.joyport.SetAttached (true);

        //                     AN0    AN1    PB0     PB1     PB2
        fixture.ExpectReads (false, false, false,  true,   false);   // left jack:  fire, left, right
        fixture.ExpectReads (false, true,  false,  false,  true);    // left jack:  fire, up, down
        fixture.ExpectReads (true,  false, true,   false,  true);    // right jack: fire, left, right
        fixture.ExpectReads (true,  true,  true,   true,   false);   // right jack: fire, up, down
    }


    TEST_METHOD (DetachedDeclinesEveryRead)
    {
        Fixture  fixture;
        Byte     value = 0;

        fixture.joyport.SetJackSwitches (JoyportJacks::kLeftJack, Switches ({ JoystickSwitch::Fire }));

        for (int index = 0; index < kButtonCount; index++)
        {
            Assert::IsFalse (fixture.joyport.TryReadButton (index, value), L"a detached Joyport never answers");
        }

        Assert::IsFalse (fixture.joyport.IsDrivingPaddles(), L"nor holds the paddles");
    }


    TEST_METHOD (AResetReleasesTheLinesForTheWindow)
    {
        Fixture  fixture;
        Byte     value = 0;

        fixture.joyport.SetAttached (true);
        fixture.cycles = kSomeCycle;
        fixture.joyport.OnMachineReset();

        Assert::IsFalse (fixture.joyport.TryReadButton (0, value), L"released at the reset");

        fixture.cycles = kSomeCycle + SiriusJoyport::kReleaseCycles - 1;
        Assert::IsFalse (fixture.joyport.TryReadButton (0, value), L"still released one cycle short");
        Assert::IsTrue  (fixture.joyport.IsDrivingPaddles(), L"the paddles read unconnected throughout");

        fixture.cycles = kSomeCycle + SiriusJoyport::kReleaseCycles;
        Assert::IsTrue (fixture.joyport.TryReadButton (0, value), L"active once the window has run");
    }


    TEST_METHOD (AttachingWhileRunningNeedsNoReset)
    {
        Fixture  fixture;
        Byte     value = 0;

        fixture.cycles = kSomeCycle;
        fixture.joyport.SetAttached (true);

        Assert::IsTrue (fixture.joyport.TryReadButton (0, value), L"the next read answers");
    }


    TEST_METHOD (AResetWhileDetachedStillOpensTheWindow)
    {
        Fixture  fixture;
        Byte     value = 0;

        fixture.cycles = kSomeCycle;
        fixture.joyport.OnMachineReset();
        fixture.joyport.SetAttached (true);

        Assert::IsFalse (fixture.joyport.TryReadButton (0, value),
            L"attaching during the window must not cut it short");

        fixture.cycles = kSomeCycle + SiriusJoyport::kReleaseCycles;
        Assert::IsTrue (fixture.joyport.TryReadButton (0, value));
    }


    TEST_METHOD (APowerCycleMeasuresFromTheZeroedCounter)
    {
        Fixture  fixture;
        Byte     value = 0;

        fixture.joyport.SetAttached (true);
        fixture.cycles = 0;
        fixture.joyport.OnMachineReset();

        fixture.cycles = SiriusJoyport::kReleaseCycles - 1;
        Assert::IsFalse (fixture.joyport.TryReadButton (0, value));

        fixture.cycles = SiriusJoyport::kReleaseCycles;
        Assert::IsTrue (fixture.joyport.TryReadButton (0, value));
    }


private:

    static constexpr int       kButtonCount = 3;
    static constexpr uint64_t  kSomeCycle   = 12'345'678;
    static constexpr Byte      kButtonBit   = 0x80;


    struct Fixture
    {
        AppleSoftSwitchBank  bank;
        uint64_t             cycles = 0;
        SiriusJoyport        joyport;

        Fixture()
        {
            joyport.SetAnnunciatorSource (&bank);
            joyport.SetCycleSource       (&cycles);
        }

        void ExpectReads (bool an0, bool an1, bool pb0Closed, bool pb1Closed, bool pb2Closed)
        {
            bool  expected[kButtonCount] = { pb0Closed, pb1Closed, pb2Closed };

            bank.Read (an0 ? 0xC059 : 0xC058);
            bank.Read (an1 ? 0xC05B : 0xC05A);

            for (int index = 0; index < kButtonCount; index++)
            {
                Byte  value     = 0xFF;
                bool  answered  = joyport.TryReadButton (index, value);
                bool  isClosed  = (value & kButtonBit) == 0;

                Assert::IsTrue (answered, L"an attached Joyport past its window answers");
                Assert::AreEqual (expected[index], isClosed,
                    std::format (L"PB{} with AN0 {} and AN1 {}", index, an0 ? L"on" : L"off", an1 ? L"on" : L"off").c_str());
            }
        }
    };


    static JoystickSwitches Switches (std::initializer_list<JoystickSwitch> closed)
    {
        JoystickSwitches  switches;

        for (JoystickSwitch sw : closed)
        {
            switches.set (static_cast<size_t> (sw));
        }

        return switches;
    }
};
