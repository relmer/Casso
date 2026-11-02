#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/AppleFileProperties.h"
#include "CassoExplorer/Model/BasicConversion.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "CassoExplorer/Model/FilesystemChoices.h"
#include "CassoExplorer/Model/CrossVolumeCopy.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/LinkedDisk.h"
#include "CommandLineParser.h"
#include "Core/MemoryBus.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImageSession.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/Dos33Volume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CpmVolumeTests
//
//  A CP/M volume built in memory. The sector layout is a table here rather than
//  the code's calculation. The table was measured: on a real CP/M disk, text in a
//  file continues from each sector to the next in this order.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CpmVolumeTests)
{
public:

    static constexpr size_t  kImageBytes  = 143360;
    static constexpr size_t  kSectorBytes = 256;
    static constexpr size_t  kEntryBytes  = 32;
    static constexpr size_t  kBlockBytes  = 1024;
    static constexpr const char *  kImage = "C:\\disks\\cpm.dsk";

    //  CP/M's sector 0 to 15 in a DOS-ordered image.
    static constexpr int  kSkew[16] = { 0, 6, 12, 3, 9, 15, 14, 5, 11, 2, 8, 7, 13, 4, 10, 1 };

    static size_t  GetOffset (int block, size_t offset)
    {
        int  track  = (3 + block / 4) % 35;
        int  sector = 4 * (block % 4) + (int) (offset / kSectorBytes);

        return ((size_t) track * 16 + (size_t) kSkew[sector]) * kSectorBytes + offset % kSectorBytes;
    }

    static std::vector<Byte>  BuildBlank()
    {
        return std::vector<Byte> (kImageBytes, CpmVolume::kFree);
    }

    //  A disk detection doesn't find, for what needs the manual choice: a free
    //  directory but for one slot that isn't a valid entry.
    static std::vector<Byte>  BuildDamaged()
    {
        std::vector<Byte>  image = BuildBlank();

        image[GetOffset (1, 63 * kEntryBytes - kBlockBytes)] = 0x7F;

        return image;
    }

    static void  PutEntry (std::vector<Byte> & image, int slot, int user, const char * name11, int extent, int records, std::initializer_list<int> blocks, bool readOnly = false)
    {
        size_t  at = (size_t) slot * kEntryBytes;
        size_t  i  = 0;

        for (i = 0; i < kEntryBytes; i++)
        {
            image[GetOffset ((int) ((at + i) / kBlockBytes), (at + i) % kBlockBytes)] = 0;
        }

        image[GetOffset (0, at)] = (Byte) user;

        for (i = 0; i < 11; i++)
        {
            image[GetOffset (0, at + 1 + i)] = (Byte) name11[i];
        }

        if (readOnly)
        {
            image[GetOffset (0, at + 9)] |= 0x80;
        }

        image[GetOffset (0, at + 12)] = (Byte) extent;
        image[GetOffset (0, at + 15)] = (Byte) records;

        i = 0;

        for (int block : blocks)
        {
            image[GetOffset (0, at + 16 + i)] = (Byte) block;
            i++;
        }
    }

    static void  FillBlock (std::vector<Byte> & image, int block, Byte seed)
    {
        for (size_t i = 0; i < kBlockBytes; i++)
        {
            image[GetOffset (block, i)] = (Byte) (seed + i);
        }
    }

    //  A program, a text file, a file with two extents, and one in user 2.
    static std::vector<Byte>  BuildSample()
    {
        std::vector<Byte>  image = BuildBlank();
        std::string        text  = "DIR\r\nSTAT\r\n\x1A";

        PutEntry (image, 0, 0, "TEST    COM", 0, 16, { 2, 3 });
        PutEntry (image, 1, 0, "NOTES   TXT", 0, 1,  { 4 });
        PutEntry (image, 2, 0, "BIG     DAT", 0, 128, { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 });
        PutEntry (image, 3, 0, "BIG     DAT", 1, 16, { 21, 22 });
        PutEntry (image, 4, 2, "OTHER      ", 0, 8, { 23 }, true);

        FillBlock (image, 2, 1);
        FillBlock (image, 3, 2);
        FillBlock (image, 5, 3);
        FillBlock (image, 21, 4);
        FillBlock (image, 23, 5);

        for (size_t i = 0; i < text.size(); i++)
        {
            image[GetOffset (4, i)] = (Byte) text[i];
        }

        return image;
    }

    static FilePayload  MakePayload (size_t size, Byte seed)
    {
        FilePayload  payload;

        payload.bytes.resize (size);

        for (size_t i = 0; i < size; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 3);
        }

        return payload;
    }

    static std::vector<std::string>  GetNames (const std::vector<Byte> & image)
    {
        VolumeListing             listing;
        std::vector<std::string>  names;

        AssertSucceeded (CpmVolume (image).Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }



    TEST_METHOD (SkewMatchesTheMeasuredTable)
    {
        for (int sector = 0; sector < 16; sector++)
        {
            Assert::AreEqual (GetOffset (sector / 4, (size_t) (sector % 4) * kSectorBytes), CpmVolume::GetSectorOffset (sector / 4, sector % 4));
        }

        //  Track 3 CP/M sector 1 is DOS sector 6, and block 128 wraps to track 0.
        Assert::AreEqual ((size_t) (3 * 16 + 6) * kSectorBytes, CpmVolume::GetSectorOffset (0, 1));
        Assert::AreEqual ((size_t) 0, CpmVolume::GetSectorOffset (128, 0));
        Assert::AreEqual ((size_t) 8, CpmVolume::GetDirectorySectors().size());
    }



    //  Detection.

    TEST_METHOD (DetectsADirectoryOfFiles)
    {
        std::vector<Byte>  image = BuildSample();

        Assert::IsTrue (CpmVolume::IsFound (image));
        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Cpm);
    }



    TEST_METHOD (ABlankDirectoryIsAnEmptyCpmDisk)
    {
        std::vector<Byte>  image = BuildBlank();

        Assert::IsTrue (CpmVolume::IsFound (image));
        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Cpm);
    }



    TEST_METHOD (AOneFileDirectoryIsDetectedWhenNothingElseIsInvalid)
    {
        std::vector<Byte>  image = BuildBlank();

        PutEntry (image, 0, 0, "ONE     COM", 0, 8, { 2 });

        Assert::IsTrue (CpmVolume::IsFound (image));
    }



    TEST_METHOD (LeftoverDirectoryDataNeedsSeveralEntries)
    {
        //  As one real disk has: half the directory is old program bytes.
        std::vector<Byte>  image = BuildBlank();
        size_t             at    = (size_t) 8 * kEntryBytes;

        PutEntry (image, 0, 0, "ONE     COM", 0, 8, { 2 });

        for (size_t i = 0; i < kEntryBytes; i++)
        {
            image[GetOffset (0, at + i)] = (Byte) (0x30 + i * 7);
        }

        Assert::IsFalse (CpmVolume::IsFound (image), L"One entry beside leftover data proves nothing");

        PutEntry (image, 1, 0, "TWO     COM", 0, 8, { 3 });
        PutEntry (image, 2, 0, "THREE   COM", 0, 8, { 4 });

        Assert::IsTrue (CpmVolume::IsFound (image));

        {
            VolumeListing  listing;

            AssertSucceeded (CpmVolume (image).Enumerate (listing));

            Assert::AreEqual ((size_t) 3, listing.entries.size());
            Assert::AreEqual ((size_t) 1, listing.damage.size(), L"The bad entry is reported");
        }
    }



    TEST_METHOD (DataThatIsNotADirectoryIsNotDetected)
    {
        std::vector<Byte>  zeros    (kImageBytes, 0);
        std::vector<Byte>  text     (kImageBytes, 'A');
        std::vector<Byte>  counting (kImageBytes, 0);

        for (size_t i = 0; i < kImageBytes; i++)
        {
            counting[i] = (Byte) (i * 13 + (i >> 8));
        }

        Assert::IsFalse (CpmVolume::IsFound (zeros));
        Assert::IsFalse (CpmVolume::IsFound (text));
        Assert::IsFalse (CpmVolume::IsFound (counting));
    }



    TEST_METHOD (EachBadFieldMakesAnEntryInvalid)
    {
        struct Case { size_t offset; Byte value; };

        for (Case bad : { Case { 0, 32 }, Case { 1, 0x07 }, Case { 12, 32 }, Case { 14, 1 }, Case { 15, 129 }, Case { 16, 200 }, Case { 16, 1 } })
        {
            std::vector<Byte>  image = BuildBlank();

            PutEntry (image, 0, 0, "ONE     COM", 0, 8, { 2 });

            image[GetOffset (0, bad.offset)] = bad.value;

            Assert::IsFalse (CpmVolume::IsFound (image));
        }
    }



    TEST_METHOD (OtherFileSystemsWinOverCpm)
    {
        std::vector<Byte>  dos (kImageBytes, 0);
        std::vector<Byte>  pro (kImageBytes, 0);

        AssertSucceeded (Dos33Skeleton::Write (dos, 254));
        AssertSucceeded (ProDosSkeleton::Write (pro, "NOTCPM"));

        Assert::IsTrue (VolumeImage::DetectFilesystem (dos) == VolumeKind::Dos33);
        Assert::IsTrue (VolumeImage::DetectFilesystem (pro) == VolumeKind::ProDos);
    }



    //  Reading.

    TEST_METHOD (ListsFilesBySizeInRecords)
    {
        std::vector<Byte>  image = BuildSample();
        VolumeListing      listing;

        AssertSucceeded (CpmVolume (image).Enumerate (listing));

        Assert::IsFalse (listing.hasVolumeName);
        Assert::IsTrue (listing.damage.empty());
        Assert::AreEqual ((uint32_t) 128, listing.totalUnits);
        Assert::AreEqual ((uint32_t) (128 - 2 - 22), listing.freeUnits);
        Assert::IsTrue (GetNames (image) == std::vector<std::string> { "TEST.COM", "NOTES.TXT", "BIG.DAT", "2:OTHER" });

        Assert::AreEqual ((uint32_t) 2, listing.entries[0].sizeUnits);
        Assert::AreEqual ((uint32_t) (16 * 128), listing.entries[0].eofBytes);
        Assert::AreEqual ((uint32_t) 18, listing.entries[2].sizeUnits);
        Assert::AreEqual ((uint32_t) (128 + 16) * 128, listing.entries[2].eofBytes, L"Both extents");
        Assert::IsTrue (listing.entries[3].isLocked, L"The read-only attribute");
    }



    TEST_METHOD (ReadsAFileAcrossBlocksAndExtents)
    {
        std::vector<Byte>  image = BuildSample();
        FilePayload        payload;

        AssertSucceeded (CpmVolume (image).Read (FilePath::Parse ("big.dat"), payload));

        Assert::AreEqual ((size_t) (128 + 16) * 128, payload.bytes.size());
        Assert::AreEqual ((int) (Byte) (3 + 5), (int) payload.bytes[5]);
        Assert::AreEqual ((int) (Byte) (4 + 7), (int) payload.bytes[16 * kBlockBytes + 7]);
        Assert::IsTrue (payload.textForm == PayloadTextForm::CpmRecords);
    }



    TEST_METHOD (ReadsAHoleAsZeros)
    {
        std::vector<Byte>  image = BuildBlank();
        FilePayload        payload;

        PutEntry (image, 0, 0, "HOLE    DAT", 0, 24, { 0, 2, 3 });
        FillBlock (image, 2, 1);
        FillBlock (image, 3, 2);

        AssertSucceeded (CpmVolume (image).Read (FilePath::Parse ("HOLE.DAT"), payload));

        Assert::AreEqual ((int) 0, (int) payload.bytes[10]);
        Assert::AreEqual ((int) (Byte) (1 + 10), (int) payload.bytes[kBlockBytes + 10]);
    }



    TEST_METHOD (ASystemEntryReadsFromTheBootTracks)
    {
        std::vector<Byte>  image = BuildSample();
        FilePayload        payload;

        PutEntry (image, 5, 31, "cp/am   sys", 0, 8, { 128 });
        FillBlock (image, 128, 9);

        AssertSucceeded (CpmVolume (image).Read (FilePath::Parse ("31:cp/am.sys"), payload));

        Assert::AreEqual ((size_t) 1024, payload.bytes.size());
        Assert::AreEqual ((int) 9, (int) image[0]);
        Assert::AreEqual ((int) 9, (int) payload.bytes[0]);
    }



    TEST_METHOD (ReadOfAMissingFileFails)
    {
        std::vector<Byte>  image = BuildSample();
        FilePayload        payload;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), CpmVolume (image).Read (FilePath::Parse ("NOPE.COM"), payload));
    }



    //  Writing.

    TEST_METHOD (WritesAFileInFreeBlocksAndSlots)
    {
        std::vector<Byte>  image   = BuildSample();
        std::vector<Byte>  edited;
        FilePayload        read;
        FilePayload        payload = MakePayload (2500, 1);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("new.com"), payload, edited));

        AssertSucceeded (CpmVolume (edited).Read (FilePath::Parse ("NEW.COM"), read));

        Assert::AreEqual ((size_t) 20 * 128, read.bytes.size(), L"Whole records");
        Assert::IsTrue (std::equal (payload.bytes.begin(), payload.bytes.end(), read.bytes.begin()));
        Assert::AreEqual ((int) CpmVolume::kTextEnd, (int) read.bytes[2500], L"The last record is padded");

        //  The first free slot and the first free blocks.
        Assert::AreEqual ((int) 'N', (int) edited[GetOffset (0, 5 * kEntryBytes + 1)]);
        Assert::AreEqual ((int) 24, (int) edited[GetOffset (0, 5 * kEntryBytes + 16)]);
        Assert::AreEqual ((int) 26, (int) edited[GetOffset (0, 5 * kEntryBytes + 18)]);
        Assert::AreEqual ((int) 20, (int) edited[GetOffset (0, 5 * kEntryBytes + 15)]);
    }



    TEST_METHOD (WriteSplitsALargeFileIntoExtents)
    {
        std::vector<Byte>  image   = BuildBlank();
        std::vector<Byte>  edited;
        FilePayload        read;
        FilePayload        payload = MakePayload (40 * 1024, 2);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("LARGE.DAT"), payload, edited));
        AssertSucceeded (CpmVolume (edited).Read (FilePath::Parse ("LARGE.DAT"), read));

        Assert::IsTrue (read.bytes == payload.bytes);

        Assert::AreEqual ((int) 0,   (int) edited[GetOffset (0, 12)]);
        Assert::AreEqual ((int) 128, (int) edited[GetOffset (0, 15)]);
        Assert::AreEqual ((int) 1,   (int) edited[GetOffset (0, kEntryBytes + 12)]);
        Assert::AreEqual ((int) 128, (int) edited[GetOffset (0, kEntryBytes + 15)]);
        Assert::AreEqual ((int) 2,   (int) edited[GetOffset (0, 2 * kEntryBytes + 12)]);
        Assert::AreEqual ((int) 64,  (int) edited[GetOffset (0, 2 * kEntryBytes + 15)]);
    }



    TEST_METHOD (WriteOfNothingHasAnEntryAndNoBlocks)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;
        FilePayload        read;

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("EMPTY"), MakePayload (0, 0), edited));
        AssertSucceeded (CpmVolume (edited).Read (FilePath::Parse ("EMPTY"), read));

        Assert::IsTrue (read.bytes.empty());
    }



    TEST_METHOD (WriteMakesABlankDiskDetectable)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("A.COM"), MakePayload (10, 0), edited));

        Assert::IsTrue (VolumeImage::DetectFilesystem (edited) == VolumeKind::Cpm);
    }



    TEST_METHOD (WriteReplacesAFileAndReusesItsBlocks)
    {
        std::vector<Byte>  image   = BuildSample();
        std::vector<Byte>  edited;
        FilePayload        read;
        FilePayload        payload = MakePayload (1024, 7);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("TEST.COM"), payload, edited));

        Assert::AreEqual ((size_t) 4, GetNames (edited).size());

        AssertSucceeded (CpmVolume (edited).Read (FilePath::Parse ("TEST.COM"), read));
        Assert::IsTrue (read.bytes == payload.bytes);

        //  Blocks 2 and 3 were TEST.COM's and are free again, so it gets block 2.
        Assert::AreEqual ((int) 2, (int) edited[GetOffset (0, 16)]);
    }



    TEST_METHOD (WriteTheSameNameInAnotherUserIsAnotherFile)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        AssertSucceeded (CpmVolume (image).Write (FilePath::FromName ("3:TEST.COM"), MakePayload (10, 0), edited));

        Assert::IsTrue (GetNames (edited) == std::vector<std::string> { "TEST.COM", "NOTES.TXT", "BIG.DAT", "2:OTHER", "3:TEST.COM" });
    }



    TEST_METHOD (WriteRefusesAReadOnlyFile)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), CpmVolume (image).Write (FilePath::FromName ("2:OTHER"), MakePayload (10, 0), edited));
    }



    TEST_METHOD (WriteRefusesWhenBlocksRunOut)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), CpmVolume (image).Write (FilePath::Parse ("HUGE.DAT"), MakePayload (127 * kBlockBytes, 0), edited));
        Assert::IsTrue (edited.empty());

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("HUGE.DAT"), MakePayload (126 * kBlockBytes, 0), edited));
    }



    TEST_METHOD (WriteStopsAt48SlotsOnADiskWithNothingPastThem)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        for (int slot = 0; slot < CpmVolume::kCompatibleEntries; slot++)
        {
            PutEntry (image, slot, 0, std::format ("F{:<7}COM", slot).c_str(), 0, 0, {});
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), CpmVolume (image).Write (FilePath::Parse ("A.COM"), MakePayload (10, 0), edited));
    }

    TEST_METHOD (WriteUsesSlotsPast48WhenAValidEntryIsThere)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        for (int slot = 0; slot < CpmVolume::kCompatibleEntries; slot++)
        {
            PutEntry (image, slot, 0, std::format ("F{:<7}COM", slot).c_str(), 0, 0, {});
        }

        PutEntry (image, 50, 0, "HIGH    COM", 0, 0, {});

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("A.COM"), MakePayload (10, 0), edited));

        Assert::AreEqual ((int) 'A', (int) edited[GetOffset (1, 48 * kEntryBytes - kBlockBytes + 1)], L"The lowest free slot past 48");
    }

    TEST_METHOD (ReadsListEntriesInAll64Slots)
    {
        std::vector<Byte>  image = BuildBlank();
        VolumeListing      listing;

        PutEntry (image, 63, 0, "LAST    COM", 0, 0, {});

        AssertSucceeded (CpmVolume (image).Enumerate (listing));

        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::IsTrue (listing.damage.empty());
    }

    static void  PutGarbage (std::vector<Byte> & image, int slot, Byte first)
    {
        size_t  at = (size_t) slot * kEntryBytes;

        for (size_t i = 0; i < kEntryBytes; i++)
        {
            image[GetOffset ((int) ((at + i) / kBlockBytes), (at + i) % kBlockBytes)] = (i == 0) ? first : 0x01;
        }

        image[GetOffset ((int) (at / kBlockBytes), at % kBlockBytes + 16)] = 40;
    }

    TEST_METHOD (GarbagePast48IsNotDamageWithoutAValidEntryThere)
    {
        std::vector<Byte>  image = BuildSample();
        VolumeListing      listing;

        PutGarbage (image, 50, 0x09);

        AssertSucceeded (CpmVolume (image).Enumerate (listing));
        Assert::IsTrue (listing.damage.empty(), L"A 48-entry system never reads it");

        PutEntry (image, 51, 0, "HIGH    COM", 0, 0, {});

        AssertSucceeded (CpmVolume (image).Enumerate (listing));
        Assert::AreEqual ((size_t) 1, listing.damage.size(), L"A disk with an entry past 48 reads the garbage too");

        PutGarbage (image, 51, 0xAA);
        PutGarbage (image, 10, 0xAA);

        AssertSucceeded (CpmVolume (image).Enumerate (listing));
        Assert::AreEqual (std::string ("1 directory entry isn't a valid CP/M entry"), listing.damage[0], L"Below 48 reports as before");
    }

    TEST_METHOD (BlocksInAnInvalidEntryWithAUserNumberAreUsed)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;
        VolumeListing      listing;

        PutGarbage (image, 0, 0x09);
        PutGarbage (image, 1, 0xAA);

        //  Block 40 in the first, and block 50 in the second, whose first byte
        //  CP/M doesn't read as a user.
        image[GetOffset (0, kEntryBytes + 16)] = 50;

        AssertSucceeded (CpmVolume (image).Enumerate (listing));

        Assert::AreEqual ((uint32_t) 125, listing.freeUnits);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("BIG.COM"), MakePayload (39 * kBlockBytes, 0), edited));
        
        //  With 125 blocks free, one more than that doesn't fit.
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), CpmVolume (image).Write (FilePath::Parse ("FULL.COM"), MakePayload (126 * kBlockBytes, 0), edited));
    }

    TEST_METHOD (WritesNeverAllocateTheBootTracksAndTotalsCountWhatIsClaimedThere)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;
        VolumeListing      listing;

        PutEntry (image, 0, 31, "cp/am   sys", 0, 96, { 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139 });

        AssertSucceeded (CpmVolume (image).Enumerate (listing));

        Assert::AreEqual ((uint32_t) 140, listing.totalUnits);
        Assert::AreEqual ((uint32_t) 126, listing.freeUnits);

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("A.COM"), MakePayload (126 * kBlockBytes, 0), edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), CpmVolume (image).Write (FilePath::Parse ("A.COM"), MakePayload (127 * kBlockBytes, 0), edited));
    }

    TEST_METHOD (TheListingReportsDamageInThePreviewAndStatus)
    {
        std::vector<Byte>  image = BuildSample();
        VolumeListing      listing;
        PreviewContent     content;
        BrowserStatus      status;

        PutGarbage (image, 10, 0xAA);

        AssertSucceeded (CpmVolume (image).Enumerate (listing));

        PreviewDecoder::RenderCatalog (listing, VolumeKind::Cpm, content);
        Assert::AreEqual (std::wstring (L"1 directory entry isn't a valid CP/M entry"), content.warning);

        BrowserCells::DescribeVolumeSpace (L"C:\\disks\\cpm.dsk", listing, VolumeKind::Cpm, status);
        Assert::IsTrue (status.freeSpace.find (L"isn't a valid CP/M entry") != std::wstring::npos);
    }

    TEST_METHOD (WriteRefusesWhenSlotsRunOut)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        for (int slot = 0; slot < CpmVolume::kEntries; slot++)
        {
            PutEntry (image, slot, 0, std::format ("F{:<7}COM", slot).c_str(), 0, 0, {});
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), CpmVolume (image).Write (FilePath::Parse ("A.COM"), MakePayload (10, 0), edited));
    }



    TEST_METHOD (WriteNeverReusesAnInvalidSlot)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        PutEntry (image, 0, 0, "ONE     COM", 0, 8, { 2 });
        PutEntry (image, 1, 0, "TWO     COM", 0, 8, { 3 });
        PutEntry (image, 2, 0, "THREE   COM", 0, 8, { 4 });

        for (size_t i = 0; i < kEntryBytes; i++)
        {
            image[GetOffset (0, 3 * kEntryBytes + i)] = (Byte) (0x30 + i);
        }

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("NEW.COM"), MakePayload (10, 0), edited));

        Assert::AreEqual ((int) 0x30, (int) edited[GetOffset (0, 3 * kEntryBytes)], L"The bad entry stays");
        Assert::AreEqual ((int) 'N', (int) edited[GetOffset (0, 4 * kEntryBytes + 1)]);
    }



    TEST_METHOD (WriteRefusesAnInvalidName)
    {
        std::vector<Byte>  image = BuildBlank();
        std::vector<Byte>  edited;

        for (const char * name : { "", "NINECHARS.COM", "A.FOUR", "A.B.C", "A B", "A*", "A?", "A[", "A,B", "A;B", "A=B", "A<B", "16:A", ":A" })
        {
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME), CpmVolume (image).Write (FilePath::FromName (name), MakePayload (1, 0), edited));
        }
    }



    TEST_METHOD (WriteLeavesTheOriginalBufferAlone)
    {
        std::vector<Byte>  image  = BuildSample();
        std::vector<Byte>  before = image;
        std::vector<Byte>  edited;

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("NEW.COM"), MakePayload (100, 0), edited));

        Assert::IsTrue (image == before);
        Assert::IsFalse (edited == before);
    }



    TEST_METHOD (WriteRecordsALockedPayloadAsReadOnly)
    {
        std::vector<Byte>  image   = BuildBlank();
        std::vector<Byte>  edited;
        FilePayload        payload = MakePayload (10, 0);
        VolumeListing      listing;

        payload.hasAccess = true;
        payload.access    = FilePayload::kAccessLocked;

        AssertSucceeded (CpmVolume (image).Write (FilePath::Parse ("A.COM"), payload, edited));
        AssertSucceeded (CpmVolume (edited).Enumerate (listing));

        Assert::IsTrue (listing.entries[0].isLocked);
    }



    //  Deleting and renaming.

    TEST_METHOD (DeleteFreesEveryExtent)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        DeleteOutcome      outcome;
        VolumeListing      listing;

        AssertSucceeded (CpmVolume (image).Delete (FilePath::Parse ("BIG.DAT"), edited, outcome));

        Assert::AreEqual ((size_t) 18, outcome.freedUnits.size());
        Assert::IsTrue (GetNames (edited) == std::vector<std::string> { "TEST.COM", "NOTES.TXT", "2:OTHER" });
        Assert::AreEqual ((int) CpmVolume::kFree, (int) edited[GetOffset (0, 2 * kEntryBytes)]);
        Assert::AreEqual ((int) CpmVolume::kFree, (int) edited[GetOffset (0, 3 * kEntryBytes)]);

        AssertSucceeded (CpmVolume (edited).Enumerate (listing));
        Assert::AreEqual ((uint32_t) (128 - 2 - 4), listing.freeUnits);
    }



    TEST_METHOD (DeleteRefusesAReadOnlyOrSystemFile)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        PutEntry (image, 5, 31, "cp/am   sys", 0, 8, { 128 });

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), CpmVolume (image).Delete (FilePath::FromName ("2:OTHER"), edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), CpmVolume (image).Delete (FilePath::Parse ("31:cp/am.sys"), edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), CpmVolume (image).Delete (FilePath::Parse ("NOPE"), edited));
    }



    TEST_METHOD (RenameChangesEveryExtent)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        FilePayload        before;
        FilePayload        after;

        AssertSucceeded (CpmVolume (image).Read (FilePath::Parse ("BIG.DAT"), before));
        AssertSucceeded (CpmVolume (image).Rename (FilePath::Parse ("BIG.DAT"), "large.bin", edited));
        AssertSucceeded (CpmVolume (edited).Read (FilePath::Parse ("LARGE.BIN"), after));

        Assert::IsTrue (before.bytes == after.bytes);
    }



    TEST_METHOD (RenameCanMoveAFileToAnotherUser)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        AssertSucceeded (CpmVolume (image).Rename (FilePath::Parse ("NOTES.TXT"), "4:NOTES.TXT", edited));

        Assert::IsTrue (GetNames (edited) == std::vector<std::string> { "TEST.COM", "4:NOTES.TXT", "BIG.DAT", "2:OTHER" });
    }



    TEST_METHOD (RenameRefuses)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),    CpmVolume (image).Rename (FilePath::Parse ("BIG.DAT"), "TEST.COM", edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),   CpmVolume (image).Rename (FilePath::Parse ("BIG.DAT"), "A B", edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),  CpmVolume (image).Rename (FilePath::FromName ("2:OTHER"), "OK", edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), CpmVolume (image).Rename (FilePath::Parse ("NOPE"), "OK", edited));
    }



    TEST_METHOD (SetAttributesChangesReadOnlyOnEveryExtent)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        FileAttributes     attributes;
        VolumeListing      listing;

        attributes.hasAccess = true;
        attributes.access    = FilePayload::kAccessLocked;

        AssertSucceeded (CpmVolume (image).SetAttributes (FilePath::Parse ("BIG.DAT"), attributes, edited));

        Assert::IsTrue ((edited[GetOffset (0, 2 * kEntryBytes + 9)] & 0x80) != 0);
        Assert::IsTrue ((edited[GetOffset (0, 3 * kEntryBytes + 9)] & 0x80) != 0);

        attributes.access = FilePayload::kAccessUnlocked;

        AssertSucceeded (CpmVolume (edited).SetAttributes (FilePath::Parse ("BIG.DAT"), attributes, edited));
        AssertSucceeded (CpmVolume (edited).Enumerate (listing));

        Assert::IsFalse (listing.entries[2].isLocked);
    }



    TEST_METHOD (SetAttributesRefusesWhatCpmDoesNotStore)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        FileAttributes     type;
        FileAttributes     modified;

        type.hasType         = true;
        modified.hasModified = true;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), CpmVolume (image).SetAttributes (FilePath::Parse ("BIG.DAT"), type, edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), CpmVolume (image).SetAttributes (FilePath::Parse ("BIG.DAT"), modified, edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), CpmVolume (image).SetStartupProgram (FilePath::Parse ("BIG.DAT"), edited));
    }



    //  Text.

    TEST_METHOD (TextEndsAtTheFirstEndOfFileByte)
    {
        std::vector<Byte>  bytes = { 'H', 'I', '\r', '\n', 0x1A, 0x1A, 'X' };
        std::string        text;

        CpmVolume::DecodeText (bytes, text);

        Assert::AreEqual (std::string ("HI\r\n"), text);
    }



    TEST_METHOD (TextGetsCrLfLineEnds)
    {
        std::vector<Byte>  bytes;

        CpmVolume::EncodeText ("A\nB\r\nC\rD", bytes);

        Assert::AreEqual (std::string ("A\r\nB\r\nC\r\nD\x1A"), std::string (bytes.begin(), bytes.end()));
    }



    //  The explorer's model.

    TEST_METHOD (TheNameSuffixIsTheTypeColumn)
    {
        FileEntry   entry;
        CatalogRow  row;

        entry.name      = "TEST.COM";
        entry.sizeUnits = 3;

        row = CatalogModel::FromFileEntry (entry, VolumeKind::Cpm, 0);

        Assert::AreEqual (std::wstring (L"COM"), row.typeText);
        Assert::AreEqual ((uint64_t) 3 * 1024, row.sizeBytes);
    }



    TEST_METHOD (HostNamesKeepTheCpmName)
    {
        FileEntry  entry;
        bool       converted = false;

        entry.name = "TEST.COM";

        Assert::AreEqual (std::wstring (L"TEST.COM"), DragPayload::GetHostName (entry, VolumeKind::Cpm, HostFileNaming::Style::Descriptive, converted));
        Assert::IsFalse (converted);
        Assert::IsTrue (CassoExplorerActions::GetEncoding (entry, VolumeKind::Cpm) == DiskOperations::Encoding::Verbatim);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Cpm, 0) == BasicConversion::Kind::None, L"Type 0 would be DOS 3.3 text");
    }



    TEST_METHOD (DragEntriesRoundTripTheVolumeKind)
    {
        std::string               image;
        VolumeKind                kind = VolumeKind::Unknown;
        std::vector<std::string>  paths;

        Assert::IsTrue (DragPayload::DecodeCatalogEntries (DragPayload::EncodeCatalogEntries ("x.dsk", VolumeKind::Cpm, { "A.COM" }), image, kind, paths));
        Assert::IsTrue (kind == VolumeKind::Cpm);
    }



    TEST_METHOD (PlansKeepTheSuffixAndHaveNoAddress)
    {
        std::vector<Byte>      binary (64, 0x01);
        ActionsTypes::PutPlan  plan = CassoExplorerActions::PlanPut (L"C:\\host\\tool.com", binary, VolumeKind::Cpm);

        Assert::AreEqual (std::string ("TOOL.COM"), plan.catalogName);
        Assert::IsFalse (plan.hasLoadAddress);
        Assert::IsFalse (plan.guessedAddress);
        Assert::IsTrue (plan.typeName.empty());
    }



    TEST_METHOD (NamesAreCleanedToEightDotThree)
    {
        Assert::AreEqual (std::string ("LONGFILE.TEX"), CassoExplorerActions::SanitizeCatalogName (L"longfilename.text", VolumeKind::Cpm));
        Assert::AreEqual (std::string ("MYGAME"),       CassoExplorerActions::SanitizeCatalogName (L"MY GAME", VolumeKind::Cpm));
        Assert::AreEqual (std::string ("FILE"),         CassoExplorerActions::SanitizeCatalogName (L"***", VolumeKind::Cpm));
        Assert::AreEqual (std::string ("TEST2.COM"),    CassoExplorerActions::MakeFreeEntryName ("TEST.COM", { "TEST.COM" }, VolumeKind::Cpm, false));
        Assert::AreEqual (std::string ("LONGNAM2.COM"), CassoExplorerActions::MakeFreeEntryName ("LONGNAME.COM", { "LONGNAME.COM" }, VolumeKind::Cpm, true));
        Assert::AreEqual (std::string ("HELLO.BIN"),    CassoExplorerActions::FitEntryName ("HELLO.BIN", VolumeKind::ProDos, VolumeKind::Cpm));
    }



    TEST_METHOD (PropertiesHaveNoTypesAndCpmNames)
    {
        Assert::IsTrue (AppleFileProperties::GetTypeChoices (VolumeKind::Cpm, 0).empty());
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::Cpm, 0) == AppleFileProperties::AuxKind::None);
        Assert::IsTrue (AppleFileProperties::IsValidName (VolumeKind::Cpm, L"A.COM"));
        Assert::IsFalse (AppleFileProperties::IsValidName (VolumeKind::Cpm, L"NINECHARS.COM"));
    }



    TEST_METHOD (CopiesHaveNoTypeOnCpmAndABinaryOnDos)
    {
        FilePayload  payload;

        payload.type = ProDosVolume::kTypeBinary;
        HostFileNaming::ConvertPayloadType (VolumeKind::ProDos, VolumeKind::Cpm, payload);
        Assert::AreEqual ((int) 0, (int) payload.type);

        std::wstring  note;

        payload = FilePayload();
        payload.bytes = { 0xC3, 0x00, 0x01 };
        CrossVolumeCopy::Convert (VolumeKind::Cpm, VolumeKind::Dos33, "A.COM", payload, note);
        Assert::AreEqual ((int) Dos33Volume::kTypeSpecial, (int) payload.type, L"Not a type that claims to be runnable");
        Assert::IsFalse (payload.hasLoadAddress);

        payload = FilePayload();
        HostFileNaming::ConvertPayloadType (VolumeKind::Cpm, VolumeKind::ProDos, payload);
        Assert::AreEqual ((int) 0, (int) payload.type);
    }



    TEST_METHOD (TextPreviewsAsTextAndProgramsAsHex)
    {
        FileEntry       entry;
        FilePayload     text;
        FilePayload     program;
        PreviewContent  content;
        MemoryBus       bus;
        std::string     words = "DIR\r\nSTAT\r\n\x1A\x1A\x1A";

        text.bytes.assign (words.begin(), words.end());
        text.textForm = PayloadTextForm::CpmRecords;

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::Cpm, text, false, bus, content));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Text);
        Assert::AreEqual ((size_t) 2, content.lines.size());
        Assert::AreEqual (std::wstring (L"STAT"), content.lines[1]);

        program.bytes    = { 0xC3, 0x67, 0x09, 0xC9, 0x00 };
        program.textForm = PayloadTextForm::CpmRecords;

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::Cpm, program, false, bus, content));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Hex);
    }



    TEST_METHOD (AFullSendCountsCpmAsRecognizedAndHasNoTitle)
    {
        std::vector<Byte>  image = BuildSample();

        Assert::IsTrue (FullSendDisk::NeedsDirectoryTrack (image), L"Tracks 0 and 17 alone recognize nothing");
        Assert::IsFalse (FullSendDisk::IsCatalogOnTrackZero (image));
        Assert::AreEqual (std::string(), FullSendDisk::GetCatalogTitle (image));
        Assert::AreEqual (std::string ("0007"), FullSendDisk::MakeImageTitle (7, FullSendDisk::GetCatalogTitle (image), std::string()));
        Assert::AreEqual (std::string (".dsk"), FullSendDisk::GetImageExtension (image, std::string()));
    }



    //  The manual choice.

    TEST_METHOD (ChoicesAreByPathAndCanBeRemoved)
    {
        FilesystemChoices  choices;

        Assert::IsTrue (choices.GetChoice (L"C:\\a.dsk") == VolumeKind::Unknown);

        choices.SetChoice (L"C:\\A.DSK", VolumeKind::Cpm);
        Assert::IsTrue (choices.GetChoice (L"c:\\a.dsk") == VolumeKind::Cpm);

        choices.SetChoice (L"c:\\a.dsk", VolumeKind::Unknown);
        Assert::IsTrue (choices.GetChoice (L"C:\\A.DSK") == VolumeKind::Unknown);
    }



    TEST_METHOD (ChoiceWordsRoundTrip)
    {
        Assert::IsTrue (FilesystemChoices::FromWord (FilesystemChoices::GetWord (VolumeKind::Cpm)) == VolumeKind::Cpm);
        Assert::IsTrue (FilesystemChoices::FromWord ("CPM") == VolumeKind::Cpm);
        Assert::IsTrue (FilesystemChoices::FromWord ("prodos") == VolumeKind::Unknown);
        Assert::AreEqual ("", FilesystemChoices::GetWord (VolumeKind::Unknown));
    }



    TEST_METHOD (ChoicesPersistInThePreferences)
    {
        CassoExplorerPrefs  written;
        CassoExplorerPrefs  read;
        JsonValue           json;

        written.filesystemChoices.SetChoice (L"C:\\Disks\\Blank.dsk", VolumeKind::Cpm);

        json = written.ToJson();
        AssertSucceeded (read.FromJson (json));

        Assert::IsTrue (read.filesystemChoices.GetChoice (L"C:\\Disks\\Blank.dsk") == VolumeKind::Cpm);
    }



    static void  Seed (FakeDiskFileIo & io, const std::vector<Byte> & image)
    {
        io.files[kImage]  = image;
        io.stamps[kImage] = FileStamp { image.size(), 100 };
    }

    TEST_METHOD (AnUnrecognizedImageOpensOnlyWithAChoice)
    {
        FakeDiskFileIo          io;
        DiskOperations          operations (io);
        FilesystemChoices       choices;
        VolumeListing           listing;
        VolumeKind              kind = VolumeKind::Unknown;
        DiskOperations::Result  result;

        Seed (io, BuildDamaged());

        operations.SetFilesystemChoices (&choices);

        result = operations.List (kImage, listing, kind);
        Assert::IsFalse (result.Succeeded());
        Assert::IsTrue (result.message.find (DiskImageSession::kNoFilesystemText) != std::string::npos);
        Assert::IsTrue (operations.IsUnrecognized (kImage));

        choices.SetChoice (TextEncoding::NarrowToWide (kImage), VolumeKind::Cpm);

        result = operations.List (kImage, listing, kind);
        Assert::IsTrue (result.Succeeded());
        Assert::IsTrue (kind == VolumeKind::Cpm);
        Assert::AreEqual ((uint32_t) 126, listing.freeUnits);
        Assert::IsTrue (operations.IsUnrecognized (kImage), L"A choice doesn't change what detection finds");
    }



    TEST_METHOD (AChoiceReachesTheWritesThatGoThroughTheRunner)
    {
        FakeDiskFileIo          io;
        DiskOperations          operations (io);
        FilesystemChoices       choices;
        DiskOperations::Result  result;
        FilePayload             read;

        Seed (io, BuildBlank());

        io.files["C:\\host\\A.COM"] = { 1, 2, 3 };

        operations.SetFilesystemChoices (&choices);
        choices.SetChoice (TextEncoding::NarrowToWide (kImage), VolumeKind::Cpm);

        result = operations.Put (kImage, "C:\\host\\A.COM", "A.COM", "", false, 0, DiskOperations::Encoding::Verbatim);
        Assert::IsTrue (result.Succeeded(), TextEncoding::NarrowToWide (result.message).c_str());

        result = operations.Read (kImage, "A.COM", read);
        Assert::IsTrue (result.Succeeded());
        Assert::AreEqual ((size_t) 128, read.bytes.size());
    }



    TEST_METHOD (ARecognizedDiskIgnoresAChoice)
    {
        FakeDiskFileIo     io;
        DiskOperations     operations (io);
        FilesystemChoices  choices;
        std::vector<Byte>  dos (kImageBytes, 0);
        VolumeListing      listing;
        VolumeKind         kind = VolumeKind::Unknown;

        AssertSucceeded (Dos33Skeleton::Write (dos, 254));

        Seed (io, dos);

        operations.SetFilesystemChoices (&choices);
        choices.SetChoice (TextEncoding::NarrowToWide (kImage), VolumeKind::Cpm);

        Assert::IsTrue (operations.List (kImage, listing, kind).Succeeded());
        Assert::IsTrue (kind == VolumeKind::Dos33);
    }



    //  The command line.

    static DiskCommandResult  RunVerb (FakeDiskFileIo & io, CommandLineOptions::DiskOptions::Command command, const char * path, const char * filesystem = "", const char * host = "", CommandLineOptions::DiskOptions::Encoding encoding = CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        CommandLineOptions  options = DiskOperations::MakeOptions (command, kImage);
        DiskCommandRunner   runner (io);

        options.disk.path       = path;
        options.disk.filesystem = filesystem;
        options.disk.hostFile   = host;
        options.disk.encoding   = encoding;

        return runner.Run (options);
    }

    TEST_METHOD (TheCommandLineListsWithAChoice)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;

        Seed (io, BuildDamaged());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::List, "");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::List, "", "cpm");
        Assert::AreNotEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.find ("Bytes Remaining On A: ") != std::string::npos);

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::List, "", "nonsense");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
    }

    TEST_METHOD (TheCommandLineListsAVolume)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;

        Seed (io, BuildSample());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::List, "");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("TEST.COM") != std::string::npos);
        Assert::IsTrue (result.output.find ("COM") != std::string::npos);
        Assert::IsTrue (result.output.find ("USER 2") != std::string::npos);
        Assert::IsTrue (result.output.find ("Not bootable: it has no system on tracks 0 to 2") != std::string::npos);
    }

    TEST_METHOD (TheCommandLinePutsGetsAndConvertsText)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;
        std::string        lines = "ONE\nTWO\n";

        Seed (io, BuildSample());

        io.files["C:\\host\\A.TXT"].assign (lines.begin(), lines.end());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Put, "", "", "C:\\host\\A.TXT", CommandLineOptions::DiskOptions::Encoding::Text);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Get, "A.TXT", "", "", CommandLineOptions::DiskOptions::Encoding::Text);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string ("ONE\r\nTWO\r\n"), std::string (result.payload.begin(), result.payload.end()));

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Delete, "A.TXT");
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }

    TEST_METHOD (TheCommandLineRefusesATypeAddressOrStartupProgram)
    {
        FakeDiskFileIo      io;
        DiskCommandResult   result;
        CommandLineOptions  options = DiskOperations::MakeOptions (CommandLineOptions::DiskOptions::Command::Put, kImage);
        DiskCommandRunner   runner (io);

        Seed (io, BuildSample());

        io.files["C:\\host\\A.COM"] = { 1 };

        options.disk.hostFile = "C:\\host\\A.COM";
        options.disk.typeName = "B";

        result = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);

        options.disk.typeName.clear();
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Boot, "TEST.COM");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no setting for a startup program") != std::string::npos);
    }



    //  Over a link, with a fake drive.

    struct FakeDrive : ILinkedSectorSource
    {
        std::vector<Byte>    image;
        std::vector<size_t>  read;
        std::vector<size_t>  written;

        HRESULT  ReadSectors (std::span<const size_t> sectors, std::vector<Byte> & outData) override
        {
            outData.clear();

            for (size_t sector : sectors)
            {
                outData.insert (outData.end(), image.begin() + (ptrdiff_t) (sector * kSectorBytes), image.begin() + (ptrdiff_t) ((sector + 1) * kSectorBytes));
                read.push_back (sector);
            }

            return S_OK;
        }

        HRESULT  WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data) override
        {
            for (size_t i = 0; i < sectors.size(); i++)
            {
                std::copy_n (data.begin() + (ptrdiff_t) (i * kSectorBytes), kSectorBytes, image.begin() + (ptrdiff_t) (sectors[i] * kSectorBytes));
                written.push_back (sectors[i]);
            }

            return S_OK;
        }
    };

    TEST_METHOD (ALinkedDiskFindsTheDirectoryAndReads)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  listing;
        FilePayload    payload;
        FilePayload    expected;

        drive.image = BuildSample();

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));

        Assert::IsTrue (disk.GetKind() == VolumeKind::Cpm);
        Assert::AreEqual ((size_t) 4, listing.entries.size());

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Read (FilePath::Parse ("BIG.DAT"), payload); }));
        AssertSucceeded (CpmVolume (drive.image).Read (FilePath::Parse ("BIG.DAT"), expected));

        Assert::IsTrue (payload.bytes == expected.bytes);
    }

    TEST_METHOD (ARecognizedLinkedDiskDoesNotReadTheCpmDirectory)
    {
        FakeDrive            drive;
        LinkedDisk           disk (drive);
        std::vector<size_t>  directory = LinkedDisk::GetCatalogSectors (VolumeKind::Cpm);

        drive.image.assign (kImageBytes, 0);

        AssertSucceeded (Dos33Skeleton::Write (drive.image, 254));
        AssertSucceeded (disk.Prefetch());

        Assert::IsTrue (disk.GetKind() == VolumeKind::Dos33);
        Assert::IsTrue (std::none_of (drive.read.begin(), drive.read.end(), [&directory] (size_t sector) { return std::find (directory.begin(), directory.end(), sector) != directory.end(); }));
    }

    TEST_METHOD (ALinkedDiskWritesTheDirectoryLast)
    {
        FakeDrive            drive;
        LinkedDisk           disk (drive);
        std::vector<size_t>  sent;
        std::vector<Byte>    expected;
        FilePayload          payload   = MakePayload (2000, 5);
        std::vector<size_t>  directory = LinkedDisk::GetCatalogSectors (VolumeKind::Cpm);

        drive.image = BuildSample();

        AssertSucceeded (CpmVolume (drive.image).Write (FilePath::Parse ("NEW.COM"), payload, expected));
        AssertSucceeded (disk.Write ([&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Write (FilePath::Parse ("NEW.COM"), payload, outBuffer); }, sent));

        Assert::IsTrue (drive.image == expected);
        Assert::IsTrue (std::find (directory.begin(), directory.end(), drive.written.back()) != directory.end());
        Assert::IsTrue (std::find (directory.begin(), directory.end(), drive.written.front()) == directory.end(), L"Data goes first");
        Assert::AreEqual ((size_t) 9, drive.written.size(), L"Two blocks of four sectors, and the directory's one");
    }

    TEST_METHOD (ALinkedBlankDiskIsFound)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  listing;

        drive.image = BuildBlank();

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));
        Assert::IsTrue (disk.GetKind() == VolumeKind::Cpm);
        Assert::AreEqual ((uint32_t) 126, listing.freeUnits);
    }

    TEST_METHOD (ALinkedDamagedDiskNeedsAChoice)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  listing;
        FilePayload    payload = MakePayload (10, 0);

        drive.image = BuildDamaged();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));

        disk.SetChosenKind (VolumeKind::Cpm);

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));
        Assert::IsTrue (disk.GetKind() == VolumeKind::Cpm);
        Assert::AreEqual ((uint32_t) 126, listing.freeUnits);

        disk.Clear();
        Assert::IsTrue (disk.GetChosenKind() == VolumeKind::Cpm, L"The choice stays across Clear");

        {
            std::vector<size_t>  sent;

            AssertSucceeded (disk.Write ([&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Write (FilePath::Parse ("A.COM"), payload, outBuffer); }, sent));
        }
    }
};
