#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Devices/Disk/Inspector/TrackExport.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExportTests
//
//  Export (FR-058, US4 scenario 10): sectors in physical, DOS 3.3 and
//  ProDOS order with the bad and missing list; nibbles one byte each from
//  the index; a one-record WOZ 2.1 that reopens with its cell times equal
//  to the original's; and the image itself left alone.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TrackExportTests)
{
public:

    static void AnalyzeWoz (bool isFlux, vector<Byte> & outWoz, DiskImage & outImage, DiskAnalysis & out)
    {
        AssertSucceeded (InspectorTrackBuilder::MakeStandardWoz (254, isFlux, false, outWoz));
        AssertSucceeded (WozLoader::Load (outWoz, outImage));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (outImage, 1, "test", 0, false), DecodeSettings::MakeStandard(), out);
    }



    static vector<Byte> Pattern (int sector)
    {
        std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes = {};



        InspectorTrackBuilder::FillPattern (sector, bytes);

        return vector<Byte> (bytes.begin(), bytes.end());
    }



    static vector<Byte> Chunk (const vector<Byte> & bytes, int index)
    {
        return vector<Byte> (bytes.begin() + index * DiskFieldFormat::kSectorBytes, bytes.begin() + (index + 1) * DiskFieldFormat::kSectorBytes);
    }



    TEST_METHOD (SectorsComeOutInTheOrderChosen)
    {
        vector<Byte>           woz;
        DiskImage              image;
        DiskAnalysis           analysis;
        vector<Byte>           bytes;
        vector<ExportProblem>  problems;
        const TrackAnalysis *  track = nullptr;
        int                    i     = 0;



        AnalyzeWoz (false, woz, image, analysis);
        track = analysis.tracks[analysis.entries[8].slot].get();

        TrackExport::ExportSectors (analysis, 2, 2, SectorOrder::Physical, bytes, problems);
        Assert::AreEqual (static_cast<size_t> (16 * 256), bytes.size());
        Assert::IsTrue   (problems.empty());
        Assert::IsTrue   (Chunk (bytes, 5) == Pattern (5));

        TrackExport::ExportSectors (analysis, 2, 2, SectorOrder::Dos33, bytes, problems);
        Assert::IsTrue   (Chunk (bytes, 1) == Pattern (0xD), L"DOS 3.3 logical sector 1 is physical sector $D");

        TrackExport::ExportSectors (analysis, 2, 2, SectorOrder::ProDos, bytes, problems);

        for (const AnalyzedSector & sector : track->sectors)
        {
            i = (sector.prodosBlock - 2 * 8) * 2 + sector.prodosHalf;
            Assert::IsTrue (Chunk (bytes, i) == Pattern (sector.sector), L"each block's halves in block order");
        }

        TrackExport::ExportSectors (analysis, 3, 5, SectorOrder::Physical, bytes, problems);
        Assert::AreEqual (static_cast<size_t> (3 * 16 * 256), bytes.size(), L"a range of tracks");
    }



    TEST_METHOD (MissingAndBadSectorsAreListed)
    {
        InspectorTrackBuilder  builder;
        TrackContext           context;
        TrackAnalysis          track;
        vector<Byte>           bytes;
        vector<ExportProblem>  problems;
        int                    s       = 0;



        builder.AppendSync (40);

        for (s = 0; s < 16; s++)
        {
            if (s != 5)
            {
                builder.AppendAddressField (DiskFieldKind::Sixteen, 254, 0, static_cast<Byte> (s));
                builder.AppendSync (6);
                builder.AppendDataField (DiskFieldKind::Sixteen, Pattern (s), s == 9);
                builder.AppendSync (20);
            }
        }

        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), track);
        TrackExport::ExportTrackSectors (&track, 0, SectorOrder::Physical, bytes, problems);

        Assert::AreEqual (static_cast<size_t> (2), problems.size());
        Assert::AreEqual (9, problems[1].sector, L"the bad one, written as decoded");
        Assert::AreEqual (-1, problems[0].sector, L"the missing one");
        Assert::IsTrue   (Chunk (bytes, 5) == vector<Byte> (256, 0), L"missing is zeros");
    }



    TEST_METHOD (NibblesAreOneByteEachFromTheIndex)
    {
        vector<Byte>           woz;
        DiskImage              image;
        DiskAnalysis           analysis;
        vector<Byte>           bytes;
        vector<std::wstring>   notes;
        const TrackAnalysis *  track = nullptr;
        uint32_t               least = UINT32_MAX;
        Byte                   first = 0;



        AnalyzeWoz (false, woz, image, analysis);
        track = analysis.tracks[analysis.entries[0].slot].get();

        for (const FramedNibble & nibble : track->framed.nibbles)
        {
            if (nibble.startCell % track->framed.cellCount < least)
            {
                least = nibble.startCell % track->framed.cellCount;
                first = nibble.value;
            }
        }

        TrackExport::ExportNibbles (*track, bytes, notes);

        Assert::AreEqual (track->framed.nibbles.size(), bytes.size());
        Assert::AreEqual (first, bytes[0]);
        Assert::IsTrue   (notes.empty());
    }



    TEST_METHOD (TrackBitsReopenWithTheSameCellTimes)
    {
        vector<Byte>  woz;
        vector<Byte>  original;
        DiskImage     image;
        DiskImage     reopened;
        DiskAnalysis  analysis;
        vector<Byte>  exported;
        int           slot      = -1;



        AnalyzeWoz (true, woz, image, analysis);
        original = woz;
        slot     = analysis.entries[8].slot;

        AssertSucceeded (TrackExport::ExportTrackBits (*analysis.copy->tracks[slot], 9, exported));
        AssertSucceeded (WozLoader::Load (exported, reopened));

        Assert::IsTrue (TrackCopy::MakeFromImage (reopened, 0)->fluxBytes == analysis.copy->tracks[slot]->fluxBytes, L"every interval unchanged");
        Assert::IsTrue (woz == original, L"the image is left alone");
    }



    TEST_METHOD (DefaultNamesFollowTheContract)
    {
        Assert::AreEqual (std::wstring (L"Disk T17 sectors.bin"),     TrackExport::GetSectorsName (L"Disk", 17, 17));
        Assert::AreEqual (std::wstring (L"Disk T03-T05 sectors.bin"), TrackExport::GetSectorsName (L"Disk", 3, 5));
        Assert::AreEqual (std::wstring (L"Disk T17.25 nibbles.bin"),  TrackExport::GetNibblesName (L"Disk", 69));
        Assert::AreEqual (std::wstring (L"Disk T17.25.woz"),          TrackExport::GetBitsName (L"Disk", 69));
    }
};
