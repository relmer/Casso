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
//  The debugger status bar's parts: the zoom percentage, the history section's
//  fill and text, which once the budget drops the oldest snapshots shows where
//  history begins, and the note while a replay runs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DebuggerStatusTextTests)
{
public:

    static HistoryStatus  MakeRecording (size_t used, size_t budget)
    {
        HistoryStatus  status;

        status.isRecording = true;
        status.usedBytes   = used;
        status.budgetBytes = budget;
        return status;
    }


    TEST_METHOD (ZoomText_IsAWholePercentage)
    {
        Assert::AreEqual (std::wstring (L"100%"), DebuggerStatusText::GetZoomText (1.0f));
        Assert::AreEqual (std::wstring (L"50%"),  DebuggerStatusText::GetZoomText (0.5f));
        Assert::AreEqual (std::wstring (L"130%"), DebuggerStatusText::GetZoomText (1.3f));
    }


    TEST_METHOD (HistoryFill_IsTheUsedFractionOrNoneWhileNotRecordingOrFull)
    {
        HistoryStatus  off  = MakeRecording (10, 100);
        HistoryStatus  full = MakeRecording (100, 100);

        off.isRecording = false;
        full.isFull     = true;

        Assert::AreEqual (0.25f, DebuggerStatusText::GetHistoryFill (MakeRecording (25, 100)));
        Assert::AreEqual (1.0f,  DebuggerStatusText::GetHistoryFill (MakeRecording (100, 100)));
        Assert::AreEqual (-1.0f, DebuggerStatusText::GetHistoryFill (off));
        Assert::AreEqual (-1.0f, DebuggerStatusText::GetHistoryFill (full));
    }


    TEST_METHOD (HistoryText_ShowsTheBufferRemainingWhileFilling)
    {
        Assert::AreEqual (std::wstring (L"History buffer remaining: 37%"),  DebuggerStatusText::GetHistoryText (MakeRecording (63, 100)));
        Assert::AreEqual (std::wstring (L"History buffer remaining: 100%"), DebuggerStatusText::GetHistoryText (MakeRecording (0, 100)));
    }


    TEST_METHOD (HistoryText_IsOffWhileNotRecording)
    {
        HistoryStatus  off = MakeRecording (10, 100);

        off.isRecording = false;
        off.isFull      = true;

        Assert::AreEqual (std::wstring (L"History off"), DebuggerStatusText::GetHistoryText (off));
    }

    TEST_METHOD (FormatTime_UsesSecondsThenMinutesThenHours)
    {
        constexpr uint64_t  kSecond = 1020484;

        Assert::AreEqual (std::wstring (L"0.0 s"),    DebuggerStatusText::FormatTime (0));
        Assert::AreEqual (std::wstring (L"12.5 s"),   DebuggerStatusText::FormatTime (kSecond * 125 / 10));
        Assert::AreEqual (std::wstring (L"1:02.0"),   DebuggerStatusText::FormatTime (kSecond * 62));
        Assert::AreEqual (std::wstring (L"1:00:05"),  DebuggerStatusText::FormatTime (kSecond * 3605));
    }


    static HistoryStatus  MakeFull()
    {
        HistoryStatus  status = MakeRecording (100, 100);

        status.isFull     = true;
        status.hasHistory = true;
        status.beginCycle = 416512334;
        return status;
    }


    TEST_METHOD (HistoryText_OnceFull_ShowsWhereHistoryBeginsWithoutAHostTime)
    {
        Assert::AreEqual (std::wstring (L"History begins at Power + 6:48.1 (cycle 416,512,334)"), DebuggerStatusText::GetHistoryText (MakeFull(), L"en-US"));
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
        Assert::IsFalse (first.isFull, L"nothing has been dropped yet");

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
        Assert::IsTrue (later.isFull, L"the store says it has started dropping");
    }


    static uint64_t  s_fakeClock;

    static uint64_t  ReadFakeClock()
    {
        return ++s_fakeClock;
    }


    TEST_METHOD (FillBudget_BeginWallTimeMovesForwardOnceTheBudgetDropsOldSnapshots)
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



        s_fakeClock             = 1000;
        settings.intervalCycles = kInterval;
        settings.wholeEvery     = 2;
        settings.budgetBytes    = kStateBytes * 4;
        store.Configure    (settings);
        store.SetWallClock (&ReadFakeClock);

        hr = store.Add (0, 0, state);
        AssertSucceeded (hr, L"Add");

        ReverseHost::FillBudget (store, first);
        Assert::AreEqual ((uint64_t) 1001, first.beginWallTime, L"the first keyframe holds the clock read when it was taken");

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
        Assert::IsTrue (later.beginWallTime > first.beginWallTime, L"dropping the oldest snapshots moves the wall-clock start forward");
        Assert::AreEqual (1000 + store.GetInfo (0).position + 1, later.beginWallTime);
    }


    static SYSTEMTIME  MakeEvening()
    {
        SYSTEMTIME  time = {};

        time.wYear   = 2026;
        time.wMonth  = 10;
        time.wDay    = 4;
        time.wHour   = 22;
        time.wMinute = 42;
        time.wSecond = 7;
        return time;
    }


    TEST_METHOD (FormatClock_FollowsTheLocaleTimeFormatWithSeconds)
    {
        Assert::AreEqual (std::wstring (L"10:42:07 PM"), DebuggerStatusText::FormatClock (MakeEvening(), L"en-US"));
        Assert::AreEqual (std::wstring (L"22:42:07"),    DebuggerStatusText::FormatClock (MakeEvening(), L"de-DE"));
    }


    TEST_METHOD (FormatTime_UsesTheLocaleDecimalSeparator)
    {
        constexpr uint64_t  kSecond = 1020484;

        Assert::AreEqual (std::wstring (L"12.5 s"),  DebuggerStatusText::FormatTime (kSecond * 125 / 10, L"en-US"));
        Assert::AreEqual (std::wstring (L"12,5 s"),  DebuggerStatusText::FormatTime (kSecond * 125 / 10, L"de-DE"));
        Assert::AreEqual (std::wstring (L"1:02,0"),  DebuggerStatusText::FormatTime (kSecond * 62,       L"de-DE"));
    }


    TEST_METHOD (HistoryText_OnceFull_ShowsTheWallClockThenPowerTimeAndCycle)
    {
        HistoryStatus  status = MakeFull();
        std::wstring   text;



        status.beginWallTime = 134000000000000000ull;    // a FILETIME in 2025

        text = DebuggerStatusText::GetHistoryText (status, L"en-US");
        Assert::IsTrue (text.starts_with (L"History begins at "),                 text.c_str());
        Assert::IsTrue (text.ends_with (L" (Power + 6:48.1, cycle 416,512,334)"), text.c_str());
        Assert::IsTrue (text.find (L"M (Power") != std::wstring::npos,            text.c_str());

        text = DebuggerStatusText::GetHistoryText (status, L"de-DE");
        Assert::IsTrue (text.ends_with (L" (Power + 6:48,1, cycle 416.512.334)"), text.c_str());
    }


    TEST_METHOD (FormatCount_UsesTheLocaleDigitGrouping)
    {
        Assert::AreEqual (std::wstring (L"416,512,334"), DebuggerStatusText::FormatCount (416512334, L"en-US"));
        Assert::AreEqual (std::wstring (L"416.512.334"), DebuggerStatusText::FormatCount (416512334, L"de-DE"));
        Assert::AreEqual (std::wstring (L"7"),           DebuggerStatusText::FormatCount (7,         L"en-US"));
    }};

uint64_t  DebuggerStatusTextTests::s_fakeClock = 0;
