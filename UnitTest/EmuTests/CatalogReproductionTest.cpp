#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include <fstream>

#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "TextScreenScraper.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/DiskIIController.h"
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

namespace
{
    static constexpr Word        kBootRomEntry        = 0xC600;
    static constexpr Word        kIntCxRomOff         = 0xC006;
    static constexpr int         kSlot6               = 6;
    static constexpr int         kDrive1              = 0;

    // DOS 3.3 cold boot from a real .dsk: boot ROM loads sector 0,
    // bootstrap chain loads more sectors from track 0..1, DOS init
    // sets up vectors, prints "APPLE //e" banner + ]. Empirically
    // this lands well under 200M cycles in debug builds.
    static constexpr uint64_t    kDos33ColdBootCycles = 200'000'000ULL;

    // CATALOG seeks to track 17 (VTOC), reads VTOC, walks the catalog
    // chain printing each entry. Real-disk timing in cycle units.
    static constexpr uint64_t    kCatalogCycles       = 60'000'000ULL;
}





TEST_CLASS (CatalogReproductionTest)
{
public:

    static std::vector<Byte> ReadDsk (const std::string & relPath)
    {
        std::vector<std::string>  candidates = {
            relPath,
            "..\\" + relPath,
            "..\\..\\" + relPath,
            "..\\..\\..\\" + relPath,
            "C:\\Users\\relmer\\source\\repos\\relmer\\Casso\\" + relPath
        };

        for (const auto & p : candidates)
        {
            std::ifstream f (p, std::ios::binary);
            if (f)
            {
                std::vector<Byte>  bytes (
                    (std::istreambuf_iterator<char> (f)),
                    std::istreambuf_iterator<char> ());
                return bytes;
            }
        }

        Assert::Fail (L"Could not locate dos33-master.dsk in any expected path");
        return {};
    }


    TEST_METHOD (DOS33_CATALOG_DoesNotErrorOnMasterDisk)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        HRESULT  hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        std::vector<Byte>  raw = ReadDsk ("Disks\\Apple\\dos33-master.dsk");
        Assert::AreEqual (size_t (143360), raw.size (),
            L"DOS 3.3 master disk must be 143360 bytes");

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            "dos33-master.dsk", DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        DiskImage *  img = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (img, L"Mounted DiskImage must be present");
        core.diskController->SetExternalDisk (kDrive1, img);

        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        core.RunCycles (kDos33ColdBootCycles);

        std::vector<std::string>  rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        bool  promptVisible = false;
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

        bool  ioError = false;
        bool  volumeBanner = false;
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
};
