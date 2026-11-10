#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
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

    static constexpr size_t           kBlock        = IBlockStore::kBlockBytes;
    static constexpr size_t           kFloppyBytes  = 143360;
    static constexpr uint32_t         kCardBlocks   = 2000;
    static constexpr uint32_t         kFirstStart   = 8;
    static constexpr uint32_t         kSecondStart  = 300;
    static constexpr uint32_t         kSecondBlocks = 800;
    static constexpr size_t           kSecondBytes  = (size_t) kSecondBlocks * kBlock;
    static constexpr const char *     kCardName     = "C:\\Disks\\card.po";
    static constexpr const wchar_t *  kCardPath     = L"C:\\Disks\\card.po";
    static constexpr const wchar_t *  kGzipPath     = L"C:\\Disks\\card.po.gz";

    static void WriteLong (vector<Byte> & bytes, size_t offset, uint32_t value)
    {
        bytes[offset]     = (Byte) (value & 0xFF);
        bytes[offset + 1] = (Byte) ((value >> 8) & 0xFF);
        bytes[offset + 2] = (Byte) ((value >> 16) & 0xFF);
        bytes[offset + 3] = (Byte) ((value >> 24) & 0xFF);
    }

    static void Place (vector<Byte> & bytes, uint32_t block, const vector<Byte> & source)
    {
        std::copy (source.begin(), source.end(), bytes.begin() + (ptrdiff_t) ((size_t) block * kBlock));
    }

    static vector<Byte> MakeProDos (uint32_t blocks, const char * volumeName)
    {
        vector<Byte>  buffer ((size_t) blocks * kBlock, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volumeName));

        return buffer;
    }

    //  A 5.25" ProDOS volume as the bytes of a `.po` file.
    static vector<Byte> MakeFloppyFile (const char * volumeName)
    {
        vector<Byte>  logical = MakeProDos (PartitionLayout::kFloppyBlocks, volumeName);
        vector<Byte>  file;

        VolumeImage::DosLogicalToProDosFile (logical, file);

        return file;
    }

    static vector<Byte> MakeDos (int tracks, int sectorsPerTrack)
    {
        Dos33Geometry  geometry = Dos33Geometry::Make (tracks, sectorsPerTrack);
        vector<Byte>   buffer (geometry.GetImageByteSize(), 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254, geometry));

        return buffer;
    }

    static FilePayload MakePayload (size_t bytes, Byte fill)
    {
        FilePayload  payload;

        payload.bytes.assign (bytes, fill);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }

    //  The second partition's volume, with a file HELLO.
    static vector<Byte> MakeSecondVolume()
    {
        vector<Byte>  volume = MakeProDos (kSecondBlocks, "SECOND");
        vector<Byte>  withHello;

        AssertSucceeded (ProDosVolume (volume).Write (FilePath::Parse ("HELLO"), MakePayload (3000, 0x11), withHello));

        return withHello;
    }

    //  A map of three partitions: a 5.25" volume, an 800K volume, and one that
    //  ends past the end of the card. Bytes after the last whole block follow.
    static vector<Byte> MakeCard()
    {
        vector<Byte>  bytes ((size_t) kCardBlocks * kBlock, 0);

        bytes[0]    = 0xCA;
        bytes[1]    = 0xCC;
        bytes[0x0C] = 2;
        bytes[0x0D] = 1;

        WriteLong (bytes, 0x20, kFirstStart);
        WriteLong (bytes, 0x40, PartitionLayout::kFloppyBlocks);
        WriteLong (bytes, 0x24, kSecondStart);
        WriteLong (bytes, 0x44, kSecondBlocks);
        WriteLong (bytes, 0x80, 1900);
        WriteLong (bytes, 0xA0, 500);

        Place (bytes, kFirstStart, MakeFloppyFile ("FLOPPY"));
        Place (bytes, kSecondStart, MakeSecondVolume());

        bytes.insert (bytes.end(), 100, 0x5A);

        return bytes;
    }

    //  Two volumes of 50 tracks and 32 sectors, one after the other.
    static vector<Byte> MakeUniDosDisk()
    {
        vector<Byte>  volume = MakeDos (50, 32);
        vector<Byte>  bytes ((size_t) 1600 * kBlock, 0);

        Place (bytes, 0, volume);
        Place (bytes, 800, volume);

        return bytes;
    }

    static vector<Byte> GetSecond (const vector<Byte> & card)
    {
        ptrdiff_t  start = (ptrdiff_t) ((size_t) kSecondStart * kBlock);

        return vector<Byte> (card.begin() + start, card.begin() + start + (ptrdiff_t) kSecondBytes);
    }

    static vector<Byte> ReadHostFile (InMemoryFileSystem & fs, const wchar_t * path)
    {
        std::string  content;

        AssertSucceeded (fs.ReadAllText (path, content));

        return vector<Byte> (content.begin(), content.end());
    }


    TEST_METHOD (List_MicroDriveCard_HasEachPartitionWithItsStepSizeAndVolume)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;

        AssertSucceeded (PartitionAccess::List (MakeCard(), kCardName, kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::MicroDrive);
        Assert::AreEqual (size_t (3), entries.size());

        Assert::AreEqual (std::string ("#1.po"), entries[0].step);
        Assert::AreEqual ((uint64_t) kFloppyBytes, entries[0].byteCount);
        Assert::AreEqual (std::string ("FLOPPY"), entries[0].volumeName);

        Assert::AreEqual (std::string ("#2.po"), entries[1].step);
        Assert::AreEqual ((uint64_t) kSecondBytes, entries[1].byteCount);
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

        GzipFile::Build (MakeCard(), "card.po", gzip);

        AssertSucceeded (PartitionAccess::List (gzip, "C:\\Disks\\card.po.gz", kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::MicroDrive);
        Assert::AreEqual (size_t (3), entries.size());
    }


    TEST_METHOD (List_UniDosDisk_HasTwoDosPartitionsWithVolumeNumbers)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;

        AssertSucceeded (PartitionAccess::List (MakeUniDosDisk(), "unidos.po", kind, entries));

        Assert::IsTrue (kind == PartitionLayoutKind::UniDos);
        Assert::AreEqual (size_t (2), entries.size());
        Assert::AreEqual (std::string ("#1.do"), entries[0].step);
        Assert::AreEqual (std::string ("#2.do"), entries[1].step);
        Assert::AreEqual ((uint64_t) 800 * kBlock, entries[1].byteCount);
        Assert::IsTrue (entries[1].hasVolumeNumber);
        Assert::AreEqual ((int) 254, (int) entries[1].volumeNumber);
    }


    TEST_METHOD (List_PlainVolume_HasNoPartitions)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::Cffa;

        AssertSucceeded (PartitionAccess::List (MakeProDos (1600, "PLAIN"), "plain.po", kind, entries));

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

        AssertSucceeded (PartitionAccess::Extract (MakeCard(), kCardName, 1, bytes, error));

        Assert::IsTrue (MakeFloppyFile ("FLOPPY") == bytes);

        AssertSucceeded (VolumeImage::Load (bytes, "#1.po", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);
    }


    TEST_METHOD (Extract_DosPartition_LoadsAsADosImage)
    {
        vector<Byte>        bytes;
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        std::string         error;

        AssertSucceeded (PartitionAccess::Extract (MakeUniDosDisk(), "unidos.po", 2, bytes, error));

        Assert::IsTrue (MakeDos (50, 32) == bytes);

        AssertSucceeded (VolumeImage::Load (bytes, "#2.do", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33);
    }


    TEST_METHOD (Extract_NumberNotInTheMap_IsNotFound)
    {
        vector<Byte>  bytes;
        std::string   error;
        HRESULT       hr = PartitionAccess::Extract (MakeCard(), kCardName, 4, bytes, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), hr);
        Assert::AreEqual (std::string ("card.po has no partition 4."), error);
    }


    TEST_METHOD (Extract_PartitionPastTheEndOfTheImage_IsNotOpened)
    {
        vector<Byte>  bytes;
        std::string   error;
        HRESULT       hr = PartitionAccess::Extract (MakeCard(), kCardName, 3, bytes, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT), hr);
        Assert::AreEqual (std::string ("Partition 3 ends past the end of card.po."), error);
        Assert::IsTrue (bytes.empty());
    }


    TEST_METHOD (Replace_ImageOfThePartitionsSize_ChangesOnlyThatPartition)
    {
        vector<Byte>  card   = MakeCard();
        vector<Byte>  fresh  = MakeProDos (kSecondBlocks, "FRESH");
        size_t        start  = (size_t) kSecondStart * kBlock;
        vector<Byte>  target = card;
        vector<Byte>  rebuilt;
        std::string   error;

        std::copy (fresh.begin(), fresh.end(), target.begin() + (ptrdiff_t) start);

        AssertSucceeded (PartitionAccess::Replace (card, kCardName, 2, fresh, rebuilt, error));

        Assert::AreEqual (card.size(), rebuilt.size());
        Assert::IsTrue (target == rebuilt);
    }


    TEST_METHOD (Replace_ShorterImage_IsWrittenOverTheStartOfThePartition)
    {
        vector<Byte>  card   = MakeCard();
        vector<Byte>  floppy = MakeFloppyFile ("LITTLE");
        vector<Byte>  before = GetSecond (card);
        vector<Byte>  after;
        vector<Byte>  rebuilt;
        std::string   error;

        AssertSucceeded (PartitionAccess::Replace (card, kCardName, 2, floppy, rebuilt, error));

        after = GetSecond (rebuilt);

        Assert::IsTrue (std::equal (floppy.begin(), floppy.end(), after.begin()));
        Assert::IsTrue (std::equal (before.begin() + (ptrdiff_t) kFloppyBytes, before.end(), after.begin() + (ptrdiff_t) kFloppyBytes));
    }


    TEST_METHOD (Replace_ImageLargerThanThePartition_IsBadLength)
    {
        vector<Byte>  card = MakeCard();
        vector<Byte>  rebuilt;
        std::string   error;
        HRESULT       hr   = PartitionAccess::Replace (card, kCardName, 1, MakeProDos (kSecondBlocks, "BIG"), rebuilt, error);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), hr);
        Assert::AreEqual (std::string ("The image is larger than partition 1 of card.po."), error);
        Assert::IsTrue (rebuilt.empty());
    }


    TEST_METHOD (ConvertImage_FloppyImageForABlockPartition_IsInBlockOrder)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind   = PartitionLayoutKind::None;
        vector<Byte>            floppy = MakeFloppyFile ("LITTLE");
        vector<Byte>            converted;
        std::string             error;

        AssertSucceeded (PartitionAccess::List (MakeCard(), kCardName, kind, entries));
        AssertSucceeded (PartitionAccess::ConvertImage (entries[1].partition, floppy, "little.po", converted, error));

        Assert::IsTrue (floppy == converted);
    }


    TEST_METHOD (ConvertImage_DosImageForADosPartition_IsInTrackAndSectorOrder)
    {
        vector<PartitionEntry>  entries;
        PartitionLayoutKind     kind = PartitionLayoutKind::None;
        vector<Byte>            dos  = MakeDos (35, 16);
        vector<Byte>            converted;
        std::string             error;

        AssertSucceeded (PartitionAccess::List (MakeUniDosDisk(), "unidos.po", kind, entries));
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

        AssertSucceeded (PartitionAccess::List (MakeCard(), kCardName, kind, entries));

        hr = PartitionAccess::ConvertImage (entries[0].partition, MakeProDos (kSecondBlocks, "BIG"), "big.po", converted, error);

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
        vector<Byte>        card = MakeCard();
        OpenedContainer     opened;
        std::string         error;

        AssertSucceeded (fs.WriteAllText (kCardPath, std::string (card.begin(), card.end())));

        AssertSucceeded (ContainerPath::Open (ContainerPath::Parse ("C:\\Disks\\card.po|#2.po"), fs, opened, error));
        Assert::IsTrue (GetSecond (card) == opened.bytes);
        Assert::AreEqual (std::string ("#2.po"), opened.name);

        AssertSucceeded (ContainerPath::Open (ContainerPath::Parse ("C:\\Disks\\card.po|#2.po|HELLO"), fs, opened, error));
        Assert::IsTrue (opened.bytes == vector<Byte> (3000, 0x11));
    }


    TEST_METHOD (Open_DamagedPartitionStep_FailsWithTheReason)
    {
        InMemoryFileSystem  fs;
        vector<Byte>        card = MakeCard();
        OpenedContainer     opened;
        std::string         error;
        HRESULT             hr   = S_OK;

        AssertSucceeded (fs.WriteAllText (kCardPath, std::string (card.begin(), card.end())));

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
        vector<Byte>        card   = MakeCard();
        vector<Byte>        edited (5000, 0x33);
        vector<Byte>        gzip;
        vector<Byte>        rebuilt;
        vector<Byte>        first;
        OpenedContainer     opened;
        std::string         gzipName;
        std::string         error;

        GzipFile::Build (card, "card.po", gzip);
        AssertSucceeded (fs.WriteAllText (kGzipPath, std::string (gzip.begin(), gzip.end())));

        AssertSucceeded (ContainerPath::Replace (path, fs, edited, error));

        AssertSucceeded (ContainerPath::Open (path, fs, opened, error));
        Assert::IsTrue (opened.bytes == edited);

        AssertSucceeded (GzipFile::Expand (ReadHostFile (fs, kGzipPath), rebuilt, gzipName, error));
        Assert::AreEqual (card.size(), rebuilt.size());
        Assert::IsTrue (std::equal (card.end() - 100, card.end(), rebuilt.end() - 100));

        AssertSucceeded (PartitionAccess::Extract (rebuilt, kCardName, 1, first, error));
        Assert::IsTrue (MakeFloppyFile ("FLOPPY") == first);
    }
};
