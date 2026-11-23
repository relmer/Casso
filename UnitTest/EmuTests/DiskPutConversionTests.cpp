#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Core/TextEncoding.h"
#include "IntegerBasicTokenizer.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPutConversionTests
//
//  disk put and get with a conversion: BASIC listings, Apple text, and the
//  Casso Explorer and CiderPress forms of a host file's name.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskPutConversionTests)
{
public:

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
        CommandLineOptions  options  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         returned;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" A 002 PROG") != std::string::npos,
            L"a listing placed with --basic lands under the Applesoft type without "
            L"anybody specifying one, and the whole rendered row shows it");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PROG",
                                         CommandLineOptions::DiskOptions::Encoding::Basic);

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);
        Assert::IsTrue (readBack.hasPayload, L"and must hand back the listing");

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (std::string ("10  REM  A SPACED REM\r\n"
                                       "20  PRINT \"PRINT AT TO\"\r\n"
                                       "30  DATA \"A:B\",C: PRINT 1\r\n"
                                       "40  FOR I = 1 TO 9: NEXT\r\n"),
                          returned,
            L"the listing must come back with the same statements and the same payload "
            L"spacing -- the leading spaces are the ones LIST puts in front of a token");
    }

    TEST_METHOD (Get_AProgramWithLineFeedsInARem_ListsThemAsLineBreaks_AndPutsBackTheSameBytes)
    {
        // TANK COMBAT's line 140: a REM whose text is two line feeds, BEGIN
        // GAME and two more, so LIST on the Apple prints blank lines around it.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  put      = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "TANK");
        CommandLineOptions  putBack  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "TANK2");
        DiskCommandResult   listed;
        DiskCommandResult   raw;
        std::string         returned;
        vector<Byte>        program  =
        {
            0x15, 0x08, 0x8C, 0x00, 0xB2, 0x0A, 0x0A,
            'B',  'E',  'G',  'I',  'N',  ' ',  'G',  'A',  'M',  'E',
            0x0A, 0x0A, 0x00,
            0x1B, 0x08, 0x96, 0x00, 0x97, 0x00,
            0x00, 0x00,
        };

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   program);

        put.disk.typeName = "A";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (put).exitStatus);

        listed = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "TANK", CommandLineOptions::DiskOptions::Encoding::Basic);

        Assert::AreEqual (DiskCommandResult::kClean, listed.exitStatus, TextEncoding::NarrowToWide (listed.diagnostics).c_str());

        returned.assign (listed.payload.begin(), listed.payload.end());

        Assert::AreEqual (std::string ("140  REM\r\n\r\nBEGIN GAME\r\n\r\n\r\n150  HOME\r\n"), returned);

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, listed.payload);

        putBack.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (putBack).exitStatus);

        raw = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "TANK2");

        Assert::IsTrue (raw.payload == program, L"the listing put back with --basic is the same program");
    }

    TEST_METHOD (Put_AnIntegerBasicListing_LandsTokenizedUnderTypeI)
    {
        FakeDiskFileIo         io;
        DiskCommandRunner      runner (io);
        CommandLineOptions     options  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult      result;
        DiskCommandResult      readBack;
        DiskCommandResult      raw;
        std::string            listing  = "20 GOTO 10\n10 PRINT \"HI\"\n";
        std::string            returned;
        std::vector<Byte>      tokens;
        ApplesoftListingError  error;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   vector<Byte> (listing.begin(), listing.end()));

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;
        options.disk.typeName = "I";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);
        Assert::IsTrue   (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" I 002 PROG") != std::string::npos);

        AssertSucceeded (IntegerBasicTokenizer::Tokenize (listing, tokens, error));

        raw = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PROG", CommandLineOptions::DiskOptions::Encoding::Verbatim);

        Assert::IsTrue (raw.payload == tokens, L"the stored bytes are the tokenized program");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PROG", CommandLineOptions::DiskOptions::Encoding::Basic);

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);
        Assert::IsTrue   (returned.find ("PRINT \"HI\"") < returned.find ("GOTO 10"), L"the lines come back in number order");
    }

    TEST_METHOD (Put_AnIntegerBasicListing_ThatIntegerBasicRejects_FailsWithTheLine)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        std::string         listing  = "10 PRINT \"HI\"\n20 SCORE=1\n";

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   vector<Byte> (listing.begin(), listing.end()));

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;
        options.disk.typeName = "INT";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue   (result.diagnostics.find ("Integer BASIC") != std::string::npos);
        Assert::IsTrue   (result.diagnostics.find ("20") != std::string::npos);
    }

    TEST_METHOD (Put_ABasicListing_StoresTheTOKENIZEDForm_NotTheTextItWasGiven)
    {
        // The companion assertion to the round trip above, and the one it
        // cannot make: a tokenizer that did nothing at all would round-trip
        // perfectly through its own inverse.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   raw;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        raw = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PROG",
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();
        std::string         listing = "10 PRINT 1\n20 PRINT 2\n20 PRINT 3\n";

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   vector<Byte> (listing.begin(), listing.end()));

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);

        Assert::IsTrue (result.diagnostics.find ("line 20 ") != std::string::npos,
            L"the refusal reports the offending line number");

        Assert::IsTrue (result.diagnostics.find ("20 PRINT 3") != std::string::npos,
            L"and quotes the line itself, because a number alone points at nothing in a "
            L"file numbered by tens");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (kProDosImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         listing;
        std::string         returned;

        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-b.dsk", kProDosImage);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);

        {
            DiskCommandRunner   reader (io);
            CommandLineOptions  listOptions = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                                           kProDosImage);

            listing = reader.Run (listOptions).output;
        }

        Assert::IsTrue (listing.find (" PROG                 $FC     1  eof=71 aux=$0801") != std::string::npos,
            L"the whole rendered row: the BASIC type nobody specified, the size, the exact "
            L"length of the tokenized program, and the address it loads at");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, kProDosImage, "PROG",
                                          CommandLineOptions::DiskOptions::Encoding::Basic);

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (std::string ("10  REM  A SPACED REM\r\n"
                                       "20  PRINT \"PRINT AT TO\"\r\n"
                                       "30  DATA \"A:B\",C: PRINT 1\r\n"
                                       "40  FOR I = 1 TO 9: NEXT\r\n"),
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   BasicListingBytes());

        options.disk.encoding       = CommandLineOptions::DiskOptions::Encoding::Basic;
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("$0801") != std::string::npos,
            L"and reports where an Applesoft program does load");

        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount);
    }

    TEST_METHOD (Put_WithText_WritesTheDiskConventionAndReadsBackAsHostText)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "NOTES");
        DiskCommandResult   stored;
        DiskCommandResult   asText;
        std::string         source   = "HELLO\r\nWORLD\r\n";
        vector<Byte>        hostText (source.begin(), source.end());
        size_t              i        = 0;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   hostText);

        options.disk.typeName = "T";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        stored = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "NOTES");

        Assert::AreEqual (DiskCommandResult::kClean, stored.exitStatus);
        Assert::IsTrue (stored.payload.size() > 0, L"something must actually have been stored");

        for (i = 0; i < stored.payload.size(); i++)
        {
            Assert::IsTrue (stored.payload[i] >= 0x80,
                L"the stored file is in the disk's own high-bit convention");
            Assert::IsTrue (stored.payload[i] != 0x0A,
                L"and carries none of the host's line endings");
        }

        asText = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "NOTES",
                                        CommandLineOptions::DiskOptions::Encoding::Text);

        Assert::IsTrue (asText.payload == hostText,
            L"Windows text placed and read back as text is the identity");
    }

    TEST_METHOD (Put_WithTextAndNoNamedType_TakesTheFilesystemsOwnTextType)
    {
        // A caller who named no type gets the one that matches the conversion
        // they asked for. Defaulting to a binary instead would refuse this
        // invocation outright, for want of the load address a binary needs --
        // which is a confusing way to be told the type was wrong.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "NOTES");
        std::string         source  = "HELLO\n";
        vector<Byte>        hostText (source.begin(), source.end());

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   hostText);

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"text with no explicit type needs no load address");

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" T 002 NOTES") != std::string::npos,
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::IsTrue (options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim,
            L"nothing was specified, which is the case under test");

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" B 004 PROG") != std::string::npos,
            L"specifying neither a type nor a conversion lands the filesystem's binary type");
    }

    TEST_METHOD (Put_WithTextThatHasNoAppleRepresentation_NamesTheOffendingByte)
    {
        // A smart quote pasted into a listing looks identical to a plain one in
        // an editor. "Somewhere in this file" is not something anybody can act
        // on, so the offset is the value of the refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "NOTES");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();
        vector<Byte>        source  = { 'H', 'I', ' ', 'T', 'H', 0xE2, 'R', 'E' };

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   source);

        options.disk.typeName = "T";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("byte 5") != std::string::npos,
            L"the message points at the byte, not merely at the file");

        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\build\\sub\\PROG.BIN", nullptr);
        DiskCommandResult   result;
        std::string         listing;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\build\\sub\\PROG.BIN", DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        result  = runner.Run (options);
        listing = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        Assert::IsTrue (listing.find (" B 004 PROG.BIN\n") != std::string::npos,
            L"the on-disk name is the host file's own last component and nothing else");

        Assert::IsTrue (listing.find ("BUILD") == std::string::npos,
            L"and carries none of the host's directories with it");
    }

    //  Casso Explorer copies a file out under a name with its type and address,
    //  and put with no --as reads that name back, so a file goes out of one
    //  program and back in through the other as the same entry.
    TEST_METHOD (Put_ACassoExplorerListingName_GoesInAsTheProgramUnderItsBareName)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\HELLO.Applesoft BASIC.txt", nullptr);
        DiskCommandResult   result;
        std::string         listing;
        std::string         text    = "10 PRINT \"HI\"\n";



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\HELLO.Applesoft BASIC.txt", vector<Byte> (text.begin(), text.end()));

        result  = runner.Run (options);
        listing = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (listing.find (" A 002 HELLO\n") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
    }


    TEST_METHOD (Put_ACassoExplorerBinaryName_TakesItsTypeAndLoadAddress)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\PROG.Binary.$6000.bin", nullptr);
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         listing;



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\PROG.Binary.$6000.bin", DiskRunnerFixtures::MakePayload());

        result   = runner.Run (options);
        listing  = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage);
        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PROG");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (listing.find (" B 004 PROG\n") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
        Assert::IsTrue (readBack.diagnostics.find ("loads at $6000") != std::string::npos, TextEncoding::NarrowToWide (readBack.diagnostics).c_str());
    }


    TEST_METHOD (Put_ACiderPressName_TakesItsTypeAndLoadAddress)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\PIC#062000", nullptr);
        DiskCommandResult   result;
        DiskCommandResult   readBack;



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\PIC#062000", DiskRunnerFixtures::MakePayload());

        result   = runner.Run (options);
        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PIC");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" B 004 PIC\n") != std::string::npos);
        Assert::IsTrue (readBack.diagnostics.find ("loads at $2000") != std::string::npos, TextEncoding::NarrowToWide (readBack.diagnostics).c_str());
    }


    //  A conversion with an import goes back in as the file its program
    //  saves: Merlin source on DOS 3.3 is a binary file whose name starts T.
    TEST_METHOD (Put_ACassoExplorerMerlinName_GoesInAsMerlinSource)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\DEMO.S.Merlin.txt", nullptr);
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         listing;
        std::string         text    = "START LDA #$00 ;zero\n RTS\n";



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\DEMO.S.Merlin.txt", vector<Byte> (text.begin(), text.end()));

        result   = runner.Run (options);
        listing  = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage);
        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "T.DEMO");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, TextEncoding::NarrowToWide (result.diagnostics).c_str());
        Assert::IsTrue (listing.find (" T.DEMO\n") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
        Assert::IsTrue (listing.find (" B ") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
        Assert::IsTrue (!readBack.payload.empty() && readBack.payload.back() == 0x8D, L"Merlin's lines end in $8D");
    }


    //  A get to a file with a conversion's suffix writes that conversion,
    //  and a file in another format is an error with no file written.
    TEST_METHOD (Get_ToAConversionName_WritesTheConversion)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  put     = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\DEMO.S.Merlin.txt", nullptr);
        CommandLineOptions  get     = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get, DiskRunnerFixtures::kBlankImage);
        DiskCommandResult   written;
        DiskCommandResult   failed;
        std::string         text    = "START LDA #$00 ;zero\n RTS\n";
        std::string         back;



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\DEMO.S.Merlin.txt", vector<Byte> (text.begin(), text.end()));

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (put).exitStatus);

        get.disk.path     = "T.DEMO";
        get.disk.hostFile = "C:\\back\\DEMO.S.Merlin.txt";
        written           = runner.Run (get);

        Assert::AreEqual (DiskCommandResult::kClean, written.exitStatus, TextEncoding::NarrowToWide (written.diagnostics).c_str());
        Assert::AreEqual (size_t (1), io.files.count ("C:\\back\\DEMO.S.Merlin.txt"));

        back.assign (io.files["C:\\back\\DEMO.S.Merlin.txt"].begin(), io.files["C:\\back\\DEMO.S.Merlin.txt"].end());

        Assert::IsTrue (back.find ("LDA") != std::string::npos && back.find ("RTS") != std::string::npos, TextEncoding::NarrowToWide (back).c_str());
        Assert::IsTrue (back.find ('\x8D') == std::string::npos, L"the conversion has no Apple text bytes");
        Assert::IsTrue (written.diagnostics.find ("loads at") == std::string::npos, TextEncoding::NarrowToWide (written.diagnostics).c_str());

        get.disk.hostFile = "C:\\back\\DEMO.LZ4FH.png";
        failed            = runner.Run (get);

        Assert::AreNotEqual (DiskCommandResult::kClean, failed.exitStatus);
        Assert::IsTrue (failed.diagnostics.find ("Unable to convert T.DEMO") != std::string::npos, TextEncoding::NarrowToWide (failed.diagnostics).c_str());
        Assert::AreEqual (size_t (0), io.files.count ("C:\\back\\DEMO.LZ4FH.png"));
    }


    //  A conversion with no import is a host file like any other, so it
    //  goes in under its whole name.
    TEST_METHOD (Put_AConversionWithNoImport_IsUsedAsWritten)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\PIC.LZ4FH.png", nullptr);
        std::string         listing;



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\PIC.LZ4FH.png", DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.hasLoadAddress = true;
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        listing = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage);

        Assert::IsTrue (listing.find (" PIC\n") == std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
        Assert::IsTrue (listing.find ("PIC.") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
    }


    //  Every other host name goes in as written, and --as takes precedence
    //  over a name in Casso Explorer's form.
    TEST_METHOD (Put_ANameInNoCassoExplorerForm_OrAnAsName_IsUsedAsWritten)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  plain   = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\MEMO.txt", nullptr);
        CommandLineOptions  renamed = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "C:\\out\\PROG.Binary.$6000.bin", "OTHER");
        std::string         listing;



        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\MEMO.txt", DiskRunnerFixtures::MakePayload());
        DiskRunnerFixtures::SeedFile (io, "C:\\out\\PROG.Binary.$6000.bin", DiskRunnerFixtures::MakePayload());

        plain.disk.typeName         = "B";
        plain.disk.hasLoadAddress   = true;
        plain.disk.loadAddress      = DiskRunnerFixtures::kLoadAddress;
        renamed.disk.typeName       = "B";
        renamed.disk.hasLoadAddress = true;
        renamed.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (plain).exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (renamed).exitStatus);

        listing = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage);

        Assert::IsTrue (listing.find ("MEMO.txt\n") != std::string::npos || listing.find ("MEMO.TXT\n") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
        Assert::IsTrue (listing.find (" OTHER\n") != std::string::npos, TextEncoding::NarrowToWide (listing).c_str());
        Assert::IsTrue (listing.find (" PROG\n") == std::string::npos);
    }
};
