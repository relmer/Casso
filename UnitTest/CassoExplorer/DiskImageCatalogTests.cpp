#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/PartitionCard.h"
#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalogTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImageCatalogTests)
{
public:

    struct Rig
    {
        FakeDiskFileIo    io;
        DiskOperations    operations { io };
        DiskImageCatalog  catalog    { operations };

        void Seed (const char * path, const std::vector<Byte> & bytes)
        {
            io.files[path]  = bytes;
            io.stamps[path] = FileStamp { bytes.size(), 100 };
        }

        void SeedFixture (const char * path, const char * fixture)
        {
            FixtureProvider    fixtures;
            std::vector<Byte>  bytes;

            AssertSucceeded (fixtures.OpenFixture (fixture, bytes));
            Seed (path, bytes);
        }
    };



    TEST_METHOD (Describe_ListsTheSubdirectoriesOfAVolume)
    {
        Rig            rig;
        ImageContents  contents;

        rig.SeedFixture ("C:\\Disks\\prodos.po", "CassoExplorer/prodos.po");

        AssertSucceeded (rig.catalog.Describe (L"C:\\Disks\\prodos.po", std::string(), contents));

        Assert::IsTrue   (contents.kind == VolumeKind::ProDos);
        Assert::AreEqual ((size_t) 1, contents.subdirectories.size());
        Assert::AreEqual (std::string ("SUBDIR"), contents.subdirectories[0].name);
        Assert::AreEqual (std::string ("SUBDIR"), contents.subdirectories[0].path);
        Assert::IsFalse  (contents.subdirectories[0].hasSubdirectories);
        Assert::IsTrue   (contents.error.empty());
    }



    TEST_METHOD (Describe_ListsThePartitionsOfAnImageWithNoFileSystem)
    {
        Rig                          rig;
        ImageContents                contents;
        PartitionLayoutKind          layout = PartitionLayoutKind::None;
        std::vector<PartitionEntry>  entries;

        rig.Seed (PartitionCard::kCardName, PartitionCard::MakeCard());
        AssertSucceeded (rig.catalog.ListPartitions (PartitionCard::kCardPath, layout, entries));

        HRESULT  hr = rig.catalog.Describe (PartitionCard::kCardPath, std::string(), contents);

        Assert::IsTrue (FAILED (hr));

        Assert::IsTrue   (contents.layout == layout && layout != PartitionLayoutKind::None);
        Assert::IsFalse  (entries.empty());
        Assert::AreEqual (entries.size(), contents.partitions.size());
        Assert::IsFalse  (contents.broken, L"a partitioned image is a disk image");
    }



    TEST_METHOD (Describe_MarksAFileThatIsNotADiskImageBroken)
    {
        Rig                rig;
        ImageContents      contents;
        std::string        text  = "this is not a disk image";
        std::vector<Byte>  bytes (text.begin(), text.end());

        rig.Seed ("C:\\Disks\\notadisk.dsk", bytes);

        HRESULT  hr = rig.catalog.Describe (L"C:\\Disks\\notadisk.dsk", std::string(), contents);

        Assert::IsTrue (FAILED (hr));

        Assert::IsTrue (contents.broken);
        Assert::IsTrue (contents.error.starts_with (L"notadisk.dsk "), contents.error.c_str());
        Assert::IsTrue (contents.partitions.empty());
    }



    TEST_METHOD (Describe_ListsADirectoryInsideTheVolume)
    {
        Rig            rig;
        ImageContents  contents;

        rig.SeedFixture ("C:\\Disks\\prodos.po", "CassoExplorer/prodos.po");

        AssertSucceeded (rig.catalog.Describe (L"C:\\Disks\\prodos.po", "SUBDIR", contents));

        Assert::IsTrue (contents.subdirectories.empty());
        Assert::IsTrue (contents.partitions.empty(), L"partitions are listed for the volume directory only");
    }



    TEST_METHOD (PartitionLabel_HasTheNumberOnACardAndTheMapNameInAFile)
    {
        Rig                          rig;
        PartitionLayoutKind          layout = PartitionLayoutKind::None;
        std::vector<PartitionEntry>  entries;

        rig.Seed (PartitionCard::kCardName, PartitionCard::MakeCard());
        AssertSucceeded (rig.catalog.ListPartitions (PartitionCard::kCardPath, layout, entries));
        Assert::IsFalse (entries.empty());

        Assert::AreEqual (TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entries[0], true)),
                          DiskImageCatalog::GetPartitionLabel (L"\\\\.\\PhysicalDrive2", entries[0], true));
        Assert::AreEqual (TextEncoding::NarrowToWide (PartitionAccess::GetPartitionLabel (entries[0].partition)),
                          DiskImageCatalog::GetPartitionLabel (PartitionCard::kCardPath, entries[0], true));
        Assert::AreEqual (DiskImageCatalog::GetPartitionLabel (PartitionCard::kCardPath, entries[0], false) + L" " + DiskMessageText::kDamagedPartitionText,
                          DiskImageCatalog::DescribeDamagedPartition (PartitionCard::kCardPath, entries[0]));
    }
};
