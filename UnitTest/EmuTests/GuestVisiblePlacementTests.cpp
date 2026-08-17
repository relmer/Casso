#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/ProDosVolume.h"
#include "Devices/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisiblePlacementTests
//
//  What a real 6502 makes of a file this tool placed. Every other test of the
//  write path settles its questions against our own reader, and our own reader
//  is exactly what cannot answer this one: a file written through a wrong
//  understanding and read back through the same wrong understanding comes back
//  perfectly.
//
//  So the witnesses here are the guest's own -- DOS 3.3's CATALOG and BLOAD,
//  ProDOS's CAT and BLOAD -- run by the emulated processor off the image the
//  command line produced, through the whole path a user travels.
//
//  THE DATA IS REQUIRED, NOT OPTIONAL, and `GuestSession` enforces that: a case
//  that cannot reach the stock master FAILS rather than skipping, because "N
//  passed" must mean N things were checked and a guest-visible gate that never
//  started a guest is the emptiest version of that.
//
//  A WRONG IMAGE MAKES THE PROCESSOR EXECUTE GARBAGE, which is slow, noisy and
//  says nothing useful about what went wrong. Every case therefore asks the
//  cheap questions first -- does the written container still decode through the
//  drive, does it still carry what it carried, does the placed file read back
//  from it -- and only then starts a machine.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (GuestVisiblePlacementTests)
{
public:

    //  The ProDOS half runs against a committed fixture and so has no cache
    //  dependency. /APPLESOFT is the one that boots PRODOS into BASIC.SYSTEM,
    //  which is what makes CAT and BLOAD reachable at all -- side A launches
    //  MERLIN.SYSTEM instead and never offers a BASIC prompt.
    static constexpr const char *  kProDosFixture = "Disks/Merlin-proProdos2.33-b.dsk";

    static constexpr const char *  kImagePath = "C:\\disks\\gate.dsk";
    static constexpr const char *  kHostFile  = "C:\\build\\prog.bin";

    static constexpr const char *  kPlacedName = "PROG";

    //  What each guest prints for the placed file, measured by running it
    //  rather than taken from the prose.
    //
    //  FOUR SECTORS, NOT TWO. DOS 3.3 keeps a binary's load address and length
    //  INSIDE the file, so a 512-byte payload is 516 stored bytes -- three data
    //  sectors -- and the track/sector list is the fourth. `B 002` is what a
    //  payload of 252 bytes or fewer produces.
    //
    //  The ProDOS row is the long listing's, which is the only one that prints
    //  the auxiliary type. `A=$6000` is the guest reading back the load address
    //  the placement recorded.
    static constexpr const char *  kDos33Row  = " B 004 PROG";
    static constexpr const char *  kProDosRow =
        " PROG            BIN       1  <NO DATE>        <NO DATE>            512 A=$6000";

    //  A file each volume already carries, for the "still carries what it
    //  carried" half of the pre-boot check.
    static constexpr const char *  kMasterCarries = "HELLO";
    static constexpr const char *  kProDosCarries = "BASIC.SYSTEM";

    //  The stock master's greeting: Applesoft and locked, which is type $82 --
    //  and the row below is the guest saying so itself, rather than our own
    //  reader restating the type byte it just parsed.
    static constexpr const char *  kLockedFile = "HELLO";
    static constexpr const char *  kLockedRow  = "*A 003 HELLO";

    //  What the guest heads its own catalog with. Asserted so that a case
    //  collecting nothing at all cannot pass the rows test by vacuity.
    static constexpr const char *  kMasterHeading = "DISK VOLUME 254";
    static constexpr const char *  kProDosHeading = "/APPLESOFT";

    static constexpr size_t  kPayloadBytes  = 512;
    static constexpr Word    kLoadAddress   = 0x6000;
    static constexpr size_t  kPayloadStride = 7;
    static constexpr size_t  kPayloadSeed   = 0x21;
    static constexpr size_t  kByteMask      = 0xFF;


    //
    //  ------------------------------------------------------------------
    //  Material.
    //  ------------------------------------------------------------------
    //

    static std::vector<Byte> ProDosFixtureBytes()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (kProDosFixture, bytes));

        Assert::AreEqual (GuestSession::kImageBytes, bytes.size(),
            L"the ProDOS fixture must be a whole 5.25-inch sector image");

        return bytes;
    }


    static std::vector<Byte> MakePayload()
    {
        std::vector<Byte>  bytes (kPayloadBytes, 0);
        size_t             i = 0;

        for (i = 0; i < kPayloadBytes; i++)
        {
            bytes[i] = (Byte) ((i * kPayloadStride + kPayloadSeed) & kByteMask);
        }

        return bytes;
    }


    //
    //  ------------------------------------------------------------------
    //  Placement, through the command path a user actually travels.
    //  ------------------------------------------------------------------
    //

    static CommandLineOptions MakePutOptions (const char * onDiskName, const char * typeName)
    {
        CommandLineOptions  options;

        options.subcommand          = CommandLineOptions::Subcommand::Disk;
        options.disk.verb           = CommandLineOptions::DiskOptions::Verb::Put;
        options.disk.imagePath      = kImagePath;
        options.disk.hostFile       = kHostFile;
        options.disk.path           = onDiskName;
        options.disk.typeName       = typeName;
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        return options;
    }


    static void SeedForPut (FakeDiskFileIo & io, const std::vector<Byte> & image)
    {
        io.files[kImagePath]  = image;
        io.stamps[kImagePath] = FileStamp { image.size(), 100 };
        io.files[kHostFile]   = MakePayload();
        io.stamps[kHostFile]  = FileStamp { kPayloadBytes, 100 };
    }


    //  Runs `disk put` over `image` and hands back what was committed.
    static std::vector<Byte> PutBinaryOnto (const std::vector<Byte>  & image,
                                            const char               * typeName,
                                            DiskCommandResult        & outResult)
    {
        FakeDiskFileIo  io;

        SeedForPut (io, image);

        {
            DiskCommandRunner  runner (io);

            outResult = runner.Run (MakePutOptions (kPlacedName, typeName));
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"a commit leaves no temporary behind");

        return io.files[kImagePath];
    }


    //
    //  ------------------------------------------------------------------
    //  The cheap questions, asked before any machine starts.
    //  ------------------------------------------------------------------
    //

    //  The written container decoded the way the DRIVE sees it: laid down as
    //  physical nibbles and read back with the hardware interleave, rather than
    //  through the path that wrote it.
    static std::vector<Byte> DecodeThroughTheDrive (const std::vector<Byte> & bytes)
    {
        DiskImage           image;
        SectorDecodeReport  report;
        std::vector<Byte>   sectors;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (bytes, image));
        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, sectors, report));

        return sectors;
    }


    //  Everything a correct placement implies that costs microseconds to ask.
    //  Putting it ahead of the boot means a mis-written image is reported here,
    //  in milliseconds, rather than by a 6502 executing whatever it managed to
    //  load off it.
    static void AssertTheWrittenImageIsStillADisk (const std::vector<Byte>  & written,
                                                   const std::vector<Byte>  & before,
                                                   const char               * carries,
                                                   bool                       isProDos)
    {
        std::vector<Byte>  sectors      = DecodeThroughTheDrive (written);
        VolumeListing      listing;
        FilePayload        placed;
        bool               foundCarried = false;
        bool               foundPlaced  = false;

        Assert::AreEqual (before.size(), written.size(),
            L"a placement does not change how big the image is");

        Assert::IsFalse (written == before,
            L"and it must actually have changed the image, or nothing was placed");

        if (isProDos)
        {
            ProDosVolume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse (kPlacedName), placed));
        }
        else
        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (volume.Read (FilePath::Parse (kPlacedName), placed));
        }

        Assert::IsTrue (listing.entries.size() > 0,
            L"the written image must still enumerate as a volume");

        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == carries)
            {
                foundCarried = true;
            }

            if (entry.name == kPlacedName)
            {
                foundPlaced = true;
            }
        }

        Assert::IsTrue (foundCarried, L"and still carry what it carried before the placement");
        Assert::IsTrue (foundPlaced,  L"and now carry the file that was placed");

        Assert::IsTrue (placed.bytes == MakePayload(),
            L"which must read back byte for byte off the container the drive sees");

        Assert::IsTrue (placed.hasLoadAddress, L"with a load address recorded");
        Assert::AreEqual (kLoadAddress, placed.loadAddress);
    }


    //
    //  ------------------------------------------------------------------
    //  Driving a real 6502. The harness itself is GuestSession, shared with
    //  the boot gate beside this one: the paging rules below its surface are
    //  the ones a second copy would get subtly wrong.
    //  ------------------------------------------------------------------
    //


    //  Types the load command and asserts the bytes arrived where they were
    //  supposed to.
    //
    //  The BEFORE reading is not a formality. Without it the whole assertion is
    //  satisfied by a payload that happened to be sitting there already, and on
    //  a machine that boots the same disk every time "happened to be sitting
    //  there" is not a remote possibility.
    static void AssertBloadLandsThePayload (EmulatorCore & core, const std::string & command)
    {
        std::vector<Byte>         before = GuestSession::GuestBytesAt (core, kLoadAddress, kPayloadBytes);
        std::vector<std::string>  rows;
        std::vector<Byte>         after;

        Assert::IsFalse (before == MakePayload(),
            L"the payload must not already be at the load address, or the load proves nothing");

        rows  = GuestSession::TypeAndCollect (core, command);
        after = GuestSession::GuestBytesAt (core, kLoadAddress, kPayloadBytes);

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"),
            L"the guest must not report an error loading the file it just listed");

        Assert::IsTrue (after == MakePayload(),
            L"and the file's bytes must be at the address the placement recorded, all of them");
    }


    //
    //  ------------------------------------------------------------------
    //  The gate.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (Dos33_APlacedBinary_IsCatalogedByTheGuestAndBloadsToItsAddress)
    {
        HeadlessHost              host;
        EmulatorCore              core;
        DiskCommandResult         put;
        std::vector<Byte>         master  = GuestSession::RequireDos33Master();
        std::vector<Byte>         written = PutBinaryOnto (master, "B", put);
        std::vector<std::string>  rows;



        Assert::AreEqual (DiskCommandRunner::kClean, put.exitStatus,
            L"the placement must succeed");
        Assert::AreEqual (std::string(), put.diagnostics,
            L"with nothing to complain about");

        AssertTheWrittenImageIsStillADisk (written, master, kMasterCarries, false);

        GuestSession::BootToPrompt (host, core, written);

        rows = GuestSession::TypeAndCollect (core, "CATALOG");

        Assert::IsTrue (GuestSession::AnyRowIs (rows, kMasterHeading),
            L"the guest must have printed its own catalog heading, or nothing was cataloged");

        GuestSession::AssertTheOnlyRowsMentioning (rows, kPlacedName, kDos33Row);

        AssertBloadLandsThePayload (core, "BLOAD PROG");
    }

    TEST_METHOD (ProDos_APlacedBinary_IsCatalogedAsBinByTheGuestAndBloadsToItsAuxAddress)
    {
        HeadlessHost              host;
        EmulatorCore              core;
        DiskCommandResult         put;
        std::vector<Byte>         fixture = ProDosFixtureBytes();
        std::vector<Byte>         written = PutBinaryOnto (fixture, "BIN", put);
        std::vector<std::string>  rows;



        Assert::AreEqual (DiskCommandRunner::kClean, put.exitStatus,
            L"the placement must succeed");
        Assert::AreEqual (std::string(), put.diagnostics,
            L"with nothing to complain about");

        AssertTheWrittenImageIsStillADisk (written, fixture, kProDosCarries, true);

        GuestSession::BootToPrompt (host, core, written);

        //  EIGHTY COLUMNS FIRST, and it is not cosmetic. BASIC.SYSTEM's short
        //  listing stops at the block count, so the auxiliary type -- the whole
        //  ProDOS half of this gate -- is simply not on the screen at forty
        //  columns. The long listing carries it, and needs the width to print
        //  it. Measured: at forty columns the row ends at `1  <NO DATE>`.
        GuestSession::TypeAndCollect (core, "PR#3");

        rows = GuestSession::TypeAndCollect (core, "CATALOG");

        Assert::IsTrue (GuestSession::AnyRowContains (rows, kProDosHeading),
            L"the guest must have named the volume it is listing");

        GuestSession::AssertTheOnlyRowsMentioning (rows, kPlacedName, kProDosRow);

        //  BLOAD with no address deliberately: ProDOS then loads the file where
        //  its AUXILIARY TYPE says, which is the field a binary's load address
        //  is stored in. Naming the address here would prove only that the
        //  guest can follow an instruction it was given.
        AssertBloadLandsThePayload (core, "BLOAD PROG");
    }

    TEST_METHOD (Dos33_TheStockMastersLockedGreeting_IsRefused_AndTheGuestAgreesItIsLocked)
    {
        HeadlessHost              host;
        EmulatorCore              core;
        FakeDiskFileIo            io;
        DiskCommandResult         put;
        std::vector<Byte>         master   = GuestSession::RequireDos33Master();
        std::vector<std::string>  rows;
        size_t                    newlines = 0;
        size_t                    at       = 0;



        //  The guest's own account of the file first. Our reader would only
        //  restate the type byte it parsed; DOS drawing a lock marker beside
        //  its own greeting is independent evidence that $82 means locked.
        GuestSession::BootToPrompt (host, core, master);

        rows = GuestSession::TypeAndCollect (core, "CATALOG");

        GuestSession::AssertTheOnlyRowsMentioning (rows, kLockedFile, kLockedRow);

        //  And now the refusal, over the same bytes.
        SeedForPut (io, master);

        {
            DiskCommandRunner  runner (io);

            put = runner.Run (MakePutOptions (kLockedFile, "B"));
        }

        Assert::AreEqual (DiskCommandRunner::kNoOutput, put.exitStatus,
            L"placing over a locked file must produce no output");

        Assert::IsTrue (put.diagnostics.find ("is locked on this volume") != std::string::npos,
            L"and must say the file is locked -- not merely that something went wrong");

        Assert::IsTrue (put.diagnostics.find (kLockedFile) != std::string::npos,
            L"naming the file");
        Assert::IsTrue (put.diagnostics.find (kImagePath) != std::string::npos,
            L"and the image");

        Assert::IsTrue (put.diagnostics.find ("0x") == std::string::npos,
            L"in words rather than in a platform code");

        while ((at = put.diagnostics.find ('\n', at)) != std::string::npos)
        {
            newlines++;
            at++;
        }

        Assert::AreEqual (size_t (1), newlines,
            L"and as ONE reason: a second line means the refusal reported its cause and then "
            L"carried on far enough to trip over something else");

        Assert::IsTrue (io.files[kImagePath] == master,
            L"the image must be byte-for-byte what it was");

        Assert::IsTrue (io.HasNoTemporaryFiles(),
            L"with no temporary left beside it");

        Assert::AreEqual (size_t (2), io.files.size(),
            L"and nothing left under a name the temporary sweep does not know");
    }
};
