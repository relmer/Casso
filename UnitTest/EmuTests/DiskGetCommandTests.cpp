#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
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

    TEST_METHOD (Get_ByIndex_ReadsThatEntry_AndASharedNameAloneIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        DiskRunnerFixtures::SeedRealDisk (io);

        constexpr const char *  kNotAllowed = "--index is only allowed when identifying a specific file with a non-unique name";

        std::string    heading;
        vector<Byte>   bytes;
        VolumeListing  listing;

        //  The third entry is one of ten headings that share a name, read from
        //  the catalog including its padding.
        {
            FixtureProvider  fixtures;

            AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", bytes));

            Dos33Volume  volume (bytes);

            AssertSucceeded (volume.Enumerate (listing));

            heading = listing.entries[2].name;
        }

        //  Ten entries share the heading's name, so the name alone selects none
        //  of them.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        options.disk.path = heading;
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("the name is not unique") != std::string::npos);

        //  The name and the index together select that entry.
        options.disk.hasIndex = true;
        options.disk.index    = 3;
        result                = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("--index is only allowed") == std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("the name is not unique") == std::string::npos,
            L"the index resolves to the heading, regardless of the read result for an entry with no sectors");

        //  --index requires the name as well, so a wrong index cannot select a
        //  different file on its own.
        options.disk.path = "";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("<name>") != std::string::npos);

        //  A name given with the index must match that entry.
        options.disk.path = "MERLIN";
        result            = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("MERLIN: does not match index 3") != std::string::npos);

        //  HELLO, the fourth entry, is the only entry with its name, so --index
        //  for it is an error, as is an index past the end of the catalog.
        options.disk.path  = "HELLO";
        options.disk.index = 4;
        result             = runner.Run (options);

        Assert::IsFalse (result.hasPayload);
        Assert::IsTrue  (result.diagnostics.find (kNotAllowed) != std::string::npos);

        options.disk.index = 500;
        result             = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find (kNotAllowed) != std::string::npos);

        //  ProDOS names are unique, so --index is an error on a ProDOS disk.
        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk");

        options.disk.path  = "PRODOS";
        options.disk.index = 1;
        result             = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find (kNotAllowed) != std::string::npos);

        DiskRunnerFixtures::SeedRealDisk (io);
    }


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

    TEST_METHOD (Get_WithNoOutputFile_ReturnsPayloadForStandardOutput)
    {
        // With no destination named the bytes go to the process's own output,
        // which is what makes extraction pipeable.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"the payload is returned for the caller to deliver");
        DiskRunnerFixtures::AssertIsMakeDumpPayload (result.payload);
        Assert::AreEqual (size_t (0), io.files.count ("C:\\out.bin"),
            L"and nothing was written to the host");
    }

    TEST_METHOD (Get_WithOutputFile_WritesThroughTheSeam)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path     = "MAKE DUMP";
        options.disk.hostFile = "C:\\out.bin";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"an explicit destination is not the process output");
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.bin"));
        DiskRunnerFixtures::AssertIsMakeDumpPayload (io.files["C:\\out.bin"]);
    }

    TEST_METHOD (Get_PayloadCorrectnessStopsAtTheSeam_NotAtTheHostEdge)
    {
        // WHAT THIS TEST DOES NOT PROVE, stated because the omission is easy to
        // miss and a green run here looks exactly like coverage.
        //
        // MAKE DUMP's payload contains 29 line-feed bytes and no pre-existing
        // CR/LF pair. Delivered through a standard output left in text mode,
        // those 29 become 58 and the file arrives as 618 bytes rather than 589
        // -- purely additive corruption, so any count other than those two
        // numbers means something else is wrong.
        //
        // That translation happens in the runtime BELOW the seam, at the host
        // edge. The substitute used here does not translate, so this assertion
        // passes whether or not the edge sets binary mode. The fake's own
        // header says as much; this test says it at the point of temptation.
        //
        // No automated test can cover it: unit tests may not touch real system
        // state, and no test may run the console binary. The edge is therefore
        // verified by the manual check in quickstart.md, and the mode call
        // carries a comment saying why it must not be removed.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions  options   = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult   result;
        int                 lineFeeds = 0;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        for (Byte b : result.payload)
        {
            if (b == 0x0A) { lineFeeds++; }
        }

        Assert::AreEqual (29, lineFeeds,
            L"the payload carries the line feeds that make the host edge's mode matter");
    }

    TEST_METHOD (Get_ReportsTheLoadAddressOnTheErrorStream)
    {
        // The load address is information about the extraction, not part of it.
        // Putting it on the diagnostic stream keeps piped output byte-exact.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io);
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
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path = "NOSUCHFILE";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload);
        Assert::IsTrue (result.diagnostics.find (DiskRunnerFixtures::kImage) != std::string::npos,
            L"the message identifies the image");
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"and the file");
    }

    TEST_METHOD (Get_WithText_ConvertsFromAppleTextToHostText)
    {
        // T.SENDMSG is a real type-T file: high-bit ASCII with $8D line
        // terminators. Asked for as text it must arrive as something a host
        // editor opens, and asked for verbatim it must arrive untouched.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions  options   = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult   verbatim;
        DiskCommandResult   converted;
        size_t              highBytes = 0;
        size_t              newlines  = 0;
        size_t              i         = 0;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path = "T.SENDMSG";

        verbatim = runner.Run (options);

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
        converted             = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, verbatim.exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean, converted.exitStatus);

        for (i = 0; i < verbatim.payload.size(); i++)
        {
            if (verbatim.payload[i] >= 0x80) { highBytes++; }
        }

        Assert::IsTrue (highBytes > 0,
            L"the stored file must be high-bit for the conversion to mean anything");

        for (i = 0; i < converted.payload.size(); i++)
        {
            Assert::IsTrue (converted.payload[i] < 0x80,
                L"converted text carries no high-bit bytes");

            if (converted.payload[i] == '\n') { newlines++; }
        }

        Assert::IsTrue (newlines > 0, L"and its line endings are the host's");

        Assert::IsFalse (verbatim.payload == converted.payload,
            L"the two encodings must not deliver the same bytes -- otherwise the flag did nothing");
    }

    TEST_METHOD (Get_WithBasicOnAFileThatIsNoProgram_IsRefusedRatherThanRenderingGarbage)
    {
        // A source file is not a tokenized program, and the failure this forbids
        // is a listing rendered out of bytes that are not one: it would look
        // like a successful conversion and tokenize back to something else
        // entirely. Refusing needs the structural checks to be real -- walking
        // by the terminator alone would happily produce a listing here.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"nothing may be delivered under a conversion not performed");
        Assert::IsTrue (result.diagnostics.find ("--basic") != std::string::npos,
            L"and the refusal must quote the flag it is refusing");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
    }

    //  The in-use probe used to be described by a paragraph of help text, and a
    //  test here read that paragraph. Both are gone: a user meets the probe by
    //  having a write refused, so the refusal is the whole explanation, and
    //  DiskFailureModeTests already drives the refusal end to end against the
    //  wording it now shares with the runner.

    TEST_METHOD (Get_WithText_ToANamedFile_ConvertsThereToo)
    {
        // The conversion belongs to the payload, not to the destination. A
        // caller who redirects to a file and a caller who pipes must get the
        // same bytes.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  piped;
        DiskCommandResult  written;

        DiskRunnerFixtures::SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        piped = runner.Run (options);

        options.disk.hostFile = "C:\\out.txt";
        written               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, written.exitStatus);
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.txt"));
        Assert::IsTrue (io.files["C:\\out.txt"] == piped.payload,
            L"the same conversion, whichever way the bytes leave");
    }
};
