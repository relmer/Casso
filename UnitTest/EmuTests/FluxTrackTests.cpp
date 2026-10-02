#include "Pch.h"
#include "Devices/Disk/FluxTrack.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrackTests
//
//  The raw flux stream: decoding runs of 255, walking with a cursor across
//  the end of a revolution, validation, and splicing a write burst in without
//  moving the rest of the track.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FluxTrackTests)
{
public:

    static FluxTrack MakeTrack (const vector<Byte> & bytes)
    {
        FluxTrack  track;

        track.Assign (bytes);
        return track;
    }


    TEST_METHOD (TotalTicksIsTheSumOfEveryByte)
    {
        FluxTrack  track = MakeTrack ({ 32, 32, 64, 255, 10, 31 });

        Assert::AreEqual (static_cast<uint64_t> (32 + 32 + 64 + 255 + 10 + 31), track.GetTotalTicks());
        Assert::AreEqual (static_cast<size_t> (5), track.GetTransitionCount());
    }


    TEST_METHOD (RunOf255DecodesAsOneGap)
    {
        FluxTrack         track = MakeTrack ({ 255, 255, 10, 32 });
        vector<uint64_t>  ticks;

        track.GetTransitionTicks (ticks);

        Assert::AreEqual (static_cast<size_t> (2), ticks.size());
        Assert::AreEqual (static_cast<uint64_t> (520), ticks[0]);
        Assert::AreEqual (static_cast<uint64_t> (552), ticks[1]);
    }


    TEST_METHOD (CursorWrapsIntoTheNextRevolution)
    {
        FluxTrack          track  = MakeTrack ({ 10, 20, 30 });
        FluxTrack::Cursor  cursor = track.FindTransitionAtOrAfter (0);

        Assert::AreEqual (static_cast<uint64_t> (10), cursor.tick);

        track.AdvanceCursor (cursor);
        track.AdvanceCursor (cursor);
        Assert::AreEqual (static_cast<uint64_t> (60), cursor.tick);

        track.AdvanceCursor (cursor);
        Assert::AreEqual (static_cast<uint64_t> (70), cursor.tick);

        track.AdvanceCursor (cursor);
        Assert::AreEqual (static_cast<uint64_t> (90), cursor.tick);
    }


    TEST_METHOD (SeekInsideAGapFindsTheTransitionThatEndsIt)
    {
        FluxTrack          track  = MakeTrack ({ 10, 20, 30 });
        FluxTrack::Cursor  cursor = track.FindTransitionAtOrAfter (11);

        Assert::AreEqual (static_cast<uint64_t> (30), cursor.tick);

        cursor = track.FindTransitionAtOrAfter (30);
        Assert::AreEqual (static_cast<uint64_t> (30), cursor.tick);
    }


    TEST_METHOD (EmptyTrackHasNoTransitionsAndANominalRevolution)
    {
        FluxTrack          track  = MakeTrack ({});
        FluxTrack::Cursor  cursor = track.FindTransitionAtOrAfter (0);

        Assert::IsFalse (track.HasTransitions());
        Assert::AreEqual (UINT64_MAX, cursor.tick);
        Assert::AreEqual (FluxTrack::GetCellStartTick (FluxTrack::kNominalRevolutionCells),
                          track.GetRevolutionTicks());
    }


    TEST_METHOD (TrailingRunByteIsInvalid)
    {
        Assert::IsFalse (FluxTrack::IsValidStream ({ 32, 255 }));
        Assert::IsTrue  (FluxTrack::IsValidStream ({ 255, 0 }));
        Assert::IsTrue  (FluxTrack::IsValidStream ({}));
    }


    TEST_METHOD (CellStartTickDoesNotDrift)
    {
        Assert::AreEqual (static_cast<uint64_t> (31),       FluxTrack::GetCellStartTick (1));
        Assert::AreEqual (static_cast<uint64_t> (1408),     FluxTrack::GetCellStartTick (45));
        Assert::AreEqual (static_cast<uint64_t> (1408000),  FluxTrack::GetCellStartTick (45000));
    }


    //  A track of evenly spaced transitions, every 32 ticks, ending exactly at
    //  the end of the revolution.
    static vector<Byte> MakeEven (size_t count)
    {
        return vector<Byte> (count, 32);
    }


    TEST_METHOD (SpliceKeepsTheRevolutionLength)
    {
        FluxTrack         track  = MakeTrack (MakeEven (1000));
        uint64_t          before = track.GetTotalTicks();
        vector<uint8_t>   bits   = { 1, 0, 1, 1, 0, 0, 1, 0 };

        track.SpliceWrite (3200, bits);

        Assert::AreEqual (before, track.GetTotalTicks());
    }


    TEST_METHOD (SpliceLeavesEveryTransitionOutsideTheBurstInPlace)
    {
        FluxTrack         track = MakeTrack (MakeEven (1000));
        vector<uint64_t>  before;
        vector<uint64_t>  after;
        vector<uint8_t>   bits (16, 1);
        uint64_t          start = 3200;
        uint64_t          end   = start + FluxTrack::GetCellStartTick (bits.size());
        size_t            i     = 0;
        size_t            kept  = 0;

        track.GetTransitionTicks (before);
        track.SpliceWrite (start, bits);
        track.GetTransitionTicks (after);

        for (i = 0; i < before.size(); i++)
        {
            if (before[i] >= start && before[i] < end)
            {
                continue;
            }

            Assert::IsTrue (find (after.begin(), after.end(), before[i]) != after.end());
            kept++;
        }

        Assert::IsTrue (kept > 900);
    }


    TEST_METHOD (SpliceWritesOneTransitionPerOneBitAtTheControllerCell)
    {
        FluxTrack         track = MakeTrack (MakeEven (1000));
        vector<uint64_t>  ticks;
        vector<uint8_t>   bits  = { 1, 0, 0, 1, 1 };
        uint64_t          start = 6400;
        uint64_t          end   = start + FluxTrack::GetCellStartTick (bits.size());
        vector<uint64_t>  inside;
        size_t            i     = 0;

        track.SpliceWrite (start, bits);
        track.GetTransitionTicks (ticks);

        for (i = 0; i < ticks.size(); i++)
        {
            if (ticks[i] >= start && ticks[i] < end)
            {
                inside.push_back (ticks[i]);
            }
        }

        Assert::AreEqual (static_cast<size_t> (3), inside.size());
        Assert::AreEqual (start + (FluxTrack::GetCellStartTick (0) + FluxTrack::GetCellStartTick (1)) / 2, inside[0]);
        Assert::AreEqual (start + (FluxTrack::GetCellStartTick (3) + FluxTrack::GetCellStartTick (4)) / 2, inside[1]);
        Assert::AreEqual (start + (FluxTrack::GetCellStartTick (4) + FluxTrack::GetCellStartTick (5)) / 2, inside[2]);
    }


    TEST_METHOD (SpliceOfAllZerosLeavesOneLongGap)
    {
        FluxTrack         track = MakeTrack (MakeEven (1000));
        vector<uint64_t>  ticks;
        vector<uint8_t>   bits (100, 0);
        uint64_t          start = 6400;
        uint64_t          end   = start + FluxTrack::GetCellStartTick (bits.size());
        size_t            i     = 0;

        track.SpliceWrite (start, bits);
        track.GetTransitionTicks (ticks);

        for (i = 0; i < ticks.size(); i++)
        {
            Assert::IsFalse (ticks[i] >= start && ticks[i] < end);
        }

        Assert::AreEqual (static_cast<uint64_t> (32000), track.GetTotalTicks());
    }


    TEST_METHOD (SpliceAcrossTheEndOfTheRevolutionKeepsItsLength)
    {
        FluxTrack         track = MakeTrack (MakeEven (1000));
        vector<uint8_t>   bits (64, 1);

        track.SpliceWrite (32000 - 640, bits);

        Assert::AreEqual (static_cast<uint64_t> (32000), track.GetTotalTicks());
        Assert::IsTrue   (FluxTrack::IsValidStream (track.GetBytes()));
    }


    TEST_METHOD (SplicedBitsReadBackThroughACursor)
    {
        FluxTrack          track  = MakeTrack (MakeEven (1000));
        vector<uint8_t>    bits   = { 1, 1, 0, 1 };
        uint64_t           start  = 9600;
        FluxTrack::Cursor  cursor;

        track.SpliceWrite (start, bits);

        cursor = track.FindTransitionAtOrAfter (start);
        Assert::AreEqual (start + (FluxTrack::GetCellStartTick (0) + FluxTrack::GetCellStartTick (1)) / 2, cursor.tick);

        track.AdvanceCursor (cursor);
        Assert::AreEqual (start + (FluxTrack::GetCellStartTick (1) + FluxTrack::GetCellStartTick (2)) / 2, cursor.tick);

        track.AdvanceCursor (cursor);
        Assert::AreEqual (start + (FluxTrack::GetCellStartTick (3) + FluxTrack::GetCellStartTick (4)) / 2, cursor.tick);
    }


    TEST_METHOD (SpliceEncodesALongGapAsARun)
    {
        FluxTrack         track = MakeTrack (MakeEven (1000));
        vector<uint8_t>   bits (40, 0);
        size_t            runs  = 0;
        size_t            i     = 0;

        bits[39] = 1;
        track.SpliceWrite (6400, bits);

        for (i = 0; i < track.GetBytes().size(); i++)
        {
            runs += (track.GetBytes()[i] == FluxTrack::kRunByte) ? 1 : 0;
        }

        Assert::IsTrue (runs >= 4);
        Assert::IsTrue (FluxTrack::IsValidStream (track.GetBytes()));
    }
};
