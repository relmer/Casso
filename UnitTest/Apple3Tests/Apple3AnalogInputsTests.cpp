#include "Pch.h"

#include "Machines/Apple3/Apple3AnalogInputs.h"
#include "Machines/Apple3/Apple3TimeBase.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3AnalogInputsTests
//
//  The A/D's ramp for each joystick axis on ports A and B, selected by the
//  three select bits, and the four joystick switches.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3AnalogInputsTests)
{
public:

    static constexpr Word  kSelect0Off = 0xC058;
    static constexpr Word  kSelect0On  = 0xC059;
    static constexpr Word  kSelect2Off = 0xC05A;
    static constexpr Word  kSelect2On  = 0xC05B;
    static constexpr Word  kCharge     = 0xC05C;
    static constexpr Word  kStart      = 0xC05D;
    static constexpr Word  kSelect1Off = 0xC05E;
    static constexpr Word  kSelect1On  = 0xC05F;


    //  Sets the three select bits to the input's number.
    static void Select (Apple3AnalogInputs & analog, Byte input)
    {
        analog.HandleSwitch ((input & 1) != 0 ? kSelect0On : kSelect0Off);
        analog.HandleSwitch ((input & 2) != 0 ? kSelect1On : kSelect1Off);
        analog.HandleSwitch ((input & 4) != 0 ? kSelect2On : kSelect2Off);
    }


    TEST_METHOD (Selection_PicksEachAxisOfEachPort)
    {
        Apple3TimeBase      timeBase;
        Apple3AnalogInputs  analog (timeBase);



        analog.SetAxis (0, 10);     // port A X
        analog.SetAxis (1, 20);     // port B X
        analog.SetAxis (2, 30);     // port A Y
        analog.SetAxis (3, 40);     // port B Y

        Select (analog, 3);
        Assert::AreEqual<uint32_t> (360 + 8 * 10, analog.GetRampMicros(), L"input 3 is port A X");
        Select (analog, 4);
        Assert::AreEqual<uint32_t> (360 + 8 * 30, analog.GetRampMicros(), L"input 4 is port A Y");
        Select (analog, 1);
        Assert::AreEqual<uint32_t> (360 + 8 * 20, analog.GetRampMicros(), L"input 1 is port B X");
        Select (analog, 2);
        Assert::AreEqual<uint32_t> (360 + 8 * 40, analog.GetRampMicros(), L"input 2 is port B Y");
        Select (analog, 0);
        Assert::AreEqual<uint32_t> (0, analog.GetRampMicros(), L"input 0 is ground");
    }


    TEST_METHOD (Ramp_TimesOutAfterTheSelectedAxisDuration)
    {
        Apple3TimeBase      timeBase;
        Apple3AnalogInputs  analog (timeBase);
        uint64_t            ticks = 0;



        analog.SetAxis (0, 100);
        Select (analog, 3);
        analog.HandleSwitch (kCharge);
        analog.HandleSwitch (kStart);
        Assert::IsTrue (analog.IsTiming());

        //  360 + 800 microseconds, in 1 MHz cycles of two ticks, and a little
        //  past.
        ticks = (uint64_t) (1160e-6 * Apple3TimeBase::kMasterClockHz / Apple3TimeBase::kMasterPerTick);
        timeBase.SetOneMhz (true);
        timeBase.ChargeCycles ((uint32_t) (ticks / 2 - 10), Apple3Access::Ram);
        Assert::IsTrue (analog.IsTiming(), L"timing just before");

        timeBase.ChargeCycles (20, Apple3Access::Ram);
        Assert::IsFalse (analog.IsTiming(), L"timed out just after");
    }


    TEST_METHOD (Switches_ArePortAAndPortB)
    {
        Apple3TimeBase      timeBase;
        Apple3AnalogInputs  analog (timeBase);



        for (int button = 0; button < Apple3AnalogInputs::kButtonCount; button++)
        {
            analog.SetButton (button, true);
            Assert::IsTrue (analog.IsButtonPressed ((Word) (Apple3AnalogInputs::kFirstButton + button)));

            analog.SetButton (button, false);
            Assert::IsFalse (analog.IsButtonPressed ((Word) (Apple3AnalogInputs::kFirstButton + button)));
        }
    }
};
