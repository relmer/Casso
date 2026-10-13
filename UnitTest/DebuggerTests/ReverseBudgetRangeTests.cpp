#include "Pch.h"

#include "Ui/Debugger/ReverseBudget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudgetRangeTests
//
//  The History slider's range from the memory available, a saved budget
//  placed on it, and the estimate of the history a budget has room for.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseBudgetRangeTests)
{
public:

    static constexpr uint64_t  kMb = 1024 * 1024;

    TEST_METHOD (TheMaximumIsTheLowerOfNinetyPercentAndFourMbLess)
    {
        Assert::AreEqual (7372, ReverseBudget::GetRange (8192 * kMb).maxMb, L"8 GB: 90% is lower than 8188 MB");
        Assert::AreEqual (16,   ReverseBudget::GetRange (20 * kMb).maxMb,   L"20 MB: 16 MB is lower than 18 MB");
        Assert::AreEqual (1,    ReverseBudget::GetRange (20 * kMb).minMb);
    }


    TEST_METHOD (TheRangeIsNeverBelowOneMb)
    {
        Assert::AreEqual (1, ReverseBudget::GetRange (0).maxMb,       L"no memory reported");
        Assert::AreEqual (1, ReverseBudget::GetRange (3 * kMb).maxMb, L"less than 4 MB available");
    }


    TEST_METHOD (APageIsATenthOfTheRange)
    {
        Assert::AreEqual (737, ReverseBudget::GetRange (8192 * kMb).pageMb);
        Assert::AreEqual (1,   ReverseBudget::GetRange (5 * kMb).pageMb, L"never less than 1 MB");
    }


    TEST_METHOD (ASavedBudgetAboveTheMaximumSitsAtTheMaximum)
    {
        ReverseBudgetRange  range = ReverseBudget::GetRange (100 * kMb);



        Assert::AreEqual (range.maxMb, ReverseBudget::PlaceOnRange (4096, range));
        Assert::AreEqual (64,          ReverseBudget::PlaceOnRange (64,   range));
        Assert::AreEqual (1,           ReverseBudget::PlaceOnRange (0,    range));
    }


    TEST_METHOD (TheEstimateFollowsTheMeasuredRecordings)
    {
        double  low  = 0.0;
        double  high = 0.0;



        ReverseBudget::EstimateMinutes (64, 10, low, high);
        Assert::AreEqual (24.1, low,  0.001, L"a game filled 64 MB in 24.1 minutes");
        Assert::AreEqual (43.8, high, 0.001, L"hi-res drawing in 43.8");

        ReverseBudget::EstimateMinutes (128, 20, low, high);
        Assert::AreEqual (24.1 * 4, low, 0.001, L"twice the memory at half the snapshots lasts four times as long");

        Assert::AreEqual (std::wstring (L"Room for about 24 to 44 minutes of history; a busy program fills it sooner."),
                          ReverseBudget::GetEstimateText (64));
    }
};
