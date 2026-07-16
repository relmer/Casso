#include "Pch.h"
#include <filesystem>
#include <fstream>
#include <set>

#include "HeadlessHost.h"
#include "FixtureProvider.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;


////////////////////////////////////////////////////////////////////////////////
//
//  GameBootTests
//
//  Phase D deliverable for #67 (Disk II copy-protection fidelity).
//  Mounts real WOZ images from Apple2/Demos/ and verifies the slot 6
//  boot ROM is able to drive the controller far enough that the head
//  actually visits multiple tracks. A copy-protection failure (wrong
//  spin-up, broken bit timing, missing non-standard sync handling)
//  typically manifests as the head stuck on track 0 spinning forever
//  waiting for an address mark that never resolves.
//
//  These tests are coarse smoke tests, not pixel-perfect "title screen
//  rendered" checks -- detecting that requires a frame grabber. The
//  multi-track signal is enough to catch the regressions Phase A/B/C
//  guard against and to flag genuine boot stalls.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int        kMaxAncestorWalk = 10;
    static constexpr int        kSlot6           = 6;
    static constexpr int        kDrive1          = 0;
    static constexpr Word       kBootRomEntry    = 0xC600;
    static constexpr Word       kIntCxRomOff     = 0xC006;
    static constexpr uint64_t   kChunkCycles     =    250'000ULL;
    static constexpr uint64_t   kBootCycleBudget = 60'000'000ULL;

    // The //c cold-boots through a ~14M-cycle RAM test before its firmware
    // autoboots the internal drive, so give the //c path a larger ceiling.
    // (Early-exit on the track threshold returns long before this is spent.)
    static constexpr uint64_t   kApple2cBootBudget = 90'000'000ULL;
    static constexpr size_t     kApple2cRomSize    = 0x8000;   // 32K, two 16K banks


    ////////////////////////////////////////////////////////////////////////
    //
    //  Walk up from cwd looking for a repo-relative file. Returns empty
    //  path if not found.
    //
    ////////////////////////////////////////////////////////////////////////

    fs::path FindRepoFile (const std::string & relPath)
    {
        std::error_code   ec;
        fs::path          cursor = fs::current_path (ec);
        int               i      = 0;

        if (ec)
        {
            return fs::path ();
        }

        for (i = 0; i < kMaxAncestorWalk; i++)
        {
            fs::path   candidate = cursor / relPath;

            if (fs::exists (candidate, ec))
            {
                return candidate;
            }

            if (!cursor.has_parent_path () || cursor == cursor.parent_path ())
            {
                break;
            }

            cursor = cursor.parent_path ();
        }

        return fs::path ();
    }


    std::vector<Byte> ReadFileBytes (const fs::path & path)
    {
        std::ifstream   f (path, std::ios::binary);

        if (!f)
        {
            return std::vector<Byte> ();
        }

        return std::vector<Byte> ((std::istreambuf_iterator<char> (f)),
                                  std::istreambuf_iterator<char> ());
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Pump the CPU in fixed-size chunks, sampling the controller's
    //  track position after each chunk so we can prove the boot loader
    //  is actually seeking, not just spinning on a single track.
    //
    ////////////////////////////////////////////////////////////////////////

    void RunAndSampleTracks (
        EmulatorCore        &  core,
        uint64_t               totalBudget,
        std::set<int>       &  outTracksVisited,
        int                    earlyExitThreshold)
    {
        uint64_t   spent = 0;

        outTracksVisited.insert (core.diskController->GetCurrentTrack ());

        while (spent < totalBudget)
        {
            uint64_t   chunk = std::min<uint64_t> (kChunkCycles, totalBudget - spent);

            core.RunCycles (chunk);
            spent += chunk;

            outTracksVisited.insert (core.diskController->GetCurrentTrack ());

            if (outTracksVisited.size () >= static_cast<size_t> (earlyExitThreshold))
            {
                return;
            }
        }
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Drive the named WOZ through a full //e cold boot and assert the
    //  head walks at least `minTracks` distinct tracks within the cycle
    //  budget. Skips (with Logger output) if the demo file is missing.
    //
    ////////////////////////////////////////////////////////////////////////

    void AssertGameBoots (
        const std::string  &  relPath,
        const wchar_t      *  label,
        int                   minTracks)
    {
        fs::path   wozPath = FindRepoFile (relPath);

        if (wozPath.empty ())
        {
            Logger::WriteMessage ("SKIPPED: WOZ file not found: ");
            Logger::WriteMessage (relPath.c_str ());
            Logger::WriteMessage ("\n");
            return;
        }

        std::vector<Byte>   bytes        = ReadFileBytes (wozPath);
        HeadlessHost        host;
        EmulatorCore        core;
        HRESULT             hr           = S_OK;
        DiskImage        *  external     = nullptr;
        std::set<int>       tracksVisited;
        size_t              bitsAfter    = 0;
        wchar_t             failMsg[256] = {};

        Assert::IsFalse (bytes.empty (), L"WOZ file must not be empty");

        hr = host.BuildApple2eWithDisk2 (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            wozPath.string (), DiskFormat::Woz, bytes);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed for real WOZ");

        external = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (external, L"Store must yield a DiskImage after mount");

        core.diskController->SetExternalDisk (kDrive1, external);

        core.bus->WriteByte (kIntCxRomOff, 0);

        core.cpu->SetPC (kBootRomEntry);

        RunAndSampleTracks (core, kBootCycleBudget, tracksVisited, minTracks);

        bitsAfter = core.diskController->GetEngine (kDrive1).GetBitPosition ();

        Assert::IsTrue (core.diskController->IsMotorOn (),
            L"Boot ROM must turn the motor on");

        // Either the bit cursor advanced OR the head walked off track 0.
        // Bit cursor wraps at the end of each track, so a sample exactly
        // on a track boundary can read 0 even after millions of bits
        // were consumed -- multi-track movement is a wrap-proof signal.
        Assert::IsTrue (bitsAfter > 0 || tracksVisited.size () > 1,
            L"Boot ROM must drive the controller (bit cursor advanced or head moved)");

        swprintf_s (failMsg,
            L"%ls: head only visited %zu distinct tracks (need >= %d); "
            L"boot loader likely stuck on copy-protection check",
            label, tracksVisited.size (), minTracks);
        Assert::IsTrue (
            tracksVisited.size () >= static_cast<size_t> (minTracks),
            failMsg);
    }


    bool Apple2cRomAvailable()
    {
        FixtureProvider        fp;
        std::vector<uint8_t>   bytes;
        return SUCCEEDED (fp.OpenFixture ("Apple2c.rom", bytes)) &&
               bytes.size() == kApple2cRomSize;
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  Same real disk game, but booted end-to-end on the Apple //c through
    //  its built-in slot-6 IWM -- the //c has no card slots, so the internal
    //  drive is the only disk path. Unlike the //e helper (which forces the
    //  $C600 boot ROM entry), this cold-boots and lets the //c firmware
    //  autoboot its internal drive, the real user boot path proven by
    //  Apple2cBootTests.BootsFromInternalDriveViaIwm. Skips when the
    //  copyrighted //c ROM 4 fixture is absent (CI never needs a machine ROM).
    //
    ////////////////////////////////////////////////////////////////////////

    void AssertApple2cGameBoots (
        const std::string  &  relPath,
        const wchar_t      *  label,
        int                   minTracks)
    {
        if (!Apple2cRomAvailable())
        {
            Logger::WriteMessage (
                "SKIPPED: UnitTest/Fixtures/Apple2c.rom absent "
                "(copyrighted //c ROM 4, provisioned on demand).\n");
            return;
        }

        fs::path   wozPath = FindRepoFile (relPath);

        if (wozPath.empty())
        {
            Logger::WriteMessage ("SKIPPED: WOZ file not found: ");
            Logger::WriteMessage (relPath.c_str());
            Logger::WriteMessage ("\n");
            return;
        }

        std::vector<Byte>   bytes         = ReadFileBytes (wozPath);
        HeadlessHost        host;
        EmulatorCore        core;
        HRESULT             hr            = S_OK;
        DiskImage        *  internal      = nullptr;
        std::set<int>       tracksVisited;
        wchar_t             failMsg[256]  = {};

        Assert::IsFalse (bytes.empty (), L"WOZ file must not be empty");

        hr = host.BuildApple2c (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2c must succeed");

        // PowerCycle first (re-seeds DRAM + rebinds the drive to its empty
        // internal disk), THEN mount -- matching the production ordering.
        core.PowerCycle();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
            wozPath.string(), DiskFormat::Woz, bytes);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed for real WOZ");

        internal = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (internal, L"Store must yield a DiskImage after mount");

        core.diskController->SetExternalDisk (kDrive1, internal);   // drive 1 = internal

        // Cold-boot: firmware runs the RAM test then autoboots the slot-6
        // IWM. No forced $C600 entry -- this is the real //c boot path.
        RunAndSampleTracks (core, kApple2cBootBudget, tracksVisited, minTracks);

        Assert::IsTrue (core.diskController->IsMotorOn(),
            L"Boot ROM must turn the motor on");

        swprintf_s (failMsg,
            L"%ls: head only visited %zu distinct tracks on the //c (need >= %d); "
            L"the //c IWM boot path likely stalled",
            label, tracksVisited.size(), minTracks);
        Assert::IsTrue (
            tracksVisited.size() >= static_cast<size_t> (minTracks),
            failMsg);
    }
}


TEST_CLASS (GameBootTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //  Choplifter (1982, Broderbund). Standard DOS 3.3 boot with light
    //  copy-protection that formats outer tracks (12+) on half-track
    //  boundaries. The loader sweeps the disk to stage the game; with the
    //  quarter-track pipeline and apple2js stepper model it reaches the
    //  protected half-track region well past track 12. Strong "actually
    //  loading content" signal: >= 20 distinct tracks.
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Choplifter_WozBoot_HeadVisitsContentTracks)
    {
        AssertGameBoots ("Apple2/Demos/Choplifter.woz", L"Choplifter", 20);
    }


    ////////////////////////////////////////////////////////////////////////
    //  Karateka (1984, Broderbund). Aggressive copy-protection: nibble
    //  counts, non-standard sync patterns, and the Jordan-Mechner-era
    //  RWTS18 trickery. With the quarter-track pipeline and apple2js
    //  stepper model the loader now clears the protection checks, sweeps
    //  the disk to stage the game, and turns the motor off once the load
    //  completes -- the same boot signature as Choplifter. The head walks
    //  well past the old 3-track protection stall (empirically ~14 distinct
    //  tracks up to track 32), so require >= 10 as a "really loading" signal.
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Karateka_WozBoot_HeadVisitsContentTracks)
    {
        AssertGameBoots ("Apple2/Demos/Karateka.woz", L"Karateka", 10);
    }


    ////////////////////////////////////////////////////////////////////////
    //  Lode Runner (1983, Broderbund). Same copy-protection family as
    //  Karateka. Previously stalled at 4 tracks; with the quarter-track
    //  pipeline + apple2js stepper it now boots, sweeping the disk and
    //  reaching the high outer tracks (empirically ~21 distinct tracks up
    //  to track 33) before handing off with the motor off. Require >= 15
    //  distinct tracks as the "really loading" signal.
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (LodeRunner_WozBoot_HeadVisitsContentTracks)
    {
        AssertGameBoots ("Apple2/Demos/LodeRunner.woz", L"Lode Runner", 15);
    }


    ////////////////////////////////////////////////////////////////////////
    //  A real disk game boots end-to-end on the Apple //c
    //  through its built-in slot-6 IWM. Choplifter is a standard DOS 3.3
    //  boot that runs on //e-class hardware; the //c shares the //e's WOZ
    //  nibble engine + RDDATA path (IWM mode only adds the MODE/STATUS
    //  register), so the loader sweeps content tracks exactly as on the //e.
    //  A multi-track head sweep == a disk title actually loading on the //c,
    //  not stalling on track 0. Threshold kept conservative (>= 10) vs. the
    //  //e's 20 so //c cold-boot/autoboot timing can't make it brittle.
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Choplifter_WozBoot_OnApple2c_HeadVisitsContentTracks)
    {
        AssertApple2cGameBoots ("Apple2/Demos/Choplifter.woz", L"Choplifter (//c)", 10);
    }
};
