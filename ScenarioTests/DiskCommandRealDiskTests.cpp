#include "Pch.h"

#include "../UnitTest/EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"
#include "RealDiskFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRealDiskTests
//
//  The disk commands run against the Merlin disks: get, list, boot, create
//  and put over catalogs, files and boot paths that real DOS 3.3 and ProDOS
//  wrote.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskCommandRealDiskTests)
{
public:

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
        size_t       at   = Dos33Skeleton::GetSectorOffset (kGreetingTrack, kGreetingSector)
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

    //  The raw thirty bytes of the field, not merely the name they spell: the
    //  name in high ASCII, padded with high-ASCII spaces to the width of a
    //  catalog name field, which is the shape a booting DOS matches against
    //  its own catalog. A field holding the right letters in the wrong
    //  encoding names a file DOS then fails to find.
    static void AssertGreetingFieldHolds (const vector<Byte> & image, const char * name)
    {
        size_t       at   = Dos33Skeleton::GetSectorOffset (kGreetingTrack, kGreetingSector)
                          + kGreetingOffset;
        std::string  text = name;
        size_t       i    = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            Byte  expected = (i < text.size()) ? (Byte) (text[i] | 0x80) : (Byte) 0xA0;

            Assert::AreEqual ((int) expected, (int) image[at + i],
                L"the greeting field must hold the name in high ASCII, space-padded to "
                L"the width of a catalog name field");
        }
    }

    CommandLineOptions MakeBootOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Boot, image);

        options.disk.path = path;

        return options;
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

    //  The trailing free-space report, which the listing separates from the
    //  entries with a blank line. Taken as text rather than reparsed, so a
    //  change of either number shows up in the failure message.
    static std::string FreeSpaceLine (const std::string & listing)
    {
        size_t  at = listing.rfind ("\n\n");

        Assert::IsTrue (at != std::string::npos, L"the listing must carry a free-space report");

        return listing.substr (at);
    }


    TEST_METHOD (Get_ByIndex_ReadsThatEntry_AndASharedNameAloneIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        RealDiskFixtures::SeedRealDisk (io);

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
        RealDiskFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk");

        options.disk.path  = "PRODOS";
        options.disk.index = 1;
        result             = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find (kNotAllowed) != std::string::npos);

        RealDiskFixtures::SeedRealDisk (io);
    }


    TEST_METHOD (Get_WithNoOutputFile_ReturnsPayloadForStandardOutput)
    {
        // With no destination named the bytes go to the process's own output,
        // which is what makes extraction pipeable.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        RealDiskFixtures::SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"the payload is returned for the caller to deliver");
        RealDiskFixtures::AssertIsMakeDumpPayload (result.payload);
        Assert::AreEqual (size_t (0), io.files.count ("C:\\out.bin"),
            L"and nothing was written to the host");
    }


    TEST_METHOD (Get_WithOutputFile_WritesThroughTheSeam)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        RealDiskFixtures::SeedRealDisk (io);
        options.disk.path     = "MAKE DUMP";
        options.disk.hostFile = "C:\\out.bin";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"an explicit destination is not the process output");
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.bin"));
        RealDiskFixtures::AssertIsMakeDumpPayload (io.files["C:\\out.bin"]);
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

        RealDiskFixtures::SeedRealDisk (io);
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

        RealDiskFixtures::SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("$9000") != std::string::npos,
            L"the load address must be reported");
        Assert::IsTrue (result.output.empty(),
            L"and must not contaminate the payload stream");
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

        RealDiskFixtures::SeedRealDisk (io);
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

        RealDiskFixtures::SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"nothing may be delivered under a conversion not performed");
        Assert::IsTrue (result.diagnostics.find ("--basic") != std::string::npos,
            L"and the refusal must quote the flag it is refusing");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
    }


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

        RealDiskFixtures::SeedRealDisk (io);
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

        RealDiskFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
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


    TEST_METHOD (List_ShowsAnIndexOnlyBesideNamesSeveralEntriesShare)
    {
        // Merlin's disk has ten heading entries of one name and two pairs of
        // another; those fourteen rows are listed with an index, and no other row is.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        size_t             marks = 0;
        size_t             at    = 0;

        RealDiskFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        for (at = result.output.find ("  (index "); at != std::string::npos; at = result.output.find ("  (index ", at + 1))
        {
            marks++;
        }

        Assert::AreEqual ((size_t) 14, marks);
        Assert::IsTrue   (result.output.find ("HELLO  (index") == std::string::npos, L"a unique name shows no index");
    }


    //  A recursive listing's name column is as wide as its longest path, so
    //  every type lines up.
    TEST_METHOD (List_Recurse_AlignsTheColumnsToTheLongestPath)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;
        std::istringstream  lines;
        std::string         line;
        size_t              column = std::string::npos;
        size_t              rows   = 0;

        RealDiskFixtures::SeedProDosDisk (io);

        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Mkdir);
        options.disk.path = "GRAPHICS.FOLDER/PRINTSHOP.ITEMS";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.recurse = true;
        result               = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        lines.str (result.output);

        while (std::getline (lines, line))
        {
            size_t  type = line.find (" $");

            if (line.find ("eof=") != std::string::npos)
            {
                Assert::IsTrue (column == std::string::npos || type == column, TextEncoding::NarrowToWide (line).c_str());
                Assert::IsTrue (line.find ("PRINTSHOP.ITEMSDIR") == std::string::npos);

                column = type;
                rows++;
            }
        }

        Assert::IsTrue (rows > 2);
    }


    TEST_METHOD (List_WithRecurse_WritesAFullPathPerRow)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        RealDiskFixtures::SeedProDosDisk (io);

        options              = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        options.disk.recurse = true;
        result               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.output.find ('/') != std::string::npos,
            L"a row below the volume directory gives the path that reaches it");
    }


    TEST_METHOD (ADos33NameKeepsItsSlashes)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        CommandLineOptions  options;

        RealDiskFixtures::SeedRealDisk (io);

        //  DOS 3.3 has no directories and no volume names, so a leading slash
        //  is part of the name. This disk holds a file called /HELLO, and
        //  reading it is the proof that nothing stripped the slash.
        options           = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        options.disk.path = "/HELLO";
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.hasPayload);
        Assert::IsTrue   (result.diagnostics.find ("this image is") == std::string::npos,
            L"and no volume was read out of the name");
    }


    TEST_METHOD (List_SaysNotBootable_OnlyWhenTheBootTracksAreEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       stripped;


        RealDiskFixtures::SeedRealDisk (io);

        // The real disk carries DOS and says nothing about booting.
        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));
        Assert::IsTrue (result.output.find ("Not bootable") == std::string::npos,
            L"a disk with DOS on its boot tracks is not called unbootable");

        // The same disk with tracks 0 to 2 zeroed is a data disk: its catalog
        // is intact, and the boot ROM would find a BRK at $0801.
        stripped = io.files[DiskRunnerFixtures::kImage];
        std::fill (stripped.begin(), stripped.begin() + 3 * 16 * 256, (Byte) 0);
        io.files[DiskRunnerFixtures::kImage] = stripped;

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing itself is unchanged");
        Assert::IsTrue (result.output.find ("Not bootable: no DOS on tracks 0 to 2") != std::string::npos,
            L"and it says the disk cannot boot");
    }


    TEST_METHOD (List_ProDosVolume_UsesItsOwnShape)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        RealDiskFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", "C:\\disks\\merlin.po.dsk");

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          "C:\\disks\\merlin.po.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("/MERLIN") != std::string::npos,
            L"a ProDOS volume has a name, not a number");
        Assert::IsTrue (result.output.find ("blocks free of 280") != std::string::npos,
            L"and counts in blocks rather than sectors");
        Assert::IsTrue (result.output.find ("Not bootable") == std::string::npos,
            L"a volume carrying PRODOS is not called unbootable");
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
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                                   kProDosImage);
        DiskCommandResult   result;

        RealDiskFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProDosImage);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        //  A whole rendered row rather than a substring search for `eof=`: the
        //  columns have to land in their places, and the row has to stay inside
        //  80 characters now that nothing can turn them off.
        Assert::IsTrue (result.output.find ("*MERLIN.SYSTEM        $FF    37  eof=18432 aux=$2000\n")
                            != std::string::npos,
            L"the ProDOS row, with both columns, unasked for");

        DiskRunnerFixtures::AssertEveryLineFitsEightyColumns (result.output);
    }


    //  DOS 3.3 records neither field and has its own formatter, so it must be
    //  untouched by the ProDOS columns becoming unconditional.
    TEST_METHOD (List_OfADos33Volume_IsUnchangedByTheProDosColumns)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        RealDiskFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find (" B 004 MERLIN\n") != std::string::npos,
            L"the DOS 3.3 row is what a booted machine prints");
        Assert::IsTrue (result.output.find ("eof=") == std::string::npos,
            L"and carries no field this filesystem does not record");
    }


    TEST_METHOD (Boot_SetsTheNameTheGuestReadsAtBootTime_InTheCommittedImage)
    {
        // An Applesoft program, because that is what a booting DOS 3.3 RUNs.
        // The binary case is the one below, and it is a complaint rather than a
        // refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  put = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kImage, DiskRunnerFixtures::kHostFile, kDosProgram);
        DiskCommandResult   result;

        RealDiskFixtures::SeedRealDisk (io);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, DiskRunnerFixtures::MakePayload (16));

        put.disk.typeName = "A";

        Assert::AreEqual (std::string ("HELLO"), GreetingNameIn (io.files[DiskRunnerFixtures::kImage]),
            L"this disk boots its own greeting today, or the command below changes nothing");

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (put).exitStatus,
            L"the program has to be on the disk before it can be booted into");

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kImage, kDosProgram));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"specifying a program the volume holds and DOS can run is a clean operation");

        Assert::AreEqual (std::string(), result.diagnostics, L"with nothing to complain about");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and nothing left beside the image");

        Assert::AreEqual (std::string (kDosProgram), GreetingNameIn (io.files[DiskRunnerFixtures::kImage]),
            L"and the name the guest reads at boot is the one that was set -- read "
            L"back off the COMMITTED image, not out of a buffer the runner still held");

        AssertGreetingFieldHolds (io.files[DiskRunnerFixtures::kImage], kDosProgram);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage).find (" A 004 HELLO") != std::string::npos,
            L"while the greeting it used to run is still on the disk, untouched: this "
            L"changes what runs, not what is on the volume");
    }


    //  A STARTUP PROGRAM DOS 3.3 CANNOT RUN IS REFUSED, AND NOTHING IS WRITTEN.
    //
    //  Measured on the stock master with a real 6502: the disk boots and the
    //  binary is never executed, because DOS 3.3's boot command is RUN.
    //
    //  This used to set the name, commit the image and then say so, on the
    //  reasoning that a DOS patched by hand to BRUN is a real thing and
    //  refusing would block it. What that produced for everyone else was a
    //  command that reported trouble and changed the disk anyway, leaving a
    //  volume configured to start a program that cannot start -- which is
    //  what ProDOS refuses outright in the same command.
    TEST_METHOD (Boot_ABinaryOnADos33Volume_IsRefused_AndTheGreetingIsUntouched)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        std::string         before;

        RealDiskFixtures::SeedRealDisk (io);

        before = GreetingNameIn (io.files[DiskRunnerFixtures::kImage]);
        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kImage, kBinaryOnTheDisk));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a startup program DOS will not run is refused, not carried out");

        Assert::IsTrue (result.diagnostics.find ("RUN") != std::string::npos,
            L"and the refusal explains what DOS does at boot");

        Assert::IsTrue (result.diagnostics.find (kBinaryOnTheDisk) != std::string::npos,
            L"identifying the file it is about");

        Assert::AreEqual (before, GreetingNameIn (io.files[DiskRunnerFixtures::kImage]),
            L"and the greeting the disk already had is left exactly as it was");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
    }


    TEST_METHOD (Boot_ANameTheVolumeDoesNotHold_IsRefusedNamingTheMissingFile)
    {
        // The refusal FR-025 asks for, and the reason it is worth having: a
        // startup program that is not there produces no symptom until somebody
        // boots the disk, and then the symptom is on a machine somewhere else.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = RealDiskFixtures::OriginalImageBytes();

        RealDiskFixtures::SeedRealDisk (io);

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kImage, "NOSUCHFILE"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"the message must identify the file that is missing, not merely report a failure");
        Assert::IsTrue (result.diagnostics.find (DiskRunnerFixtures::kImage) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("is not on this volume") != std::string::npos);

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kImage, original);

        Assert::AreEqual (std::string ("HELLO"), GreetingNameIn (io.files[DiskRunnerFixtures::kImage]),
            L"and the disk still boots what it booted before");
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

        RealDiskFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", DiskRunnerFixtures::kProImage);

        original = io.files[DiskRunnerFixtures::kProImage];

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kProImage, kProProgram));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kProProgram) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("type SYS") != std::string::npos,
            L"and reports what this boot path does launch");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kProImage, original);
    }


    TEST_METHOD (Boot_ProDos_ASystemProgramPlacedByPut_OvertakesTheOneTheDiskLaunchesNow)
    {
        // /MERLIN launches MERLIN.SYSTEM, the first system program its volume
        // directory reaches. Nominating a newly placed one has to overtake it,
        // and the proof is the ORDER in the committed image rather than any
        // field, because ProDOS stores no startup name anywhere to inspect.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kProImage, DiskRunnerFixtures::kHostFile, "CASSO.SYSTEM");
        DiskCommandResult   result;

        RealDiskFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", DiskRunnerFixtures::kProImage);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, DiskRunnerFixtures::MakePayload (60));

        options.disk.typeName = "SYS";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"the placement must succeed before the reorder can mean anything");

        AssertListedBefore (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kProImage), "MERLIN.SYSTEM", "CASSO.SYSTEM");

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kProImage, "CASSO.SYSTEM"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);
        Assert::IsTrue (io.HasNoTemporaryFiles());

        AssertListedBefore (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kProImage), "CASSO.SYSTEM", "MERLIN.SYSTEM");

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kProImage).find ("PARMS") != std::string::npos,
            L"and every other entry is still on the disk");
    }


    //  --bootable on a disk larger than 140K was dropped without a word, so a
    //  32 MB volume had no PRODOS and didn't start from a hard drive card.
    //  Found booting one on Apple's SCSI card in MAME.
    TEST_METHOD (Create_MakesALargeProDosVolumeBootable)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("big.hdv");
        DiskCommandResult   made;
        DiskCommandResult   listed;



        RealDiskFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-b.dsk", "master.dsk");

        for (const char * size : { "800K", "32M" })
        {
            options.disk.sizeName     = size;
            options.disk.formatName   = "prodos";
            options.disk.bootable     = true;
            options.disk.bootableFrom = "master.dsk";

            made = runner.Run (options);
            Assert::AreEqual (DiskCommandResult::kClean, made.exitStatus, TextEncoding::NarrowToWide (made.output + made.diagnostics).c_str());
            Assert::IsTrue   (made.output.find (", bootable") != std::string::npos, TextEncoding::NarrowToWide (made.output).c_str());
            Assert::AreNotEqual ((int) io.files["big.hdv"][0], 0, L"block 0 has the ProDOS loader");

            listed = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, "big.hdv"));
            Assert::IsTrue (listed.output.find ("PRODOS") != std::string::npos, TextEncoding::NarrowToWide (listed.output).c_str());
            Assert::IsTrue (listed.output.find ("Not bootable") == std::string::npos);

            io.files.erase ("big.hdv");
            io.stamps.erase ("big.hdv");
        }

        options.disk.formatName = "hfs";
        made = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kNoOutput, made.exitStatus, L"an HFS volume isn't made bootable");
        Assert::IsFalse  (io.Exists ("big.hdv"));
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
        CommandLineOptions  options   = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kImage, DiskRunnerFixtures::kHostFile, "MAKE DUMP");
        DiskCommandResult   extracted;
        DiskCommandResult   result;
        DiskCommandResult   readBack;

        RealDiskFixtures::SeedRealDisk (io);

        extracted = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandResult::kClean, extracted.exitStatus);
        RealDiskFixtures::AssertIsMakeDumpPayload (extracted.payload);

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, extracted.payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x9000;
        options.disk.hasLoadAddress = true;
        options.disk.encoding       = CommandLineOptions::DiskOptions::Encoding::Verbatim;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"replacing a file with its own contents must succeed");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);

        // The file, byte for byte, through a path that re-read the committed
        // image rather than trusting the runner that wrote it.
        RealDiskFixtures::AssertIsMakeDumpPayload (readBack.payload);
        Assert::IsTrue (readBack.payload == extracted.payload);

        // And the reason an image comparison could not stand in for that: the
        // file occupies four sectors, so its footprint on the disk is 1024
        // bytes while the file itself is 589. Comparing the footprint would
        // compare 431 bytes that are not the file.
        Assert::AreEqual (size_t (589), readBack.payload.size());
        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage).find ("B 004 MAKE DUMP\n") != std::string::npos,
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kImage, DiskRunnerFixtures::kHostFile, "MAKE DUMP");
        DiskCommandResult   extracted;
        std::string         before;
        std::string         after;

        RealDiskFixtures::SeedRealDisk (io);

        before    = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage);
        extracted = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kImage, "MAKE DUMP");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, extracted.payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x9000;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        after = DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kImage);

        Assert::IsTrue (before.find ("sectors free of 560") != std::string::npos,
            L"the listing must actually be reporting free space for this to mean anything");

        Assert::AreEqual (FreeSpaceLine (before), FreeSpaceLine (after),
            L"a file replaced by its own contents leaks nothing and grows nothing");
    }
};




