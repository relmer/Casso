#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/DiskAccessLog.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/SectorAccess.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessTests
//
//  Where the guest read and wrote (FR-161): the log's spans for data fields
//  taken from the latch and for runs of written nibbles, and the sectors
//  those spans cover on an analyzed track.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskAccessTests)
{
public:

    static constexpr uint32_t  kTrackCells = 51'200;

    //  A data field as the guest takes it from the latch, one nibble every 8
    //  cells from the given cell: D5 AA AD, the body, then DE AA, after which
    //  a guest that reads the field skips cells while it decodes it.
    static void ReadDataField (DiskAccessLog & log, int quarterTrack, uint32_t firstCell)
    {
        uint32_t  cell = firstCell;



        for (Byte nibble : { Byte (0xD5), Byte (0xAA), Byte (0xAD) })
        {
            log.ObserveRead (quarterTrack, nibble, cell, kTrackCells);
            cell += 8;
        }

        for (int n = 0; n < 343; n++)
        {
            log.ObserveRead (quarterTrack, 0x96, cell, kTrackCells);
            cell += 8;
        }

        for (Byte nibble : { Byte (0xDE), Byte (0xAA) })
        {
            log.ObserveRead (quarterTrack, nibble, cell, kTrackCells);
            cell += 8;
        }
    }



    static void AnalyzeDsk (DiskAnalysis & out)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage     image;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "access.dsk", 0, false), DecodeSettings::MakeStandard(), out);
    }



    TEST_METHOD (ADataFieldReadCountsFromItsPrologue)
    {
        DiskAccessLog  log;



        ReadDataField (log, 8, 1000);
        log.EndRead();

        Assert::AreEqual (static_cast<size_t> (1), log.GetReads()[8].size());
        Assert::AreEqual (1000u, log.GetReads()[8][0].start, L"from the D5");
        Assert::AreEqual (1u, log.GetReads()[8][0].count);

        ReadDataField (log, 8, 1000);
        log.EndRead();
        Assert::AreEqual (static_cast<size_t> (1), log.GetReads()[8].size(), L"the same field again");
        Assert::AreEqual (2u, log.GetReads()[8][0].count);
        Assert::IsTrue   (log.GetReads()[4].empty());
    }



    TEST_METHOD (AFieldCutShortOrOnAnotherTrackIsNotARead)
    {
        DiskAccessLog  log;
        uint32_t       cell = 0;



        for (Byte nibble : { Byte (0xD5), Byte (0xAA), Byte (0xAD), Byte (0x96), Byte (0xDE), Byte (0xAA) })
        {
            log.ObserveRead (8, nibble, cell, kTrackCells);
            cell += 8;
        }

        Assert::IsTrue (log.GetReads()[8].empty(), L"an epilogue before the body ends is not a field");

        log.ObserveRead (8, 0xD5, 0, kTrackCells);
        log.ObserveRead (8, 0xAA, 8, kTrackCells);
        log.ObserveRead (8, 0xAD, 16, kTrackCells);
        ReadDataField (log, 9, 24);
        log.EndRead();
        Assert::IsTrue (log.GetReads()[8].empty(), L"the head moved in the middle of the field");
        Assert::AreEqual (static_cast<size_t> (1), log.GetReads()[9].size());
    }



    TEST_METHOD (AFieldPassedWhileSearchingIsNotARead)
    {
        DiskAccessLog  log;



        //  A guest searching for an address field takes the nibble after the
        //  epilogue with no gap.
        ReadDataField (log, 8, 1000);
        log.ObserveRead (8, 0xEB, 1000 + 349 * 8, kTrackCells);
        log.EndRead();
        Assert::IsTrue (log.GetReads()[8].empty(), L"a field passed while searching");

        //  A guest that read the field takes its next nibble well after it.
        ReadDataField (log, 8, 5000);
        log.ObserveRead (8, 0xFF, 5000 + 349 * 8 + 2000, kTrackCells);
        Assert::AreEqual (static_cast<size_t> (1), log.GetReads()[8].size(), L"a field followed by a gap");

        //  The same, with the field ending just before the index.
        ReadDataField (log, 12, kTrackCells - 349 * 8 - 4);
        log.ObserveRead (12, 0xFF, 4, kTrackCells);
        Assert::IsTrue (log.GetReads()[12].empty(), L"the next nibble across the index is close behind");
    }



    TEST_METHOD (WritesCloseByAreOneRun)
    {
        DiskAccessLog  log;



        for (uint32_t cell = 2000; cell < 3000; cell += 8)
        {
            log.ObserveWrite (12, cell);
        }

        log.ObserveWrite (12, 9000);
        log.EndWrite();

        Assert::AreEqual (static_cast<size_t> (2), log.GetWrites()[12].size(), L"a jump starts another run");
        Assert::AreEqual (2000u, log.GetWrites()[12][0].start);

        log.Clear();
        Assert::IsTrue (log.GetWrites()[12].empty());
    }



    TEST_METHOD (ReadsAndWritesMapToTheSectorsTheyCover)
    {
        DiskAnalysis           analysis;
        const TrackAnalysis *  track = nullptr;
        vector<CellSpan>       reads;
        vector<CellSpan>       writes;
        vector<uint32_t>       readCounts;
        vector<uint32_t>       writeCounts;



        AnalyzeDsk (analysis);
        track = analysis.tracks[static_cast<size_t> (analysis.entries[0].slot)].get();

        const LocatedField &  data3 = track->fields[static_cast<size_t> (track->sectors[3].dataField)];
        const LocatedField &  addr5 = track->fields[static_cast<size_t> (track->sectors[5].addressField)];

        reads.push_back  ({ data3.startCell, data3.endCell, 2 });
        writes.push_back ({ addr5.startCell + 8, addr5.startCell + 16, 1 });

        readCounts  = SectorAccess::CountReads  (*track, reads);
        writeCounts = SectorAccess::CountWrites (*track, writes);

        for (size_t s = 0; s < track->sectors.size(); s++)
        {
            Assert::AreEqual (s == 3 ? 2u : 0u, readCounts[s],  L"only the field read");
            Assert::AreEqual (s == 5 ? 1u : 0u, writeCounts[s], L"a write over part of an address field");
        }

        Assert::AreEqual (3u, SectorAccess::CountAll ({ { 0, 10, 1 }, { 20, 30, 2 } }));
    }
};
