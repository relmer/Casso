#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TextScreenScraper.h"
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
//  THE DATA IS REQUIRED, NOT OPTIONAL. The stock DOS 3.3 master is fetched
//  rather than committed and does not travel between machines, so a case
//  needing it can find it absent. The answer to that is to FAIL. A test that
//  cannot reach its data and passes anyway makes "N passed" stop meaning N
//  things were checked, and a guest-visible gate that never started a guest is
//  the emptiest version of that.
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

    //  Where the stock master lives on a developer machine. It is fetched by
    //  the GUI rather than committed, so this is a cache path, not a fixture.
    static constexpr const wchar_t *  kMasterCacheName = L"DOS 3.3 System Master.dsk";
    static constexpr const char *     kMasterRepoPath  = "Disks/Apple/dos33-master.dsk";

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

    static constexpr size_t  kImageBytes = 143360;

    static constexpr size_t  kPayloadBytes  = 512;
    static constexpr Word    kLoadAddress   = 0x6000;
    static constexpr size_t  kPayloadStride = 7;
    static constexpr size_t  kPayloadSeed   = 0x21;
    static constexpr size_t  kByteMask      = 0xFF;

    //  Ceilings, not targets. Both are spent through MachineIdle, which stops
    //  as soon as the guest settles; they bound how long a machine reading an
    //  image it cannot make sense of is allowed to spin.
    static constexpr uint64_t  kBootCycles = 40'000'000ULL;
    static constexpr uint64_t  kLineCycles = 20'000'000ULL;

    //  How many screenfuls a command is allowed to print before this gives up.
    //  DOS 3.3's catalog pager needs one press on the master; the ProDOS
    //  fixture's startup program is a five-page slideshow.
    static constexpr int  kMaxPages = 12;

    //  A bare prompt row is the prompt glyph plus, on the frames it is lit, the
    //  cursor.
    static constexpr size_t  kPromptRowLength = 2;
    static constexpr char    kPromptGlyph     = ']';

    static constexpr Word  kBootRomEntry = 0xC600;
    static constexpr Word  kIntCxRomOff  = 0xC006;

    static constexpr int  kSlot6  = 6;
    static constexpr int  kDrive1 = 0;


    //
    //  ------------------------------------------------------------------
    //  Material.
    //  ------------------------------------------------------------------
    //

    static std::vector<Byte> ReadFileOrEmpty (const std::filesystem::path & full)
    {
        std::error_code    ec;
        std::vector<Byte>  bytes;
        bool               present = std::filesystem::exists (full, ec);

        if (present)
        {
            std::ifstream  file (full, std::ios::binary);
            bool           opened = file.good();

            if (opened)
            {
                bytes.assign ((std::istreambuf_iterator<char> (file)),
                              std::istreambuf_iterator<char> ());
            }
        }

        return bytes;
    }


    //  The stock DOS 3.3 master, or a failed test.
    //
    //  NOT A SKIP, deliberately. The existing boot harness beside this one does
    //  skip, because it predates the rule; a case whose whole subject is what a
    //  guest does cannot borrow that, since skipping leaves the suite reporting
    //  a pass for a machine that was never started.
    static std::vector<Byte> RequireDos33Master()
    {
        std::error_code        ec;
        std::filesystem::path  cursor    = std::filesystem::current_path (ec);
        std::vector<Byte>      bytes;
        wchar_t              * cacheRoot = nullptr;
        size_t                 len       = 0;
        bool                   walking   = !ec;
        bool                   hasCache  = false;
        int                    level     = 0;

        for (level = 0; walking && bytes.empty() && level < 10; level++)
        {
            bytes = ReadFileOrEmpty (cursor / kMasterRepoPath);

            if (bytes.empty())
            {
                bool  atRoot = !cursor.has_parent_path() || cursor == cursor.parent_path();

                if (atRoot)
                {
                    walking = false;
                }
                else
                {
                    cursor = cursor.parent_path();
                }
            }
        }

        if (bytes.empty())
        {
            hasCache = _wdupenv_s (&cacheRoot, &len, L"LOCALAPPDATA") == 0 && cacheRoot != nullptr;
        }

        if (hasCache)
        {
            bytes = ReadFileOrEmpty (std::filesystem::path (cacheRoot) /
                                     L"Casso" / L"Disks" / kMasterCacheName);
            free (cacheRoot);
        }

        Assert::IsFalse (bytes.empty(),
            L"the stock DOS 3.3 master is REQUIRED by this gate and was not found, either as "
            L"Disks/Apple/dos33-master.dsk in a parent of the working directory or as "
            L"%LOCALAPPDATA%\\Casso\\Disks\\DOS 3.3 System Master.dsk. This case fails rather "
            L"than skipping: a guest-visible gate that never started a guest has checked "
            L"nothing, and a skip is indistinguishable in the output from a case that ran");

        Assert::AreEqual (kImageBytes, bytes.size(),
            L"and it must be a whole 5.25-inch sector image");

        return bytes;
    }


    static std::vector<Byte> ProDosFixtureBytes()
    {
        FixtureProvider    fixtures;
        std::vector<Byte>  bytes;

        AssertSucceeded (fixtures.OpenFixture (kProDosFixture, bytes));

        Assert::AreEqual (kImageBytes, bytes.size(),
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
    //  Driving a real 6502.
    //  ------------------------------------------------------------------
    //

    static std::string TrimTrailingBlanks (const std::string & row)
    {
        std::string  trimmed = row;
        size_t       end     = trimmed.find_last_not_of (' ');

        if (end == std::string::npos)
        {
            return std::string();
        }

        trimmed.erase (end + 1);

        return trimmed;
    }


    //  True when the bottom-most non-blank row is a bare BASIC prompt.
    //
    //  THE PROMPT GLYPH IS THE LOAD-BEARING PART. DOS 3.3's catalog pager also
    //  parks the machine on a one-glyph row -- the cursor by itself -- with the
    //  drive stopped and the screen still, which is everything MachineIdle
    //  looks at. A caller that stops there takes a half-printed listing for a
    //  finished one and types its next command into the pager, where it is
    //  swallowed as the keypress the pager was waiting for. Measured: BLOAD
    //  typed at that point never runs, and the test still sees a plausible
    //  screen.
    //
    //  The two clauses are not equally load-bearing on the disks reachable
    //  here, and that was measured rather than assumed. Where the pager stops
    //  on this master the bottom row is the last catalog entry, which the
    //  length clause rejects on its own -- so dropping the glyph clause changes
    //  no verdict in this suite. It is kept because the glyph is the RULE and
    //  the shortness is a proxy for it: a listing whose last visible line is
    //  the cursor alone is a state the same pager reaches, and one the length
    //  clause would call a prompt.
    static bool IsAtBarePrompt (EmulatorCore & core)
    {
        std::vector<std::string>  rows  = TextScreenScraper::Scrape (core);
        std::string               row;
        size_t                    index = 0;

        for (index = rows.size(); index > 0; index--)
        {
            row = TrimTrailingBlanks (rows[index - 1]);

            if (!row.empty())
            {
                return row.size() <= kPromptRowLength && row[0] == kPromptGlyph;
            }
        }

        return false;
    }


    static void CollectRows (EmulatorCore & core, std::vector<std::string> & outRows)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape (core);

        for (const std::string & row : rows)
        {
            std::string  trimmed = TrimTrailingBlanks (row);

            if (!trimmed.empty())
            {
                outRows.push_back (trimmed);
            }
        }
    }


    //  Presses Return until the guest is back at a bare prompt, collecting
    //  every row it displayed on the way. Rows can repeat across screenfuls,
    //  which is why callers ask what a matching row SAYS rather than how many
    //  there are.
    static bool TryPageToPrompt (EmulatorCore & core, std::vector<std::string> & outRows)
    {
        bool  atPrompt = false;
        int   page     = 0;

        for (page = 0; page < kMaxPages && !atPrompt; page++)
        {
            CollectRows (core, outRows);

            atPrompt = IsAtBarePrompt (core);

            if (!atPrompt)
            {
                KeystrokeInjector::InjectLine (core, "", kLineCycles);
            }
        }

        return atPrompt;
    }


    //  Types a line and hands back everything the guest printed in answer.
    static std::vector<std::string> TypeAndCollect (EmulatorCore & core, const std::string & line)
    {
        std::vector<std::string>  rows;
        bool                      atPrompt = false;

        KeystrokeInjector::InjectLine (core, line, kLineCycles);

        atPrompt = TryPageToPrompt (core, rows);

        Assert::IsTrue (atPrompt,
            L"the guest must come back to its prompt after the typed line");

        Assert::IsTrue (rows.size() > 0,
            L"and must have printed something, or the assertions below compare nothing");

        return rows;
    }


    static bool AnyRowIs (const std::vector<std::string> & rows, const std::string & wanted)
    {
        return std::find (rows.begin(), rows.end(), wanted) != rows.end();
    }


    static bool AnyRowContains (const std::vector<std::string> & rows, const std::string & needle)
    {
        for (const std::string & row : rows)
        {
            if (row.find (needle) != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }


    //  Every row mentioning `needle` must be `expected`, and there must be one.
    //
    //  ASKING ONLY WHETHER THE SCREEN CONTAINS THE NAME IS NOT ENOUGH, and this
    //  feature has already shipped three weak tests of exactly that shape. A
    //  bare search for PROG is satisfied by the echo of the command that placed
    //  it, by a neighboring file whose name merely begins the same way, and --
    //  the case this gate exists for -- by a catalog line carrying the wrong
    //  type or the wrong size. Demanding the whole rendered row, and that no
    //  OTHER row mentions the name, answers all three.
    static void AssertTheOnlyRowsMentioning (const std::vector<std::string>  & rows,
                                             const std::string               & needle,
                                             const std::string               & expected)
    {
        size_t  matching = 0;

        for (const std::string & row : rows)
        {
            if (row.find (needle) == std::string::npos)
            {
                continue;
            }

            Assert::AreEqual (expected, row,
                L"every row the guest printed about this file must be the row expected");

            matching++;
        }

        Assert::IsTrue (matching > 0,
            L"and the guest must have printed at least one -- a listing that never mentions "
            L"the file satisfies a per-row rule vacuously");
    }


    static void MountAndBoot (HeadlessHost             & host,
                              EmulatorCore             & core,
                              const std::vector<Byte>  & bytes)
    {
        DiskImage *  image = nullptr;

        AssertSucceeded (host.BuildApple2eWithDisk2 (core), L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        AssertSucceeded (core.diskStore->MountFromBytes (kSlot6, kDrive1, "gate.dsk",
                                                         DiskFormat::Dsk, bytes),
            L"MountFromBytes must succeed");

        image = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (image, L"the mounted image must be present");
        core.diskController->SetExternalDisk (kDrive1, image);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kBootCycles);
    }


    //  Boots and pages through whatever the disk's own startup program prints,
    //  leaving the guest at a prompt that will accept a command.
    static void BootToPrompt (HeadlessHost             & host,
                              EmulatorCore             & core,
                              const std::vector<Byte>  & bytes)
    {
        std::vector<std::string>  rows;
        bool                      atPrompt = false;

        MountAndBoot (host, core, bytes);

        atPrompt = TryPageToPrompt (core, rows);

        Assert::IsTrue (atPrompt,
            L"the image must boot its operating system through to a BASIC prompt");

        Assert::IsFalse (AnyRowContains (rows, "I/O ERROR"),
            L"and must not have hit a read error on the way");
    }


    //  What the guest holds where the placement said the file loads.
    static std::vector<Byte> GuestBytesAt (EmulatorCore & core, Word address, size_t count)
    {
        std::vector<Byte>  bytes (count, 0);
        size_t             i = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = core.bus->ReadByte ((Word) (address + i));
        }

        return bytes;
    }


    //  Types the load command and asserts the bytes arrived where they were
    //  supposed to.
    //
    //  The BEFORE reading is not a formality. Without it the whole assertion is
    //  satisfied by a payload that happened to be sitting there already, and on
    //  a machine that boots the same disk every time "happened to be sitting
    //  there" is not a remote possibility.
    static void AssertBloadLandsThePayload (EmulatorCore & core, const std::string & command)
    {
        std::vector<Byte>         before = GuestBytesAt (core, kLoadAddress, kPayloadBytes);
        std::vector<std::string>  rows;
        std::vector<Byte>         after;

        Assert::IsFalse (before == MakePayload(),
            L"the payload must not already be at the load address, or the load proves nothing");

        rows  = TypeAndCollect (core, command);
        after = GuestBytesAt (core, kLoadAddress, kPayloadBytes);

        Assert::IsFalse (AnyRowContains (rows, "ERROR"),
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
        std::vector<Byte>         master  = RequireDos33Master();
        std::vector<Byte>         written = PutBinaryOnto (master, "B", put);
        std::vector<std::string>  rows;



        Assert::AreEqual (DiskCommandRunner::kClean, put.exitStatus,
            L"the placement must succeed");
        Assert::AreEqual (std::string(), put.diagnostics,
            L"with nothing to complain about");

        AssertTheWrittenImageIsStillADisk (written, master, kMasterCarries, false);

        BootToPrompt (host, core, written);

        rows = TypeAndCollect (core, "CATALOG");

        Assert::IsTrue (AnyRowIs (rows, kMasterHeading),
            L"the guest must have printed its own catalog heading, or nothing was cataloged");

        AssertTheOnlyRowsMentioning (rows, kPlacedName, kDos33Row);

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

        BootToPrompt (host, core, written);

        //  EIGHTY COLUMNS FIRST, and it is not cosmetic. BASIC.SYSTEM's short
        //  listing stops at the block count, so the auxiliary type -- the whole
        //  ProDOS half of this gate -- is simply not on the screen at forty
        //  columns. The long listing carries it, and needs the width to print
        //  it. Measured: at forty columns the row ends at `1  <NO DATE>`.
        TypeAndCollect (core, "PR#3");

        rows = TypeAndCollect (core, "CATALOG");

        Assert::IsTrue (AnyRowContains (rows, kProDosHeading),
            L"the guest must have named the volume it is listing");

        AssertTheOnlyRowsMentioning (rows, kPlacedName, kProDosRow);

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
        std::vector<Byte>         master   = RequireDos33Master();
        std::vector<std::string>  rows;
        size_t                    newlines = 0;
        size_t                    at       = 0;



        //  The guest's own account of the file first. Our reader would only
        //  restate the type byte it parsed; DOS drawing a lock marker beside
        //  its own greeting is independent evidence that $82 means locked.
        BootToPrompt (host, core, master);

        rows = TypeAndCollect (core, "CATALOG");

        AssertTheOnlyRowsMentioning (rows, kLockedFile, kLockedRow);

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
