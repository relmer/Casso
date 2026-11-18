#include "Pch.h"

#include "Debugger/Reverse/HistorySpan.h"
#include "Debugger/Reverse/ReplayStepTable.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HistorySpanTests
//
//  Reverse execution's span of history apart from a machine: live and
//  behind live, the pause at Maximum speed and the gap after it, a pending
//  edit; and the step table one replay of a stretch fills.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HistorySpanTests)
{
public:

    TEST_METHOD (LeavingLiveNotesWhereHistoryEnds)
    {
        HistorySpan  span;



        span.Start();

        Assert::IsTrue   (span.IsLive());
        Assert::AreEqual ((uint64_t) 500, span.GetLiveEndPosition (500), L"live, history ends where the machine is");

        Assert::IsTrue   (span.Leave (500, 9000));
        Assert::IsFalse  (span.Leave (300, 4000), L"already behind live");
        Assert::IsTrue   (span.IsInHistory());
        Assert::AreEqual ((uint64_t) 500,  span.GetLiveEndPosition (300));
        Assert::AreEqual ((uint64_t) 9000, span.GetLiveEndCycle (4000));
        Assert::AreEqual ((uint64_t) 500,  span.GetRecordedEnd (300));

        span.BecomeLive (500);

        Assert::IsFalse  (span.IsInHistory());
        Assert::AreEqual ((uint64_t) 600, span.GetLiveEndPosition (600));
    }


    TEST_METHOD (APauseEndsRecordedHistoryWhereItPaused)
    {
        HistorySpan  span;



        span.Start();

        Assert::IsTrue  (span.SetPaused (true));
        Assert::IsFalse (span.SetPaused (true), L"no change");

        span.BecomeLive (100);

        Assert::AreEqual ((uint64_t) 100, span.GetRecordedEnd (250), L"recording stopped at 100");
        Assert::IsTrue   (span.IsPausedBefore (250), L"the positions past 100 are a gap");
        Assert::IsFalse  (span.IsPausedBefore (100));

        span.NotePause (250);

        Assert::AreEqual ((uint64_t) 250, span.GetPauseStart());
    }


    TEST_METHOD (AnEditIsTakenOnce)
    {
        HistorySpan  span;



        span.MarkEdit();

        Assert::IsTrue  (span.TakeEdit());
        Assert::IsFalse (span.TakeEdit());
    }


    TEST_METHOD (StopLeavesTheMachineLiveWithNoEdit)
    {
        HistorySpan  span;



        span.Start();
        span.MarkEdit();
        (void) span.Leave (10, 100);

        span.Stop();

        Assert::IsFalse (span.IsRecording());
        Assert::IsTrue  (span.IsLive());
        Assert::IsFalse (span.TakeEdit());
    }


    TEST_METHOD (TheStepTableHasOneStepPerInstructionOfItsStretch)
    {
        ReplayStepTable            table;
        std::vector<ReplayStep>    recent;
        std::vector<ReplayStep>  & steps = table.BeginFill();
        uint64_t                   i     = 0;



        for (i = 0; i < 5; i++)
        {
            steps.push_back ({ 0, static_cast<Word> (0x1000 + i) });
        }

        AssertSucceeded (table.Commit (100, 105), L"Commit");

        Assert::IsTrue   (table.IsBuiltFor (100, 105));
        Assert::AreEqual ((size_t) 1, table.GetBuildCount());
        Assert::AreEqual ((Word) 0x1002, table.Get (102).pc);
        Assert::IsTrue   (table.Contains (104));
        Assert::IsFalse  (table.Contains (105), L"the stretch's end is the next one's start");
        Assert::IsTrue   (table.Leads (105), L"the steps that led to its end are in it");

        table.GetRecent (104, 2, recent);

        Assert::AreEqual ((size_t) 2, recent.size());
        Assert::AreEqual ((Word) 0x1002, recent[0].pc);
        Assert::AreEqual ((Word) 0x1003, recent[1].pc);

        table.Discard();

        Assert::IsFalse (table.HasSteps());
        table.GetRecent (104, 2, recent);
        Assert::IsTrue  (recent.empty());
    }
};
