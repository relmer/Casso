#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Archives/ArchiveVolume.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolumeTests
//
//  Archives opened through the image layer as volumes: listed with their
//  folders, read, written and renamed, and a ShrinkIt disk archive and a
//  gzip image opened as the disk inside them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ArchiveVolumeTests)
{
public:

    static std::vector<Byte> MakeArchive()
    {
        NuFxArchive        archive;
        NuFxRecord         a;
        NuFxRecord         b;
        std::vector<Byte>  bytes;



        a.fileType = 0x04;
        a.threads.push_back (NuFxArchive::MakeFilenameThread ("DOCS/README"));
        a.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDataFork, std::vector<Byte> (2000, 'A')));

        b.fileType  = 0x06;
        b.extraType = 0x2000;
        b.threads.push_back (NuFxArchive::MakeFilenameThread ("PROGRAM"));
        b.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDataFork, std::vector<Byte> (10, 0x60)));

        archive.records = { a, b };
        archive.Build (bytes);

        return bytes;
    }


    TEST_METHOD (Archive_ListsFoldersAndFiles)
    {
        std::vector<Byte>   file = MakeArchive();
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;
        VolumeListing       root;
        VolumeListing       docs;



        AssertSucceeded (VolumeImage::Load (file, "test.shk", sectors, report));
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Archive);

        auto  volume = VolumeImage::CreateVolume (VolumeKind::Archive, sectors);

        AssertSucceeded (volume->Enumerate (root));
        Assert::AreEqual ((size_t) 2, root.entries.size());
        Assert::IsTrue (root.entries[0].isDirectory);
        Assert::AreEqual (std::string ("DOCS"), root.entries[0].name);

        AssertSucceeded (volume->EnumerateDirectory (FilePath::Parse ("DOCS"), docs));
        Assert::AreEqual ((size_t) 1, docs.entries.size());
        Assert::AreEqual ((uint32_t) 2000, docs.entries[0].eofBytes);
    }


    TEST_METHOD (Archive_WritesRenamesAndDeletes)
    {
        std::vector<Byte>   file = MakeArchive();
        std::vector<Byte>   written;
        std::vector<Byte>   renamed;
        std::vector<Byte>   deleted;
        std::vector<Byte>   saved;
        FilePayload         payload;
        FilePayload         read;
        VolumeListing       listing;
        std::string         reason;



        payload.type = 0x04;
        payload.bytes.assign (5000, 'Z');

        AssertSucceeded (ArchiveVolume (file).Write (FilePath::Parse ("DOCS/NEW"), payload, written));
        AssertSucceeded (ArchiveVolume (written).Read (FilePath::Parse ("docs/new"), read));
        Assert::IsTrue (read.bytes == payload.bytes);

        AssertSucceeded (ArchiveVolume (written).Rename (FilePath::Parse ("PROGRAM"), "TOOL", renamed));
        AssertSucceeded (ArchiveVolume (renamed).Read (FilePath::Parse ("TOOL"), read));
        Assert::AreEqual ((Word) 0x2000, read.loadAddress);

        AssertSucceeded (ArchiveVolume (renamed).Delete (FilePath::Parse ("TOOL"), deleted));
        AssertSucceeded (ArchiveVolume (deleted).Enumerate (listing));
        Assert::AreEqual ((size_t) 1, listing.entries.size());

        AssertSucceeded (VolumeImage::Save (file, "test.shk", deleted, saved, reason));
        Assert::IsTrue (saved == deleted);
    }


    TEST_METHOD (ShrinkItDisk_OpensAsTheDiskInside)
    {
        NuFxArchive         archive;
        NuFxRecord          record;
        std::vector<Byte>   volume (1600 * 512, 0);
        std::vector<Byte>   file;
        std::vector<Byte>   sectors;
        std::vector<Byte>   saved;
        std::vector<Byte>   again;
        SectorDecodeReport  report;
        std::string         reason;



        AssertSucceeded (ProDosSkeleton::Write (volume, "SHRUNK"));

        record.extraType   = 1600;
        record.storageType = 512;
        record.threads.push_back (NuFxArchive::MakeFilenameThread ("SHRUNK"));
        record.threads.push_back (NuFxArchive::MakeDataThread (NuFxThread::kKindDiskImage, volume));
        archive.records.push_back (record);
        archive.Build (file);

        AssertSucceeded (VolumeImage::Load (file, "disk.sdk", sectors, report));
        Assert::IsTrue (sectors == volume);
        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::ProDos);

        sectors[600 * 512] = 0x42;

        AssertSucceeded (VolumeImage::Save (file, "disk.sdk", sectors, saved, reason));
        AssertSucceeded (VolumeImage::Load (saved, "disk.sdk", again, report));
        Assert::IsTrue (again == sectors);
    }


    TEST_METHOD (Gzip_OpensTheImageInside)
    {
        std::vector<Byte>   volume (1600 * 512, 0);
        std::vector<Byte>   file;
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;



        AssertSucceeded (ProDosSkeleton::Write (volume, "ZIPPED"));
        GzipFile::Build (volume, "disk.po", file);

        AssertSucceeded (VolumeImage::Load (file, "disk.po.gz", sectors, report));
        Assert::IsTrue (sectors == volume);
    }
};