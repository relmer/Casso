#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "ApplesoftTokenizer.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleBasicTests
//
//  What Applesoft itself makes of a listing this tool tokenized, settled by
//  Applesoft rather than by our own inverse.
//
//  TWO WITNESSES, AND THEY ANSWER DIFFERENT QUESTIONS.
//
//  The first types the listing into a booted machine and reads the bytes back
//  out of the memory Applesoft stored them in. That is the only oracle for
//  tokenization that is not our own code: a tokenizer checked against its own
//  detokenizer agrees with itself perfectly while storing something no guest
//  would recognize, and a round trip cannot see the difference. It is the same
//  shape as the sector-order defect this branch spent a phase learning to
//  distrust, and the remedy is the same -- consult evidence the code does not
//  own.
//
//  The second places a listing through the command line a user actually types,
//  boots the image, and reads what LIST prints. That settles the whole path
//  rather than the conversion: the type byte, the length header, the sector
//  chain and the links all have to be right before a guest will show a line.
//
//  A WRONG IMAGE MAKES THE PROCESSOR EXECUTE GARBAGE, which this build traces
//  one line per illegal opcode and which has previously written gigabytes before
//  anybody noticed. So every case asks the cheap structural questions first and
//  only then starts a machine.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (GuestVisibleBasicTests)
{
public:

    static constexpr const char *  kImagePath = "C:\\disks\\gate.dsk";
    static constexpr const char *  kHostFile  = "C:\\build\\prog.bas";
    static constexpr const char *  kPlacedName = "PROG";

    //  Where Applesoft keeps the start of the program and the end of it.
    static constexpr Word  kTxtTab = 0x0067;
    static constexpr Word  kVarTab = 0x0069;

    //  A file the master already carries, so the "still a disk" half of the
    //  pre-boot check is not satisfied by an image holding only what was placed.
    static constexpr const char *  kMasterCarries = "HELLO";


    //
    //  ------------------------------------------------------------------
    //  Material.
    //  ------------------------------------------------------------------
    //

    //  Every rule the tokenizer has, in four lines short enough to type into a
    //  guest: a bare statement, a string, a compound line with an operator and
    //  two keyword collisions in it (`TO` after a digit, `NEXT` after a colon),
    //  and a terminator.
    static std::string Listing()
    {
        return "10 HOME\n"
               "20 PRINT \"CASSO\"\n"
               "30 FOR I = 1 TO 3: PRINT I: NEXT\n"
               "40 END\n";
    }


    static std::vector<Byte> ListingBytes()
    {
        std::string  text = Listing();

        return std::vector<Byte> (text.begin(), text.end());
    }


    static std::vector<Byte> TokenizedListing()
    {
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;

        AssertSucceeded (ApplesoftTokenizer::Tokenize (Listing(), bytes, error),
            L"the listing under test must tokenize");

        Assert::IsTrue (bytes.size() > 20,
            L"and must be a program of some size, or the comparisons below compare almost "
            L"nothing");

        return bytes;
    }


    //
    //  ------------------------------------------------------------------
    //  Witness one: what Applesoft stores for the same text.
    //  ------------------------------------------------------------------
    //

    static Word ReadGuestWord (EmulatorCore & core, Word address)
    {
        std::vector<Byte>  bytes = GuestSession::GuestBytesAt (core, address, 2);

        return (Word) (bytes[0] | (bytes[1] << 8));
    }


    static std::wstring Hex (const std::vector<Byte> & bytes)
    {
        std::wstring  out;
        wchar_t       buf[8] = {};

        for (size_t i = 0; i < bytes.size(); i++)
        {
            swprintf_s (buf, L"%02X ", bytes[i]);
            out += buf;
        }

        return out;
    }


    TEST_METHOD (Tokenize_ProducesTheBytesApplesoftItselfStoresForTheSameListing)
    {
        HeadlessHost       host;
        EmulatorCore       core;
        std::vector<Byte>  master   = GuestSession::RequireDos33Master();
        std::vector<Byte>  expected = TokenizedListing();
        std::vector<Byte>  stored;
        std::wstring       message;
        Word               txtTab   = 0;
        Word               varTab   = 0;



        GuestSession::BootToPrompt (host, core, master);

        GuestSession::TypeAndCollect (core, "NEW");
        GuestSession::TypeAndCollect (core, "10 HOME");
        GuestSession::TypeAndCollect (core, "20 PRINT \"CASSO\"");
        GuestSession::TypeAndCollect (core, "30 FOR I = 1 TO 3: PRINT I: NEXT");
        GuestSession::TypeAndCollect (core, "40 END");

        txtTab = ReadGuestWord (core, kTxtTab);
        varTab = ReadGuestWord (core, kVarTab);

        Assert::AreEqual ((int) ApplesoftTokenizer::kProgramBase, (int) txtTab,
            L"Applesoft must be holding its program where this tokenizer builds its links "
            L"for -- an agreement about the base is what makes the links comparable at all");

        Assert::IsTrue (varTab >= txtTab + expected.size(),
            L"and must be holding at least as many bytes as were produced for the same "
            L"text, or the comparison below would read past the program");

        stored = GuestSession::GuestBytesAt (core, txtTab, expected.size());

        message  = L"the bytes this tokenizer produced must be the bytes Applesoft stored "
                   L"for the same lines, links and all\n  Applesoft: ";
        message += Hex (stored);
        message += L"\n  Casso:     ";
        message += Hex (expected);

        Assert::IsTrue (stored == expected, message.c_str());
    }


    //
    //  ------------------------------------------------------------------
    //  Witness two: what the guest LISTs off a disk this tool wrote.
    //  ------------------------------------------------------------------
    //

    static std::vector<Byte> PutListingOnto (const std::vector<Byte>  & image,
                                             DiskCommandResult        & outResult)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.verb      = CommandLineOptions::DiskOptions::Verb::Put;
        options.disk.imagePath = kImagePath;
        options.disk.hostFile  = kHostFile;
        options.disk.path      = kPlacedName;
        options.disk.encoding  = CommandLineOptions::DiskOptions::Encoding::Basic;

        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };
        io.files[kHostFile]   = ListingBytes();
        io.stamps[kHostFile]  = FileStamp { ListingBytes().size(), 100 };

        {
            DiskCommandRunner  runner (io);

            outResult = runner.Run (options);
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"a commit leaves no temporary behind");

        return io.files[kImagePath];
    }


    //  Everything a correct placement implies that costs microseconds, asked
    //  before any processor starts. A mis-written image is reported here rather
    //  than by a 6502 executing whatever it managed to load off it.
    static void AssertTheWrittenImageIsStillADisk (const std::vector<Byte>  & written,
                                                   const std::vector<Byte>  & before)
    {
        DiskImage           image;
        SectorDecodeReport  report;
        std::vector<Byte>   sectors;
        VolumeListing       listing;
        FilePayload         placed;
        bool                foundCarried = false;
        bool                foundPlaced  = false;

        Assert::AreEqual (before.size(), written.size(),
            L"a placement does not change how big the image is");

        Assert::IsFalse (written == before,
            L"and it must actually have changed the image, or nothing was placed");

        AssertSucceeded (NibblizationLayer::NibblizeDsk (written, image));
        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, sectors, report));

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse (kPlacedName), placed));
        }

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == kMasterCarries) { foundCarried = true; }

            if (entry.name == kPlacedName)
            {
                foundPlaced = true;

                Assert::AreEqual ((int) Dos33Volume::kTypeApplesoft, (int) entry.type,
                    L"the placed file must be cataloged as Applesoft, since a tokenized "
                    L"program under any other type is one the guest will not RUN");
            }
        }

        Assert::IsTrue (foundCarried, L"the image must still carry what it carried");
        Assert::IsTrue (foundPlaced,  L"and now carry the placed program");

        Assert::IsTrue (placed.bytes == TokenizedListing(),
            L"whose stored bytes must be the tokenized program, read back off the "
            L"container the DRIVE sees rather than the buffer the writer held");
    }


    TEST_METHOD (Dos33_APlacedListing_IsWhatTheGuestLISTsAndWhatItRUNs)
    {
        HeadlessHost              host;
        EmulatorCore              core;
        DiskCommandResult         put;
        std::vector<Byte>         master  = GuestSession::RequireDos33Master();
        std::vector<Byte>         written = PutListingOnto (master, put);
        std::vector<std::string>  rows;



        Assert::AreEqual (DiskCommandRunner::kClean, put.exitStatus,
            L"the placement must succeed");

        Assert::AreEqual (std::string(), put.diagnostics,
            L"with nothing to complain about");

        AssertTheWrittenImageIsStillADisk (written, master);

        GuestSession::BootToPrompt (host, core, written);

        GuestSession::TypeAndCollect (core, "LOAD PROG");

        rows = GuestSession::TypeAndCollect (core, "LIST");

        // ASKING ONLY WHETHER THE SCREEN CONTAINS THE TEXT IS NOT ENOUGH -- the
        // echo of the line that produced the row satisfies that, and so does a
        // line whose spacing or operands came out wrong. Each row is compared
        // whole, and every row mentioning the needle must be that row.
        GuestSession::AssertTheOnlyRowsMentioning (rows, "HOME",  "10  HOME");
        GuestSession::AssertTheOnlyRowsMentioning (rows, "CASSO", "20  PRINT \"CASSO\"");
        GuestSession::AssertTheOnlyRowsMentioning (rows, "FOR",   "30  FOR I = 1 TO 3: PRINT I: NEXT");
        GuestSession::AssertTheOnlyRowsMentioning (rows, "END",   "40  END");

        // And it is a PROGRAM, not merely something LIST could render: the
        // difference is a line whose tokens are right and whose links are not,
        // which lists perfectly and runs off the end of itself.
        rows = GuestSession::TypeAndCollect (core, "RUN");

        Assert::IsTrue (GuestSession::AnyRowIs (rows, "CASSO"),
            L"running it must print the string on a line of its own, which the listing "
            L"row containing the same word is not");

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"),
            L"and must not report an error");
    }


    //
    //  ------------------------------------------------------------------
    //  Real vendor material, for the direction a user did not ask to have
    //  changed.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (RoundTrip_TheMastersOwnGreeting_ComesBackByteForByte)
    {
        // 419 bytes of Applesoft nobody here wrote, carrying CTRL-D command
        // strings, a negative CALL, a REM, and relational operators stored as
        // two tokens. Hand-authored cases pin the rules; this one is the check
        // that the rules add up to a real program.
        DiskImage              image;
        SectorDecodeReport     report;
        std::vector<Byte>      master = GuestSession::RequireDos33Master();
        std::vector<Byte>      sectors;
        std::vector<Byte>      again;
        FilePayload            hello;
        ApplesoftListingError  error;
        std::string            listing;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (master, image));
        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, sectors, report));

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse ("HELLO"), hello));
        }

        Assert::AreEqual ((size_t) 419, hello.bytes.size(),
            L"the stock master's greeting, at the length it has always had");

        AssertSucceeded (ApplesoftTokenizer::Detokenize (hello.bytes, listing, error),
            L"a program a vendor shipped must be readable as a listing");

        Assert::IsTrue (listing.find ("DOS VERSION 3.3") != std::string::npos,
            L"and the listing must actually say what the greeting says, or the round trip "
            L"below is between two piles of the same garbage");

        AssertSucceeded (ApplesoftTokenizer::Tokenize (listing, again, error),
            L"and the listing must tokenize back");

        Assert::IsTrue (again == hello.bytes,
            L"byte for byte -- extracting a program and placing it back is the direction "
            L"nobody opted into having changed");
    }
};
