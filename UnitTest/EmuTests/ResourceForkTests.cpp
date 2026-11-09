#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"
#include "Core/AppleSingleCodec.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "CassoExplorer/Model/CrossVolumeCopy.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "FakeDiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ResourceForkTests
//
//  ProDOS extended files, storage type 5: written, read back fork by fork,
//  listed with both lengths, and deleted with every block freed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ResourceForkTests)
{
public:

    static constexpr size_t  kBlockBytes = 512;
    static constexpr size_t  kBlocks     = 1600;

    static vector<Byte> MakeVolume()
    {
        vector<Byte>  buffer (kBlocks * kBlockBytes, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, "FORKS"));

        return buffer;
    }

    static FilePayload MakeExtended (size_t dataBytes, size_t forkBytes)
    {
        FilePayload  payload;
        size_t       i       = 0;



        payload.type            = 0x50;
        payload.auxType         = 0x5445;
        payload.hasAuxType      = true;
        payload.hasResourceFork = true;
        payload.bytes.assign (dataBytes, 0);
        payload.resourceFork.assign (forkBytes, 0);
        payload.finderInfo.assign (ProDosSkeleton::kFinderInfoBytes, 0);

        for (i = 0; i < dataBytes; i++)
        {
            payload.bytes[i] = (Byte) i;
        }

        for (i = 0; i < forkBytes; i++)
        {
            payload.resourceFork[i] = (Byte) (i * 3);
        }

        for (i = 0; i < payload.finderInfo.size(); i++)
        {
            payload.finderInfo[i] = (Byte) (0x40 + i);
        }

        return payload;
    }


    TEST_METHOD (Write_ReadsBackBothForksAndTheFinderInfo)
    {
        vector<Byte>           buffer   = MakeVolume();
        FilePayload            payload  = MakeExtended (1000, 70'000);
        vector<Byte>           written;
        FilePayload            read;
        VolumeIntegrityReport  report;



        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DOC"), payload, written));

        ProDosVolume  after (written);

        AssertSucceeded (after.Read (FilePath::Parse ("DOC"), read));
        Assert::IsTrue (read.hasResourceFork);
        Assert::IsTrue (read.bytes == payload.bytes);
        Assert::IsTrue (read.resourceFork == payload.resourceFork);
        Assert::IsTrue (read.finderInfo == payload.finderInfo);

        AssertSucceeded (after.BuildIntegrityReport (report));
        Assert::IsTrue (report.IsClean());
    }


    TEST_METHOD (Enumerate_ListsBothForkLengths)
    {
        vector<Byte>   buffer  = MakeVolume();
        vector<Byte>   written;
        VolumeListing  listing;



        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DOC"), MakeExtended (1000, 2000), written));
        AssertSucceeded (ProDosVolume (written).Enumerate (listing));

        Assert::AreEqual ((size_t) 1, listing.entries.size());
        Assert::IsTrue (listing.entries[0].hasResourceFork);
        Assert::AreEqual ((uint32_t) 1000, listing.entries[0].eofBytes);
        Assert::AreEqual ((uint32_t) 2000, listing.entries[0].resourceForkBytes);
    }


    TEST_METHOD (Delete_FreesEveryBlockOfBothForks)
    {
        vector<Byte>   buffer  = MakeVolume();
        vector<Byte>   written;
        vector<Byte>   deleted;
        VolumeListing  before;
        VolumeListing  after;



        AssertSucceeded (ProDosVolume (buffer).Enumerate (before));
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DOC"), MakeExtended (140'000, 3000), written));
        AssertSucceeded (ProDosVolume (written).Delete (FilePath::Parse ("DOC"), deleted));
        AssertSucceeded (ProDosVolume (deleted).Enumerate (after));

        Assert::AreEqual (before.freeUnits, after.freeUnits);
    }


    TEST_METHOD (Write_AnEmptyResourceForkKeepsItsKeyBlock)
    {
        vector<Byte>  buffer  = MakeVolume();
        vector<Byte>  written;
        FilePayload   read;



        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("DOC"), MakeExtended (10, 0), written));
        AssertSucceeded (ProDosVolume (written).Read (FilePath::Parse ("DOC"), read));

        Assert::IsTrue (read.hasResourceFork);
        Assert::IsTrue (read.resourceFork.empty());
    }


    TEST_METHOD (Get_WritesTheResourceForkAsAppleDouble)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        vector<Byte>        written;
        FilePayload         payload = MakeExtended (100, 200);
        AppleSingleFile     single;
        std::string         error;



        AssertSucceeded (ProDosVolume (MakeVolume()).Write (FilePath::Parse ("DOC"), payload, written));
        io.files["forks.po"] = written;

        options.subcommand       = CommandLineOptions::Subcommand::Disk;
        options.disk.command     = CommandLineOptions::DiskOptions::Command::Get;
        options.disk.commandWord = "get";
        options.disk.imagePath   = "forks.po";
        options.disk.path        = "DOC";
        options.disk.hostFile    = "out\\DOC";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);
        Assert::IsTrue (io.files["out\\DOC"] == payload.bytes);
        Assert::IsTrue (io.Exists ("out\\._DOC"));

        AssertSucceeded (AppleSingleCodec::Decode (io.files["out\\._DOC"], single, error));
        Assert::IsTrue (single.resourceFork == payload.resourceFork);
        Assert::IsTrue (single.finderInfo == payload.finderInfo);
        Assert::AreEqual ((Word) 0x50, single.fileType);
    }


    TEST_METHOD (Convert_DropsTheForkOutsideProDos)
    {
        FilePayload   payload = MakeExtended (100, 200);
        std::wstring  note;



        CrossVolumeCopy::Convert (VolumeKind::ProDos, VolumeKind::Dos33, "DOC", payload, note);

        Assert::IsFalse (payload.hasResourceFork);
        Assert::IsTrue (payload.resourceFork.empty());
        Assert::IsTrue (note.find (L"resource fork") != std::wstring::npos);
    }
};