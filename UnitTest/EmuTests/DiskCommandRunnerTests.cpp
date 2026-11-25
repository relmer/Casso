#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunnerTests
//
//  Behavior common to every disk command: images that cannot be read,
//  invalid command lines, and the usage text printed for each.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskCommandRunnerTests)
{
public:

    TEST_METHOD (UnreadableImage_ProducesNoOutput)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty());
        Assert::IsTrue (result.diagnostics.find ("cannot be read") != std::string::npos);
    }

    TEST_METHOD (UnrecognizedFilesystem_IsRefusedRatherThanGuessed)
    {
        // Refusing to name a filesystem is an answer. Guessing one would send
        // the reader at structures that are not there and report whatever it
        // found as a catalog.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       noise (NibbleFormat::kImageByteSize, 0x55);

        io.files[DiskRunnerFixtures::kImage]  = noise;
        io.stamps[DiskRunnerFixtures::kImage] = FileStamp { noise.size(), 1 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty(), L"and nothing is offered as a catalog");
        Assert::IsTrue (result.diagnostics.find (DiskImageSession::kNoFilesystemText)
                            != std::string::npos,
            L"in the words a person would use");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  The console shares the emulator's reasons
    //
    //  Both arrive at a refusal through the same loaders, so a reason only one
    //  of them could give would be a reason the other went looking for and did
    //  not find. These assert on the shared clause, which is what makes the
    //  sharing real rather than coincidental.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (TruncatedImage_IsRefusedWithItsLength)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       truncated (4096, 0);

        io.files[DiskRunnerFixtures::kImage]  = truncated;
        io.stamps[DiskRunnerFixtures::kImage] = FileStamp { truncated.size(), 1 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("4,096 bytes") != std::string::npos,
            L"the refusal reports how big the file is");
        Assert::IsTrue (result.diagnostics.find ("143,360 bytes") != std::string::npos,
            L"and how big a .dsk has to be, which is what identifies a bad download");
    }


    TEST_METHOD (RenamedWozImage_IsRefusedAsNotAWoz)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       renamed (600, 0x41);
        const char *       wozPath = "C:\\disks\\notreally.woz";

        io.files[wozPath]  = renamed;
        io.stamps[wozPath] = FileStamp { renamed.size(), 1 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          wozPath));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("WOZ file header") != std::string::npos,
            L"a .woz with no WOZ header is told exactly that, not 'not a disk image'");
    }


    TEST_METHOD (EmptyImageFile_IsRefusedAsEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       nothing;

        io.files[DiskRunnerFixtures::kImage]  = nothing;
        io.stamps[DiskRunnerFixtures::kImage] = FileStamp { 0, 1 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is empty") != std::string::npos,
            L"a zero-byte file is empty, and reporting that beats an arithmetic complaint");
    }

    TEST_METHOD (UnknownCommand_AsksForThePageInsteadOfListingTheCommandsAgain)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::None);
        DiskCommandResult   result;

        options.disk.commandWord = "frobnicate";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine,
            L"which is what makes the edge print the page");
        Assert::IsTrue (result.diagnostics.find ("frobnicate") != std::string::npos,
            L"and the refusal quotes the word that could not be read");
        Assert::IsTrue (result.diagnostics.find ("catalog") == std::string::npos,
            L"without repeating the page's own command list under it");
    }

    //
    //  A REFUSED COMMAND LINE RUNS NOTHING, which is the half of the fix that
    //  lives here rather than in the parser.
    //
    //  The parser learned to refuse an option this grammar does not have --
    //  `-o`, which belongs to as65 -- and a refusal nothing acted on would have
    //  been a diagnostic on the screen and a clean exit status in the script.
    //  It matters most on a write: an option misread could have named the type
    //  the catalog records or the address a binary loads at, so the disk must
    //  not be edited on terms nobody asked for.
    //
    TEST_METHOD (RefusedCommandLine_RunsNothing_AndReportsNoOutput)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedDos33Disk (io);
        options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty(), L"a listing this command line did not earn");

        //  And nothing is said, because the parser already named the argument
        //  it could not take. One mistake, one message.
        Assert::AreEqual (std::string(), result.diagnostics);
    }

    //  THE SAME ANSWER FOR A BAD VALUE AS FOR A MISSING OPERAND. These used to
    //  differ: `sectorread` with no image answered in 18 lines and
    //  `sectorread --track 99` in 194, the whole page, though both readers had
    //  named the command they wanted and differed only in how they got it
    //  wrong. Nothing pinned it, which is how the two drifted apart.
    TEST_METHOD (BadOptionValue_ShowsThatCommandsUsage_NotEveryOtherCommands)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 99, 0, 1, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"a value out of range is a bad command line");
        Assert::IsTrue (result.usageShown,     L"and the reader is shown how to write it");

        Assert::IsTrue (result.output.find ("CassoCli disk sectorread <image>") != std::string::npos,
            L"sectorread's own grammar");
        Assert::IsTrue (result.output.find ("CassoCli disk create") == std::string::npos,
            L"and not every other command's");
    }

    //  AND THE ONE CASE THAT MUST STAY WIDE. A word the grammar does not know
    //  leaves no command to narrow to, and a reader who has not landed on one
    //  is exactly who the whole page is for. Narrowing this would answer an
    //  unrecognized command with a block belonging to some other command.
    TEST_METHOD (AnUnknownCommand_StillAnswersWithTheWholePage)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        DiskCommandResult   result;

        options.subcommand       = CommandLineOptions::Subcommand::Disk;
        options.disk.command     = CommandLineOptions::DiskOptions::Command::None;
        options.disk.commandWord = "frobnicate";
        options.disk.imagePath   = "raw.dsk";

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);

        Assert::IsTrue (result.diagnostics.find ("frobnicate") != std::string::npos,
            L"the word they typed is quoted back");

        //  usageShown is what tells the edge the runner already answered. Left
        //  false, the edge prints the whole page, which is the answer here.
        Assert::IsFalse (result.usageShown,
            L"no command to narrow to, so the edge prints the whole page");

        Assert::IsTrue (result.output.empty(),
            L"and the runner offers no block of its own to be printed instead");
    }

    TEST_METHOD (Failure_EchoesANonAsciiPathByteForByte_BecauseItHasToBePastable)
    {
        //  The measured bug. `Space Quarks (1981)(Broderbund)(II-II+)[48K].woz`
        //  came back as `Br?derbund`, which is a name nobody can paste into a
        //  command line. The mangling happened at the console, not here -- so
        //  what this pins is that the RUNNER hands its caller the same bytes it
        //  was given, leaving exactly one place for the conversion to live.
        //
        //  The escape is split because a hex escape in C++ is greedy: "\xF8d"
        //  would be read as one character numbered $F8D.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        std::string         path  = "C:\\disks\\Space Quarks (1981)(Br\xF8" "derbund).woz";
        vector<Byte>        image ((size_t) NibbleFormat::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          path.c_str()));

        Assert::IsTrue (result.diagnostics.find (path) != std::string::npos,
            L"the name comes back exactly as it went in");

        Assert::IsTrue (result.diagnostics.find ('?') == std::string::npos,
            L"and with no substitution character anywhere in the message");
    }

    TEST_METHOD (Failure_NamesAnUnreadableImage_WithoutAssertingOnTheUsersInput)
    {
        //  A user naming an image that is not there is not a coding error, so
        //  the path must not be the one that asserts.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          "C:\\disks\\absent.dsk"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("cannot be read") != std::string::npos);
    }
};
