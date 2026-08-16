#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunnerTests
//
//  The whole command path, from parsed options to what the executable would
//  print, with a real 1984 disk seeded into an in-memory platform.
//
//  Nothing here touches a file. The runner reaches the host only through the
//  seam, which is what makes the path testable at all -- the test assembly does
//  not link the console executable, so any decision placed there would be
//  unreachable from here.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskCommandRunnerTests)
{
public:

    static constexpr const char *  kImage = "C:\\disks\\merlin.dsk";

    //  Seeds the fake platform with a real disk, so the command path is
    //  exercised against material nobody here authored.
    void SeedRealDisk (FakeDiskFileIo & io, const char * fixture = "Disks/Merlin-proDos2.23.dsk",
                       const char * asPath = kImage)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));

        io.files[asPath]  = bytes;
        io.stamps[asPath] = FileStamp { bytes.size(), 100 };
    }

    CommandLineOptions MakeOptions (CommandLineOptions::DiskOptions::Verb verb,
                                    const char * image = kImage)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.verb      = verb;
        options.disk.imagePath = image;

        return options;
    }

    TEST_METHOD (List_RealDisk_RendersEveryCatalogEntryIncludingHeadings)
    {
        // The vendor's own printed catalog shows all sixty-three rows, twenty
        // of which occupy no sectors and exist to draw section headings. This
        // listing renders them for the same reason DOS does: they are what
        // their author put on the disk to be seen, and hiding them would make
        // this output disagree with the machine's.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        size_t             lines = 0;
        size_t             at    = 0;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus,
            L"a disk that shipped must list cleanly");

        Assert::IsTrue (result.output.find ("DISK VOLUME 254") != std::string::npos,
            L"the volume number is part of the listing");

        Assert::IsTrue (result.output.find ("MERLIN PRO - DOS 3.3") != std::string::npos,
            L"heading rows are rendered, not filtered");

        Assert::IsTrue (result.output.find ("*T 000 ") != std::string::npos,
            L"and they render in the shape the guest prints: locked, type T, zero sectors");

        Assert::IsTrue (result.output.find (" A 004 HELLO") != std::string::npos,
            L"alongside real files in the same format");

        while ((at = result.output.find ('\n', at)) != std::string::npos)
        {
            lines++;
            at++;
        }

        // 63 entries, a volume header line, a blank, a blank and a free-space
        // line. The exact total matters less than that no entry was dropped.
        Assert::IsTrue (lines >= 63, L"every catalog entry must appear");
    }

    TEST_METHOD (List_ReportsFreeSpace)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing states free space against the volume's capacity");
    }

    TEST_METHOD (List_ProDosVolume_UsesItsOwnShape)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", "C:\\disks\\merlin.po.dsk");

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List,
                                          "C:\\disks\\merlin.po.dsk"));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("/MERLIN") != std::string::npos,
            L"a ProDOS volume is named, not numbered");
        Assert::IsTrue (result.output.find ("blocks free of 280") != std::string::npos,
            L"and counts in blocks rather than sectors");
    }

    TEST_METHOD (Get_WithNoOutputFile_ReturnsPayloadForStandardOutput)
    {
        // With no destination named the bytes go to the process's own output,
        // which is what makes extraction pipeable.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"the payload is returned for the caller to deliver");
        Assert::AreEqual (size_t (589), result.payload.size(),
            L"589 bytes: the stored file's 593 less its four-byte header");
        Assert::AreEqual (size_t (0), io.files.count ("C:\\out.bin"),
            L"and nothing was written to the host");
    }

    TEST_METHOD (Get_WithOutputFile_WritesThroughTheSeam)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path     = "MAKE DUMP";
        options.disk.hostFile = "C:\\out.bin";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"a named destination is not the process output");
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.bin"));
        Assert::AreEqual (size_t (589), io.files["C:\\out.bin"].size());
    }

    TEST_METHOD (Get_ReportsTheLoadAddressOnTheErrorStream)
    {
        // The load address is information about the extraction, not part of it.
        // Putting it on the diagnostic stream keeps piped output byte-exact.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("$9000") != std::string::npos,
            L"the load address must be reported");
        Assert::IsTrue (result.output.empty(),
            L"and must not contaminate the payload stream");
    }

    TEST_METHOD (Get_MissingFile_ProducesNoOutputAndNamesBoth)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "NOSUCHFILE";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload);
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos,
            L"the message names the image");
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"and the file");
    }

    TEST_METHOD (UnreadableImage_ProducesNoOutput)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
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
        vector<Byte>       noise (NibblizationLayer::kImageByteSize, 0xE5);

        io.files[kImage]  = noise;
        io.stamps[kImage] = FileStamp { noise.size(), 1 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("recognizes") != std::string::npos);
    }

    TEST_METHOD (UnbuiltVerb_ReportsFailureRatherThanDoingNothingQuietly)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::Put));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus,
            L"an absent capability must not look like a completed operation");
        Assert::IsTrue (result.diagnostics.size() > 0);
    }

    TEST_METHOD (UnknownVerb_SuggestsTheOnesThatExist)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::None));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("list") != std::string::npos,
            L"a bad verb should say what the good ones are");
    }
};
