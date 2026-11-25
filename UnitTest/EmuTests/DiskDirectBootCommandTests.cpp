#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/DirectBootBuilder.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDirectBootCommandTests
//
//  disk create --boot: a disk that starts a binary with no operating system.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskDirectBootCommandTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  create --boot: a disk that starts a binary with no operating system.
    //
    //  The builder underneath is already gated by a real-CPU test that boots a
    //  6502 over its output. What these cover is the half that did not exist
    //  until now: reaching it from a command, and writing what it produces into
    //  the container the caller asked for.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  THE COMMAND AND THE BUILDER AGREE, byte for byte. A .dsk is the sector
    //  buffer verbatim, so the whole path can be checked against the thing the
    //  guest-visible tests already boot, with no CPU in the loop.
    TEST_METHOD (DirectBoot_WritesExactlyWhatTheBuilderProduced)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (32, (Byte) 0xEA);
        vector<Byte>       expected;
        vector<Byte>       written;
        DirectBootSpec     spec;
        std::string        refusal;
        DiskCommandResult  result;

        io.files["prog.bin"] = payload;

        result = runner.Run (DiskRunnerFixtures::MakeDirectBoot ("boot.dsk", "prog.bin"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, expected, refusal));
        AssertSucceeded (io.ReadAllBytes ("boot.dsk", written));

        Assert::IsTrue (written == expected, L"the image is the builder's own sectors");
    }

    //  The container follows the name here as it does everywhere else, and a
    //  WOZ is a bit stream rather than sectors, so its size differs.
    TEST_METHOD (DirectBoot_HonorsTheContainerTheNameAsksFor)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (32, (Byte) 0xEA);
        vector<Byte>       written;

        io.files["prog.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (DiskRunnerFixtures::MakeDirectBoot ("boot.po", "prog.bin")).exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (DiskRunnerFixtures::MakeDirectBoot ("boot.woz", "prog.bin")).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("boot.po", written));
        Assert::AreEqual ((size_t) NibbleFormat::kImageByteSize, written.size(),
                          L"a .po is the same sectors in ProDOS order");

        AssertSucceeded (io.ReadAllBytes ("boot.woz", written));
        Assert::IsTrue (written.size() > (size_t) NibbleFormat::kImageByteSize,
                        L"and a .woz is a bit stream, which is larger");
    }

    //  AN ENTRY AWAY FROM THE LOAD ADDRESS REACHES THE BUILDER. A payload whose
    //  first byte is a header rather than an instruction is ordinary, and the
    //  flag exists so it does not have to be rebuilt to boot.
    TEST_METHOD (DirectBoot_PassesTheEntryAddressThrough)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        vector<Byte>        payload (64, (Byte) 0xEA);
        vector<Byte>        expected;
        vector<Byte>        written;
        CommandLineOptions  options = DiskRunnerFixtures::MakeDirectBoot ("boot.dsk", "prog.bin");
        DirectBootSpec      spec;
        std::string         refusal;

        io.files["prog.bin"] = payload;

        options.disk.entryAddress    = 0x0920;
        options.disk.hasEntryAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        spec.entryAddress = 0x0920;
        AssertSucceeded (DirectBootBuilder::Build (payload, spec, expected, refusal));
        AssertSucceeded (io.ReadAllBytes ("boot.dsk", written));

        Assert::IsTrue (written == expected, L"the entry the caller specified is the one built in");
    }

    //  THE TWO WAYS TO BOOT ARE REFUSED TOGETHER. Measured before the check
    //  existed, the pair honored --boot and dropped --bootable silently,
    //  because this path never reaches the code that would have caught it.
    TEST_METHOD (DirectBoot_AndCopyingAnOperatingSystem_AreRefusedTogether)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeDirectBoot ("boot.dsk", "prog.bin");
        DiskCommandResult   result;

        io.files["prog.bin"] = vector<Byte> (32, (Byte) 0xEA);

        options.disk.bootable = true;
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);
        Assert::IsFalse (io.Exists ("boot.dsk"), L"and nothing was written");
    }

    //  A direct-boot disk holds the binary and nothing else, so a filesystem
    //  asked for alongside it is refused rather than one of them dropped.
    TEST_METHOD (DirectBoot_WithAFilesystemAskedFor_IsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeDirectBoot ("boot.dsk", "prog.bin");
        DiskCommandResult   result;

        io.files["prog.bin"] = vector<Byte> (32, (Byte) 0xEA);

        options.disk.formatName = "prodos";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (io.Exists ("boot.dsk"));
    }

    //  The builder names exactly one reason and the runner passes it through,
    //  rather than restating the window arithmetic a second time.
    TEST_METHOD (DirectBoot_OutsideTheWindow_ReportsTheBuildersOwnReason)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeDirectBoot ("boot.dsk", "prog.bin");
        DiskCommandResult   result;

        io.files["prog.bin"] = vector<Byte> (32, (Byte) 0xEA);

        options.disk.loadAddress    = 0x0800;
        options.disk.hasLoadAddress = true;
        result                      = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("$0900") != std::string::npos,
                        L"the window's lower edge is reported");
        Assert::IsFalse (io.Exists ("boot.dsk"));
    }
};
