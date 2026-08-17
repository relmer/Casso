#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
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

    static constexpr size_t    kNameFieldBytes     = 30;
    static constexpr size_t    kEntryBase          = 0x0B;
    static constexpr size_t    kEntrySize          = 0x23;
    static constexpr int       kCatalogFirstSector = 15;
    static constexpr size_t    kDosBinHeaderBytes  = 4;

    //  The decorative heading rows on Merlin's disk carry eight of them.
    static constexpr size_t    kDecorativeBackspaces = 8;

    //  Geometry for the hand-built cross-linked and damaged volumes.
    static constexpr int       kSharedTrack       = 18;
    static constexpr int       kSharedSector      = 13;
    static constexpr Byte      kTrackOffTheVolume = 40;

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

    //  Sets the free-bitmap bit for a sector, marking it available. The inverse
    //  of MarkAllocated, and the state a bad delete leaves behind when it hands
    //  back a sector a catalog entry still points at.
    static void MarkFree (vector<Byte> & buffer, int track, int sector)
    {
        size_t  at     = Dos33Skeleton::SectorOffset (17, 0) + 0x38 + (size_t) track * 4;
        size_t  byteAt = (sector >= 8) ? at : at + 1;
        int     bit    = (sector >= 8) ? (sector - 8) : sector;

        buffer[byteAt] = (Byte) (buffer[byteAt] | (1 << bit));
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

    static size_t EntryOffset (int catalogSector, int slot)
    {
        return Dos33Skeleton::SectorOffset (17, catalogSector) + kEntryBase + (size_t) slot * kEntrySize;
    }

    static uint32_t FreeSectors (const vector<Byte> & buffer)
    {
        Dos33Volume    volume (buffer);
        VolumeListing  listing;

        AssertSucceeded (volume.Enumerate (listing));

        return listing.freeUnits;
    }

    //  A binary payload whose every byte records which STORED sector it belongs
    //  in, so a chain walked in the wrong order shows up as wrong CONTENT rather
    //  than merely as a wrong length. The four header bytes shift it by four.
    static FilePayload MakeBinaryPayload (size_t length, Word loadAddress)
    {
        FilePayload  payload;
        size_t       i = 0;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = loadAddress;
        payload.hasLoadAddress = true;

        payload.bytes.resize (length);

        for (i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) (((i + kDosBinHeaderBytes) / 256) & 0xFF);
        }

        return payload;
    }

    //  A catalog entry that occupies no sectors, of the shape twenty of the
    //  sixty-three entries on Merlin's own disk have: a $7F/$7F pointer never
    //  meant to be followed, a sector count of zero, and a name whose backspaces
    //  walk DOS's cursor back so the text lands at column zero.
    static void WriteDecorativeEntry (vector<Byte> & buffer, int slot)
    {
        size_t  entryAt = EntryOffset (kCatalogFirstSector, slot);
        size_t  i       = 0;

        buffer[entryAt + 0x00] = 0x7F;
        buffer[entryAt + 0x01] = 0x7F;
        buffer[entryAt + 0x02] = Dos33Volume::kTypeText;
        buffer[entryAt + 0x21] = 0;
        buffer[entryAt + 0x22] = 0;

        buffer[entryAt + 0x03] = 0xC1;   // high-ASCII 'A'

        for (i = 1; i < kDecorativeBackspaces + 1; i++)
        {
            buffer[entryAt + 0x03 + i] = 0x88;   // backspace
        }

        for (i = kDecorativeBackspaces + 1; i < kNameFieldBytes; i++)
        {
            buffer[entryAt + 0x03 + i] = 0xA0;
        }
    }

    //  Two files that both claim one sector. Deleting either must not hand that
    //  sector back: the survivor still reads through it, and the corruption from
    //  freeing it appears only the next time anything allocates.
    vector<Byte> MakeVolumeWithCrossLinkedFiles()
    {
        vector<Byte>  buffer = MakeFormattedVolume();
        size_t        entryA = EntryOffset (kCatalogFirstSector, 0);
        size_t        entryB = EntryOffset (kCatalogFirstSector, 1);
        size_t        listA  = Dos33Skeleton::SectorOffset (kSharedTrack, 15);
        size_t        listB  = Dos33Skeleton::SectorOffset (kSharedTrack, 12);
        size_t        dataB  = Dos33Skeleton::SectorOffset (kSharedTrack, 11);
        size_t        shared = Dos33Skeleton::SectorOffset (kSharedTrack, kSharedSector);

        // FILEA: list at S15, data at S14 and the shared S13.
        buffer[entryA + 0x00] = (Byte) kSharedTrack;
        buffer[entryA + 0x01] = 15;
        buffer[entryA + 0x02] = Dos33Volume::kTypeBinary;
        buffer[entryA + 0x21] = 3;

        WriteEntryName (buffer, entryA, "FILEA");

        buffer[listA + 0x0C] = (Byte) kSharedTrack;
        buffer[listA + 0x0D] = 14;
        buffer[listA + 0x0E] = (Byte) kSharedTrack;
        buffer[listA + 0x0F] = (Byte) kSharedSector;

        // FILEB: list at S12, data at S11 and the SAME S13.
        buffer[entryB + 0x00] = (Byte) kSharedTrack;
        buffer[entryB + 0x01] = 12;
        buffer[entryB + 0x02] = Dos33Volume::kTypeBinary;
        buffer[entryB + 0x21] = 3;

        WriteEntryName (buffer, entryB, "FILEB");

        buffer[listB + 0x0C] = (Byte) kSharedTrack;
        buffer[listB + 0x0D] = 11;
        buffer[listB + 0x0E] = (Byte) kSharedTrack;
        buffer[listB + 0x0F] = (Byte) kSharedSector;

        // FILEB's own bytes: a load address, a length of 508, then a pattern
        // that spans both of its data sectors so a lost sector is visible.
        buffer[dataB + 0] = 0x00;
        buffer[dataB + 1] = 0x20;
        buffer[dataB + 2] = 0xFC;
        buffer[dataB + 3] = 0x01;

        std::fill (buffer.begin() + (ptrdiff_t) dataB + 4,
                   buffer.begin() + (ptrdiff_t) dataB + 256, (Byte) 0xAA);
        std::fill (buffer.begin() + (ptrdiff_t) shared,
                   buffer.begin() + (ptrdiff_t) shared + 256, (Byte) 0xBB);

        MarkAllocated (buffer, kSharedTrack, 15);
        MarkAllocated (buffer, kSharedTrack, 14);
        MarkAllocated (buffer, kSharedTrack, kSharedSector);
        MarkAllocated (buffer, kSharedTrack, 12);
        MarkAllocated (buffer, kSharedTrack, 11);

        return buffer;
    }

    //  A file whose track/sector list chains to a track the volume does not
    //  have. Refusing to delete it would strand every sector it holds.
    vector<Byte> MakeVolumeWithDamagedChainFile()
    {
        vector<Byte>  buffer  = MakeFormattedVolume();
        size_t        entryAt = EntryOffset (kCatalogFirstSector, 0);
        size_t        listAt  = Dos33Skeleton::SectorOffset (kSharedTrack, 15);

        buffer[entryAt + 0x00] = (Byte) kSharedTrack;
        buffer[entryAt + 0x01] = 15;
        buffer[entryAt + 0x02] = Dos33Volume::kTypeBinary;
        buffer[entryAt + 0x21] = 3;

        WriteEntryName (buffer, entryAt, "BROKEN");

        buffer[listAt + 0x01] = kTrackOffTheVolume;
        buffer[listAt + 0x02] = 0;
        buffer[listAt + 0x0C] = (Byte) kSharedTrack;
        buffer[listAt + 0x0D] = 14;

        MarkAllocated (buffer, kSharedTrack, 15);
        MarkAllocated (buffer, kSharedTrack, 14);

        return buffer;
    }

    //  What a computed result must not disagree about. The whole-volume IsClean
    //  is deliberately NOT the assertion: a DOS 3.3 volume's boot tracks and
    //  catalog track are allocated and claimed by no catalog entry, so every
    //  real volume carries a standing allocated-but-unclaimed set. What an edit
    //  must not do is CHANGE it, or introduce a disagreement of its own.
    static void AssertEditLeftTheVolumeConsistent (const vector<Byte> & before, const vector<Byte> & after)
    {
        Dos33Volume            wasVolume (before);
        Dos33Volume            isVolume (after);
        VolumeIntegrityReport  was;
        VolumeIntegrityReport  is;

        AssertSucceeded (wasVolume.BuildIntegrityReport (was));
        AssertSucceeded (isVolume.BuildIntegrityReport (is));

        Assert::AreEqual (size_t (0), is.GetCrossLinked().size(),
            L"the edit must not leave a sector claimed by two entries");
        Assert::AreEqual (size_t (0), is.GetClaimedButFree().size(),
            L"the edit must not leave a referenced sector marked free");
        Assert::AreEqual (was.GetUnfollowableChains().size(), is.GetUnfollowableChains().size(),
            L"the edit must not break a chain that was whole");
        Assert::AreEqual (was.GetAllocatedButUnclaimed().size(), is.GetAllocatedButUnclaimed().size(),
            L"the edit must not leak space, nor reclaim space it did not own");
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

    TEST_METHOD (BuildIntegrityReport_AFreshlyFormattedVolume_IsClean)
    {
        // For a long time it was not, and the comment above the pass said
        // otherwise. Tracks 0-2 and track 17 are allocated by every formatter
        // and named by no catalog entry, so sixty-four sectors came back as
        // space allocated to nobody -- the exact shape a leaked delete leaves.
        // A healthy volume that reads as damaged makes damage unreportable.
        vector<Byte>           buffer = MakeFormattedVolume();
        Dos33Volume            volume (buffer);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        Assert::AreEqual (size_t (0), report.GetAllocatedButUnclaimed().size(),
            L"a blank volume's own tracks are owned by the volume, not by nobody");
        Assert::AreEqual (size_t (0), report.GetClaimedButFree().size());
        Assert::AreEqual (size_t (0), report.GetCrossLinked().size());
        Assert::IsTrue (report.IsClean(), L"a freshly formatted volume is consistent");
    }

    TEST_METHOD (BuildIntegrityReport_TheBootAndCatalogTracks_AreCreditedNotMerelyIgnored)
    {
        // Attributed, not skipped. Skipping them would also empty the
        // allocated-but-unclaimed set, and would then let a file's chain reach
        // into DOS or the catalog without the pass calling it a cross-link.
        constexpr uint32_t  kReservedUnits = 4 * 16;   // tracks 0, 1, 2 and 17

        vector<Byte>           buffer = MakeFormattedVolume();
        Dos33Volume            volume (buffer);
        VolumeIntegrityReport  report;
        uint32_t               unit     = 0;
        uint32_t               credited = 0;

        AssertSucceeded (volume.BuildIntegrityReport (report));

        for (unit = 0; unit < Dos33Volume::kUnitCount; unit++)
        {
            const vector<uint16_t> &  claimants = report.GetClaimantsOf (unit);

            if (claimants.size() == 1 && claimants[0] == VolumeIntegrityReport::kVolumeOwner)
            {
                credited++;
            }
        }

        Assert::AreEqual (kReservedUnits, credited,
            L"three tracks of DOS and the catalog track, and nothing else");

        Assert::AreEqual (size_t (1), report.GetClaimantsOf (0).size(),
            L"the boot sector is claimed");
        Assert::IsTrue (report.GetClaimantsOf (0)[0] == VolumeIntegrityReport::kVolumeOwner,
            L"and claimed by the volume rather than by a file");
        Assert::AreEqual (size_t (1), report.GetClaimantsOf (17 * 16).size(),
            L"and so is the VTOC");
        Assert::AreEqual (size_t (1), report.GetClaimantsOf (17 * 16 + 15).size(),
            L"and the first catalog sector");
        Assert::AreEqual (size_t (0), report.GetClaimantsOf (18 * 16).size(),
            L"a track holding no volume structure is not credited to anyone");
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

    TEST_METHOD (Write_BinaryOntoAFreshVolume_ReadsBackByteForByte)
    {
        vector<Byte>   buffer  = MakeFormattedVolume();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (512, 0x6000);
        FilePayload    back;
        VolumeListing  listing;
        size_t         i       = 0;

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("PROG"), listing.entries[0].name);
        Assert::AreEqual (Dos33Volume::kTypeBinary, listing.entries[0].type);
        Assert::IsFalse (listing.entries[0].isLocked);

        // 512 payload bytes plus the four-byte header DOS stores inside the
        // file is 516, which needs three data sectors; the track/sector list is
        // the fourth. A CATALOG of this disk reads B 004, not B 002.
        Assert::AreEqual (uint32_t (4), listing.entries[0].sizeUnits);
        Assert::AreEqual (kFreeOnBlankVolume - 4, listing.freeUnits,
            L"exactly the sectors the file occupies must leave the free pool");

        AssertSucceeded (written.Read (FilePath::Parse ("PROG"), back));

        Assert::AreEqual (size_t (512), back.bytes.size());
        Assert::IsTrue (back.hasLoadAddress);
        Assert::AreEqual (Word (0x6000), back.loadAddress);

        for (i = 0; i < back.bytes.size(); i++)
        {
            if (back.bytes[i] != payload.bytes[i])
            {
                Assert::Fail (L"the bytes read back must be the bytes placed");
            }
        }

        AssertEditLeftTheVolumeConsistent (buffer, result);
    }

    TEST_METHOD (Write_ProducesNothingWhenItRefuses_AndNeverTouchesTheInput)
    {
        // The volume holds its buffer as an immutable input, which is what makes
        // all-or-nothing structural rather than remembered. A refusal must leave
        // both the caller's image and the output untouched.
        vector<Byte>   buffer   = MakeVolumeWithHello();
        vector<Byte>   original = buffer;
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        FilePayload    payload  = MakeBinaryPayload (16, 0x0300);
        HRESULT        hr       = S_OK;
        size_t         i        = 0;

        hr = volume.Write (FilePath::Parse ("1BADNAME"), payload, result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME), hr);
        Assert::AreEqual (size_t (0), result.size(), L"a refused write produces nothing");

        for (i = 0; i < buffer.size(); i++)
        {
            if (buffer[i] != original[i])
            {
                Assert::Fail (L"a refused write must not have touched the source image");
            }
        }
    }

    TEST_METHOD (Write_OverAnExistingName_ReplacesItRatherThanRefusing)
    {
        // The interim refusal this supersedes. What must never appear is a
        // second catalog entry of the same name, so the assertion is on the
        // count as much as on the contents.
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (512, 0x6000);
        FilePayload    back;
        VolumeListing  listing;

        AssertSucceeded (volume.Write (FilePath::Parse ("HELLO"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size(),
            L"one entry of the name, never two");
        Assert::AreEqual (string ("HELLO"), listing.entries[0].name);
        Assert::AreEqual (Dos33Volume::kTypeBinary, listing.entries[0].type,
            L"the replacement's type, not the type of what it replaced");
        Assert::AreEqual (uint32_t (4), listing.entries[0].sizeUnits);

        // Two sectors came back before four went out. Had the old file's space
        // merely been abandoned this would read kFreeOnBlankVolume - 6.
        Assert::AreEqual (kFreeOnBlankVolume - 4, listing.freeUnits,
            L"the replaced file's sectors return to the pool");

        AssertSucceeded (written.Read (FilePath::Parse ("HELLO"), back));

        Assert::IsTrue (payload.bytes == back.bytes,
            L"the bytes read back must be the bytes placed");

        AssertEditLeftTheVolumeConsistent (buffer, result);
    }

    TEST_METHOD (Write_AReplacementThatCannotBeCompleted_LeavesTheOriginalWhereItWas)
    {
        // The failure replace exists to prevent. A removal APPLIED to the image
        // followed by a placement that fails loses the file outright: the entry
        // is gone, its sectors are back in the pool, and nothing was written in
        // its place. The whole computation has to be discardable, and it is only
        // because the volume never writes through to the buffer it was handed.
        vector<Byte>   buffer    = MakeFormattedVolume();
        vector<Byte>   withProg;
        vector<Byte>   original;
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    loaded (withProg);
        Dos33Volume    after (withProg);
        FilePayload    placed    = MakeBinaryPayload (16, 0x0300);
        FilePayload    oversized = MakeBinaryPayload (60'000, 0x0300);
        FilePayload    back;
        HRESULT        hr        = S_OK;
        int            track     = 0;
        int            sector    = 0;

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), placed, withProg));

        // Every sector spoken for, so the replacement cannot fit even once the
        // original's two come back.
        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
            {
                MarkAllocated (withProg, track, sector);
            }
        }

        original = withProg;

        hr = loaded.Write (FilePath::Parse ("PROG"), oversized, result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), hr);
        Assert::AreEqual (size_t (0), result.size(),
            L"a refused replacement must not hand back the half-done removal");
        Assert::IsTrue (withProg == original,
            L"nor touch the image it was computed from");

        AssertSucceeded (after.Read (FilePath::Parse ("PROG"), back));

        Assert::IsTrue (placed.bytes == back.bytes,
            L"the file it would have replaced must still be readable, byte for byte");
    }

    TEST_METHOD (Write_OverALockedFile_IsRefusedForBeingLockedNotForExisting)
    {
        // The two refusals differ in what the user must do about them, so they
        // must not collapse into one message.
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        FilePayload    payload = MakeBinaryPayload (16, 0x0300);
        size_t         entryAt = EntryOffset (kCatalogFirstSector, 0);
        HRESULT        hr      = S_OK;

        buffer[entryAt + 0x02] = (Byte) (buffer[entryAt + 0x02] | 0x80);

        hr = volume.Write (FilePath::Parse ("HELLO"), payload, result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }

    TEST_METHOD (Write_ToAVolumeWithNoRoom_IsRefusedNamingDiskFull)
    {
        vector<Byte>   buffer  = MakeFormattedVolume();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        FilePayload    payload = MakeBinaryPayload (16, 0x0300);
        int            track   = 0;
        int            sector  = 0;
        HRESULT        hr      = S_OK;

        for (track = 0; track < NibblizationLayer::kTrackCount; track++)
        {
            for (sector = 0; sector < NibblizationLayer::kSectorsPerTrack; sector++)
            {
                MarkAllocated (buffer, track, sector);
            }
        }

        hr = volume.Write (FilePath::Parse ("PROG"), payload, result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), hr);
        Assert::AreEqual (size_t (0), result.size(),
            L"a partial allocation must not reach the caller");
    }

    TEST_METHOD (Write_NamesTheGuestCouldNotOpenAgain_AreRefused)
    {
        // Validating a name being CREATED is not the printable-text check on
        // names being READ that was tried and reverted. Reading imposes no rule
        // -- twenty entries on Merlin's own disk are drawn with backspaces --
        // while a name DOS's own SAVE would reject is one nobody can open.
        vector<Byte>   buffer   = MakeFormattedVolume();
        Dos33Volume    volume (buffer);
        FilePayload    payload  = MakeBinaryPayload (16, 0x0300);
        vector<string> rejected;
        size_t         i        = 0;

        rejected.push_back ("");
        rejected.push_back ("1PROG");
        rejected.push_back (" PROG");
        rejected.push_back ("PROG,X");
        rejected.push_back ("PROGRAMPROGRAMPROGRAMPROGRAMPRO1");
        rejected.push_back ("SUB/PROG");

        Assert::IsTrue (rejected.size() > 0, L"the case list must not be empty");

        for (i = 0; i < rejected.size(); i++)
        {
            vector<Byte>  result;
            HRESULT       hr = volume.Write (FilePath::Parse (rejected[i]), payload, result);

            Assert::IsTrue (FAILED (hr), L"an unusable name must be refused");
            Assert::AreEqual (size_t (0), result.size());
        }
    }

    TEST_METHOD (Write_LowerCaseName_IsStoredTheWayTheGuestCanTypeIt)
    {
        vector<Byte>   buffer = MakeFormattedVolume();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (16, 0x0300);
        VolumeListing  listing;

        AssertSucceeded (volume.Write (FilePath::Parse ("prog"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (string ("PROG"), listing.entries[0].name,
            L"the catalog is upper case and so is the keyboard that reads it");
    }

    TEST_METHOD (Write_DoesNotReuseACatalogSlotHoldingAZeroSectorEntry)
    {
        // Twenty of the sixty-three entries on Merlin's own disk occupy no
        // sectors and exist to draw headings. A slot is free when its track byte
        // says never-used or deleted -- never because the entry claims nothing.
        vector<Byte>   buffer  = MakeFormattedVolume();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (16, 0x0300);
        VolumeListing  listing;
        size_t         slotAt  = EntryOffset (kCatalogFirstSector, 0);

        WriteDecorativeEntry (buffer, 0);

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (2), listing.entries.size(),
            L"the heading row must survive; the new file needs its own slot");
        Assert::AreEqual (uint32_t (0), listing.entries[0].sizeUnits,
            L"the first slot is still the entry that occupies nothing");
        Assert::AreEqual (string ("PROG"), listing.entries[1].name);
        Assert::AreEqual (Byte (0x7F), result[slotAt],
            L"the decoration's own bytes must be untouched");
    }

    TEST_METHOD (Write_NeverHandsOutASectorTheCatalogStillClaims)
    {
        // A free map calling a sector free while an entry still points at it is
        // exactly what a bad delete leaves behind. Handing that sector to the
        // next file destroys the first one, and nothing says so until someone
        // reads it. HELLO's data sector is marked free here and must be skipped.
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (16, 0x0300);
        FilePayload    hello;

        MarkFree (buffer, 18, 14);

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Read (FilePath::Parse ("HELLO"), hello));

        Assert::AreEqual (size_t (8), hello.bytes.size(),
            L"the existing file must still be the file it was");
        Assert::AreEqual (Byte (0xB2), hello.bytes[4], L"its REM token must be intact");
    }

    TEST_METHOD (Write_FileNeedingThreeTrackSectorLists_ChainsThemInOrder)
    {
        // One list holds 122 sectors, so 245 data sectors need three of them.
        // Three, not two: a next-pointer defect that reads its halves from
        // different sectors walks the FIRST hop correctly by coincidence, and
        // only the second hop tells a correct implementation from a broken one.
        constexpr size_t  kPayloadBytes  = 62700;   // 62704 stored -> 245 sectors
        constexpr size_t  kLastListStart = 244;

        vector<Byte>   buffer  = MakeFormattedVolume();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (kPayloadBytes, 0x0800);
        FilePayload    back;
        VolumeListing  listing;

        AssertSucceeded (volume.Write (FilePath::Parse ("BIG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (uint32_t (248), listing.entries[0].sizeUnits,
            L"245 data sectors plus the three lists that describe them");

        AssertSucceeded (written.Read (FilePath::Parse ("BIG"), back));

        Assert::AreEqual (kPayloadBytes, back.bytes.size(),
            L"the whole file must come back, not just the first list's worth");

        // Each byte names the stored sector it belongs to, so these three pin
        // content from the first, second and third lists respectively.
        Assert::AreEqual (Byte (121), back.bytes[121 * 256 - kDosBinHeaderBytes],
            L"the last sector of the first list");
        Assert::AreEqual (Byte (130), back.bytes[130 * 256 - kDosBinHeaderBytes],
            L"content past the first list");
        Assert::AreEqual (Byte ((Byte) kLastListStart), back.bytes[kLastListStart * 256 - kDosBinHeaderBytes],
            L"content past the SECOND list is what a two-list file cannot prove");

        AssertEditLeftTheVolumeConsistent (buffer, result);
    }

    TEST_METHOD (Delete_AFileWithNoComplications_ReturnsExactlyItsSectors)
    {
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        DeleteOutcome  outcome;
        VolumeListing  listing;
        size_t         entryAt = EntryOffset (kCatalogFirstSector, 0);

        AssertSucceeded (volume.Delete (FilePath::Parse ("HELLO"), result, outcome));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (0), listing.entries.size(), L"the entry must be gone");
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits,
            L"both of its sectors return to the pool");
        Assert::AreEqual (size_t (2), outcome.freedUnits.size());
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (size_t (0), outcome.warnings.size(), L"a clean delete warns about nothing");
        Assert::IsTrue (outcome.catalogFullyParsed);

        // DOS's own tombstone: $FF in the track byte, the original track stashed
        // in the last byte of the name so an undelete tool can find it.
        Assert::AreEqual (Byte (0xFF), result[entryAt + 0x00]);
        Assert::AreEqual (Byte (18), result[entryAt + 0x20]);

        AssertEditLeftTheVolumeConsistent (buffer, result);
    }

    TEST_METHOD (Delete_FreesOnlyWhatTheFileUniquelyOwns_AndReportsTheRestAsLeaked)
    {
        // The one failure in this feature that damages a DIFFERENT file, and
        // does it silently: freeing a shared sector succeeds, the volume looks
        // healthy, and the survivor is destroyed by the next allocation. Leaked
        // space is recoverable; a cross-linked free is not.
        vector<Byte>   buffer   = MakeVolumeWithCrossLinkedFiles();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        DeleteOutcome  outcome;
        VolumeListing  listing;
        FilePayload    survivor;
        uint32_t       freeBefore = FreeSectors (buffer);

        AssertSucceeded (volume.Delete (FilePath::Parse ("FILEA"), result, outcome));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (2), outcome.freedUnits.size(),
            L"only the list sector and the data sector nothing else claims");
        Assert::AreEqual (size_t (1), outcome.leakedUnits.size(),
            L"the shared sector is reported, not returned");
        Assert::AreEqual (uint32_t (kSharedTrack * 16 + kSharedSector), outcome.leakedUnits[0]);
        Assert::IsTrue (outcome.warnings.size() > 0, L"leaked space must be said out loud");

        Assert::AreEqual (freeBefore + 2, listing.freeUnits,
            L"three sectors were claimed and exactly two may come back");

        AssertSucceeded (written.Read (FilePath::Parse ("FILEB"), survivor));

        Assert::AreEqual (size_t (508), survivor.bytes.size(),
            L"the other file must still be whole");
        Assert::AreEqual (Byte (0xBB), survivor.bytes[300],
            L"its bytes in the shared sector must still be its bytes");

        AssertEditLeftTheVolumeConsistent (buffer, result);
    }

    TEST_METHOD (Delete_AFileWhoseChainIsDamaged_StillRemovesIt)
    {
        // Refusing would strand the volume: the entry could never be removed and
        // its sectors never reclaimed, on precisely the degraded disks this
        // feature exists to serve.
        vector<Byte>   buffer  = MakeVolumeWithDamagedChainFile();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        DeleteOutcome  outcome;
        VolumeListing  listing;

        AssertSucceeded (volume.Delete (FilePath::Parse ("BROKEN"), result, outcome));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (0), listing.entries.size(), L"a bad file must not be permanent");
        Assert::IsTrue (outcome.chainWasDamaged, L"the damage must be reported, not hidden");
        Assert::AreEqual (size_t (2), outcome.freedUnits.size(),
            L"what the walk reached is freed; what it never saw is not touched");
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits);
        Assert::IsTrue (outcome.warnings.size() > 0);
    }

    TEST_METHOD (Delete_FromAVolumeWhoseCatalogDidNotFullyParse_WarnsDistinctly)
    {
        // The one case the unique-ownership rule can still lose data: an entry
        // that could not be read claims nothing observable, so a sector it
        // shares looks unowned. The system must not claim otherwise.
        vector<Byte>   buffer    = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        DeleteOutcome  outcome;
        size_t         catalogAt = Dos33Skeleton::SectorOffset (17, kCatalogFirstSector);
        size_t         i         = 0;
        bool           saidSo    = false;

        buffer[catalogAt + 0x01] = kTrackOffTheVolume;

        AssertSucceeded (volume.Delete (FilePath::Parse ("HELLO"), result, outcome));

        Assert::IsFalse (outcome.catalogFullyParsed);
        Assert::IsTrue (outcome.warnings.size() > 0, L"an unbounded answer must say so");

        for (i = 0; i < outcome.warnings.size(); i++)
        {
            if (outcome.warnings[i].find ("catalog did not parse") != string::npos)
            {
                saidSo = true;
            }
        }

        Assert::IsTrue (saidSo, L"the catalog warning must be its own, not folded into another");
    }

    TEST_METHOD (Delete_AZeroSectorEntry_RemovesItAndFreesNothing)
    {
        // A decorative heading has no chain. Following its $7F/$7F pointer would
        // be inventing one; the discriminator is the entry's own sector count.
        vector<Byte>   buffer     = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        DeleteOutcome  outcome;
        VolumeListing  before;
        VolumeListing  after;

        WriteDecorativeEntry (buffer, 1);

        AssertSucceeded (volume.Enumerate (before));
        Assert::AreEqual (size_t (2), before.entries.size());

        AssertSucceeded (volume.Delete (FilePath::Parse (before.entries[1].name), result, outcome));
        AssertSucceeded (written.Enumerate (after));

        Assert::AreEqual (size_t (1), after.entries.size(), L"the decoration is gone");
        Assert::AreEqual (before.freeUnits, after.freeUnits,
            L"an entry that occupies nothing returns nothing");
        Assert::AreEqual (size_t (0), outcome.freedUnits.size());
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());

        // AND it is not damage. Following the $7F/$7F pointer would report a
        // chain that could not be walked, on an entry that never had one.
        Assert::IsFalse (outcome.chainWasDamaged);
        Assert::AreEqual (size_t (0), outcome.warnings.size());
    }

    TEST_METHOD (Delete_AZeroSectorEntryWhosePointerIsNotTheSentinel_StillFreesNothing)
    {
        // The discriminator is the entry's own sector count, NOT a $7F/$7F
        // pointer. Every decorative entry on Merlin's disk happens to carry that
        // sentinel, so a reader keyed on the pointer passes every test built
        // from those disks and is still wrong -- this entry claims no sectors
        // and points straight at another file's track/sector list.
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        DeleteOutcome  outcome;
        FilePayload    hello;
        VolumeListing  listing;
        size_t         entryAt = EntryOffset (kCatalogFirstSector, 1);

        buffer[entryAt + 0x00] = 18;   // HELLO's track/sector list
        buffer[entryAt + 0x01] = 15;
        buffer[entryAt + 0x02] = Dos33Volume::kTypeText;
        buffer[entryAt + 0x21] = 0;
        buffer[entryAt + 0x22] = 0;

        WriteEntryName (buffer, entryAt, "GHOST");

        AssertSucceeded (volume.Delete (FilePath::Parse ("GHOST"), result, outcome));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (0), outcome.freedUnits.size(),
            L"an entry declaring no sectors owns none, whatever its pointer says");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size(),
            L"it must not have walked another file's chain at all");
        Assert::AreEqual (size_t (0), outcome.warnings.size());

        AssertSucceeded (written.Read (FilePath::Parse ("HELLO"), hello));

        Assert::AreEqual (size_t (8), hello.bytes.size(), L"the real file is untouched");
    }

    TEST_METHOD (Write_IntoASectorAPreviousFileUsedAsAList_DoesNotInheritItsPairs)
    {
        // A track/sector list still holding the previous occupant's pairs is
        // read as THIS file's data, so the new file silently claims sectors
        // another file is using. The symptom is a cross-link, not a bad read,
        // which is why the assertion is on the integrity pass.
        vector<Byte>           buffer  = MakeVolumeWithCrossLinkedFiles();
        vector<Byte>           emptied;
        vector<Byte>           result;
        Dos33Volume            volume (buffer);
        Dos33Volume            afterDelete (emptied);
        Dos33Volume            written (result);
        FilePayload            payload = MakeBinaryPayload (16, 0x0300);
        VolumeIntegrityReport  report;

        AssertSucceeded (volume.Delete (FilePath::Parse ("FILEA"), emptied));
        AssertSucceeded (afterDelete.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.BuildIntegrityReport (report));

        Assert::AreEqual (size_t (0), report.GetCrossLinked().size(),
            L"the new file must claim only the sectors it was given");
    }

    TEST_METHOD (Delete_AMultiListFile_ReturnsItsLaterListSectorsToo)
    {
        // A file needing three track/sector lists occupies those three sectors
        // as surely as it occupies its data. An accounting that saw only the
        // head list left the other two allocated with nothing owning them --
        // space silently lost on every delete of a file over 31 KB, and not even
        // reported as leaked, because nothing knew they were the file's.
        vector<Byte>   buffer = MakeVolumeWithMultiListFile();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    written (result);
        DeleteOutcome  outcome;
        VolumeListing  listing;

        AssertSucceeded (volume.Delete (FilePath::Parse ("BIG"), result, outcome));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (kDataSectorCount + 3), outcome.freedUnits.size(),
            L"250 data sectors and all THREE lists");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size());
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits,
            L"the volume must be as empty as it started");

        AssertEditLeftTheVolumeConsistent (buffer, result);
    }

    TEST_METHOD (Delete_ALockedFile_IsRefused)
    {
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        size_t         entryAt = EntryOffset (kCatalogFirstSector, 0);
        HRESULT        hr      = S_OK;

        buffer[entryAt + 0x02] = (Byte) (buffer[entryAt + 0x02] | 0x80);

        hr = volume.Delete (FilePath::Parse ("HELLO"), result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (size_t (0), result.size());
    }

    TEST_METHOD (Delete_AMissingFile_IsNotFound)
    {
        vector<Byte>   buffer = MakeVolumeWithHello();
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        HRESULT        hr     = volume.Delete (FilePath::Parse ("NOPE"), result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), hr);
        Assert::AreEqual (size_t (0), result.size());
    }

    TEST_METHOD (Delete_ThenWrite_HandsTheReturnedSectorsBackOut)
    {
        // What "returning space to the volume" has to mean in the end.
        vector<Byte>   buffer  = MakeVolumeWithHello();
        vector<Byte>   emptied;
        vector<Byte>   result;
        Dos33Volume    volume (buffer);
        Dos33Volume    afterDelete (emptied);
        Dos33Volume    written (result);
        FilePayload    payload = MakeBinaryPayload (16, 0x0300);
        VolumeListing  listing;

        AssertSucceeded (volume.Delete (FilePath::Parse ("HELLO"), emptied));
        AssertSucceeded (afterDelete.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.Enumerate (listing));

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::AreEqual (kFreeOnBlankVolume - 2, listing.freeUnits,
            L"the new file fits in exactly the space the old one gave back");

        AssertEditLeftTheVolumeConsistent (emptied, result);
    }

    TEST_METHOD (PreCommitCheck_AResultClaimingASectorTwice_IsRefusedAsOurOwnDefect)
    {
        // Correct code cannot produce a result that fails this, which is what
        // the check is for -- so the only way to exercise the refusal is to hand
        // it one on purpose.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input  = MakeFormattedVolume();
        vector<Byte>           result = MakeVolumeWithCrossLinkedFiles();
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        HRESULT                hr     = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr,
            L"a bad buffer from our own writer is a defect, not a verdict on input");
        Assert::AreEqual (size_t (0), handedBack.size(),
            L"and a refused buffer must not reach the caller at all");
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AResultWhoseFreeMapDisagreesWithItsCatalog_IsRefused)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input  = MakeVolumeWithHello();
        vector<Byte>           result = input;
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        HRESULT                hr     = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        // HELLO's data sector still referenced by its list, and handed back to
        // the pool -- the state that lets the next allocation destroy it.
        MarkFree (result, 18, 14);

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AResultBreakingAChainTheInputHadWhole_IsRefused)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input  = MakeVolumeWithHello();
        vector<Byte>           result = input;
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        size_t                 listAt = Dos33Skeleton::SectorOffset (18, 15);
        HRESULT                hr     = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        // HELLO's track/sector list chained onward to a track the disk lacks.
        result[listAt + 0x01] = kTrackOffTheVolume;

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AResultThatMovesACrossLinkRatherThanAddingOne_IsRefused)
    {
        // The case that separates comparing MEMBERSHIP from comparing counts.
        // One sector was shared before and one is shared after, so every set in
        // the report is the same SIZE it was -- and a different file's data is
        // now at risk. A check that counted would wave this through.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input  = MakeVolumeWithCrossLinkedFiles();
        vector<Byte>           result = input;
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        VolumeIntegrityReport  after;
        size_t                 listA  = Dos33Skeleton::SectorOffset (kSharedTrack, 15);
        HRESULT                hr     = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        // FILEA's second data sector moves off the sector it shared with FILEB
        // and onto the other one FILEB holds.
        result[listA + 0x0F] = 11;

        {
            Dos33Volume  moved (result);

            AssertSucceeded (moved.BuildIntegrityReport (after));
        }

        Assert::AreEqual (size_t (1), before.GetCrossLinked().size());
        Assert::AreEqual (size_t (1), after.GetCrossLinked().size(),
            L"the sizes must match, or this proves nothing about membership");
        Assert::AreEqual (before.GetAllocatedButUnclaimed().size(),
                          after.GetAllocatedButUnclaimed().size());
        Assert::AreEqual (before.GetClaimedButFree().size(), after.GetClaimedButFree().size());

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AResultThatMovesADisagreementRatherThanAddingOne_IsRefused)
    {
        // The same argument for the other dangerous set. One referenced sector
        // was marked free before and one is marked free after, so the counts
        // match and a different sector is now the one the next allocation can
        // destroy.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input  = MakeVolumeWithHello();
        vector<Byte>           result;
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        VolumeIntegrityReport  after;
        HRESULT                hr     = S_OK;

        MarkFree (input, 18, 14);   // HELLO's data sector

        result = input;

        MarkAllocated (result, 18, 14);
        MarkFree      (result, 18, 15);   // its track/sector list instead

        AssertSucceeded (volume.BuildIntegrityReport (before));

        {
            Dos33Volume  moved (result);

            AssertSucceeded (moved.BuildIntegrityReport (after));
        }

        Assert::AreEqual (size_t (1), before.GetClaimedButFree().size());
        Assert::AreEqual (size_t (1), after.GetClaimedButFree().size(),
            L"the sizes must match, or this proves nothing about membership");
        Assert::AreEqual (before.GetAllocatedButUnclaimed().size(),
                          after.GetAllocatedButUnclaimed().size());
        Assert::AreEqual (before.GetCrossLinked().size(), after.GetCrossLinked().size());

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AResultThatAbandonsSpaceItStoppedClaiming_IsRefused)
    {
        // An edit that removed an entry and forgot to hand its sectors back.
        // Nothing is cross-linked and nothing referenced is marked free; the
        // only symptom is space allocated with nothing claiming it.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input   = MakeVolumeWithHello();
        vector<Byte>           result  = input;
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        size_t                 entryAt = EntryOffset (kCatalogFirstSector, 0);
        HRESULT                hr      = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        result[entryAt + 0x00] = 0xFF;   // tombstoned, bitmap untouched

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AResultWhoseCatalogStopsParsing_IsRefused)
    {
        // The bound on every other answer. An entry the pass cannot read claims
        // nothing observable, so a result that lost half its catalog looks
        // tidier than the input rather than worse -- which is exactly why this
        // is asked separately rather than inferred from the sets.
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>           input     = MakeVolumeWithHello();
        vector<Byte>           result    = input;
        vector<Byte>           handedBack;
        Dos33Volume            volume (input);
        VolumeIntegrityReport  before;
        size_t                 catalogAt = Dos33Skeleton::SectorOffset (17, kCatalogFirstSector);
        HRESULT                hr        = S_OK;

        AssertSucceeded (volume.BuildIntegrityReport (before));

        Assert::IsTrue (before.IsCatalogFullyParsed());

        result[catalogAt + 0x01] = kTrackOffTheVolume;

        hr = Dos33Volume::HandBackVerifiedResult (before, result, handedBack);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::AreEqual (size_t (0), handedBack.size());
        expect.RequireCount (1);
    }

    TEST_METHOD (PreCommitCheck_AnEditOnAVolumeThatAlreadyDisagrees_IsNotRefused)
    {
        // The reason this is not a whole-volume IsClean gate. Here a catalog
        // entry references a sector the free map calls free, before anything is
        // edited -- a real state, left by an earlier bad delete, and the one the
        // allocator is careful to work around. A check that demanded a clean
        // result would make such a disk permanently uneditable, which is the
        // opposite of what a recovery tool owes its user.
        vector<Byte>           buffer  = MakeVolumeWithHello();
        vector<Byte>           result;
        vector<Byte>           handedBack;
        Dos33Volume            volume (buffer);
        Dos33Volume            written (result);
        FilePayload            payload = MakeBinaryPayload (16, 0x0300);
        VolumeIntegrityReport  before;
        VolumeIntegrityReport  after;

        MarkFree (buffer, 18, 14);

        AssertSucceeded (volume.BuildIntegrityReport (before));

        Assert::IsFalse (before.IsClean(),
            L"the fixture must actually carry the disagreement this is about");
        Assert::AreEqual (size_t (1), before.GetClaimedButFree().size());

        AssertSucceeded (volume.Write (FilePath::Parse ("PROG"), payload, result));
        AssertSucceeded (written.BuildIntegrityReport (after));

        Assert::IsFalse (after.IsClean(),
            L"the pre-existing disagreement is still there, and was not the write's to fix");
        AssertSucceeded (Dos33Volume::HandBackVerifiedResult (before, result, handedBack));

        Assert::IsTrue (handedBack == result, L"an accepted buffer is handed over unchanged");
    }

    TEST_METHOD (Write_OntoARealDisk_LeavesEveryDecorativeEntryWhereItWas)
    {
        // Merlin's own DOS 3.3 disk, unmodified 1984 material with twenty
        // zero-sector heading rows among sixty-three entries. A slot-finder that
        // judged occupancy by sector count would overwrite one of them, and the
        // user would see a file where a heading used to be.
        FixtureProvider  fixtures;
        vector<Byte>     disk;
        vector<Byte>     result;
        Dos33Volume      volume (disk);
        Dos33Volume      written (result);
        FilePayload      payload      = MakeBinaryPayload (16, 0x0300);
        VolumeListing    before;
        VolumeListing    after;
        size_t           zeroesBefore = 0;
        size_t           zeroesAfter  = 0;
        size_t           i            = 0;

        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", disk));

        AssertSucceeded (volume.Enumerate (before));
        AssertSucceeded (volume.Write (FilePath::Parse ("CASSOTEST"), payload, result));
        AssertSucceeded (written.Enumerate (after));

        for (i = 0; i < before.entries.size(); i++)
        {
            zeroesBefore += (before.entries[i].sizeUnits == 0) ? 1 : 0;
        }

        for (i = 0; i < after.entries.size(); i++)
        {
            zeroesAfter += (after.entries[i].sizeUnits == 0) ? 1 : 0;
        }

        Assert::IsTrue (zeroesBefore > 0, L"this disk must carry the entries the case is about");
        Assert::AreEqual (zeroesBefore, zeroesAfter, L"no heading row may be consumed as free space");
        Assert::AreEqual (before.entries.size() + 1, after.entries.size());
        Assert::AreEqual (before.freeUnits - 2, after.freeUnits);
    }

    TEST_METHOD (Delete_OnARealDisk_ReturnsExactlyWhatItSaysItReturned)
    {
        FixtureProvider  fixtures;
        vector<Byte>     disk;
        vector<Byte>     result;
        Dos33Volume      volume (disk);
        Dos33Volume      written (result);
        DeleteOutcome    outcome;
        VolumeListing    before;
        VolumeListing    after;
        size_t           chosen   = 0;
        size_t           i        = 0;
        bool             haveOne  = false;

        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", disk));

        AssertSucceeded (volume.Enumerate (before));

        for (i = 0; i < before.entries.size() && !haveOne; i++)
        {
            if (before.entries[i].sizeUnits > 0 && !before.entries[i].isLocked)
            {
                chosen  = i;
                haveOne = true;
            }
        }

        Assert::IsTrue (haveOne,
            L"the disk must hold an unlocked file for this to be testing anything");

        AssertSucceeded (volume.Delete (FilePath::Parse (before.entries[chosen].name),
                                        result, outcome));
        AssertSucceeded (written.Enumerate (after));

        Assert::AreEqual (before.entries.size() - 1, after.entries.size());
        Assert::AreEqual (before.freeUnits + (uint32_t) outcome.freedUnits.size(), after.freeUnits,
            L"the free map must move by exactly what the outcome claims");
        Assert::AreEqual (size_t (0), outcome.leakedUnits.size(),
            L"nothing on this disk is cross-linked, so nothing should be left behind");
        Assert::IsTrue (outcome.catalogFullyParsed);
    }
};
