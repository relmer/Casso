#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBootCommandTests
//
//  disk boot: the startup program a DOS 3.3 or ProDOS volume runs at boot.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskBootCommandTests)
{
public:

    //
    //  ------------------------------------------------------------------
    //  boot.
    //
    //  The one edit with nothing to show for itself in a listing: what a disk
    //  runs at boot is not a file and does not appear beside the files. So the
    //  assertions here read the bytes the mechanism actually uses, and the
    //  refusals are held to the same standard as put's and delete's -- image
    //  byte-for-byte as it was, no temporary, one reason in words.
    //  ------------------------------------------------------------------
    //

    CommandLineOptions MakeBootOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Boot, image);

        options.disk.path = path;

        return options;
    }

    //  A REQUIRED OPERAND THAT IS NOT THERE ANSWERS WITH THAT COMMAND'S USAGE.
    //
    //  This asserted the sentence "no program named", which pinned wording
    //  rather than behavior and stood in the way of saying it better. What
    //  matters is that the command is refused, that the reader is shown the
    //  usage for the command they asked about rather than for all eight, and
    //  that the image is not touched.
    TEST_METHOD (Boot_WithNoProgramGiven_ShowsThatCommandsUsage_AndTouchesNothing)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedDos33Disk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Boot));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"a missing operand is a bad command line");
        Assert::IsTrue (result.usageShown,     L"and the reader is shown how to write it");

        //  boot's block, and not the whole page: the grammar of the command
        //  asked about is there, and a command nobody asked about is not.
        Assert::IsTrue (result.output.find ("CassoCli disk boot <image> <name>") != std::string::npos,
            L"boot's own grammar");
        Assert::IsTrue (result.output.find ("CassoCli disk sectorwrite") == std::string::npos,
            L"and not every other command's");

        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kImage, original);
    }

    TEST_METHOD (Boot_AVolumeWithNoOperatingSystemOnIt_IsRefusedInWordsRatherThanPatchedAnyway)
    {
        // A formatted data disk reserves the tracks DOS would occupy but has
        // nothing in them. Patching a name there would report success for a
        // disk that cannot boot at all.
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

        committed = io.files[DiskRunnerFixtures::kBlankImage];

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no operating system") != std::string::npos,
            L"and reports that is what is missing, rather than blaming the file");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, committed);
    }
};
