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

    //  The bits from one track's sector 0 to the next's, in a turn.
    static long long  GetOffset (const TrackLayout & before, const TrackLayout & after)
    {
        return ((after.sector0 - before.sector0) % after.bits + after.bits) % after.bits;
    }

    //  A WOZ built with sector 0 at a different place on each track, as a
    //  real disk's, measured track by track: each track's nibbles in a turn
    //  come back, and each offset from the track before is within 3 nibbles.
    TEST_METHOD (Measures_OfABuiltWoz_GiveBackItsLayouts)
    {
        static constexpr int        kBits        = 51244;
        static constexpr int        kNibbles     = 6337;
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
                long long  error = GetOffset (measured[(size_t) track - 1], layout) - GetOffset (built[(size_t) track - 1], built[(size_t) track]);

                Assert::IsTrue (std::abs (error) <= kToleranceBits, std::format (L"Track {}'s offset is {} bits off", track, error).c_str());
            }
        }
    }

    //  A turn's bits: the revolution's cycles at 4 a bit.
    TEST_METHOD (BitsPerTurn_AreTheRevolutionsCyclesOver4)
    {
        Assert::AreEqual (51024, TrackMeasures::GetBitsPerTurn (200.0));
        Assert::AreEqual (TrackLayoutWriter::kDefaultBits, TrackMeasures::GetBitsPerTurn (0.0));
    }
};
