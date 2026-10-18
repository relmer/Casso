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


    TEST_METHOD (TheMaximumIsNoMoreThanFortyEightHoursOfABusyProgram)
    {
        Assert::AreEqual (11460, ReverseBudget::GetBudgetForHours (48.0), L"172,800 seconds at 69,540 bytes a second, rounded up");
        Assert::AreEqual (11460, ReverseBudget::GetRange (65536 * kMb).maxMb, L"64 GB available");
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


    //  A recording of the given emulated seconds at 1,000 bytes a second,
    //  with half its budget left to the snapshots.
    static HistoryStatus MakeRecording (uint64_t seconds)
    {
        HistoryStatus  status;



        status.isRecording = true;
        status.hasHistory  = true;
        status.beginCycle  = 0;
        status.cycle       = (uint64_t) (seconds * HistoryStatus::kCyclesPerSecond);
        status.storedBytes = (size_t) (seconds * 1000);
        status.budgetBytes = 64 * kMb;
        status.arenaBytes  = 32 * kMb;

        return status;
    }


    TEST_METHOD (WithoutARecordingTheEstimateRunsFromTheBusiestToTheIdlest)
    {
        Assert::AreEqual (std::wstring (L"Room for about 16 minutes of history for a program that redraws the whole screen, up to 27 hours for one waiting for a key."),
                          ReverseBudget::GetEstimateText (64, HistoryStatus()));
        Assert::AreEqual (std::wstring (L"Room for about 1 minute of history for a program that redraws the whole screen, up to 25 minutes for one waiting for a key."),
                          ReverseBudget::GetEstimateText (1, HistoryStatus()), L"never fewer than one minute");
    }


    TEST_METHOD (ARecordingOfThirtySecondsOrMoreUsesItsOwnRate)
    {
        Assert::AreEqual (std::wstring (L"At this program's rate so far, room for about 9 hours of history."),
                          ReverseBudget::GetEstimateText (64, MakeRecording (60)), L"559 minutes for 32 MB at 1,000 bytes a second");
        Assert::AreEqual (std::wstring (L"Room for about 16 minutes of history for a program that redraws the whole screen, up to 27 hours for one waiting for a key."),
                          ReverseBudget::GetEstimateText (64, MakeRecording (20)), L"20 seconds is too short to measure");
    }
};