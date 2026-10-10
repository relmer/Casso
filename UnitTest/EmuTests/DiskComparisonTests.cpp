#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparer.h"
#include "ComparisonTestImages.h"
#include "FileMapTestImages.h"
#include "InspectorTrackBuilder.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Pairs = ComparisonTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskComparisonTests
//
//  Comparing made-up pairs with one planted difference each (SC-021): the
//  changed track gets the planted verdict and every other whole track is
//  identical, the planted difference is listed once and nothing else is,
//  Nothing recorded and Standard layout are never counted, and the options
//  change only what is listed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskComparisonTests)
{
public:

    static constexpr int  kChanged = 9;

    static DiskComparison ComparePlanted (const Pairs::TrackSpec & changed, bool isFlux = false)
    {
        DiskAnalysis  a;
        DiskAnalysis  b;



        Pairs::AnalyzeDisk (-1, {}, a, isFlux);
        Pairs::AnalyzeDisk (kChanged, changed, b, isFlux);

        return DiskComparer::Compare (a, b);
    }



    //  Every whole track but the changed one identical, every quarter track
    //  set aside, and the changed track's verdict.
    static void AssertOnlyChanged (const DiskComparison & c, TrackVerdict verdict)
    {
        for (const TrackComparison & t : c.tracks)
        {
            if (t.quarterTrack == kChanged * 4)
            {
                Assert::AreEqual (static_cast<int> (verdict), static_cast<int> (t.verdict), L"the planted verdict");
            }
            else if (t.quarterTrack % 4 == 0 && t.quarterTrack < 35 * 4)
            {
                Assert::AreEqual (static_cast<int> (TrackVerdict::Identical), static_cast<int> (t.verdict), std::to_wstring (t.quarterTrack).c_str());
            }
            else
            {
                Assert::IsTrue (t.verdict == TrackVerdict::StandardLayout || t.verdict == TrackVerdict::NothingRecorded || t.verdict == verdict,
                                std::to_wstring (t.quarterTrack).c_str());
            }
        }
    }



    static int CountKind (const vector<Difference> & listed, DifferenceKind kind)
    {
        return static_cast<int> (std::count_if (listed.begin(), listed.end(), [&] (const Difference & d) { return d.kind == kind; }));
    }



    TEST_METHOD (TwoCopiesAreIdenticalWithNothingListed)
    {
        DiskComparison  c           = ComparePlanted ({});
        int             identical   = 0;
        int             differ      = 0;
        int             onlyA       = 0;
        int             onlyB       = 0;
        int             notCompared = 0;



        AssertOnlyChanged (c, TrackVerdict::Identical);
        Assert::IsTrue (c.differences.empty());

        c.Count (identical, differ, onlyA, onlyB, notCompared);
        Assert::AreEqual (35, identical, L"each whole track counted once; standard layout and nothing recorded left out");
        Assert::AreEqual (0, differ + onlyA + onlyB + notCompared);
    }



    TEST_METHOD (OneChangedByteIsOneSectorDifference)
    {
        Pairs::TrackSpec  spec;
        DiskComparison    c;



        spec.changedSector = 5;
        c = ComparePlanted (spec);

        AssertOnlyChanged (c, TrackVerdict::SectorsDiffer);
        Assert::AreEqual (static_cast<size_t> (1), c.differences.size());
        Assert::AreEqual (static_cast<int> (DifferenceKind::SectorBytes), static_cast<int> (c.differences[0].kind));
        Assert::AreEqual (1, c.differences[0].count);
        Assert::AreEqual (0x40, c.differences[0].firstOffset);
    }



    TEST_METHOD (ARotatedTrackHasTheSameCells)
    {
        Pairs::TrackSpec  spec;
        DiskComparison    c;



        spec.rotation = 1000;
        c = ComparePlanted (spec);

        AssertOnlyChanged (c, TrackVerdict::SameCells);
        Assert::AreNotEqual (0, c.tracks[kChanged * 4].rotationCells);
        Assert::IsTrue (c.differences.empty(), L"a rotation is no difference");
    }



    TEST_METHOD (ExtraSyncIsTheSameNibblesWithOnlySyncListed)
    {
        Pairs::TrackSpec    spec;
        DiskComparison      c;
        ComparisonOptions   ignoreSync;



        spec.extraSync = 12;
        c = ComparePlanted (spec);

        AssertOnlyChanged (c, TrackVerdict::SameNibbles);
        Assert::AreEqual (static_cast<size_t> (1), c.differences.size(), L"only the longer sync run");
        Assert::IsTrue   (c.differences[0].isSyncOnly);

        ignoreSync.isIgnoringSync = true;
        Assert::IsTrue (c.GetListed (ignoreSync).empty(), L"ignoring sync leaves it out");
        Assert::AreEqual (static_cast<int> (TrackVerdict::SameNibbles), static_cast<int> (c.tracks[kChanged * 4].verdict), L"and never changes the verdict");
    }



    TEST_METHOD (ATrackInOneImageOnly)
    {
        Pairs::TrackSpec  spec;
        DiskComparison    c;



        spec.isPresent = false;
        c = ComparePlanted (spec);

        Assert::AreEqual (static_cast<int> (TrackVerdict::OnlyInA), static_cast<int> (c.tracks[kChanged * 4].verdict));
        Assert::AreEqual (1, CountKind (c.differences, DifferenceKind::TrackOnlyInA));
    }



    TEST_METHOD (FluxTimingAndBitAgainstFlux)
    {
        Pairs::TrackSpec  retimed;
        DiskComparison    c;
        DiskAnalysis      bits;
        DiskAnalysis      flux;



        retimed.isFlux     = true;
        retimed.retimeFrom = 100;
        c = ComparePlanted (retimed, true);

        AssertOnlyChanged (c, TrackVerdict::SameCells);
        Assert::IsTrue (c.tracks[kChanged * 4].isTimingDiffer);
        Assert::AreEqual (1, CountKind (c.differences, DifferenceKind::Timing), L"one run of retimed cells");

        Pairs::AnalyzeDisk (-1, {}, bits, false);
        Pairs::AnalyzeDisk (-1, {}, flux, true);
        c = DiskComparer::Compare (bits, flux);

        Assert::AreEqual (static_cast<int> (TrackVerdict::SameCells), static_cast<int> (c.tracks[0].verdict), L"a bit track against a flux track is at most the same cells");
        Assert::IsTrue (c.tracks[0].isFluxOnOneSide);
    }



    TEST_METHOD (VolumeNumbersAreTheirOwnDifference)
    {
        Pairs::TrackSpec    spec;
        DiskComparison      c;
        ComparisonOptions   ignore;



        spec.volume = 100;
        c = ComparePlanted (spec);

        AssertOnlyChanged (c, TrackVerdict::SameSectorData);
        Assert::AreEqual (static_cast<size_t> (1), c.differences.size());
        Assert::AreEqual (static_cast<int> (DifferenceKind::Volume), static_cast<int> (c.differences[0].kind));

        ignore.isIgnoringVolumes = true;
        Assert::IsTrue (c.GetListed (ignore).empty());
    }



    TEST_METHOD (ThirteenAgainstSixteenSectorsDiffersEverywhere)
    {
        Pairs::TrackSpec  spec;
        DiskComparison    c;



        spec.isThirteen = true;
        c = ComparePlanted (spec);

        Assert::AreEqual (static_cast<int> (TrackVerdict::SectorsDiffer), static_cast<int> (c.tracks[kChanged * 4].verdict));
        Assert::AreEqual (16 + 13, c.tracks[kChanged * 4].sectorsDiffer, L"no sector pairs");
    }



    TEST_METHOD (ADamagedRecordIsNotComparedAndNeverCounted)
    {
        DiskAnalysis    a;
        DiskAnalysis    b;
        DiskComparison  c;
        int             identical   = 0;
        int             differ      = 0;
        int             onlyA       = 0;
        int             onlyB       = 0;
        int             notCompared = 0;



        Pairs::AnalyzeDisk (-1, {}, a);
        Pairs::AnalyzeDisk (-1, {}, b);
        b.entries[kChanged * 4].content = QuarterTrackContent::Damaged;
        c = DiskComparer::Compare (a, b);

        Assert::AreEqual (static_cast<int> (TrackVerdict::NotCompared), static_cast<int> (c.tracks[kChanged * 4].verdict));
        c.Count (identical, differ, onlyA, onlyB, notCompared);
        Assert::AreEqual (34, identical, L"the damaged track is never counted as matching");
        Assert::AreEqual (1, notCompared);
    }



    TEST_METHOD (ChangedAndMissingFilesAreListed)
    {
        for (int system = 0; system < 3; system++)
        {
            vector<Byte>  imageA = (system == 0) ? FileMapTestImages::MakeDos33() : (system == 1) ? FileMapTestImages::MakePascal() : FileMapTestImages::MakeCpm();
            vector<Byte>  imageB = imageA;
            bool          isPo   = system == 1;
            DiskAnalysis  a;
            DiskAnalysis  b;

            //  One byte of a file's data changed: HELLO's first data sector,
            //  SYSTEM.PASCAL's first block, or PIP.COM's first block.
            if (system == 0)
            {
                FileMapTestImages::GetDosSector (imageB, 18, 14)[3] ^= 0x01;
            }
            else if (system == 1)
            {
                FileMapTestImages::GetBlock (imageB, 6)[3] ^= 0x01;
            }
            else
            {
                FileMapTestImages::GetCpmSector (imageB, 3, 8)[3] ^= 0x01;
            }

            FileMapTestImages::Analyze (imageA, isPo, a);
            FileMapTestImages::Analyze (imageB, isPo, b);

            DiskComparison  c = DiskComparer::Compare (a, b);

            Assert::AreEqual (1, CountKind (c.differences, DifferenceKind::FileContents), std::to_wstring (system).c_str());
            Assert::AreEqual (1, CountKind (c.differences, DifferenceKind::SectorBytes));
            Assert::AreEqual (static_cast<size_t> (2), c.differences.size(), L"the sector and the file, nothing else");

            //  The changed file's pair marks exactly the one cell that differs.
            auto  changed = std::find_if (c.files.begin(), c.files.end(), [] (const FilePair & p) { return p.outcome == DifferenceKind::FileContents && !p.isSame; });

            Assert::IsTrue (changed != c.files.end());
            Assert::AreEqual (static_cast<size_t> (1), changed->differingCells.size(), L"one differing cell");
        }
    }



    TEST_METHOD (AFileOnlyInOneImage)
    {
        vector<Byte>    imageA = FileMapTestImages::MakeDos33();
        vector<Byte>    imageB = imageA;
        DiskAnalysis    a;
        DiskAnalysis    b;
        DiskComparison  c;



        //  B's catalog has SPARSE's entry cleared.
        FileMapTestImages::GetDosSector (imageA, 17, 15)[0x0B + 2 * 0x23] = 0;
        FileMapTestImages::Analyze (imageA, false, a);
        FileMapTestImages::Analyze (imageB, false, b);
        c = DiskComparer::Compare (a, b);

        Assert::AreEqual (1, CountKind (c.differences, DifferenceKind::FileOnlyInB));
    }



    TEST_METHOD (TwoDisksCompareWellWithinASecond)
    {
        DiskAnalysis      a;
        DiskAnalysis      b;
        Pairs::TrackSpec  spec;



        spec.changedSector = 3;
        Pairs::AnalyzeDisk (-1, {}, a);
        Pairs::AnalyzeDisk (kChanged, spec, b);

        auto  start = std::chrono::steady_clock::now();

        (void) DiskComparer::Compare (a, b);

        Assert::IsTrue (std::chrono::steady_clock::now() - start < std::chrono::seconds (1));
    }



    TEST_METHOD (ADskAndAWozOfOneDiskHoldTheSameSectorData)
    {
        vector<Byte>    dsk (FileMapTestImages::kImageBytes, 0);
        DiskImage       image;
        DiskAnalysis    a;
        DiskAnalysis    b;
        DiskComparison  c;
        HRESULT         hr  = S_OK;



        //  The DSK holds, at each track's DOS order position, what the WOZ's
        //  standard track holds in the physical sector there.
        for (int t = 0; t < 35; t++)
        {
            for (int p = 0; p < 16; p++)
            {
                std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes = {};

                InspectorTrackBuilder::FillPattern (p, bytes);
                memcpy (FileMapTestImages::GetDosSector (dsk, t, NibblizationLayer::GetDosFileIndexForPhysicalSector (p)), bytes.data(), bytes.size());
            }
        }

        hr = NibblizationLayer::NibblizeDsk (dsk, image);
        Assert::IsTrue (SUCCEEDED (hr));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "disk.dsk", dsk.size(), false), DecodeSettings::MakeStandard(), a);
        Pairs::AnalyzeDisk (-1, {}, b);
        c = DiskComparer::Compare (a, b);

        for (int t = 0; t < 35; t++)
        {
            TrackVerdict  verdict = c.tracks[t * 4].verdict;

            Assert::IsTrue (verdict == TrackVerdict::SameSectorData || verdict == TrackVerdict::SameNibbles || verdict == TrackVerdict::Identical,
                            std::to_wstring (t).c_str());
        }

        Assert::IsTrue (std::none_of (c.differences.begin(), c.differences.end(), [] (const Difference & d) { return !d.isSyncOnly; }),
                        L"nothing but sync widths differs");
    }



    TEST_METHOD (OneHundredSixtyFluxTracksCompareWithinFiveSeconds)
    {
        vector<WozSyntheticTrack>  tracksA;
        vector<WozSyntheticTrack>  tracksB;
        DiskAnalysis               a;
        DiskAnalysis               b;
        Pairs::TrackSpec           plain;
        Pairs::TrackSpec           retimed;



        plain.isFlux       = true;
        retimed.isFlux     = true;
        retimed.retimeFrom = 0;

        for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            tracksA.push_back (Pairs::MakeTrack (qt / 4, plain));
            tracksB.push_back (Pairs::MakeTrack (qt / 4, retimed));
            tracksA.back().quarterTracks = { qt };
            tracksB.back().quarterTracks = { qt };
        }

        Pairs::Analyze (tracksA, a);
        Pairs::Analyze (tracksB, b);

        auto  start = std::chrono::steady_clock::now();

        DiskComparison  c = DiskComparer::Compare (a, b);

        Assert::IsTrue (std::chrono::steady_clock::now() - start < std::chrono::seconds (5));
        Assert::IsTrue (c.tracks[37].isTimingDiffer, L"every quarter track compared on its own");
    }



    TEST_METHOD (DiffFindsInsertedAndDeletedNibbles)
    {
        const vector<Byte>          a = { 1, 2, 3, 4, 5, 6, 7, 8 };
        const vector<Byte>          b = { 1, 2, 9, 9, 3, 4, 5, 7, 8 };
        vector<DiskComparer::Hunk>  hunks;



        Assert::IsTrue   (DiskComparer::Diff (a, b, hunks));
        Assert::AreEqual (static_cast<size_t> (2), hunks.size(), L"two inserted, one deleted");
        Assert::AreEqual (2, hunks[0].firstA);
        Assert::AreEqual (0, hunks[0].countA);
        Assert::AreEqual (2, hunks[0].countB);
        Assert::AreEqual (5, hunks[1].firstA);
        Assert::AreEqual (1, hunks[1].countA);
    }
};
