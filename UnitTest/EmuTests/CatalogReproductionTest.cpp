#include "Pch.h"

#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "TextScreenScraper.h"
#include "Devices/Disk/BlankDiskBuilder.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Disk/NibblizationLayer.h"

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
////////////////////////////////////////////////////////////////////////////////






TEST_CLASS (CatalogReproductionTest)
{
public:

    static constexpr Word        kBootRomEntry        = 0xC600;
    static constexpr Word        kIntCxRomOff         = 0xC006;
    static constexpr int         kSlot6               = 6;
    static constexpr int         kDrive1              = 0;
    static constexpr int         kDrive2              = 1;

    // DOS 3.3 cold boot from a real .dsk: boot ROM loads sector 0,
    // bootstrap chain loads more sectors from track 0..1, DOS init
    // sets up vectors, prints "APPLE //e" banner + ]. Empirically
    // this lands well under 200M cycles in debug builds.
    static constexpr uint64_t    kDos33ColdBootCycles = 200'000'000ULL;

    // CATALOG seeks to track 17 (VTOC), reads VTOC, walks the catalog
    // chain printing each entry. Real-disk timing in cycle units.
    static constexpr uint64_t    kCatalogCycles       = 60'000'000ULL;

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
        // Walk up from the current working directory to find a sibling
        // of `Machines/` containing the requested file. Mirrors the
        // resolver in BackwardsCompatTests / Pr3AuxClearTest so the
        // test stays filesystem-portable. Returns an empty vector if
        // the file doesn't exist (CI runners don't have the DOS 3.3
        // master disk in the repo); the caller treats that as "skip".
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

        // The repo copy is optional; a developer machine usually has the
        // stock master in the GUI's asset-download cache instead, filed
        // under its catalog display name. Read-only fallback, still
        // skip-if-missing on machines with neither.
        if (bytes.empty() && relPath == "Disks/Apple/dos33-master.dsk")
        {
            wchar_t * localAppData = nullptr;
            size_t    len          = 0;

            if (_wdupenv_s (&localAppData, &len, L"LOCALAPPDATA") == 0 &&
                localAppData != nullptr)
            {
                bytes = ReadFileOrEmpty (std::filesystem::path (localAppData) /
                                         L"Casso" / L"Disks" /
                                         L"DOS 3.3 System Master.dsk");
                free (localAppData);
            }
        }

        return bytes;
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
            Logger::WriteMessage ("SKIPPED: Disks/Apple/dos33-master.dsk not "
                                  "available in this checkout (typical for CI "
                                  "runners). Test passes locally where the "
                                  "disk image is present.\n");
            return;
        }

        Assert::AreEqual (size_t (143360), raw.size(),
            L"DOS 3.3 master disk must be 143360 bytes");


        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        core.RunCycles (kDos33ColdBootCycles);

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
            Logger::WriteMessage ("SKIPPED: Disks/Apple/dos33-master.dsk not "
                                  "available in this checkout.\n");
            return;
        }

        Assert::AreEqual (size_t (143360), raw.size(),
            L"DOS 3.3 master disk must be 143360 bytes");


        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"MountFromBytes must succeed");

        img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        core.RunCycles (kDos33ColdBootCycles);

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
            Logger::WriteMessage ("SKIPPED: Disks/Apple/dos33-master.dsk not "
                                  "available in this checkout.\n");
            return;
        }

        Assert::AreEqual (size_t (143360), raw.size(),
            L"DOS 3.3 master disk must be 143360 bytes");


        hr = host.BuildApple2eWithDisk2 (core);
        AssertSucceeded (hr, L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        AssertSucceeded (hr, L"master MountFromBytes must succeed");

        masterImg = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (masterImg, L"Mounted master DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, masterImg);

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

        core.RunCycles (kDos33ColdBootCycles);

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
};

