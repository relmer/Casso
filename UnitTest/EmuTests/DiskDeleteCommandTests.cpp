#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDeleteCommandTests
//
//  disk delete: a file removed from the committed image, and the cases where
//  the image is left as it was.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskDeleteCommandTests)
{
public:

    static constexpr size_t    kEntOffTsTrack     = 0x00;
    static constexpr size_t    kEntOffTsSector    = 0x01;
    static constexpr size_t    kTsOffNextTrack    = 0x01;
    static constexpr size_t    kTsOffNextSector   = 0x02;
    static constexpr Byte      kTrackOffTheVolume = 40;

    CommandLineOptions MakeDeleteOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Delete, image);

        options.disk.path = path;

        return options;
    }

    //  A DOS 3.3 disk with MAKE DUMP and MAKE DUMP.S on it, a file and a file
    //  whose name contains it.
    static void SeedDiskWithMakeDump (FakeDiskFileIo & io)
    {
        vector<Byte>  image = DiskRunnerFixtures::MakeBlankDos33Image();
        vector<Byte>  next;
        FilePayload   payload;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.bytes          = DiskRunnerFixtures::MakePayload();
        payload.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        payload.hasLoadAddress = true;

        for (const char * name : { "MAKE DUMP", "MAKE DUMP.S" })
        {
            Dos33Volume  volume (image);

            AssertSucceeded (volume.Write (FilePath::Parse (name), payload, next));

            image = next;
        }

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kImage, image);
    }

    //  Points the first entry's track/sector list at a track that is not on the
    //  volume, so a walk of its chain cannot reach the end.
    static void BreakFirstEntrysChain (vector<Byte> & buffer)
    {
        size_t  entryAt = Dos33Skeleton::GetSectorOffset (DiskRunnerFixtures::kVtocTrack, DiskRunnerFixtures::kCatalogFirstSector) + DiskRunnerFixtures::kEntryBase;
        int     track   = buffer[entryAt + kEntOffTsTrack];
        int     sector  = buffer[entryAt + kEntOffTsSector];
        size_t  listAt  = Dos33Skeleton::GetSectorOffset (track, sector);

        buffer[listAt + kTsOffNextTrack]  = kTrackOffTheVolume;
        buffer[listAt + kTsOffNextSector] = 0;
    }

    TEST_METHOD (Delete_RemovesTheFileFromTheCommittedImage)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        DiskCommandResult   readBack;

        SeedDiskWithMakeDump (io);

        // The trailing newline matters: this disk also carries MAKE DUMP.S, and
        // a substring search would go on finding the removed file inside its
        // neighbor's name and report a delete that never happened as one that
        // did -- or, here, the reverse.
        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage).find ("MAKE DUMP\n") != std::string::npos,
            L"the file must be there before the delete for this to mean anything");

        result = runner.Run (MakeDeleteOptions (DiskRunnerFixtures::kImage, "MAKE DUMP"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (io.HasNoTemporaryFiles());

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage).find ("MAKE DUMP\n") == std::string::npos,
            L"and it is gone from the image on disk, not merely from a buffer");

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage).find ("MAKE DUMP.S\n") != std::string::npos,
            L"while the file whose name merely contains it is untouched");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandResult::kNoOutput, readBack.exitStatus,
            L"and no longer resolves by name");
    }

    TEST_METHOD (Delete_AFileTheVolumeDoesNotHave_RefusesNamingImageAndFile)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedDos33Disk (io);

        result = runner.Run (MakeDeleteOptions (DiskRunnerFixtures::kImage, "NOSUCHFILE"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (DiskRunnerFixtures::kImage) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("is not on this volume") != std::string::npos,
            L"and reports which of the refusals this is");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kImage, original);
    }

    TEST_METHOD (Delete_ALockedFile_IsRefusedInTermsThatSayItIsLocked)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        DiskRunnerFixtures::LockFirstCatalogEntry (io.files[DiskRunnerFixtures::kBlankImage]);
        committed = io.files[DiskRunnerFixtures::kBlankImage];

        result = runner.Run (MakeDeleteOptions (DiskRunnerFixtures::kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("Unable to delete PROG: it is locked") != std::string::npos);

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, committed);
    }

    TEST_METHOD (Delete_WhenTheChainCannotBeFollowed_SucceedsAndSaysWhatItCouldNotAccountFor)
    {
        // The reason the removal's ACCOUNT had to become part of the volume
        // interface. Delete stays available for a file whose chain is damaged,
        // so a bad entry cannot strand the volume -- but a caller that reported
        // only success would leave the user believing every sector came back.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        BreakFirstEntrysChain (io.files[DiskRunnerFixtures::kBlankImage]);

        result = runner.Run (MakeDeleteOptions (DiskRunnerFixtures::kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kWithComplaints, result.exitStatus,
            L"a removal that could not account for everything is not a clean one");

        Assert::IsTrue (result.diagnostics.find ("could not be followed") != std::string::npos,
            L"and reports what it could not do, rather than only that something was wrong");

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find ("PROG") == std::string::npos,
            L"while still removing the entry, so a bad file cannot strand the volume");
    }
};
