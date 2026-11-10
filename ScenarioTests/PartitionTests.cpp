#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Devices/Disk/Partitions/PartitionLayout.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/RawDevice/Win32RawDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionTests
//
//  CiderPress II's partitioned disks, compared with the volumes and free
//  space its catalog lists for each. The disks come from
//  scripts/FetchCiderPressTestFiles.ps1; each test is skipped when its disk
//  is absent.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PartitionTests)
{
public:

    static constexpr const char *  kRoot = "CiderPress";

    static bool OpenImage (const std::string & image, std::vector<Byte> & outBytes)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;
        std::string        name;
        std::string        error;
        HRESULT            hrOpen = S_OK;



        hrOpen = fixtures.OpenFixture (std::string (kRoot) + "/" + image, bytes);

        if (FAILED (hrOpen))
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine; run scripts/FetchCiderPressTestFiles.ps1\n", image).c_str());
            return false;
        }

        if (GzipFile::IsGzip (bytes))
        {
            AssertSucceeded (GzipFile::Expand (bytes, outBytes, name, error), TextEncoding::Utf8ToWide (error).c_str());
        }
        else
        {
            outBytes = std::move (bytes);
        }

        return true;
    }

    //  The free units of every partition, in the order of the list.
    static void ExpectPartitions (const std::string            & image,
                                  PartitionLayoutKind            layout,
                                  VolumeKind                     kind,
                                  const std::vector<uint32_t>  & freeUnits)
    {
        std::vector<Byte>       bytes;
        std::vector<Partition>  partitions;
        PartitionLayoutKind     found = PartitionLayoutKind::None;
        std::string             listed;
        size_t                  i     = 0;



        if (!OpenImage (image, bytes))
        {
            return;
        }

        MemoryBlockStore  store (std::move (bytes));

        AssertSucceeded (PartitionLayout::Detect (store, found, partitions));

        for (i = 0; i < partitions.size(); i++)
        {
            std::vector<Byte>         sectors;
            std::unique_ptr<IVolume>  volume;
            VolumeListing             listing;

            AssertSucceeded (PartitionLayout::ReadPartition (store, partitions[i], sectors));

            volume = VolumeImage::CreateVolume (partitions[i].volumeKind, sectors);

            if (volume != nullptr)
            {
                AssertSucceeded (volume->Enumerate (listing));
            }

            listed += std::format ("{}: block {} count {} kind {} free {}\n",
                                   partitions[i].index,
                                   partitions[i].firstBlock,
                                   partitions[i].blockCount,
                                   (int) partitions[i].volumeKind,
                                   listing.freeUnits);

            if (i < freeUnits.size())
            {
                Assert::IsTrue (partitions[i].volumeKind == kind, TextEncoding::Utf8ToWide (listed).c_str());
                Assert::IsFalse (partitions[i].damaged);
                Assert::AreEqual (freeUnits[i], listing.freeUnits, TextEncoding::Utf8ToWide (listed).c_str());
            }
        }

        Logger::WriteMessage (listed.c_str());

        Assert::IsTrue (found == layout, TextEncoding::Utf8ToWide (listed).c_str());
        Assert::AreEqual (freeUnits.size(), partitions.size(), TextEncoding::Utf8ToWide (listed).c_str());
    }


    TEST_METHOD (MicroDrive120Mb_HasTenProDosVolumes)
    {
        ExpectPartitions ("microdrive/HD120MB.po.gz",
                          PartitionLayoutKind::MicroDrive,
                          VolumeKind::ProDos,
                          { 40944, 40944, 20469, 20469, 20469, 20469, 20469, 18421, 18165, 16549 });
    }


    TEST_METHOD (AmDosSample_HasTwoDosVolumes)
    {
        ExpectPartitions ("dos800/AmDOS-sample.po.gz", PartitionLayoutKind::AmDos, VolumeKind::Dos33, { 1513, 1534 });
    }


    TEST_METHOD (UniDosSample_HasTwoDosVolumes)
    {
        ExpectPartitions ("dos800/UniDOS-sample.po.gz", PartitionLayoutKind::UniDos, VolumeKind::Dos33, { 1502, 1534 });
    }


    TEST_METHOD (OzDosSample_HasTwoDosVolumesInHalfBlocks)
    {
        ExpectPartitions ("dos800/OzDOS-sample.po.gz", PartitionLayoutKind::OzDos, VolumeKind::Dos33, { 1534, 1534 });
    }


    TEST_METHOD (OzDosHalf_IsOneVolumeAndNotPartitioned)
    {
        ExpectPartitions ("dos800/OzDOS-half.do.gz", PartitionLayoutKind::None, VolumeKind::Dos33, {});
    }


    TEST_METHOD (DosMaster140x5_HasFiveDosVolumes)
    {
        ExpectPartitions ("dos.master/dm-140x5.po.gz", PartitionLayoutKind::DosMaster, VolumeKind::Dos33, { 526, 526, 526, 526, 526 });
    }


    TEST_METHOD (DosMaster160x5Plus_HasFiveDosVolumes)
    {
        ExpectPartitions ("dos.master/dm-160x5+.po.gz", PartitionLayoutKind::DosMaster, VolumeKind::Dos33, { 494, 606, 606, 606, 606 });
    }


    TEST_METHOD (DosMaster400x2Plus_HasTwoDosVolumes)
    {
        ExpectPartitions ("dos.master/dm-400x2+.po.gz", PartitionLayoutKind::DosMaster, VolumeKind::Dos33, { 1422, 1534 });
    }


    //  A DOS 3.3 volume and a ProDOS volume on one 140K disk. The free space
    //  and files are those of CiderPress II's catalog.
    TEST_METHOD (DosProDosHybrid_HasBothVolumes)
    {
        static constexpr const char *  kImage = "hybrid/dos-prodos.do";



        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::vector<VolumeKind>   kinds;
        std::unique_ptr<IVolume>  volume;
        SectorDecodeReport        report;
        VolumeListing             dosListing;
        VolumeListing             proDosListing;
        FilePayload               hello;



        if (!OpenImage (kImage, bytes))
        {
            return;
        }

        AssertSucceeded (VolumeImage::Load (bytes, kImage, sectors, report));

        VolumeImage::DetectFilesystems (sectors, kinds);

        Assert::AreEqual (size_t (2), kinds.size());
        Assert::IsTrue (kinds[0] == VolumeKind::Dos33);
        Assert::IsTrue (kinds[1] == VolumeKind::ProDos);

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        AssertSucceeded (volume->Enumerate (dosListing));
        AssertSucceeded (volume->Read (FilePath::Parse ("HELLO"), hello));

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, sectors);
        AssertSucceeded (volume->Enumerate (proDosListing));

        Assert::AreEqual (size_t (1), dosListing.entries.size());
        Assert::AreEqual (270u, dosListing.freeUnits);
        Assert::AreEqual (size_t (39), hello.bytes.size());
        Assert::AreEqual (size_t (4), proDosListing.entries.size());
        Assert::AreEqual (58u, proDosListing.freeUnits);
    }


    TEST_METHOD (DosProDosHybrid_FileWrittenToEachVolume_FilesOfTheOtherAreIntact)
    {
        static constexpr const char *  kImage = "hybrid/dos-prodos.do";



        std::vector<Byte>         bytes;
        std::vector<Byte>         sectors;
        std::vector<Byte>         withDos;
        std::vector<Byte>         withBoth;
        std::unique_ptr<IVolume>  volume;
        SectorDecodeReport        report;
        VolumeListing             listing;
        FilePayload               payload;
        FilePayload               before;
        FilePayload               after;



        if (!OpenImage (kImage, bytes))
        {
            return;
        }

        AssertSucceeded (VolumeImage::Load (bytes, kImage, sectors, report));

        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (20000, 0xA5);

        payload.type = Dos33Volume::kTypeBinary;
        volume       = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        AssertSucceeded (volume->Write (FilePath::Parse ("CASSO"), payload, withDos));

        payload.type = ProDosVolume::kTypeBinary;
        volume       = VolumeImage::CreateVolume (VolumeKind::ProDos, withDos);
        AssertSucceeded (volume->Write (FilePath::Parse ("CASSO"), payload, withBoth));

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, sectors);
        AssertSucceeded (volume->Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            std::unique_ptr<IVolume>  edited = VolumeImage::CreateVolume (VolumeKind::ProDos, withBoth);

            AssertSucceeded (volume->Read (FilePath::Parse (entry.name), before));
            AssertSucceeded (edited->Read (FilePath::Parse (entry.name), after));

            Assert::IsTrue (before.bytes == after.bytes, TextEncoding::Utf8ToWide (entry.name).c_str());
        }

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, sectors);
        AssertSucceeded (volume->Read (FilePath::Parse ("HELLO"), before));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, withBoth);
        AssertSucceeded (volume->Read (FilePath::Parse ("HELLO"), after));
        Assert::IsTrue (before.bytes == after.bytes);

        AssertSucceeded (volume->Read (FilePath::Parse ("CASSO"), after));
        Assert::IsTrue (payload.bytes == after.bytes);

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, withBoth);
        AssertSucceeded (volume->Read (FilePath::Parse ("CASSO"), after));
        Assert::IsTrue (payload.bytes == after.bytes);
    }


    //  The drives of this PC, queried through handles with no access. No
    //  drive is read or written: a fixed drive fails to open before any
    //  access is requested.
    TEST_METHOD (PhysicalDrives_FixedDriveIsDeniedAndEveryDriveHasASize)
    {
        Win32RawDevice              device;
        std::vector<PhysicalDrive>  drives;
        PhysicalDrive               opened;
        bool                        anyFixed = false;



        AssertSucceeded (device.ListDrives (drives));
        Assert::IsFalse (drives.empty());

        for (const PhysicalDrive & drive : drives)
        {
            Assert::IsTrue (drive.bytes > 0);

            if (!drive.removable)
            {
                Assert::AreEqual (E_ACCESSDENIED, device.Open (drive.number, opened));
                anyFixed = true;
            }
        }

        Assert::IsTrue (anyFixed);
    }
};