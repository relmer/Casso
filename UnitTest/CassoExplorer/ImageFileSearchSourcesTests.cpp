#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/ImageFileSearchSources.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageFileSearchSourcesTests
//
//  File contents inside a disk image: a BASIC program searched detokenized,
//  a text file as text, and a binary file not at all.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageFileSearchSourcesTests)
{
public:

    static constexpr const char *  kImage = "C:\\prodos.po";

    struct Disk
    {
        FakeDiskFileIo  io;
        DiskOperations  operations { io };
        VolumeListing   listing;
        VolumeKind      kind       = VolumeKind::Unknown;
    };

    static void Open (Disk & disk)
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", bytes));
        disk.io.files[kImage]  = bytes;
        disk.io.stamps[kImage] = FileStamp { bytes.size(), 1 };

        Assert::IsTrue (disk.operations.List (kImage, disk.listing, disk.kind).Succeeded());
    }

    static const FileEntry & Find (const Disk & disk, const char * name)
    {
        for (const FileEntry & entry : disk.listing.entries)
        {
            if (entry.name == name)
            {
                return entry;
            }
        }

        Assert::Fail (L"The fixture has the file");
    }


    TEST_METHOD (BasicIsSearchedDetokenized)
    {
        Disk  disk;



        Open (disk);

        ImageFileSearchSources  hello (disk.operations, kImage, "HELLO", Find (disk, "HELLO"), disk.kind);

        Assert::IsTrue  (hello.ContainsText (L"cassque"));
        Assert::IsTrue  (hello.ContainsText (L"print"), L"A keyword is searched as BASIC lists it");
        Assert::IsFalse (hello.ContainsText (L"goodbye"));
    }


    TEST_METHOD (TextIsSearchedAsText)
    {
        Disk  disk;



        Open (disk);

        ImageFileSearchSources  notes (disk.operations, kImage, "NOTES", Find (disk, "NOTES"), disk.kind);

        Assert::IsTrue (notes.ContainsText (L"ordinary printable text"));
    }


    TEST_METHOD (BinaryIsNotSearched)
    {
        Disk          disk;
        std::wstring  text;



        Open (disk);

        Assert::IsFalse (ImageFileSearchSources::ReadText (disk.operations, kImage, "PICTURE", Find (disk, "PICTURE"), disk.kind, text));
        Assert::IsFalse (ImageFileSearchSources::ReadText (disk.operations, kImage, "ODD", Find (disk, "ODD"), disk.kind, text));
    }
};
