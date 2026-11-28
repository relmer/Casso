#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/Session/RawWriteArcs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcsTests
//
//  The arcs of tracks less than a full track apart, as Karateka has at
//  tracks 2 to 3.25: found by their bytes, at whatever angle each position's
//  stream starts.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RawWriteArcsTests)
{
public:

    using Bits = std::vector<uint8_t>;

    static constexpr size_t  kArcBytes   = 1000;
    static constexpr size_t  kSyncs      = 16;
    static constexpr size_t  kSyncCells  = 10;
    static constexpr size_t  kArcCells   = kSyncs * kSyncCells + (kArcBytes - kSyncs) * 8;
    static constexpr size_t  kLongBlank  = 120 + 128 * 42;
    static constexpr size_t  kShortBlank = 120 + 128 * 6;
    static constexpr size_t  kTolerance  = 200;

    //  The seeds of the four arcs, in the order they are in around the turn.
    static constexpr uint32_t  kFirstLow  = 11;
    static constexpr uint32_t  kFirstWide = 22;
    static constexpr uint32_t  kNextLow   = 33;
    static constexpr uint32_t  kNextWide  = 44;


    static void AppendByte (Byte value, size_t zeros, Bits & ioBits)
    {
        for (int bit = 7; bit >= 0; bit--)
        {
            ioBits.push_back ((uint8_t) ((value >> bit) & 1));
        }

        ioBits.insert (ioBits.end(), zeros, 0);
    }


    //  An arc's bytes after its sync bytes, which differ from seed to seed.
    static std::vector<Byte> MakeData (uint32_t seed)
    {
        std::vector<Byte>  data;



        for (size_t i = kSyncs; i < kArcBytes; i++)
        {
            seed = seed * 1103515245 + 12345;
            data.push_back ((Byte) (0x96 | (seed >> 16)));
        }

        return data;
    }


    //  The bytes of an arc from `from` on, after sync bytes when it is the
    //  whole arc.
    static void AppendArc (uint32_t seed, size_t from, Bits & ioBits)
    {
        std::vector<Byte>  data = MakeData (seed);



        for (size_t i = 0; from == 0 && i < kSyncs; i++)
        {
            AppendByte (0xFF, kSyncCells - 8, ioBits);
        }

        for (size_t i = from; i < data.size(); i++)
        {
            AppendByte (data[i], 0, ioBits);
        }
    }


    static void AppendBlank (size_t cells, Bits & ioBits)
    {
        ioBits.insert (ioBits.end(), cells, 0);
    }


    //  A track with some of the four arcs, each where it is in the turn,
    //  with the stream's bits starting at the arc or blank `first`.
    static RawWriteStream MakeTrack (bool firstLow, bool firstWide, bool nextLow, bool nextWide, size_t first)
    {
        static constexpr size_t  kSlots = 4;

        Bits            bits;
        RawWriteStream  stream;
        bool            has[kSlots]    = { firstLow, firstWide, nextLow, nextWide };
        uint32_t        seeds[kSlots]  = { kFirstLow, kFirstWide, kNextLow, kNextWide };
        size_t          blanks[kSlots] = { kLongBlank, kLongBlank, kLongBlank, kShortBlank };



        for (size_t i = 0; i < kSlots; i++)
        {
            size_t  slot = (first + i) % kSlots;

            if (has[slot])
            {
                AppendArc (seeds[slot], 0, bits);
            }
            else
            {
                AppendBlank (kArcCells, bits);
            }

            AppendBlank (blanks[slot], bits);
        }

        RawWriteStream::FromBits (bits, stream);

        return stream;
    }


    //  The track with all four arcs, as Karateka's track 2 has, and the
    //  track above with the two wide arcs alone, as its track 3.
    static RawWriteStream MakeLowTrack()  { return MakeTrack (true, true, true, true, 0); }
    static RawWriteStream MakeHighTrack() { return MakeTrack (false, true, false, true, 3); }


    //  The track between, as track 2.5: the two wide arcs and the last half
    //  of one of the others, from another angle.
    static RawWriteStream MakeMiddleTrack()
    {
        Bits            bits;
        RawWriteStream  stream;



        AppendArc   (kFirstWide, 0, bits);
        AppendBlank (2 * kLongBlank + kArcCells, bits);
        AppendArc   (kNextWide, 0, bits);
        AppendBlank (kShortBlank + kArcCells / 2, bits);
        AppendArc   (kFirstLow, (kArcBytes - kSyncs) / 2, bits);
        AppendBlank (kLongBlank, bits);

        RawWriteStream::FromBits (bits, stream);

        return stream;
    }


    //  A track with data all the way around.
    static RawWriteStream MakeFullTrack (uint32_t seed)
    {
        static constexpr size_t  kBytes = 6000;

        Bits            bits;
        RawWriteStream  stream;



        for (size_t i = 0; i < kBytes; i++)
        {
            seed = seed * 1103515245 + 12345;
            AppendByte ((Byte) (0x96 | (seed >> 16)), i % 400 < kSyncs ? 2 : 0, bits);
        }

        RawWriteStream::FromBits (bits, stream);

        return stream;
    }


    //  The arc of a group with a seed's bytes, or null.
    static const RawWriteArc * FindArc (const RawWriteArcGroup & group, uint32_t seed)
    {
        static constexpr ptrdiff_t  kBytes = 64;

        std::vector<Byte>  data = MakeData (seed);



        for (const RawWriteArc & arc : group.arcs)
        {
            if (std::search (arc.stream.values.begin(), arc.stream.values.end(), data.begin(), data.begin() + kBytes) != arc.stream.values.end())
            {
                return &arc;
            }
        }

        return nullptr;
    }


    static void AssertNear (size_t expected, size_t actual, const wchar_t * what)
    {
        size_t  apart = expected > actual ? expected - actual : actual - expected;



        Assert::IsTrue (apart <= kTolerance, std::format (L"{}: {} for {}", what, actual, expected).c_str());
    }


    //  Two arcs at tracks 2 and 2.25 only and two from 2 to 3.25, with a part
    //  of one of the first two at 2.5: four arcs, each with its positions,
    //  its length and its place in the turn.
    TEST_METHOD (Find_ListsEachArcOnce_WithThePositionsItIsFoundAt)
    {
        RawWriteStream                     low    = MakeLowTrack();
        RawWriteStream                     middle = MakeMiddleTrack();
        RawWriteStream                     high   = MakeHighTrack();
        RawWriteStream                     below  = MakeFullTrack (5);
        RawWriteStream                     above  = MakeFullTrack (6);
        std::vector<RawWriteArcs::Source>  sources;
        std::vector<RawWriteArcGroup>      groups;
        std::vector<int>                   all    = { 8, 9, 10, 11, 12, 13 };
        std::vector<int>                   lowAt  = { 8, 9 };
        std::vector<int>                   part   = { 10 };

        const RawWriteArc                * firstLow  = nullptr;
        const RawWriteArc                * firstWide = nullptr;
        const RawWriteArc                * nextLow   = nullptr;
        const RawWriteArc                * nextWide  = nullptr;
        size_t                             apart     = kArcCells + kLongBlank;
        size_t                             turn      = 0;



        sources = { { 4, &below }, { 8, &low }, { 9, &low }, { 10, &middle }, { 11, &high }, { 12, &high }, { 13, &high }, { 16, &above } };

        RawWriteArcs::Find (sources, groups);

        Assert::AreEqual ((size_t) 1, groups.size());
        Assert::AreEqual ((size_t) 4, groups[0].arcs.size());
        Assert::IsTrue (groups[0].positions == all);

        firstLow  = FindArc (groups[0], kFirstLow);
        firstWide = FindArc (groups[0], kFirstWide);
        nextLow   = FindArc (groups[0], kNextLow);
        nextWide  = FindArc (groups[0], kNextWide);
        turn      = groups[0].turn;

        Assert::IsTrue (firstLow != nullptr && firstWide != nullptr && nextLow != nullptr && nextWide != nullptr);

        Assert::IsTrue (firstLow->positions  == lowAt && firstLow->partPositions == part);
        Assert::IsTrue (nextLow->positions   == lowAt && nextLow->partPositions.empty());
        Assert::IsTrue (firstWide->positions == all   && firstWide->partPositions.empty());
        Assert::IsTrue (nextWide->positions  == all   && nextWide->partPositions.empty());

        AssertNear (4 * kArcCells + 3 * kLongBlank + kShortBlank, turn, L"The turn");

        for (const RawWriteArc * arc : { firstLow, firstWide, nextLow, nextWide })
        {
            AssertNear (kArcCells, arc->cells, L"An arc's cells");
        }

        AssertNear (apart,     (firstWide->start + turn - firstLow->start) % turn, L"The second arc's place");
        AssertNear (2 * apart, (nextLow->start   + turn - firstLow->start) % turn, L"The third arc's place");
        AssertNear (3 * apart, (nextWide->start  + turn - firstLow->start) % turn, L"The fourth arc's place");
    }


    //  An arc the track with the most arcs doesn't have is placed from a
    //  track that has it and an arc already placed.
    TEST_METHOD (Find_PlacesAnArc_FromATrackItSharesWithOneAlreadyPlaced)
    {
        RawWriteStream                     two    = MakeTrack (true, true, false, false, 1);
        RawWriteStream                     three  = MakeTrack (false, true, true, true, 2);
        std::vector<RawWriteArcs::Source>  sources;
        std::vector<RawWriteArcGroup>      groups;

        const RawWriteArc                * firstLow  = nullptr;
        const RawWriteArc                * firstWide = nullptr;
        size_t                             turn      = 0;



        sources = { { 8, &two }, { 10, &three } };

        RawWriteArcs::Find (sources, groups);

        Assert::AreEqual ((size_t) 1, groups.size());
        Assert::AreEqual ((size_t) 4, groups[0].arcs.size());

        firstLow  = FindArc (groups[0], kFirstLow);
        firstWide = FindArc (groups[0], kFirstWide);
        turn      = groups[0].turn;

        Assert::IsTrue (firstLow != nullptr && firstWide != nullptr);
        AssertNear (kArcCells + kLongBlank, (firstWide->start + turn - firstLow->start) % turn, L"The arc's place");
    }


    //  Tracks with data all the way around have no arcs, at a full track
    //  apart or closer.
    TEST_METHOD (Find_HasNoGroups_ForTracksWithDataAllTheWayAround)
    {
        RawWriteStream                     first  = MakeFullTrack (1);
        RawWriteStream                     second = MakeFullTrack (2);
        RawWriteStream                     third  = MakeFullTrack (3);
        std::vector<RawWriteArcs::Source>  sources;
        std::vector<RawWriteArcGroup>      groups;



        sources = { { 0, &first }, { 1, &first }, { 2, &second }, { 4, &third } };

        RawWriteArcs::Find (sources, groups);

        Assert::IsTrue (groups.empty());
    }


    //  The same arcs a full track apart are two tracks, not a group, and a
    //  track of arcs found nowhere else isn't one either.
    TEST_METHOD (Find_HasNoGroups_ForArcsAFullTrackApart)
    {
        RawWriteStream                     low    = MakeLowTrack();
        RawWriteStream                     high   = MakeHighTrack();
        std::vector<RawWriteArcs::Source>  sources;
        std::vector<RawWriteArcGroup>      groups;



        sources = { { 8, &low }, { 12, &high } };

        RawWriteArcs::Find (sources, groups);

        Assert::IsTrue (groups.empty());
    }


    //  The Karateka layout: the two low arcs are written at track 2 and the
    //  two wide ones at 2.5, with tracks 2, 2.5 and 3 erased first, and no
    //  arc is written over another.
    TEST_METHOD (Plan_WritesEachArcAtTheMiddleOfItsPositions)
    {
        RawWriteStream                     low    = MakeLowTrack();
        RawWriteStream                     middle = MakeMiddleTrack();
        RawWriteStream                     high   = MakeHighTrack();
        RawWriteStream                     below  = MakeFullTrack (5);
        std::vector<RawWriteArcs::Source>  sources;
        std::vector<RawWriteArcGroup>      groups;
        RawWriteArcPlan                    plan;
        std::vector<int>                   erased = { 8, 10, 12 };
        std::vector<Byte>                  lowData;
        bool                               isLow  = false;



        sources = { { 3, &below }, { 4, &below }, { 5, &below }, { 8, &low }, { 9, &low }, { 10, &middle }, { 11, &high }, { 12, &high }, { 13, &high } };

        RawWriteArcs::Find (sources, groups);

        Assert::AreEqual ((size_t) 1, groups.size());
        Assert::AreEqual (0.0, RawWriteArcs::GetLean (sources), 0.001);
        Assert::IsTrue (RawWriteArcs::Plan (groups[0], 0.0, plan));

        Assert::AreEqual ((size_t) 4, plan.arcs.size());
        Assert::IsTrue (plan.erased == erased);
        Assert::IsTrue (plan.lost.empty());

        for (const RawWriteArcPlan::Arc & arc : plan.arcs)
        {
            isLow = false;

            for (uint32_t seed : { kFirstLow, kNextLow })
            {
                lowData = MakeData (seed);
                isLow   = isLow || std::search (arc.stream.values.begin(), arc.stream.values.end(), lowData.begin(), lowData.begin() + 64) != arc.stream.values.end();
            }

            Assert::AreEqual (isLow ? 8 : 10, arc.position);
        }
    }


    //  A drive that reads every track a quarter track low finds the arcs a
    //  quarter track low too, and they are written where they were.
    TEST_METHOD (Plan_TakesOffTheLeanOfTheDriveThatReadTheImage)
    {
        RawWriteStream                     low    = MakeLowTrack();
        RawWriteStream                     middle = MakeMiddleTrack();
        RawWriteStream                     high   = MakeHighTrack();
        RawWriteStream                     below  = MakeFullTrack (5);
        std::vector<RawWriteArcs::Source>  sources;
        std::vector<RawWriteArcGroup>      groups;
        RawWriteArcPlan                    plan;
        double                             lean   = 0.0;



        sources = { { 3, &below }, { 4, &below }, { 7, &low }, { 8, &low }, { 9, &middle }, { 10, &high }, { 11, &high }, { 12, &high } };

        RawWriteArcs::Find (sources, groups);
        lean = RawWriteArcs::GetLean (sources);

        Assert::AreEqual (-0.5, lean, 0.001);
        Assert::AreEqual ((size_t) 1, groups.size());
        Assert::IsTrue (RawWriteArcs::Plan (groups[0], lean, plan));

        Assert::AreEqual (8,  plan.arcs.front().position);
        Assert::AreEqual (10, plan.arcs.back().position);
    }


    //  Two arcs at one angle, a half track apart, can't both be written, and
    //  the plan has their positions.
    TEST_METHOD (Plan_ListsArcsAtOneAngle_AHalfTrackApart)
    {
        RawWriteArcGroup  group;
        RawWriteArcPlan   plan;



        group.turn      = 50000;
        group.positions = { 8, 9, 10, 11 };
        group.arcs.resize (3);

        group.arcs[0].start     = 1000;
        group.arcs[0].cells     = 8000;
        group.arcs[0].positions = { 8, 9 };
        group.arcs[1].start     = 8500;
        group.arcs[1].cells     = 8000;
        group.arcs[1].positions = { 10, 11 };
        group.arcs[2].start     = 30000;
        group.arcs[2].cells     = 8000;
        group.arcs[2].positions = { 10, 11 };

        for (RawWriteArc & arc : group.arcs)
        {
            arc.stream.values = MakeData (kFirstLow);
            arc.stream.zeros.assign (arc.stream.values.size(), 0);
        }

        Assert::IsTrue (RawWriteArcs::Plan (group, 0.0, plan));
        Assert::AreEqual ((size_t) 1, plan.lost.size());
        Assert::IsTrue (plan.lost[0] == std::pair<int, int> (8, 10));
        Assert::AreEqual ((size_t) 2, plan.arcs.size());
    }


    //  Arcs all found at the same positions are one track's, written as a
    //  track.
    TEST_METHOD (Plan_IsNotUsed_ForArcsAtOnePosition)
    {
        RawWriteArcGroup  group;
        RawWriteArcPlan   plan;



        group.turn      = 50000;
        group.positions = { 8, 9 };
        group.arcs.resize (2);

        group.arcs[0].start     = 1000;
        group.arcs[0].cells     = 8000;
        group.arcs[0].positions = { 8, 9 };
        group.arcs[1].start     = 20000;
        group.arcs[1].cells     = 8000;
        group.arcs[1].positions = { 8, 9 };

        for (RawWriteArc & arc : group.arcs)
        {
            arc.stream.values = MakeData (kFirstLow);
            arc.stream.zeros.assign (arc.stream.values.size(), 0);
        }

        Assert::IsFalse (RawWriteArcs::Plan (group, 0.0, plan));
    }


    //  A short blank stretch, as at a track's seam, doesn't make arcs of a
    //  track, and a track of four arcs has four.
    TEST_METHOD (Split_TakesATrackWithLittleBlankAsOneTrack)
    {
        Bits                      bits;
        RawWriteStream            seamed;
        RawWriteStream            low    = MakeLowTrack();
        std::vector<RawWriteArc>  arcs;
        size_t                    turn   = 0;



        for (size_t i = 0; i < 5; i++)
        {
            AppendArc (kFirstLow + (uint32_t) i, 0, bits);
        }

        AppendBlank (kShortBlank, bits);
        RawWriteStream::FromBits (bits, seamed);

        RawWriteArcs::Split (seamed, arcs, turn);
        Assert::IsTrue (arcs.empty());

        RawWriteArcs::Split (low, arcs, turn);
        Assert::AreEqual ((size_t) 4, arcs.size());
    }
};
