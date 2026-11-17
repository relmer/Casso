#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "PartitionCard.h"
#include "Devices/Disk/Archives/ContainerPath.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccessTests
//
//  A MicroDrive card image and a UniDOS disk image built in memory: the list
//  of partitions, one partition's bytes as an image file, the file with a
//  partition replaced, and a container path with a partition step.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PartitionAccessTests)
{
public:

    TEST_METHOD (List_MicroDriveCard_HasEachPartitionWithItsStepSizeAndVolume)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeCard(), PartitionCard::kCardName, kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::MicroDrive);
        Assert::AreEqual (size_t (3), entries.size());

        Assert::AreEqual (std::string ("#1.po"), entries[0].step);
        Assert::AreEqual ((uint64_t) PartitionCard::kFloppyBytes, entries[0].byteCount);
        Assert::AreEqual (std::string ("FLOPPY"), entries[0].volumeName);

        Assert::AreEqual (std::string ("#2.po"), entries[1].step);
        Assert::AreEqual ((uint64_t) PartitionCard::kSecondBytes, entries[1].byteCount);
        Assert::AreEqual (std::string ("SECOND"), entries[1].volumeName);

        Assert::AreEqual (std::string ("#3.po"), entries[2].step);
        Assert::IsTrue (entries[2].partition.damaged);
        Assert::IsTrue (entries[2].volumeName.empty());
    }


    TEST_METHOD (List_GzipOfACard_ListsThePartitionsInside)
    {
        vector<Byte>            gzip;
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;

        GzipFile::Build (PartitionCard::MakeCard(), "card.po", gzip);

        AssertSucceeded (PartitionAccess::List (gzip, "C:\\Disks\\card.po.gz", kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::MicroDrive);
        Assert::AreEqual (size_t (3), entries.size());
    }


    TEST_METHOD (List_UniDosDisk_HasTwoDosPartitionsWithVolumeNumbers)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeUniDosDisk(), "unidos.po", kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::UniDos);
        Assert::AreEqual (size_t (2), entries.size());
        Assert::AreEqual (std::string ("#1.do"), entries[0].step);
        Assert::AreEqual (std::string ("#2.do"), entries[1].step);
        Assert::AreEqual ((uint64_t) 800 * PartitionCard::kBlock, entries[1].byteCount);
        Assert::IsTrue (entries[1].hasVolumeNumber);
        Assert::AreEqual ((int) 254, (int) entries[1].volumeNumber);
    }


    TEST_METHOD (List_PlainVolume_HasNoPartitions)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::Cffa;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeProDos (1600, "PLAIN"), "plain.po", kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::None);
        Assert::IsTrue (entries.empty());
    }


    TEST_METHOD (TryParseStepName_NumberAndImageExtension_IsAPartitionStep)
    {
        int  index = 0;

        Assert::IsTrue (PartitionAccess::TryParseStepName ("#1.po", index));
        Assert::AreEqual (1, index);

        Assert::IsTrue (PartitionAccess::TryParseStepName ("#12.DO", index));
        Assert::AreEqual (12, index);

        Assert::IsFalse (PartitionAccess::TryParseStepName ("#0.po", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("#.po", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("#1.dsk", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("#1x.po", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("1.po", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("#12345.po", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("HELLO", index));
        Assert::IsFalse (PartitionAccess::TryParseStepName ("", index));
    }


    TEST_METHOD (Extract_BlockPartition_IsTheVolumesImageFile)
    {
        vector<Byte>        bytes;
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        std::string         error;

        AssertSucceeded (PartitionAccess::Extract (PartitionCard::MakeCard(), PartitionCard::kCardName, 1, bytes, error));

        Assert::IsTrue (PartitionCard::MakeFloppyFile ("FLOPPY") == bytes);

        AssertSucceeded (VolumeImage::Load (bytes, "#1.po", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);
    }


    TEST_METHOD (Extract_DosPartition_LoadsAsADosImage)
    {
        vector<Byte>        bytes;
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        std::string         error;

        AssertSucceeded (PartitionAccess::Extract (PartitionCard::MakeUniDosDisk(), "unidos.po", 2, bytes, error));

        Assert::IsTrue (PartitionCard::MakeDos (50, 32) == bytes);

        AssertSucceeded (VolumeImage::Load (bytes, "#2.do", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33);
    }


    TEST_METHOD (Extract_NumberNotInTheMap_IsNotFound)
    {
        vector<Byte>  bytes;
        std::string   error;
        HRESULT       hr = PartitionAccess::Extract (PartitionCard::MakeCard(), PartitionCard::kCardName, 4, bytes, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr);
        Assert::AreEqual (std::string ("card.po has no partition 4."), error);
    }


    TEST_METHOD (Extract_PartitionPastTheEndOfTheImage_IsNotOpened)
    {
        vector<Byte>  bytes;
        std::string   error;
        HRESULT       hr = PartitionAccess::Extract (PartitionCard::MakeCard(), PartitionCard::kCardName, 3, bytes, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT), hr);
        Assert::AreEqual (std::string ("Partition 3 ends past the end of card.po."), error);
        Assert::IsTrue (bytes.empty());
    }


    TEST_METHOD (Replace_ImageOfThePartitionsSize_ChangesOnlyThatPartition)
    {
        vector<Byte>  card   = PartitionCard::MakeCard();
        vector<Byte>  fresh  = PartitionCard::MakeProDos (PartitionCard::kSecondBlocks, "FRESH");
        size_t        start  = (size_t) PartitionCard::kSecondStart * PartitionCard::kBlock;
        vector<Byte>  target = card;
        vector<Byte>  rebuilt;
        std::string   error;

        std::copy (fresh.begin(), fresh.end(), target.begin() + (ptrdiff_t) start);

        AssertSucceeded (PartitionAccess::Replace (card, PartitionCard::kCardName, 2, fresh, rebuilt, error));

        Assert::AreEqual (card.size(), rebuilt.size());
        Assert::IsTrue (target == rebuilt);
    }


    TEST_METHOD (Replace_ShorterImage_IsWrittenOverTheStartOfThePartition)
    {
        vector<Byte>  card   = PartitionCard::MakeCard();
        vector<Byte>  floppy = PartitionCard::MakeFloppyFile ("LITTLE");
        vector<Byte>  before = PartitionCard::GetSecond (card);
        vector<Byte>  after;
        vector<Byte>  rebuilt;
        std::string   error;

        AssertSucceeded (PartitionAccess::Replace (card, PartitionCard::kCardName, 2, floppy, rebuilt, error));

        after = PartitionCard::GetSecond (rebuilt);

        Assert::IsTrue (std::equal (floppy.begin(), floppy.end(), after.begin()));
        Assert::IsTrue (std::equal (before.begin() + (ptrdiff_t) PartitionCard::kFloppyBytes, before.end(), after.begin() + (ptrdiff_t) PartitionCard::kFloppyBytes));
    }


    TEST_METHOD (Replace_ImageLargerThanThePartition_IsBadLength)
    {
        vector<Byte>  card = PartitionCard::MakeCard();
        vector<Byte>  rebuilt;
        std::string   error;
        HRESULT       hr   = PartitionAccess::Replace (card, PartitionCard::kCardName, 1, PartitionCard::MakeProDos (PartitionCard::kSecondBlocks, "BIG"), rebuilt, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), hr);
        Assert::AreEqual (std::string ("The image is larger than partition 1 of card.po."), error);
        Assert::IsTrue (rebuilt.empty());
    }


    TEST_METHOD (ConvertImage_FloppyImageForABlockPartition_IsInBlockOrder)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind   = PartitionLayoutKind::None;
        vector<Byte>            floppy = PartitionCard::MakeFloppyFile ("LITTLE");
        vector<Byte>            converted;
        std::string             error;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeCard(), PartitionCard::kCardName, kind, entries));
        AssertSucceeded (PartitionAccess::ConvertImage (entries[1].partition, floppy, "little.po", converted, error));

        Assert::IsTrue (floppy == converted);
    }


    TEST_METHOD (ConvertImage_DosImageForADosPartition_IsInTrackAndSectorOrder)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;
        vector<Byte>            dos  = PartitionCard::MakeDos (35, 16);
        vector<Byte>            converted;
        std::string             error;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeUniDosDisk(), "unidos.po", kind, entries));
        AssertSucceeded (PartitionAccess::ConvertImage (entries[0].partition, dos, "dos.do", converted, error));

        Assert::IsTrue (dos == converted);
    }


    TEST_METHOD (ConvertImage_ImageLargerThanThePartition_IsBadLength)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;
        vector<Byte>            converted;
        std::string             error;
        HRESULT                 hr   = S_OK;

        AssertSucceeded (PartitionAccess::List (PartitionCard::MakeCard(), PartitionCard::kCardName, kind, entries));

        hr = PartitionAccess::ConvertImage (entries[0].partition, PartitionCard::MakeProDos (PartitionCard::kSecondBlocks, "BIG"), "big.po", converted, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), hr);
        Assert::AreEqual (std::string ("big.po is larger than partition 1."), error);
    }


    TEST_METHOD (Parse_NumberStepAfterAnImage_IsAPartitionStep)
    {
        ContainerPath  path = ContainerPath::Parse ("C:\\Disks\\card.po.gz|#2.po|HELLO");

        Assert::AreEqual (size_t (4), path.steps.size());
        Assert::IsTrue (path.steps[1].kind == ContainerStep::Kind::Gzip);
        Assert::IsTrue (path.steps[2].kind == ContainerStep::Kind::Partition);
        Assert::AreEqual (std::string ("#2.po"), path.steps[2].name);
        Assert::IsTrue (path.steps[3].kind == ContainerStep::Kind::ImageFile);
    }


    TEST_METHOD (Parse_NumberStepAfterAZip_IsAZipMember)
    {
        ContainerPath  path = ContainerPath::Parse ("C:\\Disks\\all.zip|#2.po");

        Assert::IsTrue (path.steps[1].kind == ContainerStep::Kind::ZipMember);
    }


    TEST_METHOD (Open_PartitionStep_IsThePartitionsImage_AndAFileInsideIt)
    {
        InMemoryFileSystem  fs;
        vector<Byte>        card = PartitionCard::MakeCard();
        OpenedContainer     opened;
        std::string         error;

        AssertSucceeded (fs.WriteAllText (PartitionCard::kCardPath, std::string (card.begin(), card.end())));

        AssertSucceeded (ContainerPath::Open (ContainerPath::Parse ("C:\\Disks\\card.po|#2.po"), fs, opened, error));
        Assert::IsTrue (PartitionCard::GetSecond (card) == opened.bytes);
        Assert::AreEqual (std::string ("#2.po"), opened.name);

        AssertSucceeded (ContainerPath::Open (ContainerPath::Parse ("C:\\Disks\\card.po|#2.po|HELLO"), fs, opened, error));
        Assert::IsTrue (opened.bytes == vector<Byte> (3000, 0x11));
    }


    TEST_METHOD (Open_DamagedPartitionStep_FailsWithTheReason)
    {
        InMemoryFileSystem  fs;
        vector<Byte>        card = PartitionCard::MakeCard();
        OpenedContainer     opened;
        std::string         error;
        HRESULT             hr   = S_OK;

        AssertSucceeded (fs.WriteAllText (PartitionCard::kCardPath, std::string (card.begin(), card.end())));

        hr = ContainerPath::Open (ContainerPath::Parse ("C:\\Disks\\card.po|#3.po"), fs, opened, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT), hr);
        Assert::AreEqual (std::string ("Partition 3 ends past the end of card.po."), error);
    }


    //  A file replaced inside a partition of a gzip card image. The other
    //  partition and the bytes after the last block are as they were.
    TEST_METHOD (Replace_FileInsideAPartitionOfAGzip_RebuildsTheCard)
    {
        InMemoryFileSystem  fs;
        ContainerPath       path   = ContainerPath::Parse ("C:\\Disks\\card.po.gz|#2.po|HELLO");
        vector<Byte>        card   = PartitionCard::MakeCard();
        vector<Byte>        edited (5000, 0x33);
        vector<Byte>        gzip;
        vector<Byte>        rebuilt;
        vector<Byte>        first;
        OpenedContainer     opened;
        std::string         gzipName;
        std::string         error;

        GzipFile::Build (card, "card.po", gzip);
        AssertSucceeded (fs.WriteAllText (PartitionCard::kGzipPath, std::string (gzip.begin(), gzip.end())));

        AssertSucceeded (ContainerPath::Replace (path, fs, edited, error));

        AssertSucceeded (ContainerPath::Open (path, fs, opened, error));
        Assert::IsTrue (opened.bytes == edited);

        AssertSucceeded (GzipFile::Expand (PartitionCard::ReadHostFile (fs, PartitionCard::kGzipPath), rebuilt, gzipName, error));
        Assert::AreEqual (card.size(), rebuilt.size());
        Assert::IsTrue (std::equal (card.end() - 100, card.end(), rebuilt.end() - 100));

        AssertSucceeded (PartitionAccess::Extract (rebuilt, PartitionCard::kCardName, 1, first, error));
        Assert::IsTrue (PartitionCard::MakeFloppyFile ("FLOPPY") == first);
    }
};
