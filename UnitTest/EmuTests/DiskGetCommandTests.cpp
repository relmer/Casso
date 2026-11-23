#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskGetCommandTests
//
//  disk get: a file read off a volume to standard output or to a host file,
//  with and without a conversion.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskGetCommandTests)
{
public:


    //  A get to a name in Casso Explorer's text form converts the file as a
    //  copy out does, the same as --text.
    TEST_METHOD (Get_ToATextFormName_ConvertsAsCopyOutDoes)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;



        DiskRunnerFixtures::SeedProDosDisk (io);
        io.files["note.txt"]  = { 'O', 'N', 'E', '\r', '\n', 'T', 'W', 'O', '\r', '\n' };
        io.stamps["note.txt"] = FileStamp { 10, 100 };

        options               = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Put);
        options.disk.hostFile = "note.txt";
        options.disk.path     = "NOTE";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
        result                = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());

        options               = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        options.disk.path     = "NOTE";
        options.disk.hostFile = "raw.bin";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options.disk.hostFile = "NOTE.Text.txt";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options.disk.hostFile = "plain.txt";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (io.files["NOTE.Text.txt"] == io.files["plain.txt"], L"the same as --text");
        Assert::IsTrue (io.files["NOTE.Text.txt"] != io.files["raw.bin"],   L"and not the stored bytes");
    }

    TEST_METHOD (Get_MissingFile_ProducesNoOutputAndNamesBoth)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedDos33Disk (io);
        options.disk.path = "NOSUCHFILE";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload);
        Assert::IsTrue (result.diagnostics.find (DiskRunnerFixtures::kImage) != std::string::npos,
            L"the message identifies the image");
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"and the file");
    }

    //  The in-use probe used to be described by a paragraph of help text, and a
    //  test here read that paragraph. Both are gone: a user meets the probe by
    //  having a write refused, so the refusal is the whole explanation, and
    //  DiskFailureModeTests already drives the refusal end to end against the
    //  wording it now shares with the runner.
};
