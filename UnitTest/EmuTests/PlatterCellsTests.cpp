#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Ui/DiskInspector/PlatterCells.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCellsTests
//
//  The bytes the platter samples: a kind per cell from the analysis, and
//  levels that keep the most important kind of each run (FR-023).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PlatterCellsTests)
{
public:

    TEST_METHOD (TheLargerByteIsTheKindToDraw)
    {
        Byte  failed = PlatterCells::Encode (PlatterKind::FailedChecksum);
        Byte  noise  = PlatterCells::Encode (PlatterKind::Noise);
        Byte  random = PlatterCells::Encode (PlatterKind::RandomBits);



        Assert::IsTrue (failed > noise && noise > random);

        for (int k = 0; k < static_cast<int> (PlatterKind::Count); k++)
        {
            PlatterKind  kind = static_cast<PlatterKind> (k);

            Assert::IsTrue (PlatterCells::Decode (PlatterCells::Encode (kind)) == kind);

            if (kind != PlatterKind::FailedChecksum && kind != PlatterKind::Noise && kind != PlatterKind::RandomBits)
            {
                Assert::IsTrue (random > PlatterCells::Encode (kind));
            }
        }
    }



    TEST_METHOD (OneCellOfAFailedChecksumSurvivesEveryLevel)
    {
        vector<Byte>          cells (51200, PlatterCells::Encode (PlatterKind::DataField));
        vector<vector<Byte>>  levels;



        cells[12345] = PlatterCells::Encode (PlatterKind::FailedChecksum);
        PlatterCells::BuildLevels (cells, levels);

        Assert::AreEqual (PlatterCells::kMaxLevels, static_cast<int> (levels.size()));
        Assert::AreEqual (static_cast<size_t> (25600), levels[1].size());

        for (size_t k = 0; k < levels.size(); k++)
        {
            Assert::IsTrue (PlatterCells::Decode (levels[k][12345 >> k]) == PlatterKind::FailedChecksum);
        }
    }



    TEST_METHOD (CellsTakeTheirNibblesKinds)
    {
        InspectorTrackBuilder  builder;
        TrackAnalysis          analysis;
        TrackContext           context;
        vector<Byte>           cells;
        const LocatedField *   address  = nullptr;
        const LocatedField *   data     = nullptr;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 17, InspectorTrackBuilder::FillPattern);
        context.physicalTrack = 17;
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), analysis);
        PlatterCells::BuildCells (analysis, cells);

        address = &analysis.fields[0];
        data    = &analysis.fields[1];

        Assert::AreEqual (static_cast<size_t> (analysis.framed.cellCount), cells.size());
        Assert::IsTrue (PlatterCells::Decode (cells[0]) == PlatterKind::Sync);
        Assert::IsTrue (PlatterCells::Decode (cells[address->startCell]) == PlatterKind::AddressMark);
        Assert::IsTrue (PlatterCells::Decode (cells[analysis.framed.nibbles[address->bodyNibble].startCell]) == PlatterKind::AddressField);
        Assert::IsTrue (PlatterCells::Decode (cells[data->startCell]) == PlatterKind::DataMark);
        Assert::IsTrue (PlatterCells::Decode (cells[analysis.framed.nibbles[data->bodyNibble].startCell + 3]) == PlatterKind::DataField);
    }



    TEST_METHOD (AFailedFieldAndRandomBitsShowAboveTheRest)
    {
        InspectorTrackBuilder                            builder;
        TrackAnalysis                                    analysis;
        TrackContext                                     context;
        vector<Byte>                                     cells;
        std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes    = {};



        builder.AppendSync         (48);
        builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 17, 0);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes, true);
        builder.AppendSync         (20);
        builder.AppendZeros        (4000);

        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), analysis);
        PlatterCells::BuildCells (analysis, cells);

        Assert::IsTrue (PlatterCells::Decode (cells[analysis.framed.nibbles[analysis.fields[1].bodyNibble].startCell]) == PlatterKind::FailedChecksum);
        Assert::IsTrue (PlatterCells::Decode (cells[cells.size() - 2000]) == PlatterKind::RandomBits);
    }
};
