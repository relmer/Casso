#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "Ui/Debugger/DebuggerStatusText.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusTextTests
//
//  The debugger status bar's parts: the zoom percentage, the history budget's
//  fill and the color its meter blends through, the time history begins at,
//  which moves forward once the budget drops the oldest snapshots, and the
//  note while a replay runs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerStatusTextTests)
{
public:

    static constexpr uint32_t  kGreen  = 0xFF00FF00u;
    static constexpr uint32_t  kYellow = 0xFFFFFF00u;
    static constexpr uint32_t  kRed    = 0xFFFF0000u;


    static HistoryStatus  MakeRecording (size_t used, size_t budget)
    {
        HistoryStatus  status;

        status.isRecording = true;
        status.usedBytes   = used;
        status.budgetBytes = budget;
        return status;
    }


    //  Every channel within one step: a blend's rounding is not the point.
    static bool  AreChannelsNear (uint32_t a, uint32_t b)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            int  delta = (int) ((a >> shift) & 0xFFu) - (int) ((b >> shift) & 0xFFu);

            if (delta > 1 || delta < -1)
            {
                return false;
            }
        }

        return true;
    }


    TEST_METHOD (ZoomText_IsAWholePercentage)
    {
        Assert::AreEqual (std::wstring (L"100%"), DebuggerStatusText::GetZoomText (1.0f));
        Assert::AreEqual (std::wstring (L"50%"),  DebuggerStatusText::GetZoomText (0.5f));
        Assert::AreEqual (std::wstring (L"130%"), DebuggerStatusText::GetZoomText (1.3f));
    }


    TEST_METHOD (BudgetFill_IsTheUsedFractionOrNoneWhileNotRecording)
    {
        HistoryStatus  off = MakeRecording (10, 100);

        off.isRecording = false;

        Assert::AreEqual (0.25f, DebuggerStatusText::GetBudgetFill (MakeRecording (25, 100)));
        Assert::AreEqual (-1.0f, DebuggerStatusText::GetBudgetFill (off));
        Assert::AreEqual (std::wstring (L"History 25% full"), DebuggerStatusText::GetBudgetText (MakeRecording (25, 100)));
        Assert::AreEqual (std::wstring (L"History off"),      DebuggerStatusText::GetBudgetText (off));
    }


    TEST_METHOD (BudgetColor_IsGreenEmptyYellowAtTwentyLeftRedAtTenLeft)
    {
        Assert::AreEqual (kGreen,  DebuggerStatusText::GetBudgetColor (0.0f,  kGreen, kYellow, kRed));
        Assert::AreEqual (kYellow, DebuggerStatusText::GetBudgetColor (0.8f,  kGreen, kYellow, kRed));
        Assert::AreEqual (kRed,    DebuggerStatusText::GetBudgetColor (0.9f,  kGreen, kYellow, kRed));
        Assert::AreEqual (kRed,    DebuggerStatusText::GetBudgetColor (1.0f,  kGreen, kYellow, kRed));
    }


    TEST_METHOD (BudgetColor_BlendsSmoothlyBetweenTheStops)
    {
        uint32_t  halfToYellow = DebuggerStatusText::GetBudgetColor (0.4f,  kGreen, kYellow, kRed);
        uint32_t  halfToRed    = DebuggerStatusText::GetBudgetColor (0.85f, kGreen, kYellow, kRed);
        uint32_t  justBefore   = DebuggerStatusText::GetBudgetColor (0.799f, kGreen, kYellow, kRed);



        Assert::IsTrue (AreChannelsNear (DxuiColor::Lerp (kGreen,  kYellow, 0.5f), halfToYellow));
        Assert::IsTrue (AreChannelsNear (DxuiColor::Lerp (kYellow, kRed,    0.5f), halfToRed));

        //  No jump at the yellow stop: just short of it is all but yellow.
        Assert::IsTrue (((justBefore >> 16) & 0xFFu) >= 0xFDu);
    }


    TEST_METHOD (FormatTime_UsesSecondsThenMinutesThenHours)
    {
        constexpr uint64_t  kSecond = 1020484;

        Assert::AreEqual (std::wstring (L"0.0 s"),    DebuggerStatusText::FormatTime (0));
        Assert::AreEqual (std::wstring (L"12.5 s"),   DebuggerStatusText::FormatTime (kSecond * 125 / 10));
        Assert::AreEqual (std::wstring (L"1:02.0"),   DebuggerStatusText::FormatTime (kSecond * 62));
        Assert::AreEqual (std::wstring (L"1:00:05"),  DebuggerStatusText::FormatTime (kSecond * 3605));
    }


    TEST_METHOD (BeginText_ShowsOnlyWhileThereIsHistory)
    {
        HistoryStatus  status = MakeRecording (1, 100);

        Assert::AreEqual (std::wstring(), DebuggerStatusText::GetBeginText (status));

        status.hasHistory = true;
        status.beginCycle = 1020484 * 3;
        Assert::AreEqual (std::wstring (L"Begins at 3.0 s"), DebuggerStatusText::GetBeginText (status));
    }


    TEST_METHOD (ReplayText_ShowsOnlyWhileReplaying)
    {
        Assert::AreEqual (std::wstring(), DebuggerStatusText::GetReplayText (false));
        Assert::IsTrue (DebuggerStatusText::GetReplayText (true).starts_with (L"Replaying history"));
    }


    TEST_METHOD (FillBudget_BeginMovesForwardOnceTheBudgetDropsOldSnapshots)
    {
        constexpr size_t    kStateBytes = 4096;
        constexpr uint64_t  kInterval   = 1000;

        KeyframeStore      store;
        KeyframeSettings   settings;
        HistoryStatus      first;
        HistoryStatus      later;
        std::vector<Byte>  state (kStateBytes);
        HRESULT            hr = S_OK;
        size_t             i  = 0;
        size_t             j  = 0;



        settings.intervalCycles = kInterval;
        settings.wholeEvery     = 2;
        settings.budgetBytes    = kStateBytes * 4;
        store.Configure (settings);

        hr = store.Add (0, 0, state);
        AssertSucceeded (hr, L"Add");

        ReverseHost::FillBudget (store, first);
        Assert::IsTrue (first.hasHistory);
        Assert::AreEqual ((uint64_t) 0, first.beginCycle);
        Assert::AreEqual (settings.budgetBytes, first.budgetBytes);
        Assert::IsTrue (first.usedBytes > 0);

        for (i = 1; i < 200; i++)
        {
            for (j = 0; j < kStateBytes; j++)
            {
                state[j] = (Byte) ((i * 131 + j * 7919) >> 3);
            }

            hr = store.Add (i, i * kInterval, state);
            AssertSucceeded (hr, L"Add");
        }

        ReverseHost::FillBudget (store, later);
        Assert::IsTrue (later.beginCycle > 0, L"dropping the oldest snapshots moves the beginning forward");
        Assert::IsTrue (later.usedBytes <= later.budgetBytes);
    }
};
