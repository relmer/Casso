#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/FindBar.h"
#include "Ui/DiskInspector/FindSession.h"
#include "Ui/DiskInspector/InspectorTables.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTablesTests
//
//  The Findings tab (FR-048, FR-049): disk order, sorting, filtering and
//  category counts; the Tracks tab (FR-047): every quarter track, with the
//  quarter tracks that share a record; and the Fields tab (FR-042).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorTablesTests)
{
public:

    //  A DSK with track 5 replaced: volume 17 instead of 254, and sector 3's
    //  data checksum broken.
    static void AnalyzePlanted (DiskAnalysis & out, const DecodeSettings & settings = DecodeSettings::MakeStandard())
    {
        vector<Byte>           sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage                                        image;
        InspectorTrackBuilder                            odd;
        std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes = {};
        int                                              s     = 0;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));

        odd.AppendSync (48);

        for (s = 0; s < 16; s++)
        {
            InspectorTrackBuilder::FillPattern (s, bytes);
            odd.AppendAddressField (DiskFieldKind::Sixteen, 17, 5, static_cast<Byte> (s));
            odd.AppendSync         (6);
            odd.AppendDataField    (DiskFieldKind::Sixteen, bytes, s == 3);
            odd.AppendSync         (20);
        }

        odd.PadToCells (NibblizationLayer::kTrackBitCapacity);
        odd.PackBits (image.GetTrackBitsForWrite (5));
        image.SetTrackBitCount (5, odd.GetCellCount());

        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "planted.dsk", 0, false), settings, out);
    }



    TEST_METHOD (FindingsComeInDiskOrderWithCounts)
    {
        DiskAnalysis      analysis;
        vector<TableRow>  rows;



        AnalyzePlanted (analysis);
        rows = InspectorTables::BuildFindings (analysis, InspectorTables::GetAllCategories(), -1, false);

        Assert::AreEqual (analysis.findings.size(), rows.size());
        Assert::AreEqual (17, static_cast<int> (rows.size()), L"16 volume findings and one checksum");

        for (size_t i = 1; i < rows.size(); i++)
        {
            Assert::IsTrue (rows[i - 1].quarterTrack <= rows[i].quarterTrack);
        }

        Assert::AreEqual (17, InspectorTables::CountCategories (analysis)[static_cast<int> (FindingCategory::Field)]);
        Assert::AreEqual (std::wstring (L"5"), rows[0].cells[1]);
    }



    TEST_METHOD (FindingsFilterByCategoryAndSortByColumn)
    {
        DiskAnalysis      analysis;
        vector<TableRow>  rows;



        AnalyzePlanted (analysis);

        rows = InspectorTables::BuildFindings (analysis, 1u << static_cast<int> (FindingCategory::Track), -1, false);
        Assert::AreEqual (static_cast<size_t> (0), rows.size(), L"no track findings on this disk");

        rows = InspectorTables::BuildFindings (analysis, InspectorTables::GetAllCategories(), 2, true);
        Assert::AreEqual (std::wstring (L"$F"), rows[0].cells[2], L"sectors sort by their hex value, highest first");
        Assert::AreEqual (std::wstring (L"$0"), rows.back().cells[2]);
    }



    TEST_METHOD (TracksListEveryQuarterTrackAndTheirSharing)
    {
        DiskAnalysis      analysis;
        vector<TableRow>  rows;



        AnalyzePlanted (analysis);
        rows = InspectorTables::BuildTracks (analysis);

        Assert::AreEqual (static_cast<size_t> (DiskImage::kQuarterTrackCount), rows.size());
        Assert::AreEqual (std::wstring (L"17.25"), rows[69].cells[0]);
        Assert::AreEqual (std::wstring (L"Bit track"), rows[68].cells[1]);
        Assert::AreEqual (InspectorFormat::FormatQuarterTrackList (analysis.entries[68].sharesWith), rows[68].cells[2]);
        Assert::AreEqual (std::wstring (L"15 of 16"), rows[20].cells[4], L"track 5 has one bad sector");
        Assert::AreEqual (std::wstring (L"Beyond the head's reach"), rows[DiskImage::kQuarterTrackCount - 1].cells[14]);
        Assert::IsFalse (InspectorTables::IsAlignmentNoteShown (analysis), L"no note for a DSK");
    }



    TEST_METHOD (FieldsListEachAddressFieldWithItsDataField)
    {
        DiskAnalysis      analysis;
        vector<TableRow>  rows;



        AnalyzePlanted (analysis);
        rows = InspectorTables::BuildFields (analysis, 20);

        Assert::AreEqual (static_cast<size_t> (16), rows.size());
        Assert::AreEqual (std::wstring (L"$3"),  rows[3].cells[2]);
        Assert::AreEqual (std::wstring (L"17"),  rows[3].cells[3]);
        Assert::AreEqual (std::wstring (L"D5 AA 96 / DE AA EB"), rows[3].cells[5]);
        Assert::IsTrue   (rows[3].cells[9].find (L"computed") != std::wstring::npos, L"the broken checksum shows both values");
        Assert::IsTrue   (rows[2].cells[9].ends_with (L" good"));
        Assert::AreEqual (3, rows[3].sectorIndex);
        Assert::AreEqual (std::wstring (L"6-and-2"), rows[3].cells[10]);
    }



    TEST_METHOD (AChecksumTurnedOffShowsAsNotChecked)
    {
        DiskAnalysis      analysis;
        DecodeSettings    settings;
        DecodeRange       range;
        vector<TableRow>  rows;



        range.checks.isDataChecksumOn = false;
        settings.AddRange (range);
        AnalyzePlanted (analysis, settings);
        rows = InspectorTables::BuildFields (analysis, 20);

        Assert::IsTrue   (rows[3].cells[9].starts_with (L"Not checked ($") && rows[3].cells[9].find (L", computed $") != std::wstring::npos, L"both values still show");
        Assert::IsTrue   (rows[3].cells[6].ends_with (L" good"), L"the address checksum is still checked");
    }



    TEST_METHOD (FindResultsListEachHitWithItsSectorAndField)
    {
        DiskAnalysis       analysis;
        SearchQuery        query;
        vector<SearchHit>  hits;
        vector<TableRow>   rows;



        AnalyzePlanted (analysis);

        query.text = L"D5 AA 96";
        hits       = InspectorSearch::Find (analysis, 20, query);
        rows       = InspectorTables::BuildSearchHits (hits, &analysis, nullptr);

        Assert::AreEqual (static_cast<size_t> (16), rows.size(), L"one address prolog per sector");
        Assert::AreEqual (std::wstring (L"5"),       rows[3].cells[0]);
        Assert::AreEqual (std::wstring (L"$3"),      rows[3].cells[1]);
        Assert::AreEqual (std::wstring (L"Address"), rows[3].cells[2]);
        Assert::IsTrue   (rows[3].cells[3].starts_with (L"Nibble $"));
        Assert::AreEqual (3, rows[3].finding, L"a row indicates its hit's index");

        query.text = L"D5 AA AD";
        rows       = InspectorTables::BuildSearchHits (InspectorSearch::Find (analysis, 20, query), &analysis, nullptr);

        Assert::AreEqual (static_cast<size_t> (16), rows.size());
        Assert::AreEqual (std::wstring (L"Data"), rows[0].cells[2]);
    }



    TEST_METHOD (FindResultsPlaceASectorDataHitAtItsByte)
    {
        DiskAnalysis      analysis;
        SearchHit         hit;
        vector<TableRow>  rows;



        AnalyzePlanted (analysis);

        hit.quarterTrack = 20;
        hit.track        = analysis.tracks[analysis.entries[20].slot].get();
        hit.sectorIndex  = 2;
        hit.offset       = 0x2C;
        hit.length       = 3;
        rows             = InspectorTables::BuildSearchHits ({ hit }, &analysis, nullptr);

        Assert::AreEqual (std::wstring (L"$2"),        rows[0].cells[1]);
        Assert::AreEqual (std::wstring (L"Data"),      rows[0].cells[2]);
        Assert::AreEqual (std::wstring (L"Byte $2C"),  rows[0].cells[3]);
    }



    TEST_METHOD (FindMarksJoinRunsAndWrapAtTheIndex)
    {
        vector<FindMarks::Run>  runs = { { 10, 3 }, { 5, 2 }, { 12, 4 }, { 7, 1 }, { 98, 4 } };



        FindMarks::Merge (runs);

        Assert::AreEqual (static_cast<size_t> (3), runs.size(), L"runs that touch or overlap are joined");
        Assert::AreEqual (5,  runs[0].first);
        Assert::AreEqual (3,  runs[0].count);
        Assert::AreEqual (10, runs[1].first);
        Assert::AreEqual (6,  runs[1].count);

        Assert::IsTrue  (FindMarks::IsInRuns (runs, 100, 15));
        Assert::IsFalse (FindMarks::IsInRuns (runs, 100, 16));
        Assert::IsFalse (FindMarks::IsInRuns (runs, 100, 9));
        Assert::IsTrue  (FindMarks::IsInRuns (runs, 100, 1),  L"the last run passes the index");
        Assert::IsFalse (FindMarks::IsInRuns (runs, 100, 2));
        Assert::IsFalse (FindMarks::IsInRuns (runs, 0, 1),    L"byte runs do not wrap");
    }



    TEST_METHOD (FindMarksCountHitsAndPlaceSectorBytes)
    {
        DiskAnalysis           analysis;
        SearchQuery            query;
        FindMarks              marks;
        SearchHit              hit;
        const TrackAnalysis *  track = nullptr;
        int                    first = 0;
        int                    count = 0;



        AnalyzePlanted (analysis);
        track = analysis.tracks[analysis.entries[20].slot].get();

        query.text = L"D5 AA 96";
        marks.Build (InspectorSearch::Find (analysis, 20, query), nullptr, query.text);

        Assert::AreEqual (16, marks.GetTrackHits (track));
        Assert::AreEqual (1,  marks.GetSectorHits (track, 3));
        Assert::IsTrue   (marks.IsNibbleMarked (track, track->fields[track->sectors[3].addressField].firstNibble));
        Assert::IsFalse  (marks.IsNibbleMarked (track, track->fields[track->sectors[3].dataField].bodyNibble));
        Assert::AreEqual (std::wstring (L"16"), InspectorTables::BuildTracks (analysis, &marks)[20].cells[12], L"the Tracks tab counts the hits");
        Assert::AreEqual (std::wstring(),       InspectorTables::BuildTracks (analysis, &marks)[16].cells[12]);

        first = track->fields[track->sectors[3].addressField].firstNibble;
        Assert::IsTrue   (marks.IsRangeMarked (track, first - 5, 6),  L"a spot that ends on a hit's first nibble");
        Assert::IsTrue   (marks.IsRangeMarked (track, first + 2, 10), L"a spot that starts on a hit's last nibble");
        Assert::IsFalse  (marks.IsRangeMarked (track, first + 3, 10));
        Assert::AreEqual (std::wstring (L"Find hit for \"D5 AA 96\""),   marks.FormatHitLine (1));
        Assert::AreEqual (std::wstring (L"3 find hits for \"D5 AA 96\""), marks.FormatHitLine (3));

        hit.quarterTrack = 20;
        hit.track        = track;
        hit.sectorIndex  = 2;
        hit.offset       = 0x2C;
        hit.length       = 3;

        Assert::IsTrue   (FindMarks::GetNibbles (hit, first, count));
        Assert::AreEqual (track->fields[track->sectors[2].dataField].bodyNibble + 86 + 0x2C, first, L"6-and-2 bytes follow 86 auxiliary nibbles");
        Assert::AreEqual (3, count);

        marks.Build ({ hit }, nullptr, L"x");

        Assert::IsTrue   (marks.IsByteMarked (track, 2, 0x2E));
        Assert::IsFalse  (marks.IsByteMarked (track, 2, 0x2F));
        Assert::IsFalse  (marks.IsByteMarked (track, 3, 0x2C));
        Assert::AreEqual (1, marks.GetSectorHits (track, 2));
        Assert::IsTrue   (marks.IsNibbleMarked (track, first + 2));
    }


    TEST_METHOD (FindBarQueriesDifferInEveryField)
    {
        SearchQuery  a;
        SearchQuery  b;



        a.text = L"D5 AA";
        b      = a;
        Assert::IsTrue (FindBar::IsSame (a, b));

        b.kind = SearchKind::SectorHex;
        Assert::IsFalse (FindBar::IsSame (a, b));

        b = a;
        b.isWholeDisk = !a.isWholeDisk;
        Assert::IsFalse (FindBar::IsSame (a, b));

        b = a;
        b.isAnyBitOffset = !a.isAnyBitOffset;
        Assert::IsFalse (FindBar::IsSame (a, b));

        b = a;
        b.text = L"D5 AB";
        Assert::IsFalse (FindBar::IsSame (a, b));

        Assert::IsTrue (FindBar::GetHint (SearchKind::Nibbles).find (L"D5 AA 96") != std::wstring::npos);
        Assert::IsTrue (FindBar::GetHint (SearchKind::SectorText).find (L"high bit") != std::wstring::npos);
    }


    TEST_METHOD (AFindSessionStepsAroundItsHits)
    {
        DiskAnalysis  analysis;
        FindSession   find;
        SearchQuery   query;



        AnalyzePlanted (analysis);
        query.text = L"D5 AA 96";

        Assert::IsTrue   (find.Run (analysis, 20, query, false));
        Assert::AreEqual (16, find.GetCount());
        Assert::AreEqual (-1, find.GetIndex(), L"nothing selected until a step");
        Assert::AreEqual (0,  find.Step (1));
        Assert::AreEqual (15, find.Step (-1), L"back from the first wraps to the last");
        Assert::AreEqual (0,  find.Step (1),  L"on from the last wraps to the first");
        Assert::IsTrue   (find.Choose (7));
        Assert::IsFalse  (find.Choose (16));
        Assert::AreEqual (7,  find.GetIndex());

        query.text = L"D5 G";
        Assert::IsFalse  (find.Run (analysis, 20, query, false), L"a pattern that cannot be read");
        Assert::IsTrue   (find.IsEmpty());
        Assert::AreEqual (-1, find.Step (1));

        query.text.clear();
        Assert::IsTrue   (find.Run (analysis, 20, query, false), L"an empty pattern is no error");
        Assert::IsTrue   (find.IsEmpty());
    }



    TEST_METHOD (AFindSessionRepeatsAPartialSearchAndDropsOutOfDateHits)
    {
        DiskAnalysis  analysis;
        FindSession   find;
        SearchQuery   query;
        int           slot = 0;



        AnalyzePlanted (analysis);
        query.text = L"D5 AA 96";

        Assert::IsTrue  (find.Run (analysis, 20, query, true));
        Assert::IsFalse (find.IsRepeatDue (true),  L"not while the analysis is still running");
        Assert::IsTrue  (find.IsRepeatDue (false), L"once it is done");

        (void) find.Step (1);
        (void) find.Step (1);
        find.Repeat (analysis, false);
        Assert::AreEqual (1, find.GetIndex(), L"the selected hit's place is kept");
        Assert::IsFalse  (find.IsRepeatDue (false));

        //  The record analyzed again: a new analysis object for the slot.
        slot = analysis.entries[20].slot;
        analysis.tracks[slot] = std::make_shared<const TrackAnalysis> (*analysis.tracks[slot]);

        Assert::AreEqual (16, find.GetCount());
        Assert::IsTrue   (find.GetCurrentHits (analysis).empty(), L"every hit was on the record analyzed again");
    }
};