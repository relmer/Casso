#include "Pch.h"

#include "EhmTestHelper.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RealPascalCpmDiskTests
//
//  Disk images from the owner's collection, which aren't in the repository
//  because their software isn't ours to distribute. Each test skips, with a message, on a
//  machine without the file. They read the images and never write them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RealPascalCpmDiskTests)
{
public:

    static bool  TryLoad (const char * name, std::vector<Byte> & outSectors)
    {
        char                      profile[MAX_PATH] = {};
        DWORD                     length            = GetEnvironmentVariableA ("USERPROFILE", profile, MAX_PATH);
        std::filesystem::path     path              = std::filesystem::path (std::string (profile, length)) / "Downloads" / "Casso Full Send" / name;
        std::ifstream             file (path, std::ios::binary);
        std::vector<Byte>         bytes;
        SectorDecodeReport        report;

        if (!file.is_open())
        {
            Logger::WriteMessage (std::format ("SKIPPED: {} is not on this machine\n", name).c_str());
            return false;
        }

        bytes.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>());

        AssertSucceeded (VolumeImage::Load (bytes, name, outSectors, report));

        return true;
    }



    TEST_METHOD (ApplePascalDiskListsAndReads)
    {
        std::vector<Byte>  sectors;
        VolumeListing      listing;
        FilePayload        code;
        FilePayload        info;
        FilePayload        syntax;
        std::string        text;
        size_t             allBlocks = 0;

        if (!TryLoad ("0006 APPLE0.dsk", sectors))
        {
            return;
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Pascal);

        PascalVolume  volume (sectors);

        AssertSucceeded (volume.Enumerate (listing));
        Assert::AreEqual (std::string ("APPLE0"), listing.volumeName);
        Assert::AreEqual ((uint32_t) 280, listing.totalUnits);
        Assert::IsTrue (listing.damage.empty());
        Assert::AreEqual ((size_t) 8, listing.entries.size());

        for (const FileEntry & entry : listing.entries)
        {
            Assert::IsTrue (entry.name.starts_with ("SYSTEM."));
            allBlocks += entry.sizeUnits;
        }

        Assert::AreEqual ((uint32_t) (280 - 6 - allBlocks), listing.freeUnits);

        Assert::AreEqual (std::string ("SYSTEM.PASCAL"), listing.entries[0].name);
        Assert::AreEqual ((int) PascalVolume::kKindCode, (int) listing.entries[0].type);
        Assert::AreEqual ((uint32_t) 44, listing.entries[0].sizeUnits);

        //  A code file starts with a segment dictionary, whose first segment
        //  starts at block 1: its word is 1, and its length 0x0C56 bytes fits
        //  the file.
        AssertSucceeded (volume.Read (FilePath::Parse ("SYSTEM.PASCAL"), code));
        Assert::AreEqual ((size_t) 44 * 512, code.bytes.size());
        Assert::AreEqual ((int) 1, code.bytes[0] | (code.bytes[1] << 8));

        //  MISCINFO's size is exact, not whole blocks.
        AssertSucceeded (volume.Read (FilePath::Parse ("SYSTEM.MISCINFO"), info));
        Assert::AreEqual ((size_t) 192, info.bytes.size());

        //  The syntax errors are text in the compiler's format.
        AssertSucceeded (volume.Read (FilePath::Parse ("SYSTEM.SYNTAX"), syntax));
        PascalVolume::DecodeText (syntax.bytes, text);
        Assert::IsTrue (text.find ("1: Error in simple type\r\n2: Identifier expected\r\n") != std::string::npos, L"The compiler's messages");
    }
};
