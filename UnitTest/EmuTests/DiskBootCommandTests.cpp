#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBootCommandTests
//
//  disk boot: the startup program a DOS 3.3 or ProDOS volume runs at boot.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskBootCommandTests)
{
public:

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

    TEST_METHOD (Boot_SetsTheNameTheGuestReadsAtBootTime_InTheCommittedImage)
    {
        // An Applesoft program, because that is what a booting DOS 3.3 RUNs.
        // The binary case is the one below, and it is a complaint rather than a
        // refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  put = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kImage, DiskRunnerFixtures::kHostFile, kDosProgram);
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedRealDisk (io);
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

        DiskRunnerFixtures::SeedRealDisk (io);

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
        vector<Byte>        original = DiskRunnerFixtures::OriginalImageBytes();

        DiskRunnerFixtures::SeedRealDisk (io);

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

    //  A REQUIRED OPERAND THAT IS NOT THERE ANSWERS WITH THAT COMMAND'S USAGE.
    //
    //  This asserted the sentence "no program named", which pinned wording
    //  rather than behavior and stood in the way of saying it better. What
    //  matters is that the command is refused, that the reader is shown the
    //  usage for the command they asked about rather than for all eight, and
    //  that the image is not touched.
    TEST_METHOD (Boot_WithNoProgramGiven_ShowsThatCommandsUsage_AndTouchesNothing)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = DiskRunnerFixtures::OriginalImageBytes();

        DiskRunnerFixtures::SeedRealDisk (io);

        result = runner.Run (DiskRunnerFixtures::MakeOptions (CommandLineOptions::DiskOptions::Command::Boot));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"a missing operand is a bad command line");
        Assert::IsTrue (result.usageShown,     L"and the reader is shown how to write it");

        //  boot's block, and not the whole page: the grammar of the command
        //  asked about is there, and a command nobody asked about is not.
        Assert::IsTrue (result.output.find ("CassoCli disk boot <image> <name>") != std::string::npos,
            L"boot's own grammar");
        Assert::IsTrue (result.output.find ("CassoCli disk sectorwrite") == std::string::npos,
            L"and not every other command's");

        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kImage, original);
    }

    TEST_METHOD (Boot_AVolumeWithNoOperatingSystemOnIt_IsRefusedInWordsRatherThanPatchedAnyway)
    {
        // A formatted data disk reserves the tracks DOS would occupy but has
        // nothing in them. Patching a name there would report success for a
        // disk that cannot boot at all.
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

        committed = io.files[DiskRunnerFixtures::kBlankImage];

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no operating system") != std::string::npos,
            L"and reports that is what is missing, rather than blaming the file");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kBlankImage, committed);
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

        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", DiskRunnerFixtures::kProImage);

        original = io.files[DiskRunnerFixtures::kProImage];

        result = runner.Run (MakeBootOptions (DiskRunnerFixtures::kProImage, kProProgram));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kProProgram) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("type SYS") != std::string::npos,
            L"and reports what this boot path does launch");

        DiskRunnerFixtures::AssertNamesNoPlatformCode (result.diagnostics);
        DiskRunnerFixtures::AssertImageMatches (io, DiskRunnerFixtures::kProImage, original);
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
        CommandLineOptions  options = DiskRunnerFixtures::MakePutOptions (DiskRunnerFixtures::kProImage, DiskRunnerFixtures::kHostFile, "CASSO.SYSTEM");
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", DiskRunnerFixtures::kProImage);
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
};
