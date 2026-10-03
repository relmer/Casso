#include "Pch.h"

#include "HResultAssert.h"
#include "Core/StateWriter.h"
#include "Debugger/Reverse/UndoRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kSharingSpacing   = 1000;
static constexpr uint64_t  s_kSharingInterval  = 10000;
static constexpr size_t    s_kSharingBudget    = 2 * 1024 * 1024;
static constexpr size_t    s_kSharingSegments  = 64;
static constexpr size_t    s_kSharingSegment   = 4096;
static constexpr size_t    s_kSharingOwn       = 64;
static constexpr size_t    s_kSharingLight     = 400;
static constexpr size_t    s_kSharingHeavy     = 40;





////////////////////////////////////////////////////////////////////////////////
//
//  UndoRingSharingTests
//
//  The ring counts what each checkpoint holds that the one before it does
//  not. Checkpoints that share nearly all their segments let the limit grow
//  well past what the first, whole one sized it for, within the budget;
//  checkpoints that share nothing bring the count back down to the budget.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UndoRingSharingTests)
{
public:

    TEST_METHOD (SharedSegmentsBuyMoreCheckpointsWithinTheBudget)
    {
        UndoRing                                               ring;
        UndoRingSettings                                       settings;
        std::vector<std::shared_ptr<const std::vector<Byte>>>  current;
        size_t                                                 firstLimit = 0;
        size_t                                                 wholeCount = 0;
        size_t                                                 minimum    = 0;
        size_t                                                 i          = 0;
        uint64_t                                               step       = 0;



        settings.checkpointCycles = s_kSharingSpacing;
        settings.budgetBytes      = s_kSharingBudget;

        AssertSucceeded (ring.Configure (settings, s_kSharingInterval), L"Configure");

        for (i = 0; i < s_kSharingSegments; i++)
        {
            current.push_back (MakeSegment (i));
        }

        Add (ring, step++, current);

        firstLimit = ring.GetCheckpointLimit();

        // Each later checkpoint replaces one segment and shares the rest.
        for (i = 0; i < s_kSharingLight; i++)
        {
            current[i % s_kSharingSegments] = MakeSegment (s_kSharingSegments + i);
            Add (ring, step++, current);
        }

        Logger::WriteMessage (std::format ("first limit {}, now {} checkpoints in {} bytes\n", firstLimit, ring.GetCheckpointCount(), ring.GetByteCount()).c_str());

        Assert::IsTrue (ring.GetCheckpointCount() > firstLimit * 3, L"the limit grew past what the whole first checkpoint sized it for");
        Assert::IsTrue (ring.GetByteCount() <= s_kSharingBudget,    L"and the ring is still within its budget");

        // Then every checkpoint is new from end to end.
        for (i = 0; i < s_kSharingHeavy; i++)
        {
            for (std::shared_ptr<const std::vector<Byte>> & segment : current)
            {
                segment = MakeSegment (step);
            }

            Add (ring, step++, current);
        }

        wholeCount = s_kSharingBudget / (s_kSharingSegments * s_kSharingSegment);
        minimum    = static_cast<size_t> (s_kSharingInterval / s_kSharingSpacing) + 1;

        Assert::IsTrue (ring.GetCheckpointCount() <= std::max (wholeCount, minimum), L"checkpoints sharing nothing are dropped back to the budget");
        Assert::IsTrue (ring.GetCheckpointCount() >= minimum,                        L"but never below one keyframe interval");
    }

private:

    static std::shared_ptr<const std::vector<Byte>> MakeSegment (size_t seed)
    {
        return std::make_shared<const std::vector<Byte>> (s_kSharingSegment, static_cast<Byte> (seed));
    }


    static void Add (UndoRing & ring, uint64_t step, const std::vector<std::shared_ptr<const std::vector<Byte>>> & shared)
    {
        std::vector<StateSegment>  segments = ring.TakeSpareSegments();
        std::vector<Byte>          own      = ring.TakeSpareBuffer();
        StateSegment               segment;
        HRESULT                    hr       = S_OK;



        // From the ring's pools, as a recorder takes them.
        own.assign (s_kSharingOwn, 0);

        for (const std::shared_ptr<const std::vector<Byte>> & bytes : shared)
        {
            segment.offset = 0;
            segment.bytes  = bytes;

            segments.push_back (segment);
        }

        hr = ring.AddCheckpoint (step, step * s_kSharingSpacing, static_cast<size_t> (step), std::move (own), std::move (segments));
        AssertSucceeded (hr, L"AddCheckpoint");
    }
};
