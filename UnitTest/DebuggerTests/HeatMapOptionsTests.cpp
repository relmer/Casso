#include "Pch.h"

#include "Debugger/HeatMapOptions.h"
#include "Shell/CpuCommandDispatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptionsTests
//
//  The heat map's options as the settings keep them and as the window sends
//  them to the machine.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapOptionsTests)
    {
    public:

        using View = HeatMapOptions::View;



        TEST_METHOD (NoTextIsTheDefaults)
        {
            HeatMapOptions  options = HeatMapOptions::FromText ("");



            Assert::IsTrue   (options == HeatMapOptions());
            Assert::IsFalse  (options.cumulative, L"the heat fades unless asked");
            Assert::AreEqual (10, options.fadeSeconds, L"ten seconds of fade");
            Assert::AreEqual ((int) View::All, (int) options.view);
        }



        TEST_METHOD (EveryOptionSurvivesTheRoundTrip)
        {
            HeatMapOptions  options;



            options.cumulative  = true;
            options.fadeSeconds = 30;
            options.view        = View::Data;

            Assert::IsTrue (HeatMapOptions::FromText (options.ToText()) == options);
            Assert::IsTrue (HeatMapOptions::FromText (HeatMapOptions().ToText()) == HeatMapOptions());
        }



        TEST_METHOD (TheTextIsNeverEmpty)
        {
            Assert::IsFalse (HeatMapOptions().ToText().empty(), L"a setting written once must read as written");
        }



        TEST_METHOD (AFadeOutOfRangeIsHeldToItAndNonsenseIsPassedOver)
        {
            Assert::AreEqual (HeatMapOptions::kMaxFadeSeconds, HeatMapOptions::FromText ("fade=9999").fadeSeconds);
            Assert::AreEqual (HeatMapOptions::kMinFadeSeconds, HeatMapOptions::FromText ("fade=0").fadeSeconds);
            Assert::AreEqual (HeatMapOptions::kDefaultFadeSeconds, HeatMapOptions::FromText ("fade=-3 fade=x fade= fade=99999999999").fadeSeconds);
            Assert::IsTrue   (HeatMapOptions::FromText ("view=sideways cumulative").cumulative);
        }



        TEST_METHOD (TheViewMessageCarriesTheOptionsText)
        {
            std::string  text;



            Assert::IsTrue   (CpuCommandDispatcher::TryGetHeatMapOptions ("options fade=5 view=code cumulative", text));
            Assert::AreEqual (std::string ("fade=5 view=code cumulative"), text);
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapOptions ("on", text));
            Assert::IsFalse  (CpuCommandDispatcher::TryGetHeatMapOptions ("options", text));
        }
    };
}
