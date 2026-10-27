#include "Pch.h"

#include "Ui/Chrome/ReplayIndicator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayIndicatorTests
//
//  The replay indicator is drawn only while the machine runs behind live,
//  not live, paused, rewinding or in a capture paint; and its opacity pulses
//  from full down to kMinOpacity and back over kPulseMs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReplayIndicatorTests)
{
public:

    TEST_METHOD (ShownOnlyWhileRunningBehindLive)
    {
        Assert::IsTrue  (ReplayIndicator::IsShown (true,  false, false, false), L"running behind live");
        Assert::IsFalse (ReplayIndicator::IsShown (false, false, false, false), L"live");
        Assert::IsFalse (ReplayIndicator::IsShown (true,  true,  false, false), L"paused behind live");
        Assert::IsFalse (ReplayIndicator::IsShown (true,  false, true,  false), L"rewinding");
        Assert::IsFalse (ReplayIndicator::IsShown (true,  false, false, true),  L"in a capture paint");
    }


    TEST_METHOD (TheOpacityPulsesBetweenFullAndTheMinimum)
    {
        constexpr float  kTolerance = 0.001f;

        float  lowest  = 1.0f;
        float  highest = 0.0f;



        for (int64_t ms = 0; ms < ReplayIndicator::kPulseMs; ms += 10)
        {
            lowest  = std::min (lowest,  ReplayIndicator::GetOpacity (ms));
            highest = std::max (highest, ReplayIndicator::GetOpacity (ms));
        }

        Assert::AreEqual (1.0f, ReplayIndicator::GetOpacity (0), kTolerance, L"full at the start of a pulse");
        Assert::AreEqual (ReplayIndicator::kMinOpacity, ReplayIndicator::GetOpacity (ReplayIndicator::kPulseMs / 2), kTolerance, L"the minimum halfway through");
        Assert::AreEqual (ReplayIndicator::GetOpacity (100), ReplayIndicator::GetOpacity (ReplayIndicator::kPulseMs + 100), kTolerance, L"the same each pulse");
        Assert::IsTrue   (lowest >= ReplayIndicator::kMinOpacity - kTolerance && highest <= 1.0f + kTolerance, L"never outside the range");
    }
};
