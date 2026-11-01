#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/AppleFileProperties.h"
#include "CassoExplorer/Model/BasicConversion.h"
#include "CassoExplorer/Model/CatalogModel.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "CassoExplorer/Model/DragPayload.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/LinkedDisk.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PascalVolumeTests
//
//  An Apple Pascal volume built in memory. The layout is restated here from
//  the format, so a test agrees with the code only where the format does: the
//  directory in blocks 2 to 5, 26 bytes an entry, and block 2's two halves at
//  track 0's DOS logical sectors 11 and 10.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PascalVolumeTests)
{
public:

    static constexpr size_t  kImageBytes     = 143360;
    static constexpr size_t  kSectorBytes    = 256;
    static constexpr size_t  kBlockBytes     = 512;
    static constexpr size_t  kBlock2Half0    = 11 * kSectorBytes;
    static constexpr size_t  kBlock2Half1    = 10 * kSectorBytes;
    static constexpr size_t  kEntryBytes     = 26;
    static constexpr int     kFirstFileBlock = 6;
    static constexpr int     kBlocks         = 280;
    static constexpr const char *  kImage    = "C:\\disks\\pascal.dsk";

    //  The DOS logical sector of each half-block, 0 to 15, in track order.
    static constexpr int  kHalfToLogical[16] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };

    static size_t  GetBlockOffset (int block, size_t offset)
    {
        int  track = block / 8;
        int  half  = (block % 8) * 2 + (int) (offset / kSectorBytes);

        return ((size_t) track * 16 + (size_t) kHalfToLogical[half]) * kSectorBytes + offset % kSectorBytes;
    }

    static void  PutBlockBytes (std::vector<Byte> & image, int block, size_t offset, std::span<const Byte> bytes)
    {
        for (size_t i = 0; i < bytes.size(); i++)
        {
            image[GetBlockOffset (block, offset + i)] = bytes[i];
        }
    }

    static void  PutWord (std::vector<Byte> & image, int block, size_t offset, Word value)
    {
        image[GetBlockOffset (block, offset)]     = (Byte) (value & 0xFF);
        image[GetBlockOffset (block, offset + 1)] = (Byte) (value >> 8);
    }

    static Word  GetWord (const std::vector<Byte> & image, int block, size_t offset)
    {
        return (Word) (image[GetBlockOffset (block, offset)] | (image[GetBlockOffset (block, offset + 1)] << 8));
    }

    //  The volume's entry at the start of the directory.
    static std::vector<Byte>  BuildEmpty (const char * name = "APPLE0")
    {
        std::vector<Byte>  image (kImageBytes, 0);
        size_t             length = strlen (name);

        PutWord (image, 2, 0, 0);
        PutWord (image, 2, 2, 6);
        PutWord (image, 2, 4, 0);
        image[GetBlockOffset (2, 6)] = (Byte) length;
        PutBlockBytes (image, 2, 7, std::span<const Byte> ((const Byte *) name, length));
        PutWord (image, 2, 14, kBlocks);
        PutWord (image, 2, 16, 0);

        return image;
    }

    //  One file entry and its data, appended in the next slot.
    static void  AddFile (std::vector<Byte> & image, int slot, const char * name, int first, int next, Byte kind, Word lastBytes, Word date, Byte seed)
    {
        size_t  at     = (size_t) slot * kEntryBytes;
        size_t  length = strlen (name);

        PutWord (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 0,  (Word) first);
        PutWord (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 2,  (Word) next);
        PutWord (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 4,  kind);
        PutBlockBytes (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 6, std::vector<Byte> { (Byte) length });
        PutBlockBytes (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 7, std::span<const Byte> ((const Byte *) name, length));
        PutWord (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 22, lastBytes);
        PutWord (image, 2 + (int) (at / kBlockBytes), at % kBlockBytes + 24, date);
        PutWord (image, 2, 16, (Word) slot);

        for (int block = first; block < next; block++)
        {
            for (size_t i = 0; i < kBlockBytes; i++)
            {
                image[GetBlockOffset (block, i)] = (Byte) (seed + (block - first) * 3 + i);
            }
        }
    }

    static std::vector<Byte>  BuildSample()
    {
        std::vector<Byte>  image = BuildEmpty();

        AddFile (image, 1, "SYSTEM.PASCAL", 6,  10, PascalVolume::kKindCode, 512, 0xAA39, 1);
        AddFile (image, 2, "NOTES.TEXT",    10, 14, PascalVolume::kKindText, 512, 0xA79C, 2);
        AddFile (image, 3, "LOG.DATA",      14, 15, PascalVolume::kKindData, 192, 0,      3);

        return image;
    }

    static FilePayload  MakePayload (size_t size, Byte seed, Byte type = PascalVolume::kKindNone)
    {
        FilePayload  payload;

        payload.type = type;
        payload.bytes.resize (size);

        for (size_t i = 0; i < size; i++)
        {
            payload.bytes[i] = (Byte) (seed + i);
        }

        return payload;
    }

    static std::vector<std::string>  GetNames (const std::vector<Byte> & image)
    {
        PascalVolume              volume (image);
        VolumeListing             listing;
        std::vector<std::string>  names;

        AssertSucceeded (volume.Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }



    TEST_METHOD (LayoutMatchesThePublishedBlockMap)
    {
        Assert::AreEqual (kBlock2Half0, GetBlockOffset (2, 0));
        Assert::AreEqual (kBlock2Half1, GetBlockOffset (2, 256));
    }



    TEST_METHOD (DetectsAPascalVolume)
    {
        std::vector<Byte>  image = BuildSample();

        Assert::IsTrue (PascalVolume::IsFound (image));
        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Pascal);
    }



    TEST_METHOD (RejectsHeadersThatAreNotAVolume)
    {
        std::vector<Byte>  image = BuildSample();

        //  Next block other than 6.
        PutWord (image, 2, 2, 7);
        Assert::IsFalse (PascalVolume::IsFound (image));

        //  A name too long.
        image = BuildSample();
        image[GetBlockOffset (2, 6)] = 8;
        Assert::IsFalse (PascalVolume::IsFound (image));

        //  An unprintable name.
        image = BuildSample();
        image[GetBlockOffset (2, 7)] = 0x01;
        Assert::IsFalse (PascalVolume::IsFound (image));

        //  More files than a directory has room for.
        image = BuildSample();
        PutWord (image, 2, 16, 78);
        Assert::IsFalse (PascalVolume::IsFound (image));

        //  A blank disk.
        image.assign (kImageBytes, 0);
        Assert::IsFalse (PascalVolume::IsFound (image));
        Assert::IsTrue (VolumeImage::DetectFilesystem (image) == VolumeKind::Unknown);
    }



    TEST_METHOD (ADos33OrProDosVolumeIsNeverPascal)
    {
        std::vector<Byte>  dos (kImageBytes, 0);
        std::vector<Byte>  pro (kImageBytes, 0);

        AssertSucceeded (Dos33Skeleton::Write (dos, 254));
        AssertSucceeded (ProDosSkeleton::Write (pro, "NOTPASCAL"));

        Assert::IsTrue (VolumeImage::DetectFilesystem (dos) == VolumeKind::Dos33);
        Assert::IsTrue (VolumeImage::DetectFilesystem (pro) == VolumeKind::ProDos);
    }



    TEST_METHOD (ListsNamesKindsSizesAndFreeSpace)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        VolumeListing      listing;

        AssertSucceeded (volume.Enumerate (listing));

        Assert::IsTrue (listing.hasVolumeName);
        Assert::AreEqual (std::string ("APPLE0"), listing.volumeName);
        Assert::AreEqual ((uint32_t) kBlocks, listing.totalUnits);
        Assert::AreEqual ((uint32_t) (kBlocks - 15), listing.freeUnits);
        Assert::AreEqual ((size_t) 3, listing.entries.size());
        Assert::IsTrue (listing.damage.empty());

        Assert::AreEqual (std::string ("SYSTEM.PASCAL"), listing.entries[0].name);
        Assert::AreEqual ((int) PascalVolume::kKindCode, (int) listing.entries[0].type);
        Assert::AreEqual ((uint32_t) 4, listing.entries[0].sizeUnits);
        Assert::AreEqual ((uint32_t) 2048, listing.entries[0].eofBytes);

        Assert::AreEqual (std::string ("LOG.DATA"), listing.entries[2].name);
        Assert::AreEqual ((uint32_t) 192, listing.entries[2].eofBytes);
        Assert::IsFalse (listing.entries[2].hasModified, L"A zero date is no date");

        Assert::IsTrue (listing.entries[0].hasModified);
    }



    TEST_METHOD (ReadsADateAsMonthDayYear)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        VolumeListing      listing;
        int64_t            expected = 0;

        //  $AA39 is month 9, day 3, year 85.
        AssertSucceeded (volume.Enumerate (listing));
        Assert::IsTrue (ProDosVolume::TryToUnixTime ((Word) ((85 << 9) | (9 << 5) | 3), 0, expected));
        Assert::AreEqual (expected, listing.entries[0].modifiedUnix);
    }



    TEST_METHOD (ReadsEachFileWithItsExactLength)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        FilePayload        payload;

        AssertSucceeded (volume.Read (FilePath::Parse ("log.data"), payload));

        Assert::AreEqual ((size_t) 192, payload.bytes.size());
        Assert::AreEqual ((int) (Byte) (3 + 100), (int) payload.bytes[100]);
        Assert::IsTrue (payload.textForm == PayloadTextForm::PascalPages);

        AssertSucceeded (volume.Read (FilePath::Parse ("SYSTEM.PASCAL"), payload));

        Assert::AreEqual ((size_t) 2048, payload.bytes.size());
        Assert::AreEqual ((int) (Byte) (1 + 2 * 3 + 7), (int) payload.bytes[2 * 512 + 7]);
    }



    TEST_METHOD (ReadOfAMissingFileFails)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        FilePayload        payload;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), volume.Read (FilePath::Parse ("NOPE"), payload));
    }



    TEST_METHOD (ADamagedEntryIsReportedAndKeptOutOfTheList)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        VolumeListing      listing;

        //  The second file's run ends past the volume.
        PutWord (image, 2, 2 * kEntryBytes + 2, 400);

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual ((size_t) 2, listing.entries.size());
        Assert::AreEqual ((size_t) 1, listing.damage.size());
    }



    TEST_METHOD (WritesANewFileInTheFirstGapWideEnough)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        std::vector<Byte>  edited;
        FilePayload        written;
        FilePayload        payload = MakePayload (1000, 7);

        AssertSucceeded (volume.Write (FilePath::Parse ("new.code"), payload, edited));

        {
            PascalVolume   after (edited);
            VolumeListing  listing;

            AssertSucceeded (after.Enumerate (listing));
            Assert::AreEqual ((size_t) 4, listing.entries.size());
            Assert::AreEqual (std::string ("NEW.CODE"), listing.entries[3].name, L"Upper case");
            Assert::AreEqual ((int) PascalVolume::kKindCode, (int) listing.entries[3].type, L"The suffix selects the kind");
            Assert::AreEqual ((uint32_t) 2, listing.entries[3].sizeUnits);
            Assert::AreEqual ((uint32_t) 1000, listing.entries[3].eofBytes);

            AssertSucceeded (after.Read (FilePath::Parse ("NEW.CODE"), written));
            Assert::IsTrue (written.bytes == payload.bytes);
        }

        Assert::AreEqual ((Word) 15, GetWord (edited, 2, 4 * kEntryBytes));
        Assert::AreEqual ((Word) 17, GetWord (edited, 2, 4 * kEntryBytes + 2));
        Assert::AreEqual ((Word) (1000 - 512), GetWord (edited, 2, 4 * kEntryBytes + 22));
    }



    TEST_METHOD (WriteZeroFillsTheLastBlock)
    {
        std::vector<Byte>  image = BuildEmpty();
        PascalVolume       volume (image);
        std::vector<Byte>  edited;

        std::fill (image.begin() + (ptrdiff_t) GetBlockOffset (kFirstFileBlock, 0), image.begin() + (ptrdiff_t) GetBlockOffset (kFirstFileBlock, 0) + (ptrdiff_t) kSectorBytes, (Byte) 0xAA);

        AssertSucceeded (volume.Write (FilePath::Parse ("A.DATA"), MakePayload (10, 1), edited));

        Assert::AreEqual ((int) 0, (int) edited[GetBlockOffset (kFirstFileBlock, 10)]);
    }



    TEST_METHOD (WriteLeavesTheOriginalBufferAlone)
    {
        std::vector<Byte>  image  = BuildSample();
        std::vector<Byte>  before = image;
        PascalVolume       volume (image);
        std::vector<Byte>  edited;

        AssertSucceeded (volume.Write (FilePath::Parse ("NEW.DATA"), MakePayload (10, 1), edited));

        Assert::IsTrue (image == before);
        Assert::IsFalse (edited == before);
    }



    TEST_METHOD (WriteUsesAGapAndKeepsTheDirectoryInBlockOrder)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  freed;
        std::vector<Byte>  edited;

        {
            PascalVolume  volume (image);

            AssertSucceeded (volume.Delete (FilePath::Parse ("NOTES.TEXT"), freed));
        }

        {
            PascalVolume  volume (freed);

            AssertSucceeded (volume.Write (FilePath::Parse ("MID.DATA"), MakePayload (1024, 9), edited));
        }

        Assert::IsTrue (GetNames (edited) == std::vector<std::string> { "SYSTEM.PASCAL", "MID.DATA", "LOG.DATA" });
        Assert::AreEqual ((Word) 10, GetWord (edited, 2, 2 * kEntryBytes));
    }



    TEST_METHOD (WriteReplacesAFileOfTheSameName)
    {
        std::vector<Byte>  image = BuildSample();
        PascalVolume       volume (image);
        std::vector<Byte>  edited;
        FilePayload        read;
        FilePayload        payload = MakePayload (100, 5);

        AssertSucceeded (volume.Write (FilePath::Parse ("log.data"), payload, edited));

        Assert::AreEqual ((size_t) 3, GetNames (edited).size());

        AssertSucceeded (PascalVolume (edited).Read (FilePath::Parse ("LOG.DATA"), read));
        Assert::IsTrue (read.bytes == payload.bytes);
    }



    TEST_METHOD (WriteRefusesWhenNoRunIsLongEnough)
    {
        std::vector<Byte>  image = BuildEmpty();
        PascalVolume       volume (image);
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), volume.Write (FilePath::Parse ("HUGE.DATA"), MakePayload ((kBlocks - 5) * kBlockBytes, 1), edited));
        Assert::IsTrue (edited.empty());
    }



    TEST_METHOD (WriteRefusesFreeSpaceInPieces)
    {
        std::vector<Byte>  image = BuildEmpty();
        std::vector<Byte>  edited;
        PascalVolume       volume (image);

        //  Files with a gap of 2 blocks after each, up to the end of the disk.
        AddFile (image, 1, "A.DATA", 6,   100, PascalVolume::kKindData, 512, 0, 0);
        AddFile (image, 2, "B.DATA", 102, 200, PascalVolume::kKindData, 512, 0, 0);
        AddFile (image, 3, "C.DATA", 202, 280, PascalVolume::kKindData, 512, 0, 0);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), volume.Write (FilePath::Parse ("D.DATA"), MakePayload (3 * kBlockBytes, 1), edited));

        AssertSucceeded (volume.Write (FilePath::Parse ("D.DATA"), MakePayload (2 * kBlockBytes, 1), edited));
    }



    TEST_METHOD (WriteRefusesAFullDirectory)
    {
        std::vector<Byte>  image = BuildEmpty();
        PascalVolume       volume (image);
        std::vector<Byte>  edited;

        for (int slot = 1; slot <= PascalVolume::kMaxFiles; slot++)
        {
            AddFile (image, slot, std::format ("F{}", slot).c_str(), 5 + slot, 6 + slot, PascalVolume::kKindData, 512, 0, 0);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), volume.Write (FilePath::Parse ("ONEMORE"), MakePayload (1, 1), edited));
    }



    TEST_METHOD (WriteRefusesAnInvalidName)
    {
        std::vector<Byte>  image = BuildEmpty();
        PascalVolume       volume (image);
        std::vector<Byte>  edited;

        for (const char * name : { "", "HAS SPACE", "A:B", "$X", "A=B", "Q?", "A,B", "[A", "A]", "SIXTEENCHARSLONG" })
        {
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME), volume.Write (FilePath::FromName (name), MakePayload (1, 1), edited));
        }
    }



    TEST_METHOD (AnEmptyFileHasOneBlockAndNoLength)
    {
        std::vector<Byte>  image = BuildEmpty();
        std::vector<Byte>  edited;
        VolumeListing      listing;
        FilePayload        read;

        AssertSucceeded (PascalVolume (image).Write (FilePath::Parse ("EMPTY.DATA"), MakePayload (0, 0), edited));
        AssertSucceeded (PascalVolume (edited).Enumerate (listing));

        Assert::AreEqual ((uint32_t) 1, listing.entries[0].sizeUnits);
        Assert::AreEqual ((uint32_t) 0, listing.entries[0].eofBytes);

        AssertSucceeded (PascalVolume (edited).Read (FilePath::Parse ("EMPTY.DATA"), read));
        Assert::IsTrue (read.bytes.empty());
    }



    TEST_METHOD (DeleteMovesLaterEntriesUpAndFreesItsBlocks)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        VolumeListing      listing;
        DeleteOutcome      outcome;

        AssertSucceeded (PascalVolume (image).Delete (FilePath::Parse ("NOTES.TEXT"), edited, outcome));

        Assert::AreEqual ((size_t) 4, outcome.freedUnits.size());
        Assert::IsTrue (GetNames (edited) == std::vector<std::string> { "SYSTEM.PASCAL", "LOG.DATA" });

        AssertSucceeded (PascalVolume (edited).Enumerate (listing));
        Assert::AreEqual ((uint32_t) (kBlocks - 11), listing.freeUnits);
        Assert::AreEqual ((Word) 2, GetWord (edited, 2, 16), L"The file count");

        //  The slot past the end is cleared.
        Assert::AreEqual ((Word) 0, GetWord (edited, 2, 3 * kEntryBytes));
    }



    TEST_METHOD (DeleteOfAMissingFileFails)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), PascalVolume (image).Delete (FilePath::Parse ("NOPE"), edited));
    }



    TEST_METHOD (RenameChangesOnlyTheName)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        FilePayload        before;
        FilePayload        after;

        AssertSucceeded (PascalVolume (image).Read (FilePath::Parse ("LOG.DATA"), before));
        AssertSucceeded (PascalVolume (image).Rename (FilePath::Parse ("LOG.DATA"), "history.data", edited));
        AssertSucceeded (PascalVolume (edited).Read (FilePath::Parse ("HISTORY.DATA"), after));

        Assert::IsTrue (before.bytes == after.bytes);
        Assert::IsTrue (GetNames (edited) == std::vector<std::string> { "SYSTEM.PASCAL", "NOTES.TEXT", "HISTORY.DATA" });
    }



    TEST_METHOD (RenameRefusesATakenOrInvalidName)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS),    PascalVolume (image).Rename (FilePath::Parse ("LOG.DATA"), "NOTES.TEXT", edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),   PascalVolume (image).Rename (FilePath::Parse ("LOG.DATA"), "A B", edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND), PascalVolume (image).Rename (FilePath::Parse ("NOPE"), "OK", edited));
    }



    TEST_METHOD (SetAttributesChangesTheKindAndDate)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        FileAttributes     attributes;
        VolumeListing      listing;

        attributes.hasType      = true;
        attributes.type         = PascalVolume::kKindFoto;
        attributes.hasModified  = true;
        attributes.modifiedDate = (Word) ((85 << 9) | (9 << 5) | 3);

        AssertSucceeded (PascalVolume (image).SetAttributes (FilePath::Parse ("LOG.DATA"), attributes, edited));
        AssertSucceeded (PascalVolume (edited).Enumerate (listing));

        Assert::AreEqual ((int) PascalVolume::kKindFoto, (int) listing.entries[2].type);
        Assert::AreEqual ((Word) 0xAA39, GetWord (edited, 2, 3 * kEntryBytes + 24));
    }



    TEST_METHOD (SetAttributesRefusesWhatPascalDoesNotStore)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        FileAttributes     access;
        FileAttributes     aux;
        FileAttributes     created;

        access.hasAccess   = true;
        aux.hasAuxType     = true;
        created.hasCreated = true;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), PascalVolume (image).SetAttributes (FilePath::Parse ("LOG.DATA"), access, edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), PascalVolume (image).SetAttributes (FilePath::Parse ("LOG.DATA"), aux, edited));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), PascalVolume (image).SetAttributes (FilePath::Parse ("LOG.DATA"), created, edited));
    }



    TEST_METHOD (HasNoStartupProgramSetting)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), PascalVolume (image).SetStartupProgram (FilePath::Parse ("LOG.DATA"), edited));
    }



    TEST_METHOD (IntegrityReportFindsSharedBlocks)
    {
        std::vector<Byte>      image = BuildSample();
        VolumeIntegrityReport  report;

        AssertSucceeded (PascalVolume (image).BuildIntegrityReport (report));
        Assert::IsTrue (report.IsClean());

        PutWord (image, 2, 2 * kEntryBytes, 9);

        AssertSucceeded (PascalVolume (image).BuildIntegrityReport (report));
        Assert::AreEqual ((size_t) 1, report.GetCrossLinked().size());
    }



    TEST_METHOD (DeleteLeavesABlockAnotherFileAlsoCovers)
    {
        std::vector<Byte>  image = BuildSample();
        std::vector<Byte>  edited;
        DeleteOutcome      outcome;
        FilePayload        survivor;

        //  NOTES.TEXT now starts in SYSTEM.PASCAL's last block.
        PutWord (image, 2, 2 * kEntryBytes, 9);

        AssertSucceeded (PascalVolume (image).Delete (FilePath::Parse ("NOTES.TEXT"), edited, outcome));

        Assert::AreEqual ((size_t) 4, outcome.freedUnits.size(), L"Block 9 is SYSTEM.PASCAL's too");
    }



    TEST_METHOD (TextDecodesIndentsAndLineEnds)
    {
        std::vector<Byte>  bytes (2048, 0);
        std::string        text;
        const char         line1[] = "HI\r";

        std::copy_n (line1, 3, bytes.begin() + 1024);
        bytes[1027] = 0x10;
        bytes[1028] = 0x20 + 3;
        bytes[1029] = 'X';
        bytes[1030] = '\r';

        PascalVolume::DecodeText (bytes, text);

        Assert::AreEqual (std::string ("HI\r\n   X\r\n"), text);
    }



    TEST_METHOD (TextEncodesWithAHeaderAndWholePages)
    {
        std::vector<Byte>  bytes;
        size_t             bad = 0;

        AssertSucceeded (PascalVolume::EncodeText ("HI\n   X\n", bytes, bad));

        Assert::AreEqual ((size_t) 2048, bytes.size());
        Assert::IsTrue (std::all_of (bytes.begin(), bytes.begin() + 1024, [] (Byte b) { return b == 0; }), L"The header");
        Assert::AreEqual ((int) 'H', (int) bytes[1024]);
        Assert::AreEqual ((int) '\r', (int) bytes[1026]);
        Assert::AreEqual ((int) 0x10, (int) bytes[1027]);
        Assert::AreEqual ((int) 0x23, (int) bytes[1028]);
    }



    TEST_METHOD (TextLinesDoNotCrossAPage)
    {
        std::vector<Byte>  bytes;
        std::string        text;
        size_t             bad = 0;
        std::string        back;

        //  Enough 100-character lines to fill more than one page.
        for (int i = 0; i < 25; i++)
        {
            text += std::string (100, (char) ('A' + i)) + "\r\n";
        }

        AssertSucceeded (PascalVolume::EncodeText (text, bytes, bad));

        Assert::AreEqual ((size_t) 1024 * 4, bytes.size());
        Assert::AreEqual ((int) 'K', (int) bytes[2048], L"The line that doesn't fit starts the next page");
        Assert::AreEqual ((int) 0, (int) bytes[2047], L"Zeros pad the first page");

        PascalVolume::DecodeText (bytes, back);
        Assert::AreEqual (text, back);
    }



    TEST_METHOD (TextWithAByteOutsideAsciiFailsAtThatOffset)
    {
        std::vector<Byte>  bytes;
        size_t             bad = 0;

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), PascalVolume::EncodeText ("AB\xC9", bytes, bad));
        Assert::AreEqual ((size_t) 2, bad);
    }



    TEST_METHOD (KindsComeFromTheSuffix)
    {
        Assert::AreEqual ((int) PascalVolume::kKindText, (int) PascalVolume::GetKindFromName ("A.TEXT"));
        Assert::AreEqual ((int) PascalVolume::kKindCode, (int) PascalVolume::GetKindFromName ("a.code"));
        Assert::AreEqual ((int) PascalVolume::kKindFoto, (int) PascalVolume::GetKindFromName ("B.FOTO"));
        Assert::AreEqual ((int) PascalVolume::kKindData, (int) PascalVolume::GetKindFromName ("PLAIN"));
    }



    //  The explorer's model.

    TEST_METHOD (CatalogRowsHaveThePascalKindAndBlocks)
    {
        FileEntry   entry;
        CatalogRow  row;

        entry.name      = "A.CODE";
        entry.type      = PascalVolume::kKindCode;
        entry.sizeUnits = 3;

        row = CatalogModel::FromFileEntry (entry, VolumeKind::Pascal, 0);

        Assert::AreEqual (std::wstring (L"CODE"), row.typeText);
        Assert::AreEqual ((uint64_t) 3 * 512, row.sizeBytes);
    }



    TEST_METHOD (BasicConversionDoesNotApply)
    {
        //  Kind 2 would be Applesoft on DOS 3.3.
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Pascal, PascalVolume::kKindCode) == BasicConversion::Kind::None);
        Assert::IsTrue (BasicConversion::GetKind (VolumeKind::Pascal, PascalVolume::kKindText) == BasicConversion::Kind::None);
    }



    TEST_METHOD (HostNamesKeepThePascalName)
    {
        FileEntry  entry;
        bool       converted = false;

        entry.name = "SYSTEM.LIBRARY";
        entry.type = PascalVolume::kKindCode;

        Assert::AreEqual (std::wstring (L"SYSTEM.LIBRARY"), DragPayload::GetHostName (entry, VolumeKind::Pascal, HostFileNaming::Style::Descriptive, converted));
        Assert::IsFalse (converted);

        entry.name = "A.TEXT";
        entry.type = PascalVolume::kKindText;

        DragPayload::GetHostName (entry, VolumeKind::Pascal, HostFileNaming::Style::Descriptive, converted);
        Assert::IsTrue (converted, L"Text is converted");

        entry.type = PascalVolume::kKindCode;
        Assert::AreEqual (std::wstring (L"A.TEXT#020000"), DragPayload::GetHostName (entry, VolumeKind::Pascal, HostFileNaming::Style::CiderPress, converted));
    }



    TEST_METHOD (TheEncodingFollowsTheKind)
    {
        FileEntry  entry;

        entry.type = PascalVolume::kKindText;
        Assert::IsTrue (CassoExplorerActions::GetEncoding (entry, VolumeKind::Pascal) == DiskOperations::Encoding::Text);

        entry.type = PascalVolume::kKindCode;
        Assert::IsTrue (CassoExplorerActions::GetEncoding (entry, VolumeKind::Pascal) == DiskOperations::Encoding::Verbatim);
    }



    TEST_METHOD (DragEntriesRoundTripTheVolumeKind)
    {
        std::string               image;
        VolumeKind                kind = VolumeKind::Unknown;
        std::vector<std::string>  paths;

        Assert::IsTrue (DragPayload::DecodeCatalogEntries (DragPayload::EncodeCatalogEntries ("x.dsk", VolumeKind::Pascal, { "A.CODE" }), image, kind, paths));
        Assert::IsTrue (kind == VolumeKind::Pascal);
    }



    TEST_METHOD (PlansHaveNoAddressOrBasic)
    {
        std::vector<Byte>      binary (64, 0x01);
        ActionsTypes::PutPlan  plan = CassoExplorerActions::PlanPut (L"C:\\host\\TOOL.CODE", binary, VolumeKind::Pascal);

        Assert::AreEqual (std::string ("TOOL.CODE"), plan.catalogName);
        Assert::IsFalse (plan.hasLoadAddress);
        Assert::IsFalse (plan.guessedAddress);
        Assert::IsTrue (plan.typeName.empty());
    }



    TEST_METHOD (TextPlansGetThePascalSuffix)
    {
        std::string            text = "HELLO\r\nWORLD\r\n";
        std::vector<Byte>      bytes (text.begin(), text.end());
        ActionsTypes::PutPlan  plan = CassoExplorerActions::PlanPut (L"C:\\host\\notes.txt", bytes, VolumeKind::Pascal);

        Assert::IsTrue (plan.encoding == DiskOperations::Encoding::Text);
        Assert::AreEqual (std::string ("NOTES.TEXT"), plan.catalogName);
    }



    TEST_METHOD (PropertyChoicesAreThePascalKinds)
    {
        std::vector<AppleFileProperties::TypeChoice>  choices = AppleFileProperties::GetTypeChoices (VolumeKind::Pascal, PascalVolume::kKindCode);

        Assert::AreEqual ((size_t) 9, choices.size());
        Assert::IsTrue (AppleFileProperties::GetAuxKind (VolumeKind::Pascal, PascalVolume::kKindCode) == AppleFileProperties::AuxKind::None);
        Assert::IsTrue (AppleFileProperties::IsValidName (VolumeKind::Pascal, L"A.CODE"));
        Assert::IsFalse (AppleFileProperties::IsValidName (VolumeKind::Pascal, L"A B"));
    }



    TEST_METHOD (CopiesMapTheTypeBetweenFileSystems)
    {
        FilePayload  payload;

        payload.type = PascalVolume::kKindCode;
        HostFileNaming::ConvertPayloadType (VolumeKind::Pascal, VolumeKind::ProDos, payload);
        Assert::AreEqual ((int) 0x02, (int) payload.type);

        payload = FilePayload();
        payload.type = PascalVolume::kKindCode;
        HostFileNaming::ConvertPayloadType (VolumeKind::Pascal, VolumeKind::Dos33, payload);
        Assert::AreEqual ((int) Dos33Volume::kTypeBinary, (int) payload.type);
        Assert::IsTrue (payload.hasLoadAddress, L"A DOS 3.3 binary has to record an address");

        payload = FilePayload();
        payload.type = ProDosVolume::kTypeText;
        HostFileNaming::ConvertPayloadType (VolumeKind::ProDos, VolumeKind::Pascal, payload);
        Assert::AreEqual ((int) PascalVolume::kKindText, (int) payload.type);

        payload = FilePayload();
        payload.type = ProDosVolume::kTypeBinary;
        HostFileNaming::ConvertPayloadType (VolumeKind::ProDos, VolumeKind::Pascal, payload);
        Assert::AreEqual ((int) PascalVolume::kKindNone, (int) payload.type, L"The name selects the kind");
    }



    TEST_METHOD (NamesFitWhenCopiedAcrossFileSystems)
    {
        Assert::AreEqual (std::string ("MY.GAME"), CassoExplorerActions::FitEntryName ("MY GAME", VolumeKind::Dos33, VolumeKind::Pascal));
        Assert::AreEqual (std::string ("A.VERY.LON.TEXT"), CassoExplorerActions::FitEntryName ("A.VERY.LONG.NAME.TEXT", VolumeKind::ProDos, VolumeKind::Pascal));
        Assert::AreEqual (std::string ("MY GAME"), CassoExplorerActions::FitEntryName ("MY GAME", VolumeKind::Dos33, VolumeKind::ProDos));
    }



    //  The full send's values.

    TEST_METHOD (AFullSendFindsTheCatalogOnTrackZero)
    {
        std::vector<Byte>  image = BuildSample();
        std::string        hex;

        Assert::IsTrue (FullSendDisk::IsCatalogOnTrackZero (image));
        Assert::IsFalse (FullSendDisk::NeedsDirectoryTrack (image));
        Assert::AreEqual (std::string ("APPLE0"), FullSendDisk::GetCatalogTitle (image));
        Assert::AreEqual (std::string (".dsk"), FullSendDisk::GetImageExtension (image, std::string()));
        Assert::AreEqual (std::string ("0005 APPLE0"), FullSendDisk::MakeImageTitle (5, FullSendDisk::GetCatalogTitle (image), std::string()));
        Assert::AreEqual (std::string (FullSendManifest::kStatusDamaged), std::string (FullSendDisk::AssessDamage (image, 0, 1)), L"A recognized file system");
    }



    //  Through the command line's verbs.

    static DiskCommandResult  RunVerb (FakeDiskFileIo & io, CommandLineOptions::DiskOptions::Command command, const char * path, const char * host = "", CommandLineOptions::DiskOptions::Encoding encoding = CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        CommandLineOptions  options = DiskOperations::MakeOptions (command, kImage);
        DiskCommandRunner   runner (io);

        options.disk.path     = path;
        options.disk.hostFile = host;
        options.disk.encoding = encoding;

        return runner.Run (options);
    }

    static void  Seed (FakeDiskFileIo & io)
    {
        std::vector<Byte>  image = BuildSample();

        io.files[kImage]  = image;
        io.stamps[kImage] = FileStamp { image.size(), 100 };
    }

    TEST_METHOD (TheCommandLineListsAVolume)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;

        Seed (io);

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::List, "");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("APPLE0:") != std::string::npos);
        Assert::IsTrue (result.output.find ("SYSTEM.PASCAL") != std::string::npos);
        Assert::IsTrue (result.output.find ("CODE") != std::string::npos);
        Assert::IsTrue (result.output.find ("265 blocks free of 280") != std::string::npos);
        Assert::IsTrue (result.output.find ("Not bootable") == std::string::npos);
    }

    TEST_METHOD (TheCommandLinePutsAndGetsAFile)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;

        Seed (io);

        io.files["C:\\host\\TOOL.CODE"] = { 1, 2, 3, 4, 5 };

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Put, "", "C:\\host\\TOOL.CODE");
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Get, "TOOL.CODE");
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.payload == std::vector<Byte> { 1, 2, 3, 4, 5 });

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Delete, "TOOL.CODE");
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Get, "TOOL.CODE");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
    }

    TEST_METHOD (TheCommandLineConvertsText)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;
        std::string        text = "PROGRAM A;\r\n  BEGIN\r\n";

        Seed (io);

        io.files["C:\\host\\PROG.TEXT"].assign (text.begin(), text.end());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Put, "", "C:\\host\\PROG.TEXT", CommandLineOptions::DiskOptions::Encoding::Text);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Get, "PROG.TEXT", "", CommandLineOptions::DiskOptions::Encoding::Text);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (text, std::string (result.payload.begin(), result.payload.end()));
    }

    TEST_METHOD (TheCommandLineRefusesAnAddressAndBasic)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options = DiskOperations::MakeOptions (CommandLineOptions::DiskOptions::Command::Put, kImage);
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        Seed (io);

        io.files["C:\\host\\TOOL.CODE"] = { 1 };

        options.disk.hostFile       = "C:\\host\\TOOL.CODE";
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
    }

    TEST_METHOD (TheCommandLineRefusesAStartupProgram)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;

        Seed (io);

        result = RunVerb (io, CommandLineOptions::DiskOptions::Command::Boot, "LOG.DATA");

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no setting for a startup program") != std::string::npos);
    }



    //  Through the window's operations.

    TEST_METHOD (TheOperationsListReadWriteAndRename)
    {
        FakeDiskFileIo          io;
        DiskOperations          operations (io);
        VolumeListing           listing;
        VolumeKind              kind    = VolumeKind::Unknown;
        FilePayload             read;
        FilePayload             payload = MakePayload (700, 3);
        DiskOperations::Result  result;

        Seed (io);

        result = operations.List (kImage, listing, kind);
        Assert::IsTrue (result.Succeeded());
        Assert::IsTrue (kind == VolumeKind::Pascal);
        Assert::AreEqual ((size_t) 3, listing.entries.size());

        result = operations.WritePayload (kImage, "NEW.DATA", payload);
        Assert::IsTrue (result.Succeeded());

        result = operations.Read (kImage, "NEW.DATA", read);
        Assert::IsTrue (result.Succeeded());
        Assert::IsTrue (read.bytes == payload.bytes);

        result = operations.Rename (kImage, "NEW.DATA", "OTHER.DATA");
        Assert::IsTrue (result.Succeeded());

        result = operations.Read (kImage, "OTHER.DATA", read);
        Assert::IsTrue (result.Succeeded());
    }



    //  Over a link, with a fake drive.

    struct FakeDrive : ILinkedSectorSource
    {
        std::vector<Byte>    image;
        std::vector<size_t>  written;

        HRESULT  ReadSectors (std::span<const size_t> sectors, std::vector<Byte> & outData) override
        {
            outData.clear();

            for (size_t sector : sectors)
            {
                outData.insert (outData.end(), image.begin() + (ptrdiff_t) (sector * kSectorBytes), image.begin() + (ptrdiff_t) ((sector + 1) * kSectorBytes));
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

    TEST_METHOD (ALinkedDiskListsAndReads)
    {
        FakeDrive      drive;
        LinkedDisk     disk (drive);
        VolumeListing  listing;
        FilePayload    payload;
        FilePayload    expected;

        drive.image = BuildSample();

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); }));

        Assert::IsTrue (disk.GetKind() == VolumeKind::Pascal);
        Assert::AreEqual ((size_t) 3, listing.entries.size());

        AssertSucceeded (disk.Run ([&] (const IVolume & volume) { return volume.Read (FilePath::Parse ("NOTES.TEXT"), payload); }));
        AssertSucceeded (PascalVolume (drive.image).Read (FilePath::Parse ("NOTES.TEXT"), expected));

        Assert::IsTrue (payload.bytes == expected.bytes);
    }

    TEST_METHOD (ALinkedDiskWritesOnlyWhatChangedAndTheDirectoryLast)
    {
        FakeDrive            drive;
        LinkedDisk           disk (drive);
        std::vector<size_t>  sent;
        std::vector<Byte>    expected;
        FilePayload          payload  = MakePayload (600, 5);
        std::vector<size_t>  catalog  = LinkedDisk::GetCatalogSectors (VolumeKind::Pascal);

        drive.image = BuildSample();

        AssertSucceeded (PascalVolume (drive.image).Write (FilePath::Parse ("NEW.DATA"), payload, expected));

        AssertSucceeded (disk.Write ([&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Write (FilePath::Parse ("NEW.DATA"), payload, outBuffer); }, sent));

        Assert::IsTrue (drive.image == expected);
        Assert::IsFalse (drive.written.empty());
        Assert::IsTrue (std::find (catalog.begin(), catalog.end(), drive.written.back()) != catalog.end(), L"The directory goes last");
        Assert::IsTrue (drive.written.size() < 16, L"Only the sectors changed");
    }

    TEST_METHOD (ALinkedDiskDeletesAndRenames)
    {
        FakeDrive            drive;
        LinkedDisk           disk (drive);
        std::vector<size_t>  sent;

        drive.image = BuildSample();

        AssertSucceeded (disk.Write ([&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Delete (FilePath::Parse ("NOTES.TEXT"), outBuffer); }, sent));
        Assert::IsTrue (GetNames (drive.image) == std::vector<std::string> { "SYSTEM.PASCAL", "LOG.DATA" });

        AssertSucceeded (disk.Write ([&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Rename (FilePath::Parse ("LOG.DATA"), "LOG2.DATA", outBuffer); }, sent));
        Assert::IsTrue (GetNames (drive.image) == std::vector<std::string> { "SYSTEM.PASCAL", "LOG2.DATA" });
    }
};
