#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTextTests
//
//  The words the views show, against the spec's quoted examples: the
//  summary chips and when they hide (FR-018), the track header (FR-032), the
//  sector row's tooltips and empty texts (FR-039), and the Sector data header
//  and its empty states (FR-040).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorTextTests)
{
public:

    static void AnalyzeDsk (DiskAnalysis & out)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage     image;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 7, "test", 0, false), DecodeSettings::MakeStandard(), out);
    }



    TEST_METHOD (ChipsShowTheSummaryAndHideZeroCounts)
    {
        DiskSummary          summary;
        vector<SummaryChip>  chips;



        summary.format         = DiskFormatClass::Sixteen;
        summary.tracksWithData = 35;
        summary.sectorsGood    = 1559;
        summary.sectorsFound   = 1560;
        summary.badSectors     = 1;
        summary.commonVolume   = 254;

        chips = InspectorText::BuildChips (summary);

        Assert::AreEqual (static_cast<size_t> (5), chips.size(), L"no nonstandard, unformatted, flux or damaged chips");
        Assert::AreEqual (std::wstring (L"16 sector"),                    chips[0].text);
        Assert::AreEqual (std::wstring (L"35 tracks with data"),          chips[1].text);
        Assert::AreEqual (std::wstring (L"1,559 of 1,560 sectors good"),  chips[2].text);
        Assert::IsTrue   (chips[2].isBad, L"the sectors-good chip takes the bad color when a sector is bad");
        Assert::AreEqual (std::wstring (L"1 bad sector"),                 chips[3].text);
        Assert::AreEqual (std::wstring (L"Volume 254"),                   chips[4].text);

        summary              = DiskSummary();
        summary.format       = DiskFormatClass::Nonstandard;
        summary.commonVolume = 254;
        summary.nonstandardTracks = 2;
        chips = InspectorText::BuildChips (summary);

        Assert::AreEqual (static_cast<size_t> (2), chips.size(), L"no volume chip without standard sectors");
        Assert::AreEqual (std::wstring (L"2 nonstandard tracks"), chips[1].text);
    }



    TEST_METHOD (TheSelectionGivesItsLengthInNibblesAndCells)
    {
        InspectorTrackBuilder  builder;
        TrackContext           context;
        TrackAnalysis          track;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 0, InspectorTrackBuilder::FillPattern);
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), track);

        Assert::AreEqual (std::wstring (L"3 nibbles") + L" " + s_kpszMiddleDot + L" 24 cells selected",
                          InspectorText::FormatSelection (track, track.fields[track.sectors[0].addressField].firstNibble, 3), L"an address prologue");
    }


    TEST_METHOD (TheStripReadoutIsTheCellsInView)
    {
        std::wstring  readout = InspectorText::FormatStripReadout (12400, 13016);



        Assert::AreEqual (std::wstring (L"Cells 12,400-13,016"), readout);
    }



    TEST_METHOD (OrdinalsAreEnglish)
    {
        Assert::AreEqual (std::wstring (L"1st"),  InspectorText::FormatOrdinal (1));
        Assert::AreEqual (std::wstring (L"2nd"),  InspectorText::FormatOrdinal (2));
        Assert::AreEqual (std::wstring (L"3rd"),  InspectorText::FormatOrdinal (3));
        Assert::AreEqual (std::wstring (L"6th"),  InspectorText::FormatOrdinal (6));
        Assert::AreEqual (std::wstring (L"11th"), InspectorText::FormatOrdinal (11));
        Assert::AreEqual (std::wstring (L"13th"), InspectorText::FormatOrdinal (13));
        Assert::AreEqual (std::wstring (L"21st"), InspectorText::FormatOrdinal (21));
    }



    TEST_METHOD (TheTrackHeaderGivesTheTrackAndItsCounts)
    {
        DiskAnalysis           analysis;
        const TrackAnalysis *  track    = nullptr;
        std::wstring           line;



        AnalyzeDsk (analysis);
        track = analysis.tracks[analysis.entries[68].slot].get();
        line  = InspectorText::FormatTrackLine (track);

        Assert::AreEqual (std::wstring (L"Track 17.25"), InspectorText::FormatTrackTitle (69));
        Assert::IsTrue   (line.find (L" cells \x00B7 ") != std::wstring::npos);
        Assert::IsTrue   (line.ends_with (L"16 sectors"), line.c_str());
        Assert::AreEqual (std::wstring (L"Nothing recorded on this quarter track"), InspectorText::FormatTrackLine (nullptr));
        Assert::IsTrue   (InspectorText::FormatMeasureLine (*track).starts_with (L"Longest sync "), L"no turn or cell items on a bit track");
    }



    TEST_METHOD (ASectorsTooltipGivesItsPlaceAndFields)
    {
        DiskAnalysis           analysis;
        const TrackAnalysis *  track    = nullptr;
        std::wstring           tip;



        AnalyzeDsk (analysis);
        track = analysis.tracks[analysis.entries[68].slot].get();
        tip   = InspectorText::FormatSectorTooltip (track->sectors[5], *track);

        Assert::AreEqual (std::wstring (L"Sector $5\nAddress field: volume 254, track 17"), tip);
        Assert::AreEqual (std::wstring (L"Reads: 2 \x00B7 Scans: 1,024"), InspectorText::FormatAccessCounts (2, 0, 1024));
        Assert::IsTrue   (InspectorText::FormatAccessCounts (0, 0, 0).empty());
    }



    TEST_METHOD (TheSectorDataHeaderAndEmptyStatesUseTheSpecsWords)
    {
        DiskAnalysis           analysis;
        const TrackAnalysis *  track    = nullptr;
        vector<HeaderItem>     header;
        QuarterTrackEntry      empty;
        QuarterTrackEntry      damaged;
        TrackAnalysis          noSectors;



        AnalyzeDsk (analysis);
        track  = analysis.tracks[analysis.entries[68].slot].get();
        header = InspectorText::BuildSectorHeader (track->sectors[1], *track);

        Assert::AreEqual (std::wstring (L"Sector"),        header[0].label);
        Assert::AreEqual (std::wstring (L"$1"),            header[0].value);
        Assert::AreEqual (std::wstring (L"Good"),          header[4].value);
        Assert::AreEqual (std::wstring (L"6-and-2"),       header[5].value);
        Assert::AreEqual (std::wstring (L"DOS 3.3 sector"), header[6].label);
        Assert::AreEqual (std::wstring (L"$7"),            header[6].value);

        damaged.content      = QuarterTrackContent::Damaged;
        damaged.damageReason = DamageReason::OutsideFile;
        noSectors.framed.cellCount = 100;
        empty.content = QuarterTrackContent::BitTrack;

        Assert::AreEqual (std::wstring (L"Nothing recorded on this quarter track."), InspectorText::FormatNoSectorData (QuarterTrackEntry(), nullptr));
        Assert::AreEqual (std::wstring (L"No standard sectors on this track. The Nibbles tab shows what is recorded on it."), InspectorText::FormatNoSectorData (empty, &noSectors));
        Assert::IsTrue   (InspectorText::FormatNoSectorData (damaged, nullptr).starts_with (L"This quarter track is damaged: its data lies outside the file"));
        Assert::AreEqual (std::wstring (L"No track"),                  InspectorText::FormatNoSectors (QuarterTrackEntry()));
        Assert::AreEqual (std::wstring (L"None in a standard format"), InspectorText::FormatNoSectors (empty));
    }
};
