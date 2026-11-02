#include "Pch.h"

#include "EhmTestHelper.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/NativeListing.h"
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

        std::vector<std::string>  lines;

        NativeListing::FormatPascal (listing, lines);

        Assert::AreEqual (std::string ("APPLE0:"), lines[0]);
        Assert::AreEqual (std::string ("SYSTEM.PASCAL     44 ") + lines[1].substr (21, 9) + "     6   512  Codefile", lines[1]);
        Assert::IsTrue   (lines.back().starts_with ("8/8 files<listed/in-dir>, 261 blocks used, 19 unused, 19 in largest"));
        Logger::WriteMessage ((lines[0] + "\n" + lines[1] + "\n" + lines.back() + "\n").c_str());

        //  The syntax errors are text in the compiler's format.
        AssertSucceeded (volume.Read (FilePath::Parse ("SYSTEM.SYNTAX"), syntax));
        PascalVolume::DecodeText (syntax.bytes, text);
        Assert::IsTrue (text.find ("1: Error in simple type\r\n2: Identifier expected\r\n") != std::string::npos, L"The compiler's messages");
    }



    static bool  HasText (const FilePayload & payload, const char * text)
    {
        return std::search (payload.bytes.begin(), payload.bytes.end(), text, text + strlen (text)) != payload.bytes.end();
    }



    //  A data disk, with leftover program bytes in half its directory.
    TEST_METHOD (CpmDataDiskListsAndReads)
    {
        std::vector<Byte>  sectors;
        VolumeListing      listing;
        FilePayload        test;
        FilePayload        data;

        if (!TryLoad ("0005.dsk", sectors))
        {
            return;
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Cpm);

        CpmVolume  volume (sectors);

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual ((size_t) 2, listing.entries.size());
        Assert::AreEqual (std::string ("TEST.COM"), listing.entries[0].name);
        Assert::AreEqual (std::string ("TESTFILE"), listing.entries[1].name);
        Assert::AreEqual ((size_t) 1, listing.damage.size(), L"The leftover bytes are reported");

        std::vector<std::string>  lines;

        NativeListing::FormatCpm (listing, lines);

        Assert::AreEqual (std::string ("Recs  Bytes  Ext Acc"), lines[0]);
        Assert::AreEqual (std::string ("  56     7k    1 R/W A:TEST.COM"), lines[1]);
        Assert::AreEqual (std::string (" 256    32k    3 R/W A:TESTFILE"), lines[2]);

        AssertSucceeded (volume.Read (FilePath::Parse ("TEST.COM"), test));

        Assert::AreEqual ((size_t) 56 * 128, test.bytes.size());
        Assert::AreEqual ((int) 0xC3, (int) test.bytes[0], L"A program starts with a jump");
        Assert::IsTrue (HasText (test, "Copyright (c) 1983 by microMethods"));

        AssertSucceeded (volume.Read (FilePath::Parse ("TESTFILE"), data));
        Assert::AreEqual ((size_t) 256 * 128, data.bytes.size(), L"Three entries, the last with no records");
    }



    //  A boot disk, whose system image is listed under user 31.
    TEST_METHOD (CpmBootDiskListsAndReads)
    {
        std::vector<Byte>  sectors;
        VolumeListing      listing;
        FilePayload        configure;
        FilePayload        sample;
        FilePayload        system;
        std::string        text;

        if (!TryLoad ("0007.dsk", sectors))
        {
            return;
        }

        Assert::IsTrue (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Cpm);

        CpmVolume  volume (sectors);

        AssertSucceeded (volume.Enumerate (listing));

        Assert::IsTrue (listing.damage.empty());
        Assert::AreEqual (std::string ("31:cp/am.sys"), listing.entries[0].name);
        Assert::IsTrue (listing.entries[0].isLocked);
        Assert::AreEqual ((uint32_t) 3, listing.freeUnits, L"Blocks 2 to 124 are in use");

        std::vector<std::string>  lines;

        NativeListing::FormatCpm (listing, lines);

        Assert::AreEqual (std::string ("Bytes Remaining On A: 3k"), lines[lines.size() - 5]);
        Assert::AreEqual (std::string ("USER 31"), lines[lines.size() - 3]);
        Assert::AreEqual (std::string ("  96    12k    1 R/W A:cp/am.sys"), lines.back());

        //  Strings that cross a sector boundary confirm the skew.
        AssertSucceeded (volume.Read (FilePath::Parse ("CONFIGIO.COM"), configure));
        Assert::IsTrue (HasText (configure, "REQUIRED (Y/N)?"));
        Assert::IsTrue (HasText (configure, "ADD/DELETE/EXIT (A/D/X)"));

        AssertSucceeded (volume.Read (FilePath::Parse ("SAMPLE.SUB"), sample));
        CpmVolume::DecodeText (sample.bytes, text);
        Assert::IsTrue (text.starts_with ("dir\r\nramdrive d:\r\n"), L"A text file");

        //  The system entry's blocks wrap to tracks 0 to 2.
        AssertSucceeded (volume.Read (FilePath::Parse ("31:cp/am.sys"), system));
        Assert::IsTrue (HasText (system, "CP/AM (C) 1985 Applied Engineering"));
    }
};
