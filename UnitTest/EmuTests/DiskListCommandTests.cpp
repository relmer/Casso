#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskListCommandTests
//
//  disk list, mkdir and rmdir: catalog rows, columns, recursive listings,
//  paths and directories.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskListCommandTests)
{
public:

    TEST_METHOD (List_ReportsFreeSpace)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedDos33Disk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing states free space against the volume's capacity");
    }


    TEST_METHOD (Mkdir_MakesEveryDirectoryAlongThePath_AndARepeatIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, L"both directories are made");

        //  The image the runner wrote is what the next command reads.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.output.find ("DEEP") != std::string::npos,
            L"and the deeper one is inside the first");

        //  Asking again for something that is there in full is an error.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("is already on this volume") != std::string::npos);
    }


    TEST_METHOD (Mkdir_OnADos33Disk_SaysTheFilesystemHasNoDirectories)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedDos33Disk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("Unable to create CASSO: this volume is DOS 3.3, which has no directories") != std::string::npos);
    }


    TEST_METHOD (Rmdir_ADirectoryHoldingThings_NeedsRecurse_ThenListsAndAsks)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;
        std::string         asked;

        DiskRunnerFixtures::SeedProDosDisk (io);

        //  A directory with something in it.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        //  Without --recurse it is turned down, and the flag is named.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Rmdir);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("--recurse") != std::string::npos);

        //  With nobody to ask, it is turned down as well, and the list is shown.
        options.disk.recurse = true;
        result               = runner.Run (options);

        Assert::IsTrue (result.output.find ("CASSO/DEEP")   != std::string::npos, L"the list names what would go");
        Assert::IsTrue (result.output.find ("entries, ")    != std::string::npos, L"and totals it");
        Assert::IsTrue (result.diagnostics.find ("--yes")   != std::string::npos);

        //  An asker that says no leaves the directory where it is.
        runner.SetConfirmAsker ([&asked] (const std::string & question) { asked = question; return false; });

        result = runner.Run (options);

        Assert::IsTrue (asked.find ("CASSO") != std::string::npos, L"the question names the directory");
        Assert::IsTrue (result.diagnostics.find ("was not removed") != std::string::npos);

        //  One that says yes removes the subtree.
        runner.SetConfirmAsker ([] (const std::string &) { return true; });

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.path = "CASSO";
        result            = runner.Run (options);

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus,
            L"the directory is gone, so listing it fails");
    }


    //  HFS has folders, as ProDOS has directories: mkdir makes each along
    //  the path, and rmdir removes them.
    TEST_METHOD (MkdirAndRmdir_ChangeAnHfsVolume)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;
        vector<Byte>        image;



        AssertSucceeded (HfsWriter::Format (image, 1600, "Mac Disk"));
        io.files["mac.po"]  = image;
        io.stamps["mac.po"] = FileStamp { image.size(), 100 };

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir, "mac.po");
        options.disk.path = "Work/Deep Folder";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, "mac.po");
        options.disk.path = "Work";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
        Assert::IsTrue   (result.output.find ("Deep Folder") != std::string::npos, L"the deeper folder is inside the first");

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Rmdir, "mac.po");
        options.disk.path    = "Work";
        options.disk.recurse = true;
        options.disk.yes     = true;
        result               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, "mac.po");
        options.disk.path = "Work";
        result            = runner.Run (options);

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus, L"the folder is gone, so listing it fails");
    }


    TEST_METHOD (Rmdir_WithYes_NeedsNobodyToAsk)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "CASSO/DEEP";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Rmdir);
        options.disk.path    = "CASSO";
        options.disk.recurse = true;
        options.disk.yes     = true;
        result               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }


    TEST_METHOD (AFullPath_NamesTheVolumeFirst_AndAnotherVolumeIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   rooted;
        DiskCommandResult   relative;
        DiskCommandResult   wrong;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedProDosDisk (io);

        //  `/MERLIN` is this image, so a path through it reaches the volume
        //  directory exactly as a relative one does.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.path = "/MERLIN";
        rooted            = runner.Run (options);

        options.disk.path = "";
        relative          = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, rooted.exitStatus);
        Assert::AreEqual (relative.output, rooted.output, L"the two forms name one directory");

        //  A full path naming another volume is acting on the wrong disk.
        options.disk.path = "/NOTTHISONE/SOURCE";
        wrong             = runner.Run (options);

        Assert::AreNotEqual (DiskCommandResult::kClean, wrong.exitStatus);
        Assert::IsTrue      (wrong.diagnostics.find ("is a path on another volume") != std::string::npos);
        Assert::IsTrue      (wrong.diagnostics.find ("this image is /MERLIN")       != std::string::npos);
    }


    //  A 143,360-byte sector image carrying boot code and no filesystem at all,
    //  which is what a great deal of Apple II software actually is.
    static vector<Byte> MakeBootableImageWithNoFilesystem()
    {
        vector<Byte>  image ((size_t) NibbleFormat::kImageByteSize, 0);
        size_t        i     = 0;

        // Track 0 sector 0 is what the drive's ROM reads and jumps into.
        for (i = 0; i < 64; i++)
        {
            image[i] = static_cast<Byte> (0xA9 + (i & 0x0F));
        }

        return image;
    }

    TEST_METHOD (List_ImageWithNoFilesystem_SaysSoInPlainWords_AndThenSaysWhatItCanTell)
    {
        //  TWELVE OF FOURTEEN REAL IMAGES LAND HERE, this project's own demo
        //  disk among them, and every one of them boots. Answering with the
        //  negative alone describes a working disk as though it were unreadable.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        const char *        path = "C:\\disks\\casso-rocks.dsk";
        vector<Byte>        image = MakeBootableImageWithNoFilesystem();

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, path));

        Assert::IsTrue (result.diagnostics.find (DiskImageSession::kNoFilesystemText)
                            != std::string::npos,
            L"the sentence a person would say, not a sentence about this tool's tables");

        Assert::IsTrue (result.diagnostics.find ("filesystem this tool recognizes")
                            == std::string::npos,
            L"and not the old one");

        Assert::IsTrue (result.diagnostics.find ("35 tracks, 16 sectors, 256 bytes per sector")
                            != std::string::npos,
            L"the geometry is still knowable and is still worth stating");

        Assert::IsTrue (result.diagnostics.find ("sector 0 contains boot code")
                            != std::string::npos,
            L"and so is the fact that it boots");

        //  THE STATUS DOES NOT MOVE. A caller still got no catalog, so a script
        //  that branches on 2 branches the same way it always did.
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a survey is not a listing");

        Assert::AreEqual (std::string(), result.output,
            L"and it goes to the error stream, so nothing appears in a pipe");
    }

    TEST_METHOD (List_ImageWithNoFilesystemAndNoBootCode_SaysThatInsteadOfClaimingItBoots)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        const char *        path  = "C:\\disks\\blank.dsk";
        vector<Byte>        image ((size_t) NibbleFormat::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, path));

        Assert::IsTrue (result.diagnostics.find ("sector 0 is blank")
                            != std::string::npos,
            L"an empty first sector is reported as empty, not as bootable");
    }
};
