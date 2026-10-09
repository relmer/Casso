#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
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
        Assert::AreEqual (std::wstring (L"Beyond the head's reach"), rows[DiskImage::kQuarterTrackCount - 1].cells[12]);
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
};
