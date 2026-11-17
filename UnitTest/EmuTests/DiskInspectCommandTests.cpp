#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommandTests
//
//  The disk tool's `inspect`: the inspector's summary line, then one line a
//  finding, for an image the fake file system has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskInspectCommandTests)
{
public:

    static DiskCommandResult  Inspect (FakeDiskFileIo & io, const std::string & path)
    {
        CommandLineOptions  options;
        DiskCommandRunner   runner (io);



        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command   = CommandLineOptions::DiskOptions::Command::Inspect;
        options.disk.imagePath = path;

        return runner.Run (options);
    }


    TEST_METHOD (ABlankDsk_IsSixteenSectorsWithNoFindings)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files["C:\\disks\\blank.dsk"] = std::vector<Byte> (143360, 0);

        result = Inspect (io, "C:\\disks\\blank.dsk");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string ("16-sector, 35 tracks with data, 560 of 560 sectors good, volume 254\n"), result.output);
    }


    TEST_METHOD (AMissingImage_Fails)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result = Inspect (io, "C:\\disks\\none.dsk");



        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.empty());
        Assert::IsFalse (result.diagnostics.empty());
    }
};
