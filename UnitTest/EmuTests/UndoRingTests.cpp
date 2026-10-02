#include "Pch.h"

#include "HResultAssert.h"
#include "Debugger/Reverse/UndoRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kRingSpacing   = 1000;
static constexpr uint64_t  s_kRingInterval  = 10000;
static constexpr size_t    s_kRingStateSize = 4096;





////////////////////////////////////////////////////////////////////////////////
//
//  UndoRingTests
//
//  The ring's bookkeeping on its own: registers per position over a
//  wrapping buffer, checkpoints that never cover less than one keyframe
//  interval whatever the budget, and truncation that keeps the ring ending
//  where the machine is.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UndoRingTests)
{
public:

    TEST_METHOD (RecordsSurviveWrappingAndARestartStartsOver)
    {
        UndoRing    ring;
        UndoRecord  record;
        uint64_t    position = 0;
        size_t      pushed   = 0;



        ring.Configure (MakeSettings (0), s_kRingInterval);

        for (position = 100; pushed < 100000; position++, pushed++)
        {
            ring.Push (position, MakeRecord (position));
        }

        Assert::AreEqual<uint64_t> (position, ring.GetEndPosition(), L"the ring ends at the newest record");
        Assert::IsTrue (ring.GetFirstPosition() > 100, L"a full ring dropped its oldest records");

        Assert::IsTrue  (ring.TryGetRecord (position - 1, record));
        Assert::AreEqual<uint64_t> (MakeRecord (position - 1).cycle, record.cycle);
        Assert::IsTrue  (ring.TryGetRecord (ring.GetFirstPosition(), record));
        Assert::AreEqual<Word> (MakeRecord (ring.GetFirstPosition()).pc, record.pc);
        Assert::IsFalse (ring.TryGetRecord (ring.GetFirstPosition() - 1, record), L"older than the ring");
        Assert::IsFalse (ring.TryGetRecord (position, record), L"not run yet");

        ring.Push (position + 50, MakeRecord (position + 50));

        Assert::AreEqual<uint64_t> (position + 50, ring.GetFirstPosition(), L"a position that does not continue the ring starts it over");
        Assert::AreEqual<uint64_t> (position + 51, ring.GetEndPosition());
    }


    TEST_METHOD (TheRingNeverCoversLessThanOneKeyframeInterval)
    {
        UndoRing  ring;
        size_t    needed  = static_cast<size_t> (s_kRingInterval / s_kRingSpacing) + 1;
        size_t    minimum = 0;
        size_t    i       = 0;
        HRESULT   hr      = S_OK;



        ring.Configure (MakeSettings (0), s_kRingInterval);

        for (i = 0; i < 50; i++)
        {
            hr = ring.AddCheckpoint (i * 10, i * s_kRingSpacing, i, std::vector<Byte> (s_kRingStateSize, static_cast<Byte> (i)));
            AssertSucceeded (hr, L"AddCheckpoint");
        }

        Assert::AreEqual (needed, ring.GetCheckpointCount(), L"a zero budget still keeps one interval's checkpoints");
        Assert::IsTrue (ring.GetCheckpoint (ring.GetCheckpointCount() - 1).cycle - ring.GetCheckpoint (0).cycle >= s_kRingInterval,
                        L"and they span a whole keyframe interval");
        minimum = UndoRing::GetMinimumBudget (MakeSettings (0), s_kRingInterval, s_kRingStateSize);

        Assert::IsTrue (minimum >= needed * s_kRingStateSize, L"the minimum budget holds that many states");
        Assert::AreEqual (needed, UndoRing::GetCheckpointLimit (MakeSettings (minimum), s_kRingInterval, s_kRingStateSize), L"and buys exactly that many");

        ring.Configure (MakeSettings (minimum * 3), s_kRingInterval);

        for (i = 0; i < 50; i++)
        {
            hr = ring.AddCheckpoint (i * 10, i * s_kRingSpacing, i, std::vector<Byte> (s_kRingStateSize, static_cast<Byte> (i)));
            AssertSucceeded (hr, L"AddCheckpoint");
        }

        Assert::AreEqual (needed * 3, ring.GetCheckpointCount(), L"a larger budget keeps more");
        Assert::AreEqual<Byte> (49, ring.GetCheckpoint (ring.GetCheckpointCount() - 1).state[0], L"the newest is kept");
    }


    TEST_METHOD (TruncationKeepsTheRingEndingAtTheMachine)
    {
        UndoRing    ring;
        UndoRecord  record;
        size_t      index = 0;
        uint64_t    i     = 0;
        HRESULT     hr    = S_OK;



        ring.Configure (MakeSettings (0), s_kRingInterval);

        for (i = 0; i < 100; i++)
        {
            if (i % 10 == 0)
            {
                hr = ring.AddCheckpoint (i, i * 100, static_cast<size_t> (i), std::vector<Byte> (s_kRingStateSize));
                AssertSucceeded (hr, L"AddCheckpoint");
            }

            ring.Push (i, MakeRecord (i));
        }

        ring.TruncateAt (40);

        Assert::AreEqual<uint64_t> (40, ring.GetEndPosition(), L"records from the restored position on are gone");
        Assert::IsTrue (ring.TryFindCheckpointAtOrBefore (99, index));
        Assert::AreEqual<uint64_t> (40, ring.GetCheckpoint (index).position, L"the checkpoint at the position stays");
        Assert::IsFalse (ring.IsCheckpointDue (4000 + 99), L"the next one falls due an interval after it");

        ring.TruncateFrom (40);

        Assert::IsTrue (ring.TryFindCheckpointAtOrBefore (99, index));
        Assert::AreEqual<uint64_t> (30, ring.GetCheckpoint (index).position, L"a change at the position drops its checkpoint too");
        Assert::IsTrue (ring.TryFindCheckpointByCycle (3500, index));
        Assert::AreEqual<uint64_t> (3000, ring.GetCheckpoint (index).cycle);
        Assert::IsTrue (ring.TryGetRecord (39, record));
        Assert::AreEqual<uint64_t> (MakeRecord (39).cycle, record.cycle);
    }


private:

    static UndoRingSettings MakeSettings (size_t budget)
    {
        UndoRingSettings  settings;



        settings.checkpointCycles = s_kRingSpacing;
        settings.budgetBytes      = budget;

        return settings;
    }


    static UndoRecord MakeRecord (uint64_t position)
    {
        UndoRecord  record;



        record.cycle = position * 3;
        record.pc    = static_cast<Word> (position * 7);
        record.sp    = static_cast<Byte> (position);

        return record;
    }
};
