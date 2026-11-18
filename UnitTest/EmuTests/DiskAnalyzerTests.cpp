#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzerTests
//
//  The whole disk: an entry per quarter track, each record analyzed once, the
//  summary behind the chips (FR-018), and no findings at all on the standard
//  layouts (SC-013).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskAnalyzerTests)
{
public:

    static void AnalyzeImage (const DiskImage & image, DiskAnalysis & out)
    {
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "test", 0, false), DecodeSettings::MakeStandard(), out);
    }



    static void AnalyzeWoz (Byte volume, bool isFlux, bool isReversed, DiskAnalysis & out)
    {
        vector<Byte>  woz;
        DiskImage     image;



        AssertSucceeded (InspectorTrackBuilder::MakeStandardWoz (volume, isFlux, isReversed, woz));
        AssertSucceeded (WozLoader::Load (woz, image));
        AnalyzeImage (image, out);
    }



    static std::wstring ListFindings (const DiskAnalysis & analysis)
    {
        std::wstring  text;



        for (const Finding & f : analysis.findings)
        {
            text += FindingFormatter::Format (f) + L"\n";
        }

        return text;
    }



    static void AssertStandardSixteenSector (const DiskAnalysis & analysis, int volume)
    {
        Assert::AreEqual (0, static_cast<int> (analysis.findings.size()), ListFindings (analysis).c_str());
        Assert::IsTrue (analysis.summary.format == DiskFormatClass::Sixteen);
        Assert::AreEqual (35,      analysis.summary.tracksWithData);
        Assert::AreEqual (35 * 16, analysis.summary.sectorsGood);
        Assert::AreEqual (35 * 16, analysis.summary.sectorsFound);
        Assert::AreEqual (0,       analysis.summary.badSectors);
        Assert::AreEqual (volume,  analysis.summary.commonVolume);
    }



    TEST_METHOD (AStandardDskHasNoFindings)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage     image;
        DiskAnalysis  analysis;
        size_t        i        = 0;



        for (i = 0; i < sectors.size(); i++)
        {
            sectors[i] = static_cast<Byte> (i * 7);
        }

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        AnalyzeImage (image, analysis);

        AssertStandardSixteenSector (analysis, NibblizationLayer::kDefaultVolume);
    }



    TEST_METHOD (AStandardLayoutWozHasNoFindings)
    {
        DiskAnalysis  bits;
        DiskAnalysis  flux;



        AnalyzeWoz (130, false, false, bits);
        AssertStandardSixteenSector (bits, 130);
        Assert::AreEqual (0, bits.summary.fluxTracks);

        AnalyzeWoz (130, true, false, flux);
        AssertStandardSixteenSector (flux, 130);
        Assert::AreEqual (35, flux.summary.fluxTracks);
    }



    TEST_METHOD (EachRecordIsAnalyzedOnceAndSharedByItsQuarterTracks)
    {
        DiskAnalysis  analysis;



        AnalyzeWoz (254, false, true, analysis);

        AssertStandardSixteenSector (analysis, 254);
        Assert::AreEqual (35, static_cast<int> (std::count_if (analysis.tracks.begin(), analysis.tracks.end(),
                                                               [] (const std::shared_ptr<const TrackAnalysis> & t) { return t != nullptr; })));
        Assert::AreEqual (34, analysis.entries[0].slot, L"reversed: track 0 plays record 34");
        Assert::AreEqual (34, analysis.entries[1].slot);
        Assert::AreEqual (33, analysis.entries[3].slot);
        Assert::IsTrue (analysis.entries[2].content == QuarterTrackContent::Nothing, L"track 0.5 is unmapped");
        Assert::AreEqual (16, analysis.entries[4].sectorsGood);
        Assert::AreEqual (1, static_cast<int> (analysis.entries[0].sharesWith.size()), L"0 and 0.25 play record 34");
        Assert::AreEqual (1, analysis.entries[0].sharesWith[0]);
    }



    TEST_METHOD (AVolumeDifferenceIsFoundOncePerField)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage     image;
        DiskAnalysis  analysis;
        int           count    = 0;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));

        {
            InspectorTrackBuilder  odd;

            odd.AppendStandardTrack (DiskFieldKind::Sixteen, 17, 5, InspectorTrackBuilder::FillPattern);
            odd.PadToCells (NibblizationLayer::kTrackBitCapacity);
            odd.PackBits (image.GetTrackBitsForWrite (5));
            image.SetTrackBitCount (5, odd.GetCellCount());
        }

        AnalyzeImage (image, analysis);

        count = static_cast<int> (std::count_if (analysis.findings.begin(), analysis.findings.end(), [] (const Finding & f) { return f.kind == FindingKind::VolumeDiffers; }));
        Assert::AreEqual (16, count);
        Assert::AreEqual (static_cast<int> (NibblizationLayer::kDefaultVolume), analysis.summary.commonVolume);
    }



    TEST_METHOD (ReanalyzeReplacesOnlyTheGivenRecords)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage                             image;
        DiskAnalysis                          analysis;
        int                                   slot      = 0;
        std::shared_ptr<const TrackAnalysis>  untouched;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        AnalyzeImage (image, analysis);

        slot      = analysis.entries[4 * 3].slot;
        untouched = analysis.tracks[analysis.entries[4 * 4].slot];

        {
            InspectorTrackBuilder  blank;

            blank.AppendZeros (static_cast<int> (NibblizationLayer::kTrackBitCapacity));
            blank.PackBits (image.GetTrackBitsForWrite (3));
            image.SetTrackBitCount (3, blank.GetCellCount());
        }

        DiskAnalyzer::Reanalyze (DiskCopy::MakeFromImage (image, 1, "test", 0, false), std::span<const int> (&slot, 1), analysis);

        Assert::IsTrue (analysis.tracks[slot]->trackClass == TrackClass::Unformatted);
        Assert::IsTrue (analysis.tracks[analysis.entries[4 * 4].slot] == untouched, L"an untouched record's analysis is kept");
        Assert::AreEqual (34 * 16, analysis.summary.sectorsGood);
        Assert::AreEqual (1, analysis.summary.unformattedTracks);
    }



    TEST_METHOD (AnalyzingA140KDiskTakesUnder100Milliseconds)
    {
#ifdef _DEBUG
        Logger::WriteMessage (L"SC-003 is measured in Release only");
#else
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage                        image;
        DiskAnalysis                     analysis;
        LARGE_INTEGER                    frequency = {};
        LARGE_INTEGER                    start     = {};
        LARGE_INTEGER                    end       = {};
        double                           ms        = 0;
        std::shared_ptr<const DiskCopy>  copy;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        copy = DiskCopy::MakeFromImage (image, 1, "test", 0, false);

        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&start);
        DiskAnalyzer::Analyze (copy, DecodeSettings::MakeStandard(), analysis);
        QueryPerformanceCounter   (&end);

        ms = 1000.0 * static_cast<double> (end.QuadPart - start.QuadPart) / static_cast<double> (frequency.QuadPart);
        Logger::WriteMessage (std::format (L"analysis took {:.1f} ms", ms).c_str());
        Assert::IsTrue (ms < 100.0);
#endif
    }
};
