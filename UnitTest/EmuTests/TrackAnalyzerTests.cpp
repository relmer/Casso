#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackAnalyzerTests
//
//  One record at a time: the class it gets (FR-016), its sectors and their
//  states (FR-020, FR-039), nibble kinds (FR-009), measurements (FR-017),
//  decode settings (FR-019) and the per-track findings (FR-015).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackAnalyzerTests)
{
public:

    using SectorBytes = std::array<Byte, DiskFieldFormat::kSectorBytes>;



    static TrackAnalysis Analyze (const InspectorTrackBuilder & builder, int physicalTrack = 17, const DecodeSettings & settings = DecodeSettings::MakeStandard())
    {
        TrackAnalysis  analysis;
        TrackContext   context;



        context.physicalTrack = physicalTrack;
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, settings, analysis);

        return analysis;
    }



    static int CountFindings (const TrackAnalysis & analysis, FindingKind kind)
    {
        return static_cast<int> (std::count_if (analysis.findings.begin(), analysis.findings.end(), [kind] (const Finding & f) { return f.kind == kind; }));
    }



    static void AppendSector (InspectorTrackBuilder & builder, DiskFieldKind kind, int sector, bool isDataBad = false)
    {
        SectorBytes  bytes = {};



        InspectorTrackBuilder::FillPattern (sector, bytes);
        builder.AppendAddressField (kind, 254, 17, static_cast<Byte> (sector));
        builder.AppendSync         (6);
        builder.AppendDataField    (kind, bytes, isDataBad);
        builder.AppendSync         (20);
    }



    TEST_METHOD (AStandardTrackIsSixteenSectorWithEverySectorGood)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        analysis = Analyze (builder);

        Assert::IsTrue (analysis.trackClass == TrackClass::Sixteen);
        Assert::AreEqual (16, analysis.sectorsFound);
        Assert::AreEqual (16, analysis.sectorsGood);
        Assert::AreEqual (0, static_cast<int> (analysis.findings.size()), L"a standard track has no findings");
        Assert::IsTrue (analysis.nibbleKinds[0] == NibbleKind::Sync);
        Assert::IsTrue (analysis.nibbleKinds[analysis.fields[0].firstNibble] == NibbleKind::AddressPrologue);
        Assert::IsTrue (analysis.nibbleKinds[analysis.fields[1].bodyNibble] == NibbleKind::DataField);
        Assert::AreEqual (48 + 20, analysis.measurements.longestSync.count, L"the seam: the run before sector 0 joined across the index to the one after sector 15");
        Assert::AreEqual (10, analysis.measurements.longestSync.widthCells);
        Assert::IsTrue (analysis.measurements.sector0Angle >= 0 && analysis.measurements.sector0Angle < 0.05);
    }



    TEST_METHOD (SectorsCarryTheirDosAndProDosNumbers)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        int                    po       = 0;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        analysis = Analyze (builder);
        po       = NibblizationLayer::GetPoFileIndexForDosLogicalSector (analysis.sectors[1].dos33Logical);

        Assert::AreEqual (7, analysis.sectors[1].dos33Logical, L"physical sector 1 holds DOS 3.3 logical sector 7");
        Assert::AreEqual (17 * 8 + po / 2, analysis.sectors[1].prodosBlock);
        Assert::AreEqual (po % 2, analysis.sectors[1].prodosHalf);
    }



    TEST_METHOD (ThirteenSectorAndMixedTracksAreClassified)
    {
        InspectorTrackBuilder  thirteen;
        InspectorTrackBuilder  mixed;
        TrackAnalysis          analysis;



        thirteen.AppendStandardTrack (DiskFieldKind::Thirteen, 254, 17, InspectorTrackBuilder::FillPattern);
        analysis = Analyze (thirteen);

        Assert::IsTrue (analysis.trackClass == TrackClass::Thirteen);
        Assert::AreEqual (13, analysis.sectorsGood);

        mixed.AppendSync (48);
        AppendSector (mixed, DiskFieldKind::Sixteen,  0);
        AppendSector (mixed, DiskFieldKind::Thirteen, 1);
        analysis = Analyze (mixed);

        Assert::IsTrue (analysis.trackClass == TrackClass::ThirteenAndSixteen);
        Assert::AreEqual (2, analysis.sectorsGood);
    }



    TEST_METHOD (NoPassingAddressFieldMakesTheTrackNonstandard)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        SectorBytes            bytes    = {};
        int                    s        = 0;



        builder.AppendSync (48);

        for (s = 0; s < 4; s++)
        {
            builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 17, static_cast<Byte> (s), true);
            builder.AppendSync         (6);
            builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
            builder.AppendSync         (20);
        }

        analysis = Analyze (builder);

        Assert::IsTrue (analysis.trackClass == TrackClass::Nonstandard);
        Assert::AreEqual (0, analysis.sectorsFound, L"its fields count toward neither found nor good");
        Assert::AreEqual (8, static_cast<int> (analysis.fields.size()), L"the fields are still listed");
        Assert::AreEqual (4, CountFindings (analysis, FindingKind::AddressChecksumFailed));
    }



    TEST_METHOD (RandomBitsAreUnformattedAndValidNibblesWithoutFieldsAreNonstandard)
    {
        InspectorTrackBuilder  blank;
        InspectorTrackBuilder  nibbles;
        int                    i       = 0;



        blank.AppendZeros (51200);
        Assert::IsTrue (Analyze (blank).trackClass == TrackClass::Unformatted);

        for (i = 0; i < 6000; i++)
        {
            nibbles.AppendNibble (0x96);
        }

        Assert::IsTrue (Analyze (nibbles).trackClass == TrackClass::Nonstandard);
    }



    TEST_METHOD (AChecksumCheckTurnedOffMakesSectorsNotChecked)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        DecodeSettings         settings;
        DecodeRange            range;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);

        range.checks.isDataChecksumOn = false;
        settings.AddRange (range);
        analysis = Analyze (builder, 17, settings);

        Assert::AreEqual (16, analysis.sectorsFound);
        Assert::AreEqual (0,  analysis.sectorsGood);
        Assert::AreEqual (16, analysis.sectorsNotChecked);
        Assert::IsTrue (analysis.sectors[0].state == SectorState::NotChecked);
        Assert::IsTrue (analysis.fields[1].data.isChecksumGood, L"the stored and computed checksums are still there");
    }



    TEST_METHOD (CustomMarksDecodeATrackTheStandardOnesDoNot)
    {
        InspectorTrackBuilder  builder;
        DecodeSettings         settings;
        DecodeRange            range;
        SectorBytes            bytes    = {};
        std::string            error;



        builder.AppendSync   (48);
        builder.AppendNibble (0xD4);
        builder.AppendNibble (0xAA);
        builder.AppendNibble (0x96);

        for (Byte v : { Byte (254), Byte (17), Byte (2), Byte (254 ^ 17 ^ 2) })
        {
            Byte  odd  = 0;
            Byte  even = 0;

            DiskFieldFormat::Encode44 (v, odd, even);
            builder.AppendNibble (odd);
            builder.AppendNibble (even);
        }

        builder.AppendNibble    (0xDE);
        builder.AppendNibble    (0xAA);
        builder.AppendSync      (6);
        builder.AppendDataField (DiskFieldKind::Sixteen, bytes);
        builder.AppendSync      (48);

        Assert::IsTrue (Analyze (builder).trackClass == TrackClass::Nonstandard);

        Assert::IsTrue (DiskMarkPattern::TryParse ("D4 AA ??", range.customMarks.address16.emplace_back(), error));
        settings.AddRange (range);

        Assert::IsFalse (settings.IsStandard());
        Assert::IsTrue (Analyze (builder, 17, settings).trackClass == TrackClass::Sixteen);
    }



    TEST_METHOD (FieldFindingsAppearOnceEach)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        SectorBytes            bytes    = {};
        vector<Byte>           body;
        Byte                   checksum = 0;



        builder.AppendSync (48);
        AppendSector (builder, DiskFieldKind::Sixteen, 0, true);                       //  data checksum fails
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 17, 1);               //  no data field
        builder.AppendSync (60);
        builder.AppendDataField (DiskFieldKind::Sixteen, bytes);                       //  no address field
        builder.AppendSync (20);
        builder.AppendNibble (0xD5);                                                    //  a stray D5 AA 9C
        builder.AppendNibble (0xAA);
        builder.AppendNibble (0x9C);
        builder.AppendSync (20);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 17, 2);               //  a nibble outside the table
        builder.AppendSync (6);
        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, bytes, body, checksum);
        body[100] = 0xAA;
        builder.AppendNibble (0xD5);
        builder.AppendNibble (0xAA);
        builder.AppendNibble (0xAD);

        for (Byte nibble : body)
        {
            builder.AppendNibble (nibble);
        }

        builder.AppendNibble (checksum);
        builder.AppendNibble (0xDE);
        builder.AppendNibble (0xAA);
        builder.AppendNibble (0xEB);
        builder.AppendSync (48);

        analysis = Analyze (builder);

        Assert::AreEqual (1, CountFindings (analysis, FindingKind::DataChecksumFailed), L"an invalid nibble decodes as 0, the value the zero-filled sector had there");
        Assert::AreEqual (1, CountFindings (analysis, FindingKind::AddressWithoutData));
        Assert::AreEqual (1, CountFindings (analysis, FindingKind::DataWithoutAddress));
        Assert::AreEqual (1, CountFindings (analysis, FindingKind::StrayFieldMark));
        Assert::AreEqual (1, CountFindings (analysis, FindingKind::NibblesOutsideTable));
        Assert::IsTrue (analysis.sectors[2].editBlock == EditBlock::NibblesOutsideTable);
    }



    TEST_METHOD (SectorNumberFindingsAppearOnceEach)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        int                    s        = 0;



        builder.AppendSync (48);

        for (s = 0; s < 15; s++)
        {
            AppendSector (builder, DiskFieldKind::Sixteen, s);
        }

        AppendSector (builder, DiskFieldKind::Sixteen, 5);
        analysis = Analyze (builder);

        Assert::AreEqual (1, CountFindings (analysis, FindingKind::SectorRepeated));
        Assert::AreEqual (1, CountFindings (analysis, FindingKind::SectorsMissing));
        Assert::AreEqual (15, std::find_if (analysis.findings.begin(), analysis.findings.end(), [] (const Finding & f) { return f.kind == FindingKind::SectorsMissing; })->sectors[0]);
    }



    TEST_METHOD (AnAddressTrackOtherThanThePhysicalOneIsFound)
    {
        InspectorTrackBuilder  builder;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);

        Assert::AreEqual (16, CountFindings (Analyze (builder, 18), FindingKind::AddressTrackDiffers));
        Assert::AreEqual (0,  CountFindings (Analyze (builder, 17), FindingKind::AddressTrackDiffers));
    }
};
