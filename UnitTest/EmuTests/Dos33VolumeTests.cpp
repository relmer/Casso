#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeTests
//
//  The DOS 3.3 reader, which did not exist before this feature and which
//  extraction depends on.
//
//  Volumes are built by the shipped skeleton writers rather than by hand, so
//  the reader is tested against structures the emulator itself produces and
//  boots -- not against a second, private idea of what DOS 3.3 looks like. A
//  reader and a writer that agree only with each other prove nothing.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (Dos33VolumeTests)
{
public:

    static constexpr Byte  kVolumeNumber = 254;

    //  Tracks 0-2 (where DOS lives) and track 17 (the catalog) are reserved by
    //  a freshly formatted volume, leaving 31 tracks of 16 sectors.
    static constexpr uint32_t  kFreeOnBlankVolume = 31 * 16;

    static constexpr size_t    kNameFieldBytes    = 30;

    //  Geometry for the hand-built multi-list file.
    static constexpr int       kPairsPerList      = 122;   // one list sector holds this many
    static constexpr int       kDataSectorCount   = 250;   // enough to need a THIRD list
    static constexpr int       kList1Track        = 18;
    static constexpr int       kList1Sector       = 0;
    static constexpr int       kList2Track        = 18;
    static constexpr int       kList2Sector       = 1;
    static constexpr int       kList3Track        = 18;
    static constexpr int       kList3Sector       = 2;
    static constexpr int       kDataStartTrack    = 19;

    vector<Byte> MakeFormattedVolume()
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber));

        return buffer;
    }

    vector<Byte> MakeVolumeWithHello()
    {
        vector<Byte>  buffer = MakeFormattedVolume();

        AssertSucceeded (Dos33FileWriter::WriteHello (buffer));

        return buffer;
    }

    //  Clears the free-bitmap bit for a sector, marking it in use. Byte 0 of a
    //  track's four bitmap bytes holds sectors 15..8 in bits 7..0; byte 1 holds
    //  sectors 7..0. A SET bit means free.
    static void MarkAllocated (vector<Byte> & buffer, int track, int sector)
    {
        size_t  at     = Dos33Skeleton::SectorOffset (17, 0) + 0x38 + (size_t) track * 4;
        size_t  byteAt = (sector >= 8) ? at : at + 1;
        int     bit    = (sector >= 8) ? (sector - 8) : sector;

        buffer[byteAt] = (Byte) (buffer[byteAt] & ~(1 << bit));
    }

    static void WriteEntryName (vector<Byte> & buffer, size_t entryAt, const string & name)
    {
        size_t  i = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            Byte  c = (i < name.size()) ? (Byte) name[i] : (Byte) ' ';

            buffer[entryAt + 0x03 + i] = (Byte) (c | 0x80);
        }
    }

    //  A file large enough to need a THIRD track/sector list.
    //
    //  Three, not two, and the distinction is the whole point. The defect this
    //  guards read the next-list pointer's two halves from different sectors --
    //  the track from the sector being left, the sector from the file's HEAD.
    //  On the first hop those are the same sector, so a two-list file walks
    //  correctly by coincidence and proves nothing. Only the second hop, which
    //  needs a third list, tells them apart.
    //
    //  Hand-built because no writer exists yet, and worth building anyway: this
    //  path is reachable by real users the moment the reader ships, and every
    //  volume the shipped skeleton writers can produce holds files far too small
    //  to reach it.
    vector<Byte> MakeVolumeWithMultiListFile()
    {
        vector<Byte>  buffer   = MakeFormattedVolume();
        size_t        entryAt  = Dos33Skeleton::SectorOffset (17, 15) + 0x0B;
        size_t        list1At  = Dos33Skeleton::SectorOffset (kList1Track, kList1Sector);
        size_t        list2At  = Dos33Skeleton::SectorOffset (kList2Track, kList2Sector);
        size_t        list3At  = Dos33Skeleton::SectorOffset (kList3Track, kList3Sector);
        int           i        = 0;
        uint16_t      sectors  = (uint16_t) (kDataSectorCount + 3);

        buffer[entryAt + 0x00] = (Byte) kList1Track;
        buffer[entryAt + 0x01] = (Byte) kList1Sector;
        buffer[entryAt + 0x02] = Dos33Volume::kTypeBinary;
        buffer[entryAt + 0x21] = (Byte) (sectors & 0xFF);
        buffer[entryAt + 0x22] = (Byte) (sectors >> 8);

        WriteEntryName (buffer, entryAt, "BIG");

        // list1 -> list2 -> list3 -> end.
        buffer[list1At + 0x01] = (Byte) kList2Track;
        buffer[list1At + 0x02] = (Byte) kList2Sector;
        buffer[list2At + 0x01] = (Byte) kList3Track;
        buffer[list2At + 0x02] = (Byte) kList3Sector;

        MarkAllocated (buffer, kList1Track, kList1Sector);
        MarkAllocated (buffer, kList2Track, kList2Sector);
        MarkAllocated (buffer, kList3Track, kList3Sector);

        for (i = 0; i < kDataSectorCount; i++)
        {
            int     track  = kDataStartTrack + (i / 16);
            int     sector = i % 16;
            size_t  listAt = (i < kPairsPerList)     ? list1At
                           : (i < kPairsPerList * 2) ? list2At
                                                     : list3At;
            size_t  pairAt = listAt + 0x0C + (size_t) ((i % kPairsPerList) * 2);
            size_t  dataAt = Dos33Skeleton::SectorOffset (track, sector);

            buffer[pairAt]     = (Byte) track;
            buffer[pairAt + 1] = (Byte) sector;

            MarkAllocated (buffer, track, sector);

            // Fill each sector with its own index so a gap or a mis-ordered
            // chain shows up as wrong CONTENT, not merely a wrong length.
            std::fill (buffer.begin() + (ptrdiff_t) dataAt,
                       buffer.begin() + (ptrdiff_t) dataAt + 256,
                       (Byte) i);
        }

        // A binary file's first four bytes are its load address and length.
        {
            size_t    firstAt = Dos33Skeleton::SectorOffset (kDataStartTrack, 0);
            uint32_t  length  = (uint32_t) (kDataSectorCount * 256 - 4);

            buffer[firstAt + 0] = 0x00;
            buffer[firstAt + 1] = 0x60;   // $6000
            buffer[firstAt + 2] = (Byte) (length & 0xFF);
            buffer[firstAt + 3] = (Byte) ((length >> 8) & 0xFF);
        }

        return buffer;
    }

    TEST_METHOD (Enumerate_FreshVolume_HasNoFilesAndReportsItsFreeSpace)
    {
        vector<Byte>    buffer = MakeFormattedVolume();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (0), listing.entries.size(), L"a fresh volume holds no files");
        Assert::IsTrue (listing.hasVolumeNumber);
        Assert::AreEqual (kVolumeNumber, listing.volumeNumber);
        Assert::AreEqual (uint32_t (Dos33Volume::kUnitCount), listing.totalUnits);
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits,
            L"DOS's own tracks and the catalog track are reserved, the rest is free");
        Assert::AreEqual (size_t (0), listing.damage.size(), L"a clean volume reports no damage");
    }

    TEST_METHOD (Enumerate_VolumeWithAFile_ReportsNameTypeSizeAndLockState)
    {
        vector<Byte>    buffer = MakeVolumeWithHello();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("HELLO"), listing.entries[0].name,
            L"the name must come back stripped of high bits and padding");
        Assert::AreEqual (Dos33Volume::kTypeApplesoft, listing.entries[0].type);
        Assert::IsFalse (listing.entries[0].isLocked);
        Assert::AreEqual (uint32_t (2), listing.entries[0].sizeUnits,
            L"one list sector plus one data sector");
        Assert::AreEqual (kFreeOnBlankVolume - 2, listing.freeUnits,
            L"the file's two sectors must have left the free pool");
    }

    TEST_METHOD (Read_ApplesoftFile_StripsTheLengthHeaderDosStoresInTheData)
    {
        // DOS 3.3 records a file's length inside the file's own first bytes
        // rather than in the catalog, so "the sectors this file occupies" and
        // "the file" are different things.
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;

        AssertSucceeded (volume.Read (FilePath::Parse ("HELLO"), payload));

        Assert::AreEqual (size_t (8), payload.bytes.size(),
            L"the two length bytes are consumed, not returned");
        Assert::AreEqual (Byte (0x07), payload.bytes[0], L"next-line pointer low byte");
        Assert::AreEqual (Byte (0x08), payload.bytes[1], L"next-line pointer high byte");
        Assert::AreEqual (Byte (0x0A), payload.bytes[2], L"line number 10");
        Assert::AreEqual (Byte (0xB2), payload.bytes[4], L"the REM token");
        Assert::IsFalse (payload.hasLoadAddress, L"Applesoft records no load address");
        Assert::AreEqual (Dos33Volume::kTypeApplesoft, payload.type);
    }

    TEST_METHOD (Read_FileSpanningThreeTrackSectorLists_ComesBackWholeAndUndamaged)
    {
        // This path failed in a way opposite to every other defect in this
        // area: it reported DAMAGE ON A HEALTHY DISK. Reading the next-list
        // pointer's two halves from different sectors chained every list back
        // to the file's own head, the traversal guard correctly stopped the
        // loop, and the volume reported an unfollowable chain -- plausible,
        // specific, and completely wrong.
        //
        // For this feature that is not just a bad message. The integrity report
        // gates writes and tells delete what it may free, so a good disk
        // carrying one 31 KB file could have had a write refused or space left
        // unreclaimed, with no way for the user to tell it from real damage.
        //
        // Every volume the shipped skeleton writers produce holds files far
        // below 122 sectors, so nothing else in this suite reaches here.
        vector<Byte>           buffer = MakeVolumeWithMultiListFile();
        Dos33Volume            volume (buffer);
        FilePayload            payload;
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.Read (FilePath::Parse ("BIG"), payload));

        Assert::AreEqual (size_t (kDataSectorCount * 256 - 4), payload.bytes.size(),
            L"the whole file must come back, not just the first list's worth");
        Assert::IsTrue (payload.hasLoadAddress);
        Assert::AreEqual (Word (0x6000), payload.loadAddress);

        // Bytes from sectors reachable only through the second and THIRD lists.
        // Sector i begins at payload offset i*256 - 4, the header having been
        // stripped. Sector 245 is the one the defect could never reach.
        Assert::AreEqual (Byte (121), payload.bytes[121 * 256 - 4],
            L"the last sector of the first list must be undisturbed");
        Assert::AreEqual (Byte (125), payload.bytes[125 * 256 - 4],
            L"content past the first list must be present and in order");
        Assert::AreEqual (Byte (245), payload.bytes[245 * 256 - 4],
            L"content past the SECOND list is what the two-list case cannot prove");

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size(),
            L"a healthy multi-list file must NOT be reported as damaged");
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size(),
            L"its sectors are marked allocated, so the free map agrees");
    }

    TEST_METHOD (Read_IsCaseInsensitive_BecauseTheCatalogIsUpperCase)
    {
        // Note this also asserts the stripped length, so a header-stripping
        // regression fails here too. The assertion message distinguishes them:
        // a size mismatch of 256 is the header, a miss is the name lookup.
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;

        AssertSucceeded (volume.Read (FilePath::Parse ("hello"), payload));

        Assert::AreEqual (size_t (8), payload.bytes.size());
    }

    TEST_METHOD (Read_MissingFile_IsNotFoundRatherThanEmpty)
    {
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;
        HRESULT        hr     = volume.Read (FilePath::Parse ("NOPE"), payload);

        Assert::IsTrue (FAILED (hr), L"a missing file must fail, not return nothing");
        Assert::AreEqual (size_t (0), payload.bytes.size());
    }

    TEST_METHOD (Read_MultiComponentPath_IsRefusedNotSilentlyTruncated)
    {
        // DOS 3.3 has no subdirectories. Quietly using the last component would
        // open a different file than the caller named, which is worse than
        // refusing: it succeeds while doing the wrong thing.
        vector<Byte>   buffer = MakeVolumeWithHello();
        Dos33Volume    volume (buffer);
        FilePayload    payload;
        HRESULT        hr     = volume.Read (FilePath::Parse ("SUBDIR/HELLO"), payload);

        Assert::IsTrue (FAILED (hr), L"a path this filesystem cannot express must be refused");
    }

    TEST_METHOD (BuildIntegrityReport_CleanVolume_AgreesWithTheFreeMap)
    {
        vector<Byte>           buffer = MakeVolumeWithHello();
        Dos33Volume            volume (buffer);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::IsTrue (report.IsCatalogFullyParsed(), L"a clean catalog parses fully");
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size(),
            L"no sector may be claimed twice");
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size(),
            L"nothing the catalog references may be marked free");
        Assert::AreEqual (size_t (0), report.GetUnfollowableChains().size());
    }

    TEST_METHOD (BuildIntegrityReport_NamesTheSectorsTheFileClaims)
    {
        // The requirement asks which sectors each file claims, not merely how
        // many claimants a sector has. HELLO occupies its list sector and one
        // data sector, both on track 18.
        vector<Byte>           buffer = MakeVolumeWithHello();
        Dos33Volume            volume (buffer);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        {
            const vector<uint32_t> &  claims = report.GetClaimsOf (0);

            Assert::AreEqual (size_t (2), claims.size(),
                L"the list sector and the data sector are both the file's footprint");
        }
    }

    TEST_METHOD (Enumerate_CatalogChainThatLoops_ReportsDamageAndKeepsWhatItRead)
    {
        // A corrupted next-pointer on the catalog track. The walk must stop and
        // say so, while still returning the entries it reached -- a catalog that
        // goes bad halfway is exactly when the first half matters most.
        vector<Byte>    buffer = MakeVolumeWithHello();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;
        size_t          catalogAt = Dos33Skeleton::SectorOffset (17, 15);

        // Point the first catalog sector back at itself.
        buffer[catalogAt + 0x01] = 17;
        buffer[catalogAt + 0x02] = 15;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size(),
            L"the entries read before the loop must survive");
        Assert::IsTrue (listing.damage.size() > 0, L"the loop must be described");
    }

    TEST_METHOD (Enumerate_CatalogPointingOutsideTheVolume_IsReportedNotFollowed)
    {
        vector<Byte>    buffer    = MakeFormattedVolume();
        Dos33Volume     volume (buffer);
        VolumeListing   listing;
        size_t          vtocAt    = Dos33Skeleton::SectorOffset (17, 0);

        buffer[vtocAt + 0x01] = 99;   // no such track

        AssertSucceeded (volume.Enumerate (listing));

        Assert::IsTrue (listing.damage.size() > 0,
            L"a pointer outside the volume must be reported rather than followed");
    }

    TEST_METHOD (Write_IsNotYetImplemented_AndSaysSoRatherThanSucceeding)
    {
        // An absent capability must not be indistinguishable from a completed
        // one. This is the same rule the decoder follows.
        vector<Byte>   buffer   = MakeFormattedVolume();
        Dos33Volume    volume (buffer);
        vector<Byte>   out;
        FilePayload    payload;
        HRESULT        hrWrite  = S_OK;
        HRESULT        hrDelete = S_OK;

        hrWrite  = volume.Write (FilePath::Parse ("X"), payload, out);
        hrDelete = volume.Delete (FilePath::Parse ("X"), out);

        Assert::IsTrue (FAILED (hrWrite),  L"an unbuilt write must report failure");
        Assert::IsTrue (FAILED (hrDelete), L"an unbuilt delete must report failure");
        Assert::AreEqual (size_t (0), out.size(), L"a refused operation produces nothing");
    }
};
