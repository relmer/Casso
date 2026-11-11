#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/Model/CollectionResults.h"
#include "CassoExplorer/Model/CollectionWalk.h"
#include "CassoExplorer/Model/DiskIdentity.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Update/ZipArchive.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionToolsTests
//
//  Disk list, Find duplicates and `casso disk search` over a folder of
//  images on a fake disk: a disk's identity in each image form, the walk
//  into subfolders, ZIP and gzip files and images stored on other images,
//  the duplicate sets and the CSV text.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CollectionToolsTests)
{
public:

    static constexpr Byte          kEmptyZip[22] = { 0x50, 0x4B, 0x05, 0x06 };
    static constexpr const char *  kRoot         = "C:\\Disks";
    static constexpr Byte          kTypeText     = 0x00;   // DOS 3.3's T
    static constexpr Byte          kTypeBinary   = 0x06;
    static constexpr Byte          kTypeNone     = 0x00;   // ProDOS's NON

    static std::vector<Byte> MakeImage (const char * path, const char * format, const char * size)
    {
        FakeDiskFileIo                  io;
        DiskOperations                  operations (io);
        DiskOperations::NewDiskRequest  request;
        DiskOperations::Result          result;



        request.formatName = format;
        request.sizeName   = size;

        result = operations.Create (path, request);

        Assert::IsTrue (result.Succeeded(), TextEncoding::NarrowToWide (std::string (path) + ": " + result.message).c_str());

        return io.files[path];
    }

    static void PutFile (FakeDiskFileIo & io, const std::string & image, const char * name, const std::vector<Byte> & bytes, Byte type)
    {
        DiskOperations          operations (io);
        FilePayload             payload;
        DiskOperations::Result  result;



        payload.bytes = bytes;
        payload.type  = type;

        result = operations.WritePayload (image, name, payload);

        Assert::IsTrue (result.Succeeded(), TextEncoding::NarrowToWide (image + ": " + result.message).c_str());
    }

    static std::vector<Byte> MakeAppleText (const char * text)
    {
        std::vector<Byte>  bytes;



        for (const char * c = text; *c != '\0'; c++)
        {
            bytes.push_back ((Byte) (*c == '\n' ? 0x8D : (*c | 0x80)));
        }

        return bytes;
    }

    static void SetFile (FakeDiskFileIo & io, const std::string & path, const std::vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 1 };
    }

    static void Walk (FakeDiskFileIo & io, std::vector<CollectionDisk> & outDisks)
    {
        CollectionWalk::Visitor  visitor;



        visitor.onDisk = [&outDisks] (CollectionDisk && disk) { outDisks.push_back (std::move (disk)); };

        AssertSucceeded (CollectionWalk::Walk (io, kRoot, visitor));
    }

    static const CollectionDisk & FindDisk (const std::vector<CollectionDisk> & disks, const std::string & path)
    {
        for (const CollectionDisk & disk : disks)
        {
            if (disk.path == path)
            {
                return disk;
            }
        }

        Assert::Fail (TextEncoding::NarrowToWide ("The walk lists " + path).c_str());
    }

    static DiskIdentity::Volume GetIdentity (const std::vector<Byte> & bytes, const char * name)
    {
        std::vector<Byte>     sectors;
        SectorDecodeReport    report;
        DiskIdentity::Volume  identity;



        AssertSucceeded (VolumeImage::Load (bytes, name, sectors, report));
        AssertSucceeded (DiskIdentity::ComputeVolume (sectors, report, identity));

        return identity;
    }

    static DiskCommandResult RunDisk (FakeDiskFileIo & io, CommandLineOptions::DiskOptions::Command command, const std::string & operand, const std::string & second)
    {
        CommandLineOptions  options;
        DiskCommandRunner   runner (io);



        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = command;
        options.disk.imagePath    = operand;
        options.disk.path         = second;

        return runner.Run (options);
    }

    //  A folder with an image in each container the walk opens, all four
    //  the same disk, and a ProDOS disk in a subfolder.
    static void MakeCollection (FakeDiskFileIo & io, std::vector<Byte> & outDsk)
    {
        std::vector<Byte>  zip;
        std::vector<Byte>  gzip;
        std::string        hd = std::string (kRoot) + "\\hd.po";



        outDsk = MakeImage ("C:\\Make\\a.dsk", "dos33", "");

        SetFile (io, std::string (kRoot) + "\\a.dsk", outDsk);
        PutFile (io, std::string (kRoot) + "\\a.dsk", "NOTES", MakeAppleText ("THE HIGH SCORE TABLE\n"), kTypeText);
        outDsk = io.files[std::string (kRoot) + "\\a.dsk"];

        AssertSucceeded (ZipArchive::Replace (kEmptyZip, "disks/d.dsk", outDsk, zip));
        SetFile (io, std::string (kRoot) + "\\c.zip", zip);

        GzipFile::Build (outDsk, "e.dsk", gzip);
        SetFile (io, std::string (kRoot) + "\\e.dsk.gz", gzip);

        SetFile (io, hd, MakeImage ("C:\\Make\\hd.po", "prodos", "800k"));
        PutFile (io, hd, "INNER.DSK", outDsk, kTypeNone);

        SetFile (io, std::string (kRoot) + "\\sub\\b.po", MakeImage ("C:\\Make\\b.po", "prodos", ""));
        SetFile (io, std::string (kRoot) + "\\readme.txt", { 'h', 'i' });
    }


    TEST_METHOD (OneDiskInEachForm_HasOneIdentity)
    {
        std::vector<Byte>     woz = MakeImage ("C:\\Make\\a.woz", "dos33", "");
        std::vector<Byte>     sectors;
        std::vector<Byte>     po;
        std::vector<Byte>     twoImg;
        SectorDecodeReport    report;
        TwoImgHeader          header;
        DiskIdentity::Volume  fromWoz;



        AssertSucceeded (VolumeImage::Load (woz, "a.woz", sectors, report));
        VolumeImage::DosLogicalToProDosFile (sectors, po);

        header.blockCount = (uint32_t) (po.size() / 512);
        TwoImgContainer::Build (header, po, twoImg);

        fromWoz = GetIdentity (woz, "a.woz");

        Assert::AreEqual (fromWoz.sha256, GetIdentity (sectors, "a.dsk").sha256);
        Assert::AreEqual (fromWoz.sha256, GetIdentity (po,      "a.po").sha256);
        Assert::AreEqual (fromWoz.sha256, GetIdentity (twoImg,  "a.2mg").sha256);
        Assert::AreEqual (fromWoz.crc32,  GetIdentity (twoImg,  "a.2mg").crc32);
        Assert::IsFalse  (fromWoz.hasMore);
        Assert::AreEqual ((size_t) 64, fromWoz.sha256.size());
    }


    TEST_METHOD (TrackItsSectorsDoNotDecode_HasMore)
    {
        std::vector<Byte>     sectors (143360, 0);
        SectorDecodeReport    report;
        DiskIdentity::Volume  identity;



        report.Reset (35);
        report.SetOutcome (17, TrackDecodeOutcome::Partial, 0x00FF, false);

        AssertSucceeded (DiskIdentity::ComputeVolume (sectors, report, identity));

        Assert::IsTrue (identity.hasMore);
    }


    TEST_METHOD (FileKey_IncludesTheType)
    {
        Assert::AreNotEqual (DiskIdentity::GetFileKey ("ab", kTypeText), DiskIdentity::GetFileKey ("ab", kTypeBinary));
        Assert::AreEqual    (DiskIdentity::GetFileKey ("ab", kTypeText), DiskIdentity::GetFileKey ("ab", kTypeText));
    }


    TEST_METHOD (Walk_ListsEachImageInEveryContainer)
    {
        FakeDiskFileIo               io;
        std::vector<Byte>            dsk;
        std::vector<CollectionDisk>  disks;
        std::string                  root = kRoot;



        MakeCollection (io, dsk);
        Walk (io, disks);

        Assert::AreEqual ((size_t) 6, disks.size());

        const CollectionDisk  & plain  = FindDisk (disks, root + "\\a.dsk");
        const CollectionDisk  & zipped = FindDisk (disks, root + "\\c.zip|disks/d.dsk");
        const CollectionDisk  & gzip   = FindDisk (disks, root + "\\e.dsk.gz");
        const CollectionDisk  & inner  = FindDisk (disks, root + "\\hd.po|INNER.DSK");
        const CollectionDisk  & hd     = FindDisk (disks, root + "\\hd.po");
        const CollectionDisk  & sub    = FindDisk (disks, root + "\\sub\\b.po");

        Assert::AreEqual (std::string ("DOS 3.3"), plain.fileSystem);
        Assert::AreEqual (std::string ("254"),     plain.volumeName);
        Assert::AreEqual ((uint32_t) 1,            plain.files);
        Assert::AreEqual (plain.sha256,            zipped.sha256);
        Assert::AreEqual (plain.sha256,            gzip.sha256);
        Assert::AreEqual (plain.sha256,            inner.sha256);
        Assert::AreEqual (std::string ("ProDOS"),  hd.fileSystem);
        Assert::AreEqual ((uint32_t) 1,            hd.files);
        Assert::AreEqual (std::string ("NEWDISK"), sub.volumeName);
        Assert::IsTrue   (sub.freeBytes > 0);
        Assert::IsFalse  (plain.container.empty());
    }


    TEST_METHOD (Walk_ReadsEveryFileOnADisk)
    {
        FakeDiskFileIo               io;
        std::vector<Byte>            dsk;
        std::vector<CollectionDisk>  disks;
        std::string                  digest;



        MakeCollection (io, dsk);
        Walk (io, disks);

        const CollectionDisk  & plain = FindDisk (disks, std::string (kRoot) + "\\a.dsk");

        AssertSucceeded (DiskIdentity::ComputeFile (MakeAppleText ("THE HIGH SCORE TABLE\n"), digest));

        Assert::AreEqual ((size_t) 1,             plain.fileList.size());
        Assert::AreEqual (std::string ("NOTES"),  plain.fileList[0].path);
        Assert::AreEqual (kTypeText,              plain.fileList[0].type);
        Assert::AreEqual (digest,                 plain.fileList[0].sha256);
    }


    TEST_METHOD (Walk_EndsWhenStopped)
    {
        FakeDiskFileIo               io;
        std::vector<Byte>            dsk;
        std::vector<CollectionDisk>  disks;
        CollectionWalk::Visitor      visitor;



        MakeCollection (io, dsk);

        visitor.onDisk    = [&disks] (CollectionDisk && disk) { disks.push_back (std::move (disk)); };
        visitor.isStopped = [] () { return true; };

        AssertSucceeded (CollectionWalk::Walk (io, kRoot, visitor));

        Assert::IsTrue (disks.empty());
    }


    TEST_METHOD (FindDuplicates_GroupsDisksAndFilesOnDisksThatDiffer)
    {
        std::vector<CollectionDisk>  disks (3);
        std::vector<DuplicateSet>    sets;



        disks[0].path     = "C:\\a.dsk";
        disks[0].sha256   = "x";
        disks[0].fileList = { { "HELLO", kTypeBinary, 10, "f" }, { "EMPTY", kTypeBinary, 0, "e" }, { "ONLY.HERE", kTypeBinary, 5, "g" } };
        disks[1].path     = "C:\\b.woz";
        disks[1].sha256   = "x";
        disks[1].fileList = disks[0].fileList;
        disks[2].path     = "C:\\c.po";
        disks[2].sha256   = "y";
        disks[2].fileList = { { "GREETING", kTypeBinary, 10, "f" }, { "EMPTY", kTypeBinary, 0, "e" }, { "AS.TEXT", kTypeText, 10, "f" } };

        CollectionResults::FindDuplicates (disks, sets);

        //  The two copies of one disk, then HELLO and GREETING. The files
        //  only on the two copies, an empty file and a file of another type
        //  aren't sets.
        Assert::AreEqual ((size_t) 2, sets.size());

        Assert::IsFalse  (sets[0].isFile);
        Assert::AreEqual ((size_t) 2, sets[0].members.size());

        Assert::IsTrue   (sets[1].isFile);
        Assert::AreEqual ((size_t) 3, sets[1].members.size());
        Assert::AreEqual (std::string ("GREETING"), sets[1].members[2].file);
        Assert::AreEqual ((size_t) 2,               sets[1].members[2].disk);
    }


    TEST_METHOD (FindDuplicates_FindsTheCopiesInEveryContainer)
    {
        FakeDiskFileIo               io;
        std::vector<Byte>            dsk;
        std::vector<CollectionDisk>  disks;
        std::vector<DuplicateSet>    sets;



        MakeCollection (io, dsk);
        Walk (io, disks);

        CollectionResults::FindDuplicates (disks, sets);

        Assert::IsFalse  (sets.empty());
        Assert::IsFalse  (sets[0].isFile);
        Assert::AreEqual ((size_t) 4, sets[0].members.size());
    }


    TEST_METHOD (Csv_HasTheColumnsAndQuotesWhatNeedsIt)
    {
        std::vector<CollectionDisk>  disks (1);
        std::vector<DuplicateSet>    sets (1);
        std::string                  list;
        std::string                  dupes;



        disks[0].path       = "C:\\Disks\\a, b.dsk";
        disks[0].container  = "DOS-order disk image";
        disks[0].fileSystem = "DOS 3.3";
        disks[0].volumeName = "254";
        disks[0].files      = 3;
        disks[0].freeBytes  = 1024;
        disks[0].crc32      = 0xABCD;
        disks[0].sha256     = "00ff";

        sets[0].isFile  = true;
        sets[0].sha256  = "00ff";
        sets[0].members = { { 0, "PRINT \"HI\"" } };

        list  = CollectionResults::FormatDiskList (disks);
        dupes = CollectionResults::FormatDuplicates (disks, sets);

        Assert::AreEqual (std::string ("path,container,fileSystem,volumeName,files,freeBytes,crc32,sha256,hasMore\n"
                                       "\"C:\\Disks\\a, b.dsk\",DOS-order disk image,DOS 3.3,254,3,1024,0000abcd,00ff,no\n"), list);
        Assert::AreEqual (std::string ("set,kind,path,name,sha256\n"
                                       "1,file,\"C:\\Disks\\a, b.dsk\",\"PRINT \"\"HI\"\"\",00ff\n"), dupes);
    }


    TEST_METHOD (DiskCommands_WriteTheWindowsResults)
    {
        FakeDiskFileIo               io;
        std::vector<Byte>            dsk;
        std::vector<CollectionDisk>  disks;
        std::vector<DuplicateSet>    sets;
        DiskCommandResult            list;
        DiskCommandResult            dupes;
        DiskCommandResult            search;
        DiskCommandResult            notFolder;



        MakeCollection (io, dsk);
        Walk (io, disks);
        CollectionResults::FindDuplicates (disks, sets);

        list      = RunDisk (io, CommandLineOptions::DiskOptions::Command::List,   kRoot, std::string());
        dupes     = RunDisk (io, CommandLineOptions::DiskOptions::Command::Dupes,  kRoot, std::string());
        search    = RunDisk (io, CommandLineOptions::DiskOptions::Command::Search, kRoot, "high score");
        notFolder = RunDisk (io, CommandLineOptions::DiskOptions::Command::Dupes,  std::string (kRoot) + "\\a.dsk", std::string());

        Assert::AreEqual (CollectionResults::FormatDiskList (disks),         list.output);
        Assert::AreEqual (CollectionResults::FormatDuplicates (disks, sets), dupes.output);
        Assert::AreEqual (DiskCommandResult::kClean, search.exitStatus);
        Assert::IsTrue   (search.output.find ("C:\\Disks\\a.dsk|NOTES\n") != std::string::npos, TextEncoding::NarrowToWide (search.output).c_str());
        Assert::IsTrue   (search.output.find ("C:\\Disks\\c.zip|disks/d.dsk|NOTES\n") != std::string::npos);
        Assert::AreEqual (DiskCommandResult::kNoOutput, notFolder.exitStatus);
    }
};
