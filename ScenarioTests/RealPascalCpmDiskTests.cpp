#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
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



    //  What the real loader does on a disk it finds no SYSTEM.APPLE on.
    TEST_METHOD (TheApplePascalDiskPrintsItsLoadersMessage)
    {
        std::vector<Byte>         sectors;
        std::vector<std::string>  rows;
        TestMachine               machine ("Apple2e");

        if (!TryLoad ("0006 APPLE0.dsk", sectors))
        {
            return;
        }

        GuestSession::MountAndBoot (machine, sectors);
        GuestSession::CollectRows (machine, rows);

        std::string  screen;

        for (const std::string & row : rows)
        {
            screen += row + "\n";
        }

        Assert::IsTrue (screen.find ("FILE SYSTEM.APPLE") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());
    }

    //  The real boot loader from 0006, on a disk from `disk create --bootable`,
    //  with a stand-in for SYSTEM.APPLE, since no real one is on this machine.
    //  The loader reads 16 KB of the file into the language card, and jumps
    //  through $FFF8, so this proves the directory and block layout it finds.
    TEST_METHOD (ACreatedPascalDiskBootsThroughTheRealLoader)
    {
        constexpr size_t          kVectorOffset = 0x2FF8;
        constexpr size_t          kAppleBytes   = 32 * 512;
        const std::string         kMessage      = "CASSO BOOT";
        std::vector<Byte>         real;
        std::vector<Byte>         source (143360, 0);
        std::vector<Byte>         next;
        std::vector<Byte>         sectors;
        std::vector<std::string>  rows;
        FilePayload               apple;
        FilePayload               pascal_;
        FakeDiskFileIo            io;
        DiskCommandRunner         runner (io);
        CommandLineOptions        options;
        DiskCommandResult         result;
        SectorDecodeReport        report;
        TestMachine               machine ("Apple2e");

        if (!TryLoad ("0006 APPLE0.dsk", real))
        {
            return;
        }

        PascalVolume::WriteBlank ("SOURCE", 0, source);

        for (int block = 0; block < 2; block++)
        {
            for (size_t i = 0; i < 512; i++)
            {
                source[ProDosSkeleton::GetBlockByteOffset (block, i)] = real[ProDosSkeleton::GetBlockByteOffset (block, i)];
            }
        }

        //  LDA #char, STA $0400+i for each character, and a loop on itself.
        apple.bytes.assign (kAppleBytes, 0xEA);

        {
            size_t  at = 0;

            for (size_t i = 0; i < kMessage.size(); i++)
            {
                apple.bytes[at++] = 0xA9;
                apple.bytes[at++] = (Byte) (kMessage[i] | 0x80);
                apple.bytes[at++] = 0x8D;
                apple.bytes[at++] = (Byte) i;
                apple.bytes[at++] = 0x04;
            }

            apple.bytes[at]     = 0x4C;
            apple.bytes[at + 1] = (Byte) ((0xD000 + at) & 0xFF);
            apple.bytes[at + 2] = (Byte) ((0xD000 + at) >> 8);
        }

        apple.bytes[kVectorOffset]     = 0x00;
        apple.bytes[kVectorOffset + 1] = 0xD0;

        pascal_.bytes.assign (512, 0);

        AssertSucceeded (PascalVolume (source).Write (FilePath::Parse ("SYSTEM.APPLE"), apple, next));
        AssertSucceeded (PascalVolume (next).Write (FilePath::Parse ("SYSTEM.PASCAL"), pascal_, source));

        io.files["C:\\src\\system.dsk"] = source;

        options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath    = "C:\\disks\\boot.dsk";
        options.disk.formatName   = "pascal";
        options.disk.bootable     = true;
        options.disk.bootableFrom = "C:\\src\\system.dsk";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        GuestSession::MountAndBoot (machine, io.files["C:\\disks\\boot.dsk"]);
        GuestSession::CollectRows (machine, rows);

        Assert::IsTrue (std::any_of (rows.begin(), rows.end(), [&kMessage] (const std::string & row) { return row.find (kMessage) != std::string::npos; }), L"The stand-in ran");
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
        Assert::AreEqual ((uint32_t) 140, listing.totalUnits, L"The system entry claims the 12 blocks past 127");

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
