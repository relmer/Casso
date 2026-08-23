#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/VolumeImage.h"

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

    //
    //  A LISTING HAS TO FIT AN 80-COLUMN TERMINAL, and that is the constraint
    //  that made the ProDOS columns safe to print unconditionally rather than
    //  behind a flag. A row that wraps is a row that reads as two entries.
    //
    static void AssertEveryLineFitsEightyColumns (const std::string & text)
    {
        size_t  lineStart = 0;
        size_t  lineEnd   = 0;

        while (lineStart < text.size())
        {
            std::string  line;

            lineEnd = text.find ('\n', lineStart);
            line    = (lineEnd == std::string::npos)
                          ? text.substr (lineStart)
                          : text.substr (lineStart, lineEnd - lineStart);

            Assert::IsTrue (line.size() <= 80,
                (std::wstring (L"line runs past column 80: ")
                     + std::wstring (line.begin(), line.end())).c_str());

            if (lineEnd == std::string::npos)
            {
                break;
            }

            lineStart = lineEnd + 1;
        }
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

    //  The independently extracted copy, minus the four-byte DOS 3.3 header the
    //  reader consumes. Comparing against these bytes is strictly stronger than
    //  comparing a length: a length check passes under any corruption that
    //  happens to preserve size, and the oracle is already sitting there.
    vector<Byte> ExpectedMakeDumpPayload()
    {
        FixtureProvider  fixtures;
        vector<Byte>     stored;

        AssertSucceeded (fixtures.OpenFixture ("Merlin/MAKE DUMP", stored));
        Assert::AreEqual (size_t (593), stored.size(), L"the stored file carries its header");

        return vector<Byte> (stored.begin() + 4, stored.end());
    }

    void AssertIsMakeDumpPayload (const vector<Byte> & actual)
    {
        vector<Byte>  expected = ExpectedMakeDumpPayload();
        size_t        i        = 0;

        // 589 is the length the header declares. Naming it here makes the
        // failure legible: 618 would mean 29 line-feed bytes were expanded on
        // the way out, which is the text-mode corruption this file's payload is
        // uniquely suited to expose.
        Assert::AreEqual (size_t (589), expected.size());
        Assert::AreEqual (expected.size(), actual.size());

        for (i = 0; i < expected.size(); i++)
        {
            if (actual[i] != expected[i])
            {
                Assert::Fail (L"payload differs from the independently extracted copy");
            }
        }
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
        AssertIsMakeDumpPayload (result.payload);
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
        AssertIsMakeDumpPayload (io.files["C:\\out.bin"]);
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
        CommandLineOptions  options   = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult   result;
        int                 lineFeeds = 0;

        SeedRealDisk (io);
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
        Assert::IsTrue (result.output.empty(), L"and nothing is offered as a catalog");
        Assert::IsTrue (result.diagnostics.find (DiskCommandRunner::kNoFilesystemText)
                            != std::string::npos,
            L"in the words a person would use");
    }

    //  IT ASKS FOR THE PAGE RATHER THAN LISTING THE VERBS ITSELF.
    //
    //  The refusal used to name all twelve, which put the same list on the
    //  screen twice once the edge started printing the disk page above it.
    //  What the refusal owes the reader now is which word it could not read;
    //  what the verbs ARE is the page's job, and a sweep over the grammar's
    //  own table already holds the page to it.
    ////////////////////////////////////////////////////////////////////////////
    //
    //  create and init.
    //
    //  The verbs that make a disk rather than edit one, and the pair the worked
    //  example needed: every step of it began `disk put mydisk.dsk`, and
    //  nothing anywhere made mydisk.dsk.
    //
    ////////////////////////////////////////////////////////////////////////////

    static CommandLineOptions MakeCreate (const char * path)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.verb      = CommandLineOptions::DiskOptions::Verb::Create;
        options.disk.verbWord  = "create";
        options.disk.imagePath = path;

        return options;
    }

    TEST_METHOD (Create_WritesAFormattedImageThatListsAsEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result = runner.Run (MakeCreate ("new.dsk"));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (io.Exists ("new.dsk"), L"the image is there afterwards");
        Assert::IsTrue (result.output.find ("DOS 3.3") != std::string::npos,
                        L"and it says what it made");
    }

    //  IT WILL NOT WRITE OVER SOMETHING. A disk somebody still wanted is one
    //  keystroke from a disk they no longer have, and the refusal names the
    //  verb for meaning it.
    TEST_METHOD (Create_RefusesToReplaceAnImageThatIsAlreadyThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  first  = runner.Run (MakeCreate ("new.dsk"));
        DiskCommandResult  second = runner.Run (MakeCreate ("new.dsk"));

        Assert::AreEqual (DiskCommandRunner::kClean,    first.exitStatus);
        Assert::AreEqual (DiskCommandRunner::kNoOutput, second.exitStatus);
        Assert::IsTrue (second.diagnostics.find ("init") != std::string::npos,
                        L"and points at the verb that does mean it");
    }

    //  The container follows the name when --type is not given, which is what
    //  makes `disk create mydisk.po` do the obvious thing.
    TEST_METHOD (Create_TakesTheContainerFromTheNameWhenTypeIsNotGiven)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.po");
        DiskCommandResult   result;

        options.disk.formatName = "prodos";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("ProDOS") != std::string::npos);
    }

    //  A word that names no container is refused BY NAME. Handing back a .dsk
    //  would be a disk they did not ask for under a name they did.
    TEST_METHOD (Create_RefusesAContainerItCannotWrite)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.2mg");
        DiskCommandResult   result;

        options.disk.containerType = "2mg";
        result                     = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"so the page prints above it");
        Assert::IsTrue (result.diagnostics.find ("2mg") != std::string::npos,
                        L"and the word they typed is quoted back");
        Assert::IsFalse (io.Exists ("new.2mg"), L"and nothing was written");
    }

    //  A DOS 3.3 volume is a NUMBER, and a word that is not one is refused
    //  rather than quietly reading as zero.
    TEST_METHOD (Create_RefusesAVolumeThatIsNotADosNumber)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.dsk");
        DiskCommandResult   result;

        options.disk.volumeName = "MYDISK";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsFalse (io.Exists ("new.dsk"));
    }

    //  init needs one to be there; create needs one not to be. Between them
    //  every state is covered and neither guesses.
    TEST_METHOD (Init_RefusesAnImageThatIsNotThere)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("missing.dsk");
        DiskCommandResult   result;

        options.disk.verb     = CommandLineOptions::DiskOptions::Verb::Init;
        options.disk.verbWord = "init";
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("create") != std::string::npos,
                        L"and points at the verb that makes one");
    }

    //  THE CONTAINER IS NOT A CHOICE UNDER init. It was decided when the file
    //  was made, so --type is refused rather than silently ignored.
    TEST_METHOD (Init_RefusesTypeBecauseTheContainerIsAlreadyDecided)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.dsk");
        DiskCommandResult   result;

        runner.Run (options);

        options.disk.verb          = CommandLineOptions::DiskOptions::Verb::Init;
        options.disk.verbWord      = "init";
        options.disk.containerType = "woz";
        result                     = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);
    }

    TEST_METHOD (UnknownVerb_AsksForThePageInsteadOfListingTheVerbsAgain)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::None);
        DiskCommandResult   result;

        options.disk.verbWord = "frobnicate";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine,
            L"which is what makes the edge print the page");
        Assert::IsTrue (result.diagnostics.find ("frobnicate") != std::string::npos,
            L"and the refusal names the word that could not be read");
        Assert::IsTrue (result.diagnostics.find ("catalog") == std::string::npos,
            L"without repeating the page's own verb list under it");
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
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::List);
        DiskCommandResult   result;

        SeedRealDisk (io);
        options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty(), L"a listing this command line did not earn");

        //  And nothing is said, because the parser already named the argument
        //  it could not take. One mistake, one message.
        Assert::AreEqual (std::string(), result.diagnostics);
    }

    //
    //  EVERY COLUMN THE VOLUME RECORDS, WITH NO FLAG TO ASK FOR IT.
    //
    //  `eof=` and `aux=` sat behind `--long`, and ProDosVolume::Enumerate fills
    //  both whether or not anybody asks -- so the flag bought nothing and cost
    //  a reading of the help plus a second run of the command. They are the two
    //  fields a build loop most wants, which is what made hiding them worst:
    //  the exact length of a file and the address a binary loads at are exactly
    //  what you check after placing one.
    //
    TEST_METHOD (List_OfAProDosVolume_CarriesEofAndAux_WithNoFlagToAskForThem)
    {
        static constexpr const char *  kProDosImage = "C:\\disks\\pro.dsk";

        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::List,
                                                   kProDosImage);
        DiskCommandResult   result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProDosImage);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);

        //  A whole rendered row rather than a substring search for `eof=`: the
        //  columns have to land in their places, and the row has to stay inside
        //  80 characters now that nothing can turn them off.
        Assert::IsTrue (result.output.find ("*MERLIN.SYSTEM        $FF    37  eof=18432 aux=$2000\n")
                            != std::string::npos,
            L"the ProDOS row, with both columns, unasked for");

        AssertEveryLineFitsEightyColumns (result.output);
    }

    //  DOS 3.3 records neither field and has its own formatter, so it must be
    //  untouched by the ProDOS columns becoming unconditional.
    TEST_METHOD (List_OfADos33Volume_IsUnchangedByTheProDosColumns)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find (" B 004 MERLIN\n") != std::string::npos,
            L"the DOS 3.3 row is what a booted machine prints");
        Assert::IsTrue (result.output.find ("eof=") == std::string::npos,
            L"and carries no field this filesystem does not record");
    }

    TEST_METHOD (Get_WithText_ConvertsFromAppleTextToHostText)
    {
        // T.SENDMSG is a real type-T file: high-bit ASCII with $8D line
        // terminators. Asked for as text it must arrive as something a host
        // editor opens, and asked for verbatim it must arrive untouched.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions  options   = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult   verbatim;
        DiskCommandResult   converted;
        size_t              highBytes = 0;
        size_t              newlines  = 0;
        size_t              i         = 0;

        SeedRealDisk (io);
        options.disk.path = "T.SENDMSG";

        verbatim = runner.Run (options);

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
        converted             = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, verbatim.exitStatus);
        Assert::AreEqual (DiskCommandRunner::kClean, converted.exitStatus);

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
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"nothing may be delivered under a conversion not performed");
        Assert::IsTrue (result.diagnostics.find ("--basic") != std::string::npos,
            L"and the refusal must name the flag it is refusing");

        AssertNamesNoPlatformCode (result.diagnostics);
    }

    //
    //  ------------------------------------------------------------------
    //  The commit path.
    //
    //  Its failure modes are the ones that never run in normal use, so they
    //  are the ones nothing has checked. The assertion that carries every
    //  test below is that the TARGET IS BYTE-IDENTICAL to what it was, plus
    //  that no temporary is left behind -- never merely that an error came
    //  back. An error verdict is satisfied by a half-written image.
    //  ------------------------------------------------------------------
    //

    //  The image exactly as it was seeded, kept for comparison. Read from the
    //  fixture rather than from the fake, so a commit that corrupted the fake's
    //  copy cannot also corrupt the oracle.
    vector<Byte> OriginalImageBytes()
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", bytes));
        Assert::IsTrue (bytes.size() > 0, L"the oracle must actually have been read");

        return bytes;
    }

    void AssertImageIsUntouched (FakeDiskFileIo & io)
    {
        vector<Byte>  original = OriginalImageBytes();
        size_t        i        = 0;

        Assert::AreEqual (size_t (1), io.files.count (kImage), L"the image must still be there");
        Assert::AreEqual (original.size(), io.files[kImage].size(),
            L"and be the size it was");

        for (i = 0; i < original.size(); i++)
        {
            if (io.files[kImage][i] != original[i])
            {
                Assert::Fail (L"the image differs from what it was before the refused commit");
            }
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(),
            L"and no temporary may be left beside it");
    }

    //  Something plausible to commit: the image with one byte changed, which is
    //  enough to prove a commit either landed or did not.
    vector<Byte> EditedImageBytes()
    {
        vector<Byte>  bytes = OriginalImageBytes();

        bytes[0] = (Byte) (bytes[0] ^ 0xFF);

        return bytes;
    }

    //  The temporary path a commit chose, taken from what the fake was asked to
    //  write rather than re-derived here -- a test that restates the derivation
    //  agrees with the code by construction and cannot disagree with it.
    std::string TemporaryPathChosen (const FakeDiskFileIo & io)
    {
        for (const std::string & path : io.writtenPaths)
        {
            if (path != kImage)
            {
                return path;
            }
        }

        Assert::Fail (L"nothing was written anywhere but the image");

        return std::string();
    }

    TEST_METHOD (Commit_OnTheHappyPath_ReplacesTheImageAndLeavesNoTemporary)
    {
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        vector<Byte>                   edited;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        edited = EditedImageBytes();

        AssertSucceeded (runner.CommitImage (opened, edited, result));

        Assert::IsTrue (io.files[kImage] == edited, L"the new bytes are the image now");
        Assert::AreEqual (1, io.replaceCount, L"and arrived by one atomic replace");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"with nothing left over");
        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
    }

    TEST_METHOD (Commit_NeverWritesTheTargetDirectly)
    {
        // The property the whole plan exists for: the new bytes go somewhere
        // they cannot be mistaken for the image, and become the image in one
        // step. A commit that wrote the target and then tidied up would pass
        // every before-and-after comparison and still truncate the user's disk
        // when it was interrupted.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));
        AssertSucceeded (runner.CommitImage (opened, EditedImageBytes(), result));

        for (const std::string & path : io.writtenPaths)
        {
            Assert::IsFalse (path == kImage,
                L"the image itself must never be handed to a plain write");
        }

        Assert::AreEqual (size_t (1), io.writtenPaths.size(),
            L"one write, to the temporary");
    }

    TEST_METHOD (Commit_WhenTheWriteFails_LeavesTheImageByteIdenticalAndSweepsThePartial)
    {
        // The fake leaves a partial file behind on a failed write, because the
        // platform does. Asserting only that an error came back would pass with
        // that partial file still sitting beside the user's disk.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.failNextWrite = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.removeCount, L"the partial temporary was swept");
        Assert::AreEqual (0, io.replaceCount, L"and nothing was ever put over the image");
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos,
            L"and the refusal names the image");
    }

    TEST_METHOD (Commit_WhenTheReplaceFails_LeavesTheImageByteIdenticalAndRemovesTheTemporary)
    {
        // Here the temporary genuinely exists and is complete -- the last and
        // most tempting moment to leave it behind, since the bytes in it are
        // good and somebody might want them.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.failNextReplace = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.replaceCount, L"the replace was attempted");
        Assert::AreEqual (1, io.removeCount,  L"and its temporary removed when it failed");
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
    }

    TEST_METHOD (Commit_WhenTheImageWasRewrittenSinceItWasRead_RefusesBeforeWritingAnything)
    {
        // Somebody else landed a write between the read and the commit. What we
        // computed describes an image that no longer exists, so committing it
        // would silently discard their work.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.mutateStampOnNextStat = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount,   L"nothing was written at all");
        Assert::AreEqual (0, io.replaceCount, L"and nothing replaced");
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("changed since it was read") != std::string::npos,
            L"and the reason given is staleness, not some other refusal");
    }

    TEST_METHOD (Commit_WhenOnlyTheSIZEChangedSinceItWasRead_RefusesToo)
    {
        // The case the modification time cannot produce: filesystem timestamps
        // are coarse, so a second write inside one tick carries the same time
        // and only the size gives it away. A staleness check comparing time
        // alone passes every other test in this file and fails here.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.stamps[kImage].sizeBytes += 1;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount, L"nothing was written at all");
        Assert::IsTrue (result.diagnostics.find ("changed since it was read") != std::string::npos);
    }

    TEST_METHOD (Commit_WhenAnotherProgramHoldsTheImageOpen_RefusesWithoutTouchingIt)
    {
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.reportHeldByOther = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount,   L"the probe refuses before anything exists");
        Assert::AreEqual (0, io.replaceCount);
        Assert::AreEqual (0, io.removeCount,  L"so there is nothing to clean up either");
        Assert::IsTrue (result.diagnostics.find ("another program") != std::string::npos,
            L"and the reason given is the holder, not staleness or a write failure");
    }

    TEST_METHOD (Commit_WithNoStampRecordedAtRead_RefusesRatherThanSkippingTheCheck)
    {
        // A platform that cannot report a size and time leaves the staleness
        // guarantee unenforceable. Proceeding anyway would be indistinguishable
        // from enforcing it, which is the shape this codebase keeps getting
        // bitten by -- a degraded state that reads as a healthy one.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        io.stamps.erase (kImage);

        AssertSucceeded (runner.OpenImage (kImage, opened, result),
            L"reading does not need the stamp and must still work");

        Assert::IsFalse (opened.stampRecorded);

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount);
        Assert::IsTrue (result.diagnostics.find ("could not be checked") != std::string::npos);
    }

    TEST_METHOD (Commit_WhenTheFirstTemporaryNameIsTaken_StepsOverItRatherThanOverwritingIt)
    {
        // Somebody's abandoned temporary is sitting at the name this commit
        // would take. Its contents are not ours to destroy, and more to the
        // point, a commit that overwrote it would be a commit that could also
        // overwrite a LIVE one belonging to a concurrent invocation.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        std::string                    taken;
        vector<Byte>                   sentinel = { 'N', 'O', 'T', 'Y', 'O', 'U', 'R', 'S' };

        // Learn the name this runner reaches for, by watching it commit once.
        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));
        AssertSucceeded (runner.CommitImage (opened, EditedImageBytes(), result));

        taken = TemporaryPathChosen (io);

        // Put the image back, park something at that name, and make the SAME
        // runner commit again -- same runner, so the same name comes up first.
        SeedRealDisk (io);
        io.files[taken]  = sentinel;
        io.stamps[taken] = FileStamp { sentinel.size(), 1 };

        {
            DiskCommandRunner::OpenedImage second;
            DiskCommandResult              secondResult;

            AssertSucceeded (runner.OpenImage (kImage, second, secondResult));
            AssertSucceeded (runner.CommitImage (second, EditedImageBytes(), secondResult));
        }

        Assert::AreEqual (size_t (1), io.files.count (taken),
            L"the file already there must survive");

        Assert::IsTrue (io.files[taken] == sentinel,
            L"byte for byte -- stepping over a name means not writing it");

        Assert::IsTrue (io.files[kImage] == EditedImageBytes(),
            L"and the commit still landed, under another name");
    }

    TEST_METHOD (Commit_TwoInvocations_DoNotReachForTheSameTemporaryName)
    {
        // The concurrency requirement, tested without concurrency. Two runners
        // stand in for two tool runs against one image; if both derive the same
        // name, both find it free at the same instant and one silently commits
        // the other's bytes. Nothing about that needs threads to demonstrate.
        FakeDiskFileIo                 ioA;
        FakeDiskFileIo                 ioB;
        DiskCommandRunner              runnerA (ioA);
        DiskCommandRunner              runnerB (ioB);
        DiskCommandRunner::OpenedImage openedA;
        DiskCommandRunner::OpenedImage openedB;
        DiskCommandResult              resultA;
        DiskCommandResult              resultB;

        SeedRealDisk (ioA);
        SeedRealDisk (ioB);

        AssertSucceeded (runnerA.OpenImage (kImage, openedA, resultA));
        AssertSucceeded (runnerB.OpenImage (kImage, openedB, resultB));
        AssertSucceeded (runnerA.CommitImage (openedA, EditedImageBytes(), resultA));
        AssertSucceeded (runnerB.CommitImage (openedB, EditedImageBytes(), resultB));

        Assert::IsFalse (TemporaryPathChosen (ioA) == TemporaryPathChosen (ioB),
            L"two invocations against one image must not reach for one name");
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
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  piped;
        DiskCommandResult  written;

        SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        piped = runner.Run (options);

        options.disk.hostFile = "C:\\out.txt";
        written               = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, written.exitStatus);
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.txt"));
        Assert::IsTrue (io.files["C:\\out.txt"] == piped.payload,
            L"the same conversion, whichever way the bytes leave");
    }

    //
    //  ------------------------------------------------------------------
    //  put and delete.
    //
    //  These are the first verbs that change a user's disk, so every refusal
    //  below asserts the image is byte-for-byte what it was rather than
    //  merely that an error came back -- an error verdict is satisfied by a
    //  half-written image.
    //  ------------------------------------------------------------------
    //

    static constexpr const char *  kBlankImage = "C:\\disks\\blank.dsk";
    static constexpr const char *  kHostFile   = "C:\\build\\prog.bin";
    static constexpr const char *  kProImage   = "C:\\disks\\merlin.po.dsk";
    static constexpr const char *  kProOrdered = "C:\\disks\\merlin.po";

    static constexpr Byte      kBlankVolumeNumber = 254;
    static constexpr size_t    kPayloadBytes      = 512;
    static constexpr Word      kLoadAddress       = 0x6000;

    //  Bigger than the 496 sectors a freshly formatted volume leaves free, so
    //  placement is refused for want of room rather than for any other reason.
    static constexpr size_t    kOversizedBytes    = 130000;

    //  Catalog geometry, restated here only for the two tests that reach into
    //  a catalog entry directly -- to lock a file, and to break its chain.
    static constexpr int       kVtocTrack          = 17;
    static constexpr int       kCatalogFirstSector = 15;

    static constexpr size_t    kEntryBase         = 0x0B;
    static constexpr size_t    kEntOffTsTrack     = 0x00;
    static constexpr size_t    kEntOffTsSector    = 0x01;
    static constexpr size_t    kEntOffType        = 0x02;
    static constexpr size_t    kTsOffNextTrack    = 0x01;
    static constexpr size_t    kTsOffNextSector   = 0x02;
    static constexpr Byte      kLockedBit         = 0x80;
    static constexpr Byte      kTrackOffTheVolume = 40;

    vector<Byte> MakeBlankDos33Image()
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kBlankVolumeNumber));

        return buffer;
    }

    void SeedFile (FakeDiskFileIo & io, const char * path, const vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 100 };
    }

    //  A payload nothing else on the disk could be mistaken for, so a read-back
    //  comparison fails loudly rather than matching some neighboring sector.
    vector<Byte> MakePayload (size_t count = kPayloadBytes)
    {
        vector<Byte>  bytes (count, 0);
        size_t        i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) ((i * 7 + 0x21) & 0xFF);
        }

        return bytes;
    }

    CommandLineOptions MakePutOptions (const char * image,
                                       const char * hostFile,
                                       const char * asName)
    {
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Put, image);

        options.disk.hostFile = hostFile;

        if (asName != nullptr)
        {
            options.disk.path = asName;
        }

        return options;
    }

    CommandLineOptions MakeDeleteOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Delete, image);

        options.disk.path = path;

        return options;
    }

    //  Reads a file back through a FRESH runner over the committed image, which
    //  is the only way to prove the bytes went to the disk rather than merely
    //  through the runner that put them there.
    DiskCommandResult GetFromCommittedImage (FakeDiskFileIo & io,
                                             const char     * image,
                                             const char     * path,
                                             CommandLineOptions::DiskOptions::Encoding encoding
                                                 = CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        DiskCommandRunner   reader (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get, image);

        options.disk.path     = path;
        options.disk.encoding = encoding;

        return reader.Run (options);
    }

    std::string ListCommittedImage (FakeDiskFileIo & io, const char * image)
    {
        DiskCommandRunner  reader (io);

        return reader.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List, image)).output;
    }

    void AssertImageMatches (FakeDiskFileIo & io, const char * path, const vector<Byte> & expected)
    {
        size_t  i = 0;

        Assert::AreEqual (size_t (1), io.files.count (path), L"the image must still be there");
        Assert::AreEqual (expected.size(), io.files[path].size(), L"and be the size it was");

        for (i = 0; i < expected.size(); i++)
        {
            if (io.files[path][i] != expected[i])
            {
                Assert::Fail (L"the image differs from what it was before the refused write");
            }
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and no temporary may be left beside it");
    }

    static void AssertNamesNoPlatformCode (const std::string & diagnostics)
    {
        // FR-014's demand is that a refusal reads as a reason. A hexadecimal
        // HRESULT is the specific failure mode it forbids, and it is the shape
        // a message picks up the moment somebody formats `hr` into it.
        Assert::IsTrue (diagnostics.find ("0x") == std::string::npos,
            L"a refusal must not carry a raw platform code");
    }

    TEST_METHOD (Put_ABinaryOntoAFreshVolume_LandsAndReadsBackByteForByte)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        vector<Byte>        payload = MakePayload();

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus,
            L"a placement with room, a legal name and an address is clean");

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and leaves nothing beside the image");

        // 512 bytes plus the four-byte load/length header DOS stores inside the
        // file is 516 bytes -- three data sectors -- and the track/sector list
        // is a fourth. The `B 002` that the task text and quickstart both
        // carried is the arithmetic for a payload of 252 bytes or fewer.
        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B 004 PROG") != std::string::npos,
            L"the guest's own listing shape, with the sector count the file really occupies");

        readBack = GetFromCommittedImage (io, kBlankImage, "PROG");

        Assert::AreEqual (DiskCommandRunner::kClean, readBack.exitStatus);
        Assert::IsTrue (readBack.payload == payload,
            L"the bytes on the disk are the bytes that went in");

        Assert::IsTrue (readBack.diagnostics.find ("$6000") != std::string::npos,
            L"and the load address survived the round trip");
    }

    TEST_METHOD (PutVerbatim_RoundTripsTheFILEBytes_NotTheImageBytes)
    {
        // THE GATE FOR THE UNCONVERTED PATH, AND THE ASSERTION IS FILE EQUALITY.
        //
        // Image equality is the wrong check and would fail here for two reasons
        // that have nothing to do with character conversion. A DOS 3.3 file
        // occupies whole sectors, so the bytes past its recorded length are
        // whatever was there before; and a replacement reallocates, so the file
        // can land somewhere else entirely on a disk that is otherwise
        // unchanged. Neither says anything about whether the bytes were
        // perturbed. What must hold -- and all that must hold -- is that the
        // file comes back identical.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options   = MakePutOptions (kImage, kHostFile, "MAKE DUMP");
        DiskCommandResult   extracted;
        DiskCommandResult   result;
        DiskCommandResult   readBack;

        SeedRealDisk (io);

        extracted = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandRunner::kClean, extracted.exitStatus);
        AssertIsMakeDumpPayload (extracted.payload);

        SeedFile (io, kHostFile, extracted.payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x9000;
        options.disk.hasLoadAddress = true;
        options.disk.encoding       = CommandLineOptions::DiskOptions::Encoding::Verbatim;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus,
            L"replacing a file with its own contents must succeed");

        readBack = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandRunner::kClean, readBack.exitStatus);

        // The file, byte for byte, through a path that re-read the committed
        // image rather than trusting the runner that wrote it.
        AssertIsMakeDumpPayload (readBack.payload);
        Assert::IsTrue (readBack.payload == extracted.payload);

        // And the reason an image comparison could not stand in for that: the
        // file occupies four sectors, so its footprint on the disk is 1024
        // bytes while the file itself is 589. Comparing the footprint would
        // compare 431 bytes that are not the file.
        Assert::AreEqual (size_t (589), readBack.payload.size());
        Assert::IsTrue (ListCommittedImage (io, kImage).find ("B 004 MAKE DUMP\n") != std::string::npos,
            L"four sectors of footprint for a 589-byte file");
    }

    TEST_METHOD (PutVerbatim_ReusesTheSpaceItFreed_AssertedSeparatelyFromTheBytes)
    {
        // The sector-reuse question, kept apart from the byte question on
        // purpose. Conflating them is what makes an image comparison look like
        // a conversion check: this asserts the volume gave back exactly what it
        // took, which is about ALLOCATION, and says nothing about the contents.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kImage, kHostFile, "MAKE DUMP");
        DiskCommandResult   extracted;
        std::string         before;
        std::string         after;

        SeedRealDisk (io);

        before    = ListCommittedImage (io, kImage);
        extracted = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        SeedFile (io, kHostFile, extracted.payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x9000;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        after = ListCommittedImage (io, kImage);

        Assert::IsTrue (before.find ("sectors free of 560") != std::string::npos,
            L"the listing must actually be reporting free space for this to mean anything");

        Assert::AreEqual (FreeSpaceLine (before), FreeSpaceLine (after),
            L"a file replaced by its own contents leaks nothing and grows nothing");
    }

    //  The trailing free-space report, which the listing separates from the
    //  entries with a blank line. Taken as text rather than reparsed, so a
    //  change of either number shows up in the failure message.
    static std::string FreeSpaceLine (const std::string & listing)
    {
        size_t  at = listing.rfind ("\n\n");

        Assert::IsTrue (at != std::string::npos, L"the listing must carry a free-space report");

        return listing.substr (at);
    }

    TEST_METHOD (Put_OverALockedFile_IsRefusedInTermsThatSayItIsLocked)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        LockFirstCatalogEntry (io.files[kBlankImage]);
        committed = io.files[kBlankImage];

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is locked on this volume") != std::string::npos,
            L"the refusal must say the file is locked, not merely that something failed");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, committed);
    }

    TEST_METHOD (Put_WhenTheVolumeHasNoRoom_RefusesAndLeavesTheImageByteIdentical)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "BIG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload (kOversizedBytes));

        options.disk.typeName = "T";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("does not fit") != std::string::npos,
            L"and says the volume has no room, not something generic");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount, L"nothing was written anywhere at all");
    }

    TEST_METHOD (Put_WithANameTheCatalogCannotStore_RefusesAndSaysWhatALegalNameIs)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "9LIVES");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("9LIVES") != std::string::npos,
            L"the message names the name that was refused");
        Assert::IsTrue (result.diagnostics.find ("starting with a letter") != std::string::npos,
            L"and says what a legal one looks like");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
    }

    //  A listing whose every interesting rule is exercised: a token form
    //  inside a string, a DATA payload with a colon in a quoted item, a REM that
    //  swallows one, and spacing that has to survive the trip unchanged.
    static std::string BasicListing()
    {
        return "10 REM  A SPACED REM\n"
               "20 PRINT \"PRINT AT TO\"\n"
               "30 DATA \"A:B\",C: PRINT 1\n"
               "40 FOR I = 1 TO 9: NEXT\n";
    }

    static vector<Byte> BasicListingBytes()
    {
        std::string  listing = BasicListing();

        return vector<Byte> (listing.begin(), listing.end());
    }

    TEST_METHOD (Put_ABasicListing_LandsTokenizedAndComesBackAsTheSameListing)
    {
        // The property that matters to a user is that the file they placed and
        // the file they get back are the same file. Asserting only that the
        // placement succeeded would pass against a tokenizer that stored the
        // listing verbatim under a BASIC type, which is exactly what the
        // refusal this replaces existed to prevent.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         returned;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" A 002 PROG") != std::string::npos,
            L"a listing placed with --basic lands under the Applesoft type without "
            L"anybody naming one, and the whole rendered row says so");

        readBack = GetFromCommittedImage (io, kBlankImage, "PROG",
                                         CommandLineOptions::DiskOptions::Encoding::Basic);

        Assert::AreEqual (DiskCommandRunner::kClean, readBack.exitStatus);
        Assert::IsTrue (readBack.hasPayload, L"and must hand back the listing");

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (std::string ("10  REM  A SPACED REM\n"
                                       "20  PRINT \"PRINT AT TO\"\n"
                                       "30  DATA \"A:B\",C: PRINT 1\n"
                                       "40  FOR I = 1 TO 9: NEXT\n"),
                          returned,
            L"the listing must come back with the same statements and the same payload "
            L"spacing -- the leading spaces are the ones LIST puts in front of a token");
    }

    TEST_METHOD (Put_ABasicListing_StoresTheTOKENIZEDForm_NotTheTextItWasGiven)
    {
        // The companion assertion to the round trip above, and the one it
        // cannot make: a tokenizer that did nothing at all would round-trip
        // perfectly through its own inverse.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   raw;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);

        raw = GetFromCommittedImage (io, kBlankImage, "PROG",
                                    CommandLineOptions::DiskOptions::Encoding::Verbatim);

        Assert::IsTrue (raw.payload.size() > 4, L"the stored file has a body");

        Assert::AreEqual ((int) 0xB2, (int) raw.payload[4],
            L"the first body byte of line 10 is the REM TOKEN, not the letter R");

        Assert::AreEqual ((int) 0x0801 + 20, (int) (raw.payload[0] | (raw.payload[1] << 8)),
            L"and the link is the absolute address of the next line, which is what a "
            L"guest follows and what a length-only tokenizer would get wrong");

        Assert::IsTrue (raw.payload.size() < BasicListingBytes().size(),
            L"tokenized is shorter than the text it came from, or nothing was tokenized");
    }

    TEST_METHOD (Put_AnUntokenizableListing_IsRefusedNamingTheLineAndQuotingIt)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();
        std::string         listing = "10 PRINT 1\n20 PRINT 2\n20 PRINT 3\n";

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   vector<Byte> (listing.begin(), listing.end()));

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);

        Assert::IsTrue (result.diagnostics.find ("line 20 ") != std::string::npos,
            L"the refusal names the offending line number");

        Assert::IsTrue (result.diagnostics.find ("20 PRINT 3") != std::string::npos,
            L"and quotes the line itself, because a number alone points at nothing in a "
            L"file numbered by tens");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount);
    }

    TEST_METHOD (Put_ABasicListingOntoProDos_RecordsWhereAnApplesoftProgramLoads)
    {
        // The two filesystems record this in entirely different places: DOS 3.3
        // keeps a length inside the file and nothing else, while ProDOS puts the
        // load address in the directory entry's auxiliary type. A placement that
        // left the auxiliary type at zero would read back through our own reader
        // perfectly and tell the machine the program loads at $0000.
        static constexpr const char *  kProDosImage = "C:\\disks\\pro.dsk";

        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kProDosImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         listing;
        std::string         returned;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-b.dsk", kProDosImage);
        SeedFile (io, kHostFile, BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);

        {
            DiskCommandRunner   reader (io);
            CommandLineOptions  listOptions = MakeOptions (CommandLineOptions::DiskOptions::Verb::List,
                                                           kProDosImage);

            listing = reader.Run (listOptions).output;
        }

        Assert::IsTrue (listing.find (" PROG                 $FC     1  eof=71 aux=$0801") != std::string::npos,
            L"the whole rendered row: the BASIC type nobody named, the size, the exact "
            L"length of the tokenized program, and the address it loads at");

        readBack = GetFromCommittedImage (io, kProDosImage, "PROG",
                                          CommandLineOptions::DiskOptions::Encoding::Basic);

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (std::string ("10  REM  A SPACED REM\n"
                                       "20  PRINT \"PRINT AT TO\"\n"
                                       "30  DATA \"A:B\",C: PRINT 1\n"
                                       "40  FOR I = 1 TO 9: NEXT\n"),
                          returned,
            L"and the listing comes back off ProDOS exactly as it comes back off DOS 3.3");
    }

    TEST_METHOD (Put_BasicWithALoadAddress_IsRefusedRatherThanIgnoringTheFlag)
    {
        // Applesoft keeps its program at $0801 and nowhere else, so an address
        // here cannot be honored. Accepting it and placing the file anyway
        // leaves the caller believing something about the result that is false.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   BasicListingBytes());

        options.disk.encoding       = CommandLineOptions::DiskOptions::Encoding::Basic;
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("$0801") != std::string::npos,
            L"and says where an Applesoft program does load");

        AssertImageMatches (io, kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount);
    }

    TEST_METHOD (Put_ABinaryWithNoLoadAddress_SaysWhichFlagIsMissing)
    {
        // $0000 is a legal load address, so defaulting would be
        // indistinguishable from an answer. The refusal has to name the flag,
        // or the caller is left guessing which of several things was wrong.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName = "B";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("--addr") != std::string::npos,
            L"the message must name the flag that would fix it");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_ToAWriteProtectedImage_SaysWriteProtectedRatherThanReportingACode)
    {
        // FR-014's harder half. Nothing about the image's CONTENTS says it may
        // not be written, so the volume layer computes a perfectly good result
        // and the platform denies access at the last step. Reported as a
        // generic failure, the user is left with two candidate causes and no
        // way to tell which.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        io.failNextReplace  = true;
        io.nextReplaceError = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("write-protected") != std::string::npos,
            L"the reason given must be write protection, not a generic replace failure");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_WithText_WritesTheDiskConventionAndReadsBackAsHostText)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kBlankImage, kHostFile, "NOTES");
        DiskCommandResult   stored;
        DiskCommandResult   asText;
        std::string         source   = "HELLO\nWORLD\n";
        vector<Byte>        hostText (source.begin(), source.end());
        size_t              i        = 0;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   hostText);

        options.disk.typeName = "T";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        stored = GetFromCommittedImage (io, kBlankImage, "NOTES");

        Assert::AreEqual (DiskCommandRunner::kClean, stored.exitStatus);
        Assert::IsTrue (stored.payload.size() > 0, L"something must actually have been stored");

        for (i = 0; i < stored.payload.size(); i++)
        {
            Assert::IsTrue (stored.payload[i] >= 0x80,
                L"the stored file is in the disk's own high-bit convention");
            Assert::IsTrue (stored.payload[i] != 0x0A,
                L"and carries none of the host's line endings");
        }

        asText = GetFromCommittedImage (io, kBlankImage, "NOTES",
                                        CommandLineOptions::DiskOptions::Encoding::Text);

        Assert::IsTrue (asText.payload == hostText,
            L"host text placed and read back as text is the identity");
    }

    TEST_METHOD (Put_WithTextAndNoNamedType_TakesTheFilesystemsOwnTextType)
    {
        // A caller who named no type gets the one that matches the conversion
        // they asked for. Defaulting to a binary instead would refuse this
        // invocation outright, for want of the load address a binary needs --
        // which is a confusing way to be told the type was wrong.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "NOTES");
        std::string         source  = "HELLO\n";
        vector<Byte>        hostText (source.begin(), source.end());

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   hostText);

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus,
            L"text with no named type needs no load address");

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" T 002 NOTES") != std::string::npos,
            L"and lands as the filesystem's text type");
    }

    //
    //  THE OTHER HALF OF THE DEFAULTING RULE, AND THE HALF THAT LOST A ROUTE IN.
    //
    //  `--text --verbatim` used to reach this branch -- the second flag
    //  cancelled the conversion, so the type fell back to the default and the
    //  file landed as DOS `B` where `--text` alone made it `T`. `--verbatim` is
    //  gone, so naming neither conversion is the only way here now, and this is
    //  what keeps the branch it leads to honest.
    //
    TEST_METHOD (Put_WithNoConversionAndNoNamedType_TakesTheFilesystemsOwnBinaryType)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::IsTrue (options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim,
            L"nothing was named, which is the case under test");

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B 004 PROG") != std::string::npos,
            L"naming neither a type nor a conversion lands the filesystem's binary type");
    }

    TEST_METHOD (Put_WithTextThatHasNoAppleRepresentation_NamesTheOffendingByte)
    {
        // A smart quote pasted into a listing looks identical to a plain one in
        // an editor. "Somewhere in this file" is not something anybody can act
        // on, so the offset is the value of the refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "NOTES");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();
        vector<Byte>        source  = { 'H', 'I', ' ', 'T', 'H', 0xE2, 'R', 'E' };

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   source);

        options.disk.typeName = "T";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("byte 5") != std::string::npos,
            L"the message points at the byte, not merely at the file");

        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_WithATypeThisFilesystemDoesNotHave_RefusesAndListsWhatItTakes)
    {
        // Defaulting instead would place the file under a type nobody asked for
        // and say nothing; the guest reports the mismatch much later, as a file
        // that will not load.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName = "SYS";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("DOS 3.3 takes") != std::string::npos,
            L"and the refusal lists what this filesystem does take");

        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_WithNoAsName_TakesTheHostFilesOwnLastComponent)
    {
        // SEARCHING FOR THE NAME AS A SUBSTRING IS NOT ENOUGH HERE, and this
        // test was written that way first. A DOS 3.3 catalog name may hold
        // colons and backslashes, so the whole host path is a legal name -- and
        // it CONTAINS the leaf, so a substring assertion is satisfied by an
        // implementation that never stripped the directories at all. Measured
        // by mutation: the weak form passed against exactly that.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, "C:\\build\\sub\\PROG.BIN", nullptr);
        DiskCommandResult   result;
        std::string         listing;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, "C:\\build\\sub\\PROG.BIN", MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result  = runner.Run (options);
        listing = ListCommittedImage (io, kBlankImage);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);

        Assert::IsTrue (listing.find (" B 004 PROG.BIN\n") != std::string::npos,
            L"the on-disk name is the host file's own last component and nothing else");

        Assert::IsTrue (listing.find ("BUILD") == std::string::npos,
            L"and carries none of the host's directories with it");
    }

    TEST_METHOD (Put_OntoAProDosVolume_RecordsTheLoadAddressAsTheAuxiliaryType)
    {
        // Where a binary's load address lives is the one thing the two
        // filesystems disagree about: DOS writes it into the file's own first
        // four bytes and ProDOS records it in the directory entry instead.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kProImage, kHostFile, "PROG");
        DiskCommandRunner   reader (io);
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        CommandLineOptions  listing  = MakeOptions (CommandLineOptions::DiskOptions::Verb::List, kProImage);
        vector<Byte>        payload  = MakePayload();

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProImage);
        SeedFile (io, kHostFile, payload);

        options.disk.typeName       = "BIN";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);

        Assert::IsTrue (reader.Run (listing).output.find ("aux=$6000") != std::string::npos,
            L"ProDOS records the load address in the entry, not in the file");

        readBack = GetFromCommittedImage (io, kProImage, "PROG");

        Assert::IsTrue (readBack.payload == payload,
            L"and the file itself carries no header of its own");
    }

    TEST_METHOD (Put_ToAProDosOrderedContainer_IsRenderedBackThroughTheSameReorder)
    {
        // A .po holds its sectors in ProDOS order, so an edited buffer has to
        // be turned back into that order on the way out. Handing the volume
        // layer's own buffer straight to the commit instead is INVISIBLE on a
        // .dsk, where the two are the same bytes -- every other test here would
        // stay green while the tool wrote a file no Apple II could read.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options    = MakePutOptions (kProOrdered, kHostFile, "PROG");
        DiskCommandResult   readBack;
        FixtureProvider     fixtures;
        vector<Byte>        dosOrdered;
        vector<Byte>        proOrdered;
        vector<Byte>        payload    = MakePayload();

        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-a.dsk", dosOrdered));

        VolumeImage::DosLogicalToProDosFile (dosOrdered, proOrdered);

        Assert::IsFalse (proOrdered == dosOrdered,
            L"the two orders must actually differ, or this test proves nothing");

        SeedFile (io, kProOrdered, proOrdered);
        SeedFile (io, kHostFile,   payload);

        options.disk.typeName       = "BIN";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        readBack = GetFromCommittedImage (io, kProOrdered, "PROG");

        Assert::AreEqual (DiskCommandRunner::kClean, readBack.exitStatus,
            L"the committed file must still be a ProDOS volume in ProDOS order");

        Assert::IsTrue (readBack.payload == payload);
    }

    //  Sets the lock bit on the first catalog entry, which is where a file
    //  placed onto a freshly formatted volume lands.
    static void LockFirstCatalogEntry (vector<Byte> & buffer)
    {
        size_t  at = Dos33Skeleton::SectorOffset (kVtocTrack, kCatalogFirstSector)
                   + kEntryBase + kEntOffType;

        buffer[at] = (Byte) (buffer[at] | kLockedBit);
    }

    //  Points the first entry's track/sector list at a track that is not on the
    //  volume, so a walk of its chain cannot reach the end.
    static void BreakFirstEntrysChain (vector<Byte> & buffer)
    {
        size_t  entryAt = Dos33Skeleton::SectorOffset (kVtocTrack, kCatalogFirstSector) + kEntryBase;
        int     track   = buffer[entryAt + kEntOffTsTrack];
        int     sector  = buffer[entryAt + kEntOffTsSector];
        size_t  listAt  = Dos33Skeleton::SectorOffset (track, sector);

        buffer[listAt + kTsOffNextTrack]  = kTrackOffTheVolume;
        buffer[listAt + kTsOffNextSector] = 0;
    }

    TEST_METHOD (Delete_RemovesTheFileFromTheCommittedImage)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        DiskCommandResult   readBack;

        SeedRealDisk (io);

        // The trailing newline matters: this disk also carries MAKE DUMP.S, and
        // a substring search would go on finding the removed file inside its
        // neighbor's name and report a delete that never happened as one that
        // did -- or, here, the reverse.
        Assert::IsTrue (ListCommittedImage (io, kImage).find ("MAKE DUMP\n") != std::string::npos,
            L"the file must be there before the delete for this to mean anything");

        result = runner.Run (MakeDeleteOptions (kImage, "MAKE DUMP"));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (io.HasNoTemporaryFiles());

        Assert::IsTrue (ListCommittedImage (io, kImage).find ("MAKE DUMP\n") == std::string::npos,
            L"and it is gone from the image on disk, not merely from a buffer");

        Assert::IsTrue (ListCommittedImage (io, kImage).find ("MAKE DUMP.S\n") != std::string::npos,
            L"while the file whose name merely contains it is untouched");

        readBack = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandRunner::kNoOutput, readBack.exitStatus,
            L"and no longer resolves by name");
    }

    TEST_METHOD (Delete_AFileTheVolumeDoesNotHave_RefusesNamingImageAndFile)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = OriginalImageBytes();

        SeedRealDisk (io);

        result = runner.Run (MakeDeleteOptions (kImage, "NOSUCHFILE"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("is not on this volume") != std::string::npos,
            L"and says which of the refusals this is");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kImage, original);
    }

    TEST_METHOD (Delete_ALockedFile_IsRefusedInTermsThatSayItIsLocked)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        LockFirstCatalogEntry (io.files[kBlankImage]);
        committed = io.files[kBlankImage];

        result = runner.Run (MakeDeleteOptions (kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is locked on this volume") != std::string::npos);

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, committed);
    }

    TEST_METHOD (Delete_WhenTheChainCannotBeFollowed_SucceedsAndSaysWhatItCouldNotAccountFor)
    {
        // The reason the removal's ACCOUNT had to become part of the volume
        // interface. Delete stays available for a file whose chain is damaged,
        // so a bad entry cannot strand the volume -- but a caller that reported
        // only success would leave the user believing every sector came back.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        BreakFirstEntrysChain (io.files[kBlankImage]);

        result = runner.Run (MakeDeleteOptions (kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kWithComplaints, result.exitStatus,
            L"a removal that could not account for everything is not a clean one");

        Assert::IsTrue (result.diagnostics.find ("could not be followed") != std::string::npos,
            L"and says what it could not do, rather than only that something was wrong");

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find ("PROG") == std::string::npos,
            L"while still removing the entry, so a bad file cannot strand the volume");
    }

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

    //  Where a booted DOS reads the name of the program it runs. Restated from
    //  the published layout rather than taken from the implementation, and
    //  corroborated by this fixture: the Merlin DOS 3.3 disk carries the
    //  high-ASCII form of HELLO here, exactly as the stock master does.
    static constexpr int     kGreetingTrack  = 1;
    static constexpr int     kGreetingSector = 9;
    static constexpr size_t  kGreetingOffset = 0x75;
    static constexpr size_t  kNameFieldBytes = 30;

    //  The Applesoft program placed on the DOS 3.3 disk to be booted into, a
    //  binary that disk already carries, and a file on the Merlin ProDOS disk
    //  that its boot path could never launch.
    static constexpr const char *  kDosProgram      = "BOOTME";
    static constexpr const char *  kBinaryOnTheDisk = "MERLIN.X";
    static constexpr const char *  kProProgram      = "PARMS";

    static std::string GreetingNameIn (const vector<Byte> & image)
    {
        size_t       at   = Dos33Skeleton::SectorOffset (kGreetingTrack, kGreetingSector)
                          + kGreetingOffset;
        std::string  name;
        size_t       i    = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            name += (char) (image[at + i] & 0x7F);
        }

        while (!name.empty() && name.back() == ' ')
        {
            name.pop_back();
        }

        return name;
    }

    CommandLineOptions MakeBootOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Boot, image);

        options.disk.path = path;

        return options;
    }

    TEST_METHOD (Boot_SetsTheNameTheGuestReadsAtBootTime_InTheCommittedImage)
    {
        // An Applesoft program, because that is what a booting DOS 3.3 RUNs.
        // The binary case is the one below, and it is a complaint rather than a
        // refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  put = MakePutOptions (kImage, kHostFile, kDosProgram);
        DiskCommandResult   result;

        SeedRealDisk (io);
        SeedFile (io, kHostFile, MakePayload (16));

        put.disk.typeName = "A";

        Assert::AreEqual (std::string ("HELLO"), GreetingNameIn (io.files[kImage]),
            L"this disk boots its own greeting today, or the command below changes nothing");

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (put).exitStatus,
            L"the program has to be on the disk before it can be booted into");

        result = runner.Run (MakeBootOptions (kImage, kDosProgram));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus,
            L"naming a program the volume holds and DOS can run is a clean operation");

        Assert::AreEqual (std::string(), result.diagnostics, L"with nothing to complain about");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and nothing left beside the image");

        Assert::AreEqual (std::string (kDosProgram), GreetingNameIn (io.files[kImage]),
            L"and the name the guest reads at boot is the one that was asked for -- read "
            L"back off the COMMITTED image, not out of a buffer the runner still held");

        Assert::IsTrue (ListCommittedImage (io, kImage).find (" A 004 HELLO") != std::string::npos,
            L"while the greeting it used to run is still on the disk, untouched: this "
            L"changes what runs, not what is on the volume");
    }

    TEST_METHOD (Boot_ABinaryOnADos33Volume_SucceedsAndSaysDosWillNotRunIt)
    {
        // Measured on the stock master with a real 6502: the disk boots and the
        // binary is never executed, because DOS 3.3's boot command is RUN. The
        // name IS set -- refusing would be wrong, since a disk whose boot
        // command has been patched by hand is a real thing -- so the honest
        // answer is the complaints status and a sentence saying which.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        SeedRealDisk (io);

        result = runner.Run (MakeBootOptions (kImage, kBinaryOnTheDisk));

        Assert::AreEqual (DiskCommandRunner::kWithComplaints, result.exitStatus,
            L"a startup program DOS will not run is not a clean outcome");

        Assert::IsTrue (result.diagnostics.find ("RUNs its greeting") != std::string::npos,
            L"and the complaint says what DOS does at boot");

        Assert::IsTrue (result.diagnostics.find (kBinaryOnTheDisk) != std::string::npos,
            L"naming the file it is about");

        Assert::AreEqual (std::string (kBinaryOnTheDisk), GreetingNameIn (io.files[kImage]),
            L"while still setting the name, which is what was asked for");

        AssertNamesNoPlatformCode (result.diagnostics);
    }

    TEST_METHOD (Boot_ANameTheVolumeDoesNotHold_IsRefusedNamingTheMissingFile)
    {
        // The refusal FR-025 asks for, and the reason it is worth having: a
        // startup program that is not there produces no symptom until somebody
        // boots the disk, and then the symptom is on a machine somewhere else.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = OriginalImageBytes();

        SeedRealDisk (io);

        result = runner.Run (MakeBootOptions (kImage, "NOSUCHFILE"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"the message must name the file that is missing, not merely report a failure");
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("is not on this volume") != std::string::npos);

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kImage, original);

        Assert::AreEqual (std::string ("HELLO"), GreetingNameIn (io.files[kImage]),
            L"and the disk still boots what it booted before");
    }

    TEST_METHOD (Boot_WithNoProgramNamed_SaysWhatItWanted)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = OriginalImageBytes();

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::Boot));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no program named") != std::string::npos);

        AssertImageMatches (io, kImage, original);
    }

    TEST_METHOD (Boot_AVolumeWithNoOperatingSystemOnIt_IsRefusedInWordsRatherThanPatchedAnyway)
    {
        // A formatted data disk reserves the tracks DOS would occupy but has
        // nothing in them. Patching a name there would report success for a
        // disk that cannot boot at all.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus);

        committed = io.files[kBlankImage];

        result = runner.Run (MakeBootOptions (kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no operating system") != std::string::npos,
            L"and says that is what is missing, rather than blaming the file");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, committed);
    }

    TEST_METHOD (Boot_ProDos_AFileTheBootPathCannotLaunch_IsRefusedInWords)
    {
        // On ProDOS the mechanism is directory order, so any file could be
        // moved to the front -- and moving one the kernel will never launch
        // produces a disk that looks configured and boots something else.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProImage);

        original = io.files[kProImage];

        result = runner.Run (MakeBootOptions (kProImage, kProProgram));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kProProgram) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("type SYS") != std::string::npos,
            L"and says what this boot path does launch");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kProImage, original);
    }

    //  Which of two names the committed listing reaches first. Both must be
    //  present: comparing two positions where one of them is "not found" is a
    //  comparison against the largest number there is, which passes.
    void AssertListedBefore (const std::string & listing,
                             const std::string & first,
                             const std::string & second)
    {
        size_t  at     = listing.find (first);
        size_t  behind = listing.find (second);

        Assert::IsTrue (at     != std::string::npos, L"the first name must be in the listing");
        Assert::IsTrue (behind != std::string::npos, L"and so must the second");
        Assert::IsTrue (at < behind, L"in that order");
    }

    TEST_METHOD (Boot_ProDos_ASystemProgramPlacedByPut_OvertakesTheOneTheDiskLaunchesNow)
    {
        // /MERLIN launches MERLIN.SYSTEM, the first system program its volume
        // directory reaches. Nominating a newly placed one has to overtake it,
        // and the proof is the ORDER in the committed image rather than any
        // field, because ProDOS stores no startup name anywhere to inspect.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kProImage, kHostFile, "CASSO.SYSTEM");
        DiskCommandResult   result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProImage);
        SeedFile (io, kHostFile, MakePayload (60));

        options.disk.typeName = "SYS";

        Assert::AreEqual (DiskCommandRunner::kClean, runner.Run (options).exitStatus,
            L"the placement must succeed before the reorder can mean anything");

        AssertListedBefore (ListCommittedImage (io, kProImage), "MERLIN.SYSTEM", "CASSO.SYSTEM");

        result = runner.Run (MakeBootOptions (kProImage, "CASSO.SYSTEM"));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);
        Assert::IsTrue (io.HasNoTemporaryFiles());

        AssertListedBefore (ListCommittedImage (io, kProImage), "CASSO.SYSTEM", "MERLIN.SYSTEM");

        Assert::IsTrue (ListCommittedImage (io, kProImage).find ("PARMS") != std::string::npos,
            L"and every other entry is still on the disk");
    }


    //  A 143,360-byte sector image carrying boot code and no filesystem at all,
    //  which is what a great deal of Apple II software actually is.
    static vector<Byte> MakeBootableImageWithNoFilesystem()
    {
        vector<Byte>  image ((size_t) NibblizationLayer::kImageByteSize, 0);
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

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List, path));

        Assert::IsTrue (result.diagnostics.find (DiskCommandRunner::kNoFilesystemText)
                            != std::string::npos,
            L"the sentence a person would say, not a sentence about this tool's tables");

        Assert::IsTrue (result.diagnostics.find ("filesystem this tool recognizes")
                            == std::string::npos,
            L"and not the old one");

        Assert::IsTrue (result.diagnostics.find ("35 tracks x 16 sectors x 256 bytes")
                            != std::string::npos,
            L"the geometry is still knowable and is still worth stating");

        Assert::IsTrue (result.diagnostics.find ("track 0 sector 0 carries code")
                            != std::string::npos,
            L"and so is the fact that it boots");

        //  THE STATUS DOES NOT MOVE. A caller still got no catalog, so a script
        //  that branches on 2 branches the same way it always did.
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus,
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
        vector<Byte>        image ((size_t) NibblizationLayer::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List, path));

        Assert::IsTrue (result.diagnostics.find ("track 0 sector 0 is blank")
                            != std::string::npos,
            L"an empty first sector is reported as empty, not as bootable");
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
        vector<Byte>        image ((size_t) NibblizationLayer::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List,
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

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List,
                                          "C:\\disks\\absent.dsk"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("cannot be read") != std::string::npos);
    }
};
