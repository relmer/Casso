#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Devices/Disk/Partitions/PartitionLayout.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

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
};