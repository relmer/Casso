#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/Session/TrackMeasures.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/TrackLayoutWriter.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasuresTests
//
//  Measure track run by the program on an emulated //c, and the layouts the
//  host computes from its measures, against a WOZ built to known layouts.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackMeasuresTests)
{
public:

    using Image = std::vector<Byte>;

    //  The share of a turn from one track's sector 0 to the next's.
    static double  GetOffset (const TrackLayout & before, const TrackLayout & after)
    {
        double  share = (double) after.sector0 / after.bits - (double) before.sector0 / before.bits;



        return share < 0.0 ? share + 1.0 : share;
    }

    //  A WOZ built with sector 0 at a different place on each track, as a
    //  real disk's, measured track by track: each track's nibbles in a turn
    //  come back, and each offset from the track before is within 3 nibbles.
    TEST_METHOD (Measures_OfABuiltWoz_GiveBackItsLayouts)
    {
        static constexpr int        kBits          = 51244;
        static constexpr int        kNibbles       = 6337;
        static constexpr long long  kToleranceBits = 3 * 8;
        Image                                     sectors (NibblizationLayer::kImageByteSize);
        std::vector<TrackLayout>                  built;
        std::vector<TrackLayout>                  measured;
        std::vector<CassoLinkHost::TrackMeasure>  measures;
        Image                                     woz;



        for (size_t at = 0; at < sectors.size(); at++)
        {
            sectors[at] = (Byte) (at * 13 + at / 256);
        }

        for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            built.push_back (TrackLayout { 0, kNibbles, kBits, (track * 30763LL + 4111) % kBits });
        }

        AssertSucceeded (TrackLayoutWriter::BuildWoz (sectors, built, woz));

        ProgramRig  rig (Image (NibblizationLayer::kImageByteSize, 0));

        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, woz));

        for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            CassoLinkHost::TrackMeasure  measure;
            CassoLinkStatus              status;

            AssertSucceeded (rig.host->MeasureTrack (1, (Byte) track, measure, status));
            Assert::AreEqual ((Byte) 0, status.error);

            measures.push_back (measure);
        }

        measured = TrackMeasures::ToLayouts (measures, kBits);

        for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            const TrackLayout  & layout = measured[(size_t) track];

            Assert::IsTrue (std::abs (layout.nibbles - kNibbles) <= 2, std::format (L"Track {} has {} nibbles in a turn", track, layout.nibbles).c_str());
            Assert::AreEqual ((Byte) 0, layout.volume);

            if (track > 0)
            {
                long long  error = llround ((GetOffset (measured[(size_t) track - 1], layout) - GetOffset (built[(size_t) track - 1], built[(size_t) track])) * kBits);

                Assert::IsTrue (std::abs (error) <= kToleranceBits, std::format (L"Track {}'s offset is {} bits off", track, error).c_str());
            }
        }
    }

    //  A turn's bits: the revolution's cycles at 4 a bit, and a turn at 300
    //  rpm for a revolution far from it.
    TEST_METHOD (BitsPerTurn_AreTheRevolutionsCyclesOver4)
    {
        Assert::AreEqual (51024, TrackMeasures::GetBitsPerTurn (200.0));
        Assert::AreEqual (52045, TrackMeasures::GetBitsPerTurn (204.0));
        Assert::AreEqual (TrackLayoutWriter::kDefaultBits, TrackMeasures::GetBitsPerTurn (0.0));
        Assert::AreEqual (TrackLayoutWriter::kDefaultBits, TrackMeasures::GetBitsPerTurn (2.0));
        Assert::AreEqual (TrackLayoutWriter::kDefaultBits, TrackMeasures::GetBitsPerTurn (230.0));
    }

    //  A track with no sector 0 and a track whose turn can't fit 16 sectors
    //  have no layout and the volume of the track before, and the tracks
    //  after them are placed from the measures.
    TEST_METHOD (Layouts_OfAnUnmeasuredOrImpossibleTrack_AreTheDefault)
    {
        std::vector<CassoLinkHost::TrackMeasure>  measures = { { 0, 6330, 7 }, { 0, 0, 0 }, { 1000, 4000, 7 }, { 1000, 6330, 9 } };
        std::vector<TrackLayout>                  layouts  = TrackMeasures::ToLayouts (measures, TrackLayoutWriter::kDefaultBits);

        Assert::AreEqual ((size_t) 4, layouts.size());
        Assert::AreEqual (6334, layouts[0].nibbles);
        Assert::AreEqual (0, layouts[1].nibbles);
        Assert::AreEqual ((Byte) 7, layouts[1].volume);
        Assert::AreEqual (0, layouts[2].nibbles);
        Assert::AreEqual ((Byte) 7, layouts[2].volume);
        Assert::AreEqual (6334, layouts[3].nibbles);
        Assert::AreEqual ((Byte) 9, layouts[3].volume);
        Assert::IsTrue (layouts[3].sector0 > 0, L"Placed from the measures");
    }

    //  A drive 2% slow has more of its bits in a turn than the disk has: each
    //  track fits 16 sectors, its bits follow its nibbles, and the step covers
    //  less of the turn.
    TEST_METHOD (Layouts_FromASlowDrive_HaveTheDisksLengths)
    {
        std::vector<CassoLinkHost::TrackMeasure>  measures = { { 0, 6337, 0 }, { 3000, 6337, 0 } };
        std::vector<TrackLayout>                  normal   = TrackMeasures::ToLayouts (measures, TrackLayoutWriter::kDefaultBits);
        std::vector<TrackLayout>                  slow     = TrackMeasures::ToLayouts (measures, TrackLayoutWriter::kDefaultBits * 102 / 100);

        Assert::AreEqual (6341, slow[1].nibbles, L"The track fits 16 sectors");
        Assert::AreEqual (normal[1].bits, slow[1].bits, L"The track's length");
        Assert::IsTrue (slow[1].sector0 < normal[1].sector0, L"The step covers less of a slow turn");
    }
};
