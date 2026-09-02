#include "Pch.h"

#include "GuestSession.h"
#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "TextScreenScraper.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "MachineIdle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CatalogReproductionTest
//
//  End-to-end gate for the GUI's "DOS 3.3 boots to ] but CATALOG returns
//  I/O ERROR" failure: mounts the real DOS 3.3 master .dsk, runs the
//  slot-6 boot ROM through DOS init to the ] prompt, types CATALOG, and
//  verifies the directory listing was actually printed.
//
//  This used to fail because of two bugs in the Disk II controller:
//    1. Motor-off was applied immediately rather than after a ~1 second
//       spindown, so DOS RWTS lost rotational sync between commands.
//    2. The phase-stepper's "highest set bit" model didn't match the
//       real Disk II's adjacent-magnet pull, so multi-track seeks
//       drifted off target. CATALOG seeks 17 tracks from the boot
//       position to the VTOC; that drift caused address-field reads
//       to land on the wrong sector and RWTS gave up with I/O ERROR.
//
//  EVERY CASE HERE ASKS THE CHEAP QUESTIONS BEFORE IT STARTS A MACHINE. These
//  boots are long -- 200M cycles of ceiling apiece -- and a guest handed an
//  image it cannot read spends them executing data, one instruction look-back
//  written per illegal opcode. Twice on this project that has filled a disk and
//  killed the run. So the master is decoded through the DRIVE and enumerated
//  before it is mounted, and the container the drive ends up holding is asked
//  for its boot sector before the processor is pointed at the ROM.
//
////////////////////////////////////////////////////////////////////////////////






TEST_CLASS (CatalogReproductionTest)
{
public:

    static constexpr Word        kBootRomEntry        = 0xC600;
    static constexpr Word        kIntCxRomOff         = 0xC006;
    static constexpr int         kSlot6               = 6;
    static constexpr int         kDrive1              = 0;
    static constexpr int         kDrive2              = 1;

    // Ceilings, not targets. Both are spent through MachineIdle, which
    // stops as soon as the machine goes quiet; they only bound how long a
    // machine that never settles is allowed to spin before the assertions
    // below report what went wrong.
    //
    // DOS 3.3 cold boot from a real .dsk: boot ROM loads sector 0,
    // bootstrap chain loads more sectors from track 0..1, DOS init sets up
    // vectors, prints the banner + ], then loads and runs HELLO. Measured
    // at 12.2M cycles, so the ceiling is roughly 16x headroom.
    static constexpr uint64_t    kDos33ColdBootCycles = 200'000'000ULL;

    // CATALOG seeks to track 17 (VTOC), reads VTOC, walks the catalog
    // chain printing each entry. Measured at 3.6M; the other typed lines
    // this ceiling is shared with run from 1.2M (HOME, NEW, LIST) to 6.4M
    // (SAVE).
    static constexpr uint64_t    kCatalogCycles       = 60'000'000ULL;

    // The greeting the stock master runs on the way to its prompt. Its
    // presence in the catalog the DRIVE can reach is the cheap stand-in for
    // "this image will still boot".
    static constexpr const char *  kGreetingName = "HELLO";

    // What a case logs when it cannot find its image, before returning
    // without checking anything. Both places the resolver tries are listed,
    // with the two ways to fill one, so the line in the output is enough to
    // act on.
    static constexpr const char *  kDos33Missing  =
        "SKIPPED: DOS 3.3 System Master not found\n"
        "         Tried Disks/Apple/dos33-master.dsk under the working directory and its\n"
        "         parents, then %LOCALAPPDATA%\\Casso\\Disks\\DOS 3.3 System Master.dsk.\n"
        "         To get it, pick the DOS 3.3 row in Casso's Boot Disk or Insert Disk\n"
        "         picker once, which downloads it into that cache, or place a copy at\n"
        "         the repo path, which is gitignored. This case checked nothing.\n";

    static constexpr const char *  kProDosMissing =
        "SKIPPED: ProDOS Users Disk not found\n"
        "         Tried Disks/Apple/prodos-users.dsk under the working directory and its\n"
        "         parents, then %LOCALAPPDATA%\\Casso\\Disks\\ProDOS Users Disk.dsk.\n"
        "         To get it, pick the ProDOS row in Casso's Boot Disk or Insert Disk\n"
        "         picker once, which downloads it into that cache, or place a copy at\n"
        "         the repo path, which is not gitignored. This case checked nothing.\n";

    static std::vector<Byte> ReadFileOrEmpty (const std::filesystem::path & full)
    {
        std::error_code    ec;
        std::vector<Byte>  bytes;

        if (std::filesystem::exists (full, ec))
        {
            std::ifstream f (full, std::ios::binary);

            if (f)
            {
                bytes.assign ((std::istreambuf_iterator<char> (f)),
                              std::istreambuf_iterator<char> ());
            }
        }

        return bytes;
    }


    static std::vector<Byte> ReadDskOrEmpty (const std::string & relPath)
    {
        // Two places are tried, in order: `relPath` under the working
        // directory and each of its parents, up to ten levels (the same
        // walk BackwardsCompatTests and Pr3AuxClearTest use), then the
        // emulator's download cache, %LOCALAPPDATA%\Casso\Disks, under the
        // file name the emulator saves the image as. Returns an empty
        // vector when neither has it; the caller logs SKIPPED and returns
        // without checking anything, on a developer machine and on CI alike.
        //
        // The images are Apple-owned and not committed. To get one, pick the
        // DOS 3.3 or ProDOS row in the Boot Disk or Insert Disk picker once,
        // which downloads it into the cache, or place a copy at the repo
        // path. Only dos33-master.dsk is gitignored there.
        std::error_code        ec;
        std::filesystem::path  cursor  = std::filesystem::current_path (ec);
        std::vector<Byte>      bytes;
        bool                   walking = !ec;

        for (int i = 0; walking && bytes.empty() && i < 10; i++)
        {
            bytes = ReadFileOrEmpty (cursor / relPath);

            // Nothing read yet -- including the exists-but-unreadable case,
            // which keeps climbing rather than giving up at that level.
            if (bytes.empty())
            {
                if (!cursor.has_parent_path() || cursor == cursor.parent_path())
                {
                    walking = false;
                }
                else
                {
                    cursor = cursor.parent_path();
                }
            }
        }

        // The cache the emulator downloads into, and the same one
        // GuestSession::RequireDos33Master reads. Read-only: nothing here
        // downloads.
        if (bytes.empty())
        {
            const wchar_t *  cacheName = nullptr;

            if (relPath == "Disks/Apple/dos33-master.dsk")
            {
                cacheName = L"DOS 3.3 System Master.dsk";
            }
            else if (relPath == "Disks/Apple/prodos-users.dsk")
            {
                cacheName = L"ProDOS Users Disk.dsk";
            }

            if (cacheName != nullptr)
            {
                wchar_t * localAppData = nullptr;
                size_t    len          = 0;

                if (_wdupenv_s (&localAppData, &len, L"LOCALAPPDATA") == 0 &&
                    localAppData != nullptr)
                {
                    bytes = ReadFileOrEmpty (std::filesystem::path (localAppData) /
                                             L"Casso" / L"Disks" / cacheName);
                    free (localAppData);
                }
            }
        }

        return bytes;
    }


    //
    //  ------------------------------------------------------------------
    //  The cheap questions, asked before any machine starts.
    //  ------------------------------------------------------------------
    //

    // What a booting DOS is about to go looking for, read off the container
    // the DRIVE presents rather than out of the file. A disk whose catalog the
    // drive cannot reach fails here in milliseconds; left to the guest it fails
    // as a 6502 running through data with a look-back written for every illegal
    // opcode it hits.
    static void AssertTheGreetingIsReachable (const std::vector<Byte> & sectors)
    {
        VolumeListing  listing;
        bool           greeting = false;

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing),
                L"the disk must still enumerate as a DOS 3.3 volume through the drive");
        }

        for (const FileEntry & entry : listing.entries)
        {
            greeting = greeting || entry.name == kGreetingName;
        }

        Assert::IsTrue (greeting,
            L"and must still carry the greeting DOS runs on the way to the prompt, or "
            L"the boot below has nothing to reach");
    }


    static void AssertTheMasterIsStillADos33Disk (const std::vector<Byte> & raw)
    {
        AssertTheGreetingIsReachable (GuestSession::DecodeThroughTheDrive (raw));
    }


    // The same question of a disk this tool BUILT, which exists only as bit
    // streams and so has no sector buffer of its own to be compared against.
    static void AssertTheBuiltDiskIsStillADos33Disk (const DiskImage & image)
    {
        SectorDecodeReport  report;

        AssertTheGreetingIsReachable (
            GuestSession::DecodeThroughTheDrive (image, report));
    }


    TEST_METHOD (DOS33_CATALOG_DoesNotErrorOnMasterDisk)
    {
        HeadlessHost    host;
        EmulatorCore    core;
        HRESULT         hr            = S_OK;
        DiskImage     * img           = nullptr;
        bool            promptVisible = false;
        bool            ioError       = false;
        bool            volumeBanner  = false;



        std::vector<Byte>  raw = ReadDskOrEmpty ("Disks/Apple/dos33-master.dsk");
        if (raw.empty())
        {
            Logger::WriteMessage (kDos33Missing);
            return;
        }

        Assert::AreEqual (size_t (143360), raw.size(),
            L"DOS 3.3 master disk must be 143360 bytes");

        AssertTheMasterIsStillADos33Disk (raw);


        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        GuestSession::AssertTheDrivePresentsWhatWasMounted (
            *img, raw, L"the master this gate boots");

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kDos33ColdBootCycles);

        std::vector<std::string>  rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        for (const auto & r : rows)
        {
            if (r.find (']') != std::string::npos)
            {
                promptVisible = true;
                break;
            }
        }

        Assert::IsTrue (promptVisible,
            L"DOS 3.3 must reach the ] prompt within the cold-boot budget.");

        size_t  consumed = KeystrokeInjector::InjectLine (
            core, "CATALOG", kCatalogCycles);
        Assert::AreEqual (size_t (8), consumed,
            L"CATALOG + Return must be fully consumed by the keyboard latch");

        rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        for (const auto & r : rows)
        {
            if (r.find ("I/O ERROR") != std::string::npos)
            {
                ioError = true;
            }

            if (r.find ("DISK VOLUME") != std::string::npos)
            {
                volumeBanner = true;
            }
        }

        Assert::IsFalse (ioError,
            L"CATALOG must not return I/O ERROR.");

        Assert::IsTrue (volumeBanner,
            L"CATALOG must print a DISK VOLUME header.");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  DOS33_SAVE_RoundTripsToDsk (GH #89)
    //
    //  End-to-end gate for the .dsk write path: boot the real DOS 3.3
    //  master, enter a one-line program, SAVE it, then LOAD it back and
    //  LIST. Per #89 the write streams a corrupt data field through the
    //  LSS, so SAVE returns I/O ERROR and the program never round-trips.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (DOS33_SAVE_RoundTripsToDsk)
    {
        HeadlessHost    host;
        EmulatorCore    core;
        HRESULT         hr        = S_OK;
        DiskImage     * img       = nullptr;
        std::string     afterSave;
        std::string     afterList;



        std::vector<Byte>  raw = ReadDskOrEmpty ("Disks/Apple/dos33-master.dsk");
        if (raw.empty())
        {
            Logger::WriteMessage (kDos33Missing);
            return;
        }

        Assert::AreEqual (size_t (143360), raw.size(),
            L"DOS 3.3 master disk must be 143360 bytes");

        AssertTheMasterIsStillADos33Disk (raw);


        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        GuestSession::AssertTheDrivePresentsWhatWasMounted (
            *img, raw, L"the master this gate boots");

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kDos33ColdBootCycles);

        auto Screen = [&] () -> std::string
        {
            std::string               joined;

            std::vector<std::string>  scr = TextScreenScraper::Scrape40 (
                *core.bus, TextScreenScraper::kTextPage1);
            for (const auto & r : scr) { joined += r; joined += "\n"; }
            return joined;
        };

        Assert::IsTrue (Screen().find (']') != std::string::npos,
            L"DOS 3.3 must reach the ] prompt within the cold-boot budget.");

        // Enter a one-line program and SAVE it.
        KeystrokeInjector::InjectLine (core, "10 REM TEST PROGRAM", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "SAVE TEST", kCatalogCycles);
        afterSave = Screen();

        // Clear the program, LOAD it back, clear the screen, and LIST so
        // the recovered text is unambiguous (not a SAVE echo left onscreen).
        KeystrokeInjector::InjectLine (core, "NEW", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "LOAD TEST", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "HOME", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "LIST", kCatalogCycles);
        afterList = Screen();

        Assert::IsTrue (afterSave.find ("I/O ERROR") == std::string::npos &&
                        afterList.find ("I/O ERROR") == std::string::npos,
            L"SAVE/LOAD to a .dsk must not return I/O ERROR (GH #89).");

        // DOS 3.3 LIST re-formats the line with its own spacing
        // ("10  REM  TEST PROGRAM"), so match the REM comment body,
        // which round-trips verbatim. afterList was captured after a
        // HOME wipe, so this text can only be the LOADed program.
        Assert::IsTrue (afterList.find ("TEST PROGRAM") != std::string::npos,
            L"LOAD+LIST must recover the saved program text (write round-trip).");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  DOS33_SAVE_ToBuiltBlankWoz_SurvivesRemount
    //
    //  End-to-end gate for the created-blank-disk write path: boot the
    //  real DOS 3.3 master in drive 1, mount a default-built blank WOZ in
    //  drive 2, and have the real CPU SAVE a program to it, then LOAD and
    //  LIST it back -- proving a freshly created disk is immediately
    //  usable with no INIT step. Then eject the blank (the store's
    //  auto-flush serializes the written image through the same path the
    //  GUI uses), remount the serialized bytes fresh, and LOAD again --
    //  proving the write survives a full serialize/reload cycle.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (DOS33_SAVE_ToBuiltBlankWoz_SurvivesRemount)
    {
        HeadlessHost    host;
        EmulatorCore    core;
        HRESULT         hr        = S_OK;
        DiskImage     * masterImg = nullptr;
        DiskImage     * blankImg  = nullptr;
        BlankDiskSpec   spec;
        vector<Byte>    blankWoz;
        vector<Byte>    flushedWoz;
        std::string     afterSave;
        std::string     afterList;
        std::string     afterRemount;



        std::vector<Byte>  raw = ReadDskOrEmpty ("Disks/Apple/dos33-master.dsk");
        if (raw.empty())
        {
            Logger::WriteMessage (kDos33Missing);
            return;
        }

        Assert::AreEqual (size_t (143360), raw.size(),
            L"DOS 3.3 master disk must be 143360 bytes");

        AssertTheMasterIsStillADos33Disk (raw);


        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"master MountFromBytes must succeed");

        masterImg = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (masterImg, L"Mounted master DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, masterImg);

        GuestSession::AssertTheDrivePresentsWhatWasMounted (
            *masterImg, raw, L"the master this gate boots");

        // The blank under test is exactly what the create dialog produces
        // when the user accepts the defaults.
        hr = BlankDiskBuilder::Build (spec, BootPayload{}, blankWoz);
        AssertSucceeded (hr, L"Build must produce the default blank WOZ");

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive2,
            "new-blank.woz", DiskFormat::Woz, blankWoz);
        AssertSucceeded (hr, L"blank MountFromBytes must succeed");

        blankImg = core.diskStore->GetImage (kSlot6, kDrive2);
        Assert::IsNotNull (blankImg, L"Mounted blank DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive2, blankImg);

        // Capture the eject-time auto-flush in memory instead of letting it
        // touch the host filesystem.
        core.diskStore->SetFlushSink (
            [&flushedWoz] (const string & path, const vector<Byte> & bytes)
        {
            if (path == "new-blank.woz")
            {
                flushedWoz = bytes;
            }

            return S_OK;
        });

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kDos33ColdBootCycles);

        auto Screen = [&] () -> std::string
        {
            std::string               joined;

            std::vector<std::string>  scr = TextScreenScraper::Scrape40 (
                *core.bus, TextScreenScraper::kTextPage1);
            for (const auto & r : scr) { joined += r; joined += "\n"; }
            return joined;
        };

        Assert::IsTrue (Screen().find (']') != std::string::npos,
            L"DOS 3.3 must reach the ] prompt within the cold-boot budget.");

        // Enter a one-line program and SAVE it to the blank in drive 2.
        KeystrokeInjector::InjectLine (core, "10 REM BLANK DISK TEST", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "SAVE TEST,D2", kCatalogCycles);
        afterSave = Screen();

        KeystrokeInjector::InjectLine (core, "NEW", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "LOAD TEST,D2", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "HOME", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "LIST", kCatalogCycles);
        afterList = Screen();

        Assert::IsTrue (afterSave.find ("I/O ERROR") == std::string::npos &&
                        afterList.find ("I/O ERROR") == std::string::npos,
            L"SAVE/LOAD on a built blank WOZ must not return I/O ERROR.");

        Assert::IsTrue (afterList.find ("BLANK DISK TEST") != std::string::npos,
            L"LOAD+LIST from drive 2 must recover the program saved to the blank.");

        // Eject (auto-flush serializes the dirty image into the sink), then
        // remount the serialized bytes fresh and read the file again.
        core.diskController->SetExternalDisk (kDrive2, nullptr);
        core.diskStore->Eject (kSlot6, kDrive2);

        Assert::IsFalse (flushedWoz.empty(),
            L"Eject must flush the written blank through the sink.");

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive2,
            "reloaded-blank.woz", DiskFormat::Woz, flushedWoz);
        AssertSucceeded (hr, L"remount of the flushed bytes must succeed");

        blankImg = core.diskStore->GetImage (kSlot6, kDrive2);
        Assert::IsNotNull (blankImg, L"Remounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive2, blankImg);

        KeystrokeInjector::InjectLine (core, "NEW", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "LOAD TEST,D2", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "HOME", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "LIST", kCatalogCycles);
        afterRemount = Screen();

        Assert::IsTrue (afterRemount.find ("BLANK DISK TEST") != std::string::npos,
            L"The saved program must survive serialize + fresh remount.");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  DOS33_SAVE_RespectsImageWriteProtect
    //
    //  SC-005 gate: with the image write-protect flag set, a guest SAVE
    //  fails with WRITE PROTECTED; clearing the flag lets the same SAVE
    //  succeed and round-trip.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (DOS33_SAVE_RespectsImageWriteProtect)
    {
        HeadlessHost    host;
        EmulatorCore    core;
        HRESULT         hr        = S_OK;
        DiskImage     * masterImg = nullptr;
        DiskImage     * blankImg  = nullptr;
        BlankDiskSpec   spec;
        vector<Byte>    blankWoz;
        std::string     afterProtectedSave;
        std::string     afterWritableSave;



        std::vector<Byte>  raw = ReadDskOrEmpty ("Disks/Apple/dos33-master.dsk");
        if (raw.empty())
        {
            Logger::WriteMessage (kDos33Missing);
            return;
        }

        AssertTheMasterIsStillADos33Disk (raw);

        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"master MountFromBytes must succeed");

        masterImg = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (masterImg);
        core.diskController->SetExternalDisk (kDrive1, masterImg);

        GuestSession::AssertTheDrivePresentsWhatWasMounted (
            *masterImg, raw, L"the master this gate boots");

        hr = BlankDiskBuilder::Build (spec, BootPayload{}, blankWoz);
        AssertSucceeded (hr, L"blank WOZ must build");

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive2,
            "wp-blank.woz", DiskFormat::Woz, blankWoz);
        AssertSucceeded (hr, L"blank MountFromBytes must succeed");

        blankImg = core.diskStore->GetImage (kSlot6, kDrive2);
        Assert::IsNotNull (blankImg);
        core.diskController->SetExternalDisk (kDrive2, blankImg);

        // Protected first: the guest must refuse the write.
        blankImg->SetImageWriteProtected (true);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kDos33ColdBootCycles);

        auto Screen = [&] () -> std::string
        {
            std::string               joined;

            std::vector<std::string>  scr = TextScreenScraper::Scrape40 (
                *core.bus, TextScreenScraper::kTextPage1);
            for (const auto & r : scr) { joined += r; joined += "\n"; }
            return joined;
        };

        Assert::IsTrue (Screen().find (']') != std::string::npos,
            L"DOS 3.3 must reach the ] prompt within the cold-boot budget.");

        KeystrokeInjector::InjectLine (core, "10 REM WP TEST", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "SAVE TEST,D2", kCatalogCycles);
        afterProtectedSave = Screen();

        Assert::IsTrue (afterProtectedSave.find ("WRITE PROTECTED") != std::string::npos,
            L"a SAVE to a protected image must fail with WRITE PROTECTED (SC-005)");

        // Unprotect and repeat: the same SAVE must now succeed.
        blankImg->SetImageWriteProtected (false);

        KeystrokeInjector::InjectLine (core, "HOME", kCatalogCycles);
        KeystrokeInjector::InjectLine (core, "SAVE TEST,D2", kCatalogCycles);
        afterWritableSave = Screen();

        Assert::IsTrue (afterWritableSave.find ("WRITE PROTECTED") == std::string::npos &&
                        afterWritableSave.find ("I/O ERROR") == std::string::npos,
            L"clearing the flag must let the SAVE through");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  BootableDos33Woz_BootsToCleanPrompt
    //
    //  SC-006 gate: a bootable DOS 3.3 WOZ built from the real System
    //  Master must cold-boot to the Applesoft prompt with the HELLO
    //  greeting found and run -- no FILE NOT FOUND, no I/O ERROR.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BootableDos33Woz_BootsToCleanPrompt)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        HRESULT        hr  = S_OK;
        DiskImage    * img = nullptr;
        BlankDiskSpec  spec;
        BootPayload    payload;
        vector<Byte>   woz;
        std::string    screen;



        payload.dosMasterSectors = ReadDskOrEmpty ("Disks/Apple/dos33-master.dsk");
        if (payload.dosMasterSectors.empty())
        {
            Logger::WriteMessage (kDos33Missing);
            return;
        }

        spec.bootable = true;

        hr = BlankDiskBuilder::Build (spec, payload, woz);
        AssertSucceeded (hr, L"bootable DOS 3.3 WOZ must build");

        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "bootable-blank.woz", DiskFormat::Woz, woz);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        GuestSession::AssertTheDriveCanReadTheBootSector (
            *img, L"the disk this gate built");

        AssertTheBuiltDiskIsStillADos33Disk (*img);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kDos33ColdBootCycles);

        {
            std::vector<std::string>  rows = TextScreenScraper::Scrape40 (
                *core.bus, TextScreenScraper::kTextPage1);

            for (const auto & r : rows) { screen += r; screen += "\n"; }
        }

        Assert::IsTrue (screen.find (']') != std::string::npos,
            L"the built disk must boot DOS to the Applesoft prompt");
        Assert::IsTrue (screen.find ("FILE NOT FOUND") == std::string::npos,
            L"the HELLO greeting must be found and run");
        Assert::IsTrue (screen.find ("I/O ERROR") == std::string::npos,
            L"the boot must be error-free");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  BootableProDosWoz_BootsToBasicPrompt
    //
    //  SC-006 gate: a bootable ProDOS WOZ built from the real Users Disk
    //  must cold-boot through PRODOS into BASIC.SYSTEM's prompt.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BootableProDosWoz_BootsToBasicPrompt)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        HRESULT        hr  = S_OK;
        DiskImage    * img = nullptr;
        BlankDiskSpec  spec;
        BootPayload    payload;
        vector<Byte>   woz;
        std::string    screen;



        payload.proDosUsersDisk = ReadDskOrEmpty ("Disks/Apple/prodos-users.dsk");
        if (payload.proDosUsersDisk.empty())
        {
            Logger::WriteMessage (kProDosMissing);
            return;
        }

        spec.contents = BlankDiskContents::ProDos;
        spec.bootable = true;

        hr = BlankDiskBuilder::Build (spec, payload, woz);
        AssertSucceeded (hr, L"bootable ProDOS WOZ must build");

        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "bootable-prodos.woz", DiskFormat::Woz, woz);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        // The ProDOS half stops at the boot sector: this file's only
        // filesystem reader is the DOS 3.3 one, and pulling ProDosVolume in
        // for one assertion would put a second reader in a file whose subject
        // is the drive.
        GuestSession::AssertTheDriveCanReadTheBootSector (
            *img, L"the ProDOS disk this gate built");

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        MachineIdle::RunUntilIdle (core, kDos33ColdBootCycles);

        {
            std::vector<std::string>  rows = TextScreenScraper::Scrape40 (
                *core.bus, TextScreenScraper::kTextPage1);

            for (const auto & r : rows) { screen += r; screen += "\n"; }
        }

        Assert::IsTrue (screen.find (']') != std::string::npos,
            L"the built disk must boot ProDOS into BASIC.SYSTEM's prompt");
        Assert::IsTrue (screen.find ("UNABLE") == std::string::npos &&
                        screen.find ("ERROR") == std::string::npos,
            L"the ProDOS boot must be error-free");
    }
};

