#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/Session/RawWritePass.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePassTests
//
//  A plan's arcs as the one stream the raw write module takes: the arcs, the
//  stretches the drive doesn't write between them, and the arm's steps.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RawWritePassTests)
{
public:

    static constexpr size_t  kTurn     = 51200;
    static constexpr size_t  kArcBytes = 800;
    static constexpr size_t  kPage     = 256;
    static constexpr size_t  kMaxBytes = RawWriteStream::kMaxBytes;
    static constexpr int     kLow      = 8;
    static constexpr int     kHigh     = 10;


    //  An arc of bytes that differ from seed to seed, at a percentage of
    //  the turn.
    static RawWriteArcPlan::Arc MakeArc (int position, size_t percent, uint32_t seed)
    {
        RawWriteArcPlan::Arc  arc;



        for (size_t i = 0; i < kArcBytes; i++)
        {
            seed = seed * 1103515245 + 12345;
            arc.stream.values.push_back ((Byte) (0x96 | (seed >> 16)));
            arc.stream.zeros.push_back (0);
        }

        arc.position = position;
        arc.start    = kTurn * percent / 100;
        arc.cells    = arc.stream.GetCellCount();

        return arc;
    }


    //  Two arcs at track 2 and two at 2.5, as Karateka's are.
    static RawWriteArcPlan MakePlan()
    {
        RawWriteArcPlan  plan;



        plan.turn = kTurn;
        plan.arcs = { MakeArc (kLow, 55, 1), MakeArc (kLow, 0, 2), MakeArc (kHigh, 27, 3), MakeArc (kHigh, 82, 4) };

        return plan;
    }


    //  The places in the stream of the stretches the drive doesn't write.
    static std::vector<size_t> FindStretches (const RawWritePass & pass)
    {
        std::vector<size_t>  found;



        for (size_t at = 0; at < pass.count; at++)
        {
            if (pass.stream[kMaxBytes + at] == RawWritePass::kGapZeros)
            {
                found.push_back (at);
            }
        }

        return found;
    }


    //  A wait is within a few cycles of the cycles given, with one round of
    //  256 at least, from the shortest to more than a turn.
    TEST_METHOD (GetWait_IsWithinAFewCycles)
    {
        static constexpr size_t  kLeast     = 1287;
        static constexpr size_t  kMost      = 300000;
        static constexpr size_t  kStep      = 7;
        static constexpr size_t  kTolerance = 8;

        Byte    longRounds  = 0;
        Byte    shortRounds = 0;
        size_t  waited      = 0;



        for (size_t cycles = kLeast; cycles < kMost; cycles += kStep)
        {
            size_t  ones = 0;

            RawWritePass::GetWait (cycles, longRounds, shortRounds, waited);

            ones = shortRounds == 0 ? kPage : shortRounds;

            Assert::IsTrue (longRounds >= 1);
            Assert::AreEqual (longRounds * (size_t) 1284 + ones * 5 - 2, waited);
            Assert::IsTrue ((waited > cycles ? waited - cycles : cycles - waited) <= kTolerance, std::format (L"{} cycles for {}", waited, cycles).c_str());
        }
    }


    //  The arcs at the lower position are written first, then the arm steps
    //  up a half track and the others are written, each as far around the
    //  turn from the one before as its angle is.
    TEST_METHOD (Build_WritesEachPositionsArcs_AtTheirAngles)
    {
        static constexpr size_t  kTolerance = 2;

        RawWriteArcPlan      plan = MakePlan();
        RawWritePass         pass;
        std::vector<size_t>  stretches;



        Assert::IsTrue (pass.Build (plan, kTurn));

        Assert::AreEqual (kLow, pass.position);
        Assert::AreEqual (plan.arcs.size(), pass.arcs.size());
        Assert::AreEqual (kLow,  pass.arcs[0].position);
        Assert::AreEqual (kLow,  pass.arcs[1].position);
        Assert::AreEqual (kHigh, pass.arcs[2].position);
        Assert::AreEqual (kHigh, pass.arcs[3].position);

        for (size_t i = 1; i < pass.arcs.size(); i++)
        {
            size_t  angle  = (plan.arcs[pass.arcs[i].index].start + kTurn - plan.arcs[pass.arcs[0].index].start) % kTurn;
            size_t  around = pass.arcs[i].cell % kTurn;
            size_t  apart  = (around + kTurn - angle) % kTurn;

            Assert::IsTrue (pass.arcs[i].cell > pass.arcs[i - 1].cell + pass.arcs[i - 1].cells, L"An arc is written after the one before ends");
            Assert::IsTrue (std::min (apart, kTurn - apart) <= kTolerance, std::format (L"Arc {} is {} cells around, for {}", i, around, angle).c_str());
        }

        stretches = FindStretches (pass);
        Assert::AreEqual (plan.arcs.size() - 1, stretches.size());
    }


    //  The stretch between the two positions turns the next phase on, the
    //  last one off, counts the arm a half track up, turns the phase off
    //  once the arm has settled, and then writes; the others only write.
    TEST_METHOD (Build_StepsTheArm_BetweenPositions)
    {
        RawWriteArcPlan      plan      = MakePlan();
        RawWritePass         pass;
        std::vector<size_t>  stretches;
        std::vector<Byte>    steps     = { 3, 0, RawWritePass::kArmUp, 2, RawWritePass::kWrite };
        std::vector<Byte>    followed;



        Assert::IsTrue (pass.Build (plan, kTurn));

        stretches = FindStretches (pass);
        Assert::AreEqual ((size_t) 3, stretches.size());

        Assert::AreEqual (RawWritePass::kWrite, pass.stream[stretches[0] + 3]);
        Assert::AreEqual (RawWritePass::kWrite, pass.stream[stretches[2] + 3]);

        for (size_t part = 0; part < steps.size(); part++)
        {
            followed.push_back (pass.stream[stretches[1] + 3 * (part + 1)]);
        }

        Assert::IsTrue (followed == steps);
    }


    //  A stretch's bytes are all in one page of the stream, short of its
    //  last byte, wherever the arcs' lengths put them.
    TEST_METHOD (Build_KeepsAStretchsBytes_InOnePage)
    {
        static constexpr size_t  kLengths = 300;

        for (size_t extra = 0; extra < kLengths; extra++)
        {
            RawWriteArcPlan  plan = MakePlan();
            RawWritePass     pass;

            for (RawWriteArcPlan::Arc & arc : plan.arcs)
            {
                arc.stream.values.resize (kArcBytes - extra, 0x96);
                arc.stream.zeros.resize  (kArcBytes - extra, 0);
                arc.cells = arc.stream.GetCellCount();
            }

            Assert::IsTrue (pass.Build (plan, kTurn));

            for (size_t at : FindStretches (pass))
            {
                size_t  end = at;

                while (pass.stream[end + 3] != RawWritePass::kWrite)
                {
                    end += 3;
                }

                end += 3;

                Assert::AreEqual (at / kPage, end / kPage, std::format (L"A stretch at {} with arcs {} bytes shorter", at, extra).c_str());
                Assert::IsTrue (end % kPage != kPage - 1);
            }
        }
    }


    //  Two arcs at one position with too little of the turn between them
    //  for the drive to stop writing and start again can't be written.
    TEST_METHOD (Build_Fails_ForArcsTooNearAtOnePosition)
    {
        RawWriteArcPlan  plan;
        RawWritePass     pass;



        plan.turn = kTurn;
        plan.arcs = { MakeArc (kLow, 0, 1), MakeArc (kLow, 13, 2) };

        Assert::IsFalse (pass.Build (plan, kTurn));
    }
};
