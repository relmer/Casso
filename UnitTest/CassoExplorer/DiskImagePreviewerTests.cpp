#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/PartitionCard.h"
#include "BrowserPartsFixture.h"
#include "CassoExplorer/Browser/Preview/DiskImagePreviewer.h"
#include "Machines/Apple2/Common/PascalVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImagePreviewerTests
//
//  The preview of a disk image and of a file in one: partitions, WOZ details
//  and a file read from the image. The saved games are in
//  SavedGamePreviewTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskImagePreviewerTests)
{
public:

    static constexpr size_t  kDiskBytes = 143360;


    struct Rig
    {
        BrowserPartsFixture  fixture;
        DiskImagePreviewer   previewer { fixture.parts, fixture.previewParts };
    };


    //  A blank Pascal disk with one data file on it.
    static std::vector<Byte> MakeDisk (const char * name, const std::vector<Byte> & bytes)
    {
        std::vector<Byte>  disk (kDiskBytes, 0);
        std::vector<Byte>  next;
        FilePayload        payload;



        PascalVolume::WriteBlank ("DATA", PascalVolume::MakeDate (1981, 9, 1), disk);

        payload.bytes = bytes;
        payload.type  = PascalVolume::kKindData;

        AssertSucceeded (PascalVolume (disk).Write (FilePath::Parse (name), payload, next));

        return next;
    }


    TEST_METHOD (ImageOfPartitions_ListsEachPartition)
    {
        Rig                          rig;
        PartitionLayoutKind          layout = PartitionLayoutKind::None;
        std::vector<PartitionEntry>  entries;

        rig.fixture.Seed (PartitionCard::kCardName, PartitionCard::MakeCard());
        AssertSucceeded (rig.fixture.catalog.ListPartitions (PartitionCard::kCardPath, layout, entries));
        Assert::IsFalse (entries.empty(), L"The card has partitions to list");

        rig.previewer.PreviewImageCatalog (PartitionCard::kCardPath);

        const PreviewContent &  preview = rig.fixture.preview;

        Assert::IsTrue   (preview.kind == PreviewContent::Kind::Details);
        Assert::AreEqual (entries.size() + 1, preview.details.size(), L"The partition map and a line for each partition");
        Assert::AreEqual (std::wstring (L"Partition map"), preview.details[0].first);
    }


    TEST_METHOD (DiskWithAFileSystem_PreviewsItsCatalogWithNoWozPath)
    {
        Rig  rig;

        rig.fixture.Seed ("C:\\Disks\\notes.dsk", MakeDisk ("NOTES.DATA", std::vector<Byte> (100, 1)));
        rig.previewer.PreviewImageCatalog (L"C:\\Disks\\notes.dsk");

        Assert::IsTrue (rig.fixture.preview.kind == PreviewContent::Kind::Catalog);
        Assert::IsTrue (rig.fixture.preview.wozPath.empty(), L"Only a WOZ image has one");
    }


    TEST_METHOD (CatalogOfAnImage_IsCachedForTheNextPreview)
    {
        Rig            rig;
        VolumeListing  listing;
        VolumeKind     kind = VolumeKind::Unknown;

        rig.fixture.Seed ("C:\\Disks\\notes.dsk", MakeDisk ("NOTES.DATA", std::vector<Byte> (100, 1)));
        Assert::IsFalse (rig.fixture.model.TryGetCachedCatalog (L"C:\\Disks\\notes.dsk", listing, kind));

        rig.previewer.PreviewImageCatalog (L"C:\\Disks\\notes.dsk");

        Assert::IsTrue (rig.fixture.model.TryGetCachedCatalog (L"C:\\Disks\\notes.dsk", listing, kind));
        Assert::IsTrue (kind == VolumeKind::Pascal);
    }


    TEST_METHOD (FileInAnImage_PreviewsItsContents)
    {
        Rig            rig;
        ImageContents  contents;

        rig.fixture.Seed ("C:\\Disks\\notes.dsk", MakeDisk ("NOTES.DATA", std::vector<Byte> (100, 1)));
        AssertSucceeded (rig.fixture.catalog.Describe (L"C:\\Disks\\notes.dsk", std::string(), contents));
        Assert::IsFalse (contents.listing.entries.empty());

        rig.previewer.PreviewEntry (L"C:\\Disks\\notes.dsk", "NOTES.DATA", contents.listing.entries[0], contents.kind);

        Assert::IsFalse (rig.fixture.preview.kind == PreviewContent::Kind::Error);
        Assert::IsTrue  (rig.fixture.preview.message.empty());
    }


    TEST_METHOD (FileInAnImageThatCannotBeRead_PreviewsAsAnError)
    {
        Rig        rig;
        FileEntry  entry;

        entry.name = "NOTES.DATA";

        rig.previewer.PreviewEntry (L"C:\\Disks\\missing.dsk", "NOTES.DATA", entry, VolumeKind::Pascal);

        Assert::IsTrue  (rig.fixture.preview.kind == PreviewContent::Kind::Error);
        Assert::IsFalse (rig.fixture.preview.message.empty(), L"The reason is given");
    }
};
