#include "Pch.h"

#include "Machines/Apple2/Common/Denibblizer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "CassoExplorer/Model/HostAppleFile.h"
#include "Core/AppleSingleCodec.h"
#include "Core/TextEncoding.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPutCommandTests
//
//  disk put: the type, name and address a file gets on the volume, where it
//  lands, and the cases where the image is left as it was.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskPutCommandTests)
{
public:

    //  Put reads `._NAME` beside the file, as get writes it: the resource fork
    //  and the ProDOS type go onto the disk, and come back out.
    TEST_METHOD (Put_ReadsTheAppleDoubleFileBesideIt)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        AppleSingleFile     single;
        AppleSingleFile     back;
        vector<Byte>        sidecar;
        std::string         error;



        DiskRunnerFixtures::SeedProDosDisk (io);

        single.resourceFork    = { 1, 2, 3, 4, 5 };
        single.hasResourceFork = true;
        single.hasProDosInfo   = true;
        single.fileType        = 0x50;
        single.auxType         = 0x5445;
        single.finderInfo.assign (32, 0);
        AppleSingleCodec::EncodeDouble (single, sidecar);

        io.files["DOC"]    = { 'T', 'E', 'A', 'C', 'H' };
        io.files["._DOC"]  = sidecar;
        io.stamps["DOC"]   = FileStamp { 5, 100 };
        io.stamps["._DOC"] = FileStamp { sidecar.size(), 100 };

        options               = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Put);
        options.disk.hostFile = "DOC";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options               = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        options.disk.path     = "DOC";
        options.disk.hostFile = "out";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue  (io.files["out"] == io.files["DOC"]);
        AssertSucceeded (AppleSingleCodec::Decode (io.files["._out"], back, error));
        Assert::IsTrue   (back.resourceFork == single.resourceFork, L"the resource fork went onto the disk");
        Assert::AreEqual ((uint32_t) 0x50,   (uint32_t) back.fileType);
        Assert::AreEqual ((uint32_t) 0x5445, (uint32_t) back.auxType);
    }


    //  The entry `name` in the committed ProDOS image's catalog.
    static FileEntry FindProDosEntry (FakeDiskFileIo & io, const char * name)
    {
        vector<Byte>        sectors;
        SectorDecodeReport  report;
        VolumeListing       listing;
        FileEntry           found;



        AssertSucceeded (VolumeImage::Load (io.files[DiskRunnerFixtures::kImage], DiskRunnerFixtures::kImage, sectors, report));
        AssertSucceeded (VolumeImage::CreateVolume (VolumeKind::ProDos, sectors)->Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == name)
            {
                found = entry;
            }
        }

        return found;
    }


    static Byte GetProDosType (FakeDiskFileIo & io, const char * name)
    {
        return FindProDosEntry (io, name).type;
    }


    //  A file put from the PC is dated when it was last written there, in
    //  local time.
    TEST_METHOD (Put_DatesTheFileWhenItWasWrittenOnThePc)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Put);
        FilePayload         expected;
        FileEntry           entry;



        DiskRunnerFixtures::SeedProDosDisk (io);

        io.files["note.txt"]  = { 'H', 'I' };
        io.stamps["note.txt"] = FileStamp { 2, 1700000000 };
        HostAppleFile::SetHostDates (1700000000, expected);

        options.disk.hostFile = "note.txt";
        options.disk.path     = "NOTE";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        entry = FindProDosEntry (io, "NOTE");
        Assert::IsTrue   (expected.hasDates);
        Assert::AreEqual ((int) (23 << 9 | 11 << 5), (int) (expected.modifiedDate & 0xFFE0), L"November 2023");
        Assert::AreEqual ((int) expected.modifiedDate, (int) entry.modifiedDate);
        Assert::AreEqual ((int) expected.modifiedTime, (int) entry.modifiedTime);
        Assert::AreEqual ((int) expected.modifiedDate, (int) entry.createdDate);
    }


    //  --type takes any type in ProDOS's table, by its abbreviation or its
    //  number, and a host name's type applies with --as too.
    TEST_METHOD (Put_TakesAProDosTypeByNumberOrAbbreviation_AndAHostNamesTypeWithAs)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;



        DiskRunnerFixtures::SeedProDosDisk (io);

        io.files["pic.bin"]     = vector<Byte> (64, 0x11);
        io.stamps["pic.bin"]    = FileStamp { 64, 100 };
        io.files["ART#C10001"]  = vector<Byte> (64, 0x22);
        io.stamps["ART#C10001"] = FileStamp { 64, 100 };

        options                = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Put);
        options.disk.hostFile  = "pic.bin";
        options.disk.path      = "PAINT";
        options.disk.typeName  = "$C0";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options.disk.path      = "SOUND";
        options.disk.typeName  = "snd";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        options.disk.hostFile  = "ART#C10001";
        options.disk.path      = "PICTURE";
        options.disk.typeName  = "";
        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::AreEqual ((int) 0xC0, (int) GetProDosType (io, "PAINT"));
        Assert::AreEqual ((int) 0xD8, (int) GetProDosType (io, "SOUND"));
        Assert::AreEqual ((int) 0xC1, (int) GetProDosType (io, "PICTURE"));

        options.disk.path      = "ODD";
        options.disk.typeName  = "$100";
        Assert::AreEqual (DiskCommandResult::kNoOutput, runner.Run (options).exitStatus, L"past a byte");
    }

    //
    //  ------------------------------------------------------------------
    //  put and delete.
    //
    //  These are the first commands that change a user's disk, so every refusal
    //  below asserts the image is byte-for-byte what it was rather than
    //  merely that an error came back -- an error verdict is satisfied by a
    //  half-written image.
    //  ------------------------------------------------------------------
    //

    static constexpr const char *  kProOrdered = "C:\\disks\\merlin.po";

    //  Bigger than the 496 sectors a freshly formatted volume leaves free, so
    //  placement is refused for want of room rather than for any other reason.
    static constexpr size_t    kOversizedBytes    = 130000;

    //  For the refusals that must state exactly one reason: a count of one
    //  newline is what separates a single sentence from a message that
    //  reported its cause and then went on into something else.
    static size_t CountNewlines (const std::string & text)
    {
        size_t  newlines = 0;
        size_t  at       = 0;

        while ((at = text.find ('\n', at)) != std::string::npos)
        {
            newlines++;
            at++;
        }

        return newlines;
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  WHAT put's THREE NAMING OPTIONS DEFAULT TO.
    //
    //  The help says all of this in a paragraph, and a test used to assert the
    //  paragraph. Quoting a sentence proves somebody wrote it, not that the
    //  command does it: the wording could be perfect while the default moved,
    //  and the test would pass. These assert the defaults themselves, so the
    //  page can be reworded freely and cannot become untrue.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Put_WithNoNameGiven_TakesTheOneTheFileHasOnTheHost)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, "GREET", "");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, "GREET",     DiskRunnerFixtures::MakePayload());

        options.disk.path           = "";
        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find ("GREET") != std::string::npos,
            L"the name on the disk is the name it had on the host");
    }

    //  A BINARY IS WHAT put ASSUMES, which matters because the type decides
    //  whether the guest can do anything with the file at all.
    ////////////////////////////////////////////////////////////////////////////
    //
    //  THE TYPE IS READ OFF THE FILE WHEN NOBODY NAMED ONE.
    //
    //  The help says so, which is the only reason this has to be true: a page
    //  that claims a behavior the code does not have is worse than a page
    //  that claims nothing.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  An Applesoft program: a chain of lines, each a next-pointer, a line
    //  number, tokens, and a zero, ending on a zero pointer.
    static vector<Byte> MakeApplesoftProgram()
    {
        vector<Byte>  program;
        Word          start = 0x0801;
        Word          next  = (Word) (start + 7);

        //  10 END
        program.push_back ((Byte) (next & 0xFF));
        program.push_back ((Byte) (next >> 8));
        program.push_back (10);
        program.push_back (0);
        program.push_back (0x80);              // END
        program.push_back (0);

        //  20 END
        program.push_back (0);                 // the chain ends
        program.push_back (0);

        return program;
    }

    TEST_METHOD (Put_WithNoType_ReadsApplesoftOffTheFilesOwnBytes)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   MakeApplesoftProgram());

        options.disk.typeName       = "";
        options.disk.hasLoadAddress = false;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"a program needs no address, so nothing is refused");

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" A ") != std::string::npos,
            L"the catalog records Applesoft, unasked");
    }

    //  AND ANYTHING IT DOES NOT RECOGNIZE IS STILL A BINARY. A detector that
    //  fired on arbitrary bytes would file a build's output where the guest
    //  cannot run it, which is worse than never guessing.
    TEST_METHOD (Put_WithNoType_LeavesUnrecognizedBytesABinary)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" B ") != std::string::npos,
            L"an assembled binary stays a binary");
    }

    //  A NAMED TYPE BEATS THE BYTES. The detector informs a default; it does
    //  not argue with the operator.
    TEST_METHOD (Put_WithATypeNamed_KeepsItEvenWhenTheBytesSayOtherwise)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   MakeApplesoftProgram());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" B ") != std::string::npos,
            L"--type B was given and --type B is what the catalog holds");
    }

    TEST_METHOD (Put_WithNoTypeGiven_StoresABinary)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" B ") != std::string::npos,
            L"the catalog records a binary");
    }

    //  A DOS 3.3 BINARY WITHOUT AN ADDRESS IS REFUSED. The header carries the
    //  load address, so there is no binary to write without one.
    TEST_METHOD (Put_ABinaryWithNoAddress_IsRefusedRatherThanGuessing)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.hasLoadAddress = false;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a binary with nowhere to load is not written");
        Assert::IsFalse (result.diagnostics.empty(), L"and the refusal reports it");
    }

    TEST_METHOD (Put_ABinaryOntoAFreshVolume_LandsAndReadsBackByteForByte)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        vector<Byte>        payload = DiskRunnerFixtures::MakePayload();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"a placement with room, a legal name and an address is clean");

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and leaves nothing beside the image");

        // 512 bytes plus the four-byte load/length header DOS stores inside the
        // file is 516 bytes -- three data sectors -- and the track/sector list
        // is a fourth. The `B 002` that the task text and quickstart both
        // carried is the arithmetic for a payload of 252 bytes or fewer.
        Assert::IsTrue (DiskRunnerFixtures::ListCommittedImage (io, DiskRunnerFixtures::kBlankImage).find (" B 004 PROG") != std::string::npos,
            L"the guest's own listing shape, with the sector count the file really occupies");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kBlankImage, "PROG");

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);
        Assert::IsTrue (readBack.payload == payload,
            L"the bytes on the disk are the bytes that went in");

        Assert::IsTrue (readBack.diagnostics.find ("$6000") != std::string::npos,
            L"and the load address survived the round trip");
    }

    //  The committed image decoded the way the DRIVE sees it -- laid down as
    //  nibbles and read back through the hardware interleave -- rather than
    //  through the path that wrote it. This is the unit-suite half of the
    //  guest-visible placement gate: a placement written through a wrong
    //  understanding reads back perfectly through the same wrong
    //  understanding, and only the drive's own decode or a booted guest can
    //  say otherwise. The booted guest stays in the scenario suite.
    TEST_METHOD (Put_TheCommittedImage_DecodesThroughTheDriveWithThePlacedFileIntact)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskImage           image;
        SectorDecodeReport  report;
        vector<Byte>        payload = DiskRunnerFixtures::MakePayload();
        vector<Byte>        decoded;
        FilePayload         placed;

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::MakeBlankDos33Image());
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"the placement must succeed before there is anything to decode");

        AssertSucceeded (Nibblizer::NibblizeDsk (io.files[DiskRunnerFixtures::kBlankImage], image));
        AssertSucceeded (Denibblizer::Denibblize (image, DiskFormat::Dsk, decoded, report));

        Assert::IsTrue (decoded == io.files[DiskRunnerFixtures::kBlankImage],
            L"every sector of the committed image must come back off the drive as it "
            L"went on");

        {
            Dos33Volume  volume (decoded);

            AssertSucceeded (volume.Read (FilePath::Parse ("PROG"), placed));
        }

        Assert::IsTrue (placed.bytes == payload,
            L"and the placed file must read back byte for byte off the container the "
            L"drive presents");

        Assert::IsTrue (placed.hasLoadAddress, L"with a load address recorded");
        Assert::AreEqual (DiskRunnerFixtures::kLoadAddress, placed.loadAddress);
    }

    TEST_METHOD (Put_OverALockedFile_IsRefusedInTermsThatSayItIsLocked)
    {
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

        DiskRunnerFixtures::LockFirstCatalogEntry (io.files[DiskRunnerFixtures::kBlankImage]);
        committed = io.files[DiskRunnerFixtures::kBlankImage];

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("Unable to write PROG: it is locked") != std::string::npos,
            L"the refusal must state the file is locked, not merely that something failed");

        Assert::IsTrue (result.diagnostics.find ("PROG") != std::string::npos,
            L"identifying the file");
        Assert::IsTrue (result.diagnostics.find (DiskRunnerFixtures::kBlankImage) != std::string::npos,
            L"and the image");

        Assert::AreEqual (size_t (1), CountNewlines (result.diagnostics),
            L"and as ONE reason: a second line means the refusal reported its cause and "
            L"then went on far enough to trip over something else");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, committed);
    }

    TEST_METHOD (Put_WhenTheVolumeHasNoRoom_RefusesAndLeavesTheImageByteIdentical)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "BIG");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload (kOversizedBytes));

        options.disk.typeName = "T";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("Unable to write BIG: the volume has no room") != std::string::npos,
            L"and reports the volume has no room, not something generic");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount, L"nothing was written anywhere at all");
    }

    TEST_METHOD (Put_WithANameTheCatalogCannotStore_RefusesAndSaysWhatALegalNameIs)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "9LIVES");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("9LIVES") != std::string::npos,
            L"the message quotes the name that was refused");
        Assert::IsTrue (result.diagnostics.find ("starting with a letter") != std::string::npos,
            L"and shows what a legal one looks like");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
    }

    TEST_METHOD (Put_ABinaryWithNoLoadAddress_SaysWhichFlagIsMissing)
    {
        // $0000 is a legal load address, so defaulting would be
        // indistinguishable from an answer. The refusal has to name the flag,
        // or the caller is left guessing which of several things was wrong.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName = "B";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("--load") != std::string::npos,
            L"the message must give the flag that would fix it");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        io.failNextReplace  = true;
        io.nextReplaceError = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("write-protected") != std::string::npos,
            L"the reason given must be write protection, not a generic replace failure");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
    }

    TEST_METHOD (Put_WithATypeThisFilesystemDoesNotHave_RefusesAndListsWhatItTakes)
    {
        // Defaulting instead would place the file under a type nobody asked for
        // and say nothing; the guest reports the mismatch much later, as a file
        // that will not load.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kBlankImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = DiskRunnerFixtures::MakeBlankDos33Image();

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kBlankImage, blank);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   DiskRunnerFixtures::MakePayload());

        options.disk.typeName = "SYS";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("DOS 3.3 takes") != std::string::npos,
            L"and the refusal lists what this filesystem does take");

        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, blank);
    }


    TEST_METHOD (Put_OntoAProDosVolume_RecordsTheLoadAddressAsTheAuxiliaryType)
    {
        // Where a binary's load address lives is the one thing the two
        // filesystems disagree about: DOS writes it into the file's own first
        // four bytes and ProDOS records it in the directory entry instead.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kProImage, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandRunner   reader (io);
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        CommandLineOptions  listing  = DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::List, DiskRunnerFixtures::kProImage);
        vector<Byte>        payload  = DiskRunnerFixtures::MakePayload();

        DiskRunnerFixtures::SeedProDosDisk (io, DiskRunnerFixtures::kProImage);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, payload);

        options.disk.typeName       = "BIN";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        Assert::IsTrue (reader.Run (listing).output.find ("aux=$6000") != std::string::npos,
            L"ProDOS records the load address in the entry, not in the file");

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, DiskRunnerFixtures::kProImage, "PROG");

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
        CommandLineOptions  options    = DiskRunnerFixtures::MakePutOptions (kProOrdered, DiskRunnerFixtures::kHostFile, "PROG");
        DiskCommandResult   readBack;
        vector<Byte>        dosOrdered = DiskRunnerFixtures::MakeBlankProDosDsk();
        vector<Byte>        proOrdered;
        vector<Byte>        payload    = DiskRunnerFixtures::MakePayload();

        VolumeImage::DosLogicalToProDosFile (dosOrdered, proOrdered);

        Assert::IsFalse (proOrdered == dosOrdered,
            L"the two orders must actually differ, or this test proves nothing");

        DiskRunnerFixtures::SeedFile (io, kProOrdered, proOrdered);
        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile,   payload);

        options.disk.typeName       = "BIN";
        options.disk.loadAddress    = DiskRunnerFixtures::kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        readBack = DiskRunnerFixtures::GetFromCommittedImage (io, kProOrdered, "PROG");

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus,
            L"the committed file must still be a ProDOS volume in ProDOS order");

        Assert::IsTrue (readBack.payload == payload);
    }


    //  A WOZ whose INFO marks it write-protected isn't written, as the
    //  emulated drive won't write it.
    TEST_METHOD (Put_OnAWriteProtectedWoz_IsRefused_AndChangesNothing)
    {
        constexpr const char *  kImage = "C:\\disks\\wp.woz";
        FakeDiskFileIo          io;
        DiskCommandRunner       runner (io);
        CommandLineOptions      options;
        DiskCommandResult       result;
        vector<Byte>            before;

        options.disk.command    = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.imagePath  = kImage;
        options.disk.formatName = "dos";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);
        AssertSucceeded (WozLoader::SetWriteProtectFlag (io.files[kImage], true));

        before = io.files[kImage];

        DiskRunnerFixtures::SeedFile (io, DiskRunnerFixtures::kHostFile, DiskRunnerFixtures::MakePayload());
        result = runner.Run (DiskRunnerFixtures::MakeBinaryPutOptions (kImage));

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue      (result.diagnostics.find ("write-protected") != std::string::npos, TextEncoding::NarrowToWide (result.diagnostics).c_str());
        Assert::IsTrue      (io.files[kImage] == before, L"and the image is untouched");
    }
};
