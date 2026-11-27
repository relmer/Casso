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



    TEST_METHOD (Describe_MarksAPartitionThatEndsPastTheEndOfTheFileDamaged)
    {
        static constexpr size_t  kCutBytes = 2400 * 512;

        Rig                rig;
        ImageContents      contents;
        ImageContents      damaged;
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;
        HRESULT            hr = S_OK;

        //  CiderPress II's test files aren't committed, so the test is
        //  skipped until scripts/FetchCiderPressTestFiles.ps1 runs.
        hr = fixtures.OpenFixture ("CiderPress/turducken/MultiPart.hdv", bytes);

        if (FAILED (hr))
        {
            Logger::WriteMessage (L"Skipped: run scripts/FetchCiderPressTestFiles.ps1 for MultiPart.hdv");
            return;
        }

        bytes.resize (kCutBytes);
        rig.Seed ("C:\\Disks\\cut.hdv", bytes);

        hr = rig.catalog.Describe (L"C:\\Disks\\cut.hdv", std::string(), contents);
        IGNORE_RETURN_VALUE (hr, S_OK);

        Assert::AreEqual ((size_t) 2, contents.partitions.size());
        Assert::IsFalse  (contents.partitions[0].partition.damaged, L"the ProDOS partition is whole");
        Assert::IsTrue   (contents.partitions[1].partition.damaged, L"the HFS partition ends past the end of the file");

        //  The partition itself fails to open with the same reason, by its label.
        hr = rig.catalog.Describe (L"C:\\Disks\\cut.hdv|" + TextEncoding::NarrowToWide (contents.partitions[1].step), std::string(), damaged);
        Assert::IsTrue   (FAILED (hr));
        Assert::AreEqual (DiskImageCatalog::DescribeDamagedPartition (L"C:\\Disks\\cut.hdv", contents.partitions[1]), damaged.error);
        Assert::IsTrue   (damaged.error.find (L'|') == std::wstring::npos, damaged.error.c_str());
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



    TEST_METHOD (PartitionTitle_IsThePartitionLabelForAStepPathAndEmptyForAFile)
    {
        Rig                          rig;
        PartitionLayoutKind          layout = PartitionLayoutKind::None;
        std::vector<PartitionEntry>  entries;
        std::wstring                 card   = PartitionCard::kCardPath;

        rig.Seed (PartitionCard::kCardName, PartitionCard::MakeCard());
        AssertSucceeded (rig.catalog.ListPartitions (card, layout, entries));
        Assert::IsFalse (entries.empty());

        Assert::AreEqual (DiskImageCatalog::GetPartitionLabel (card, entries[0], false),
                          DiskImageCatalog::GetPartitionTitle (rig.io, card + L"|" + TextEncoding::NarrowToWide (entries[0].step)));
        Assert::IsTrue   (DiskImageCatalog::GetPartitionTitle (rig.io, card).empty());
    }
};
