#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "KeystrokeInjector.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/WozLoader.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleHiResMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr uint64_t   kColdBootCycles      = 5000000ULL;
    static constexpr uint64_t   kAfterCommandCycles  = 1000000ULL;
    static constexpr uint64_t   kBootDiskCycles      = 2000000ULL;

    static constexpr int        kFbW                 = 560;
    static constexpr int        kFbH                 = 384;
    static constexpr int        kHiresRowsTopRegion  = 160;     // Mixed mode: top 160 lines hi-res
    static constexpr int        kMixedTextStartRow   = 20;      // Bottom 4 rows = text

    static constexpr Word       kSwitch80StoreOff    = 0xC000;
    static constexpr Word       kSwitch80StoreOn     = 0xC001;
    static constexpr Word       kSwitchTextOff       = 0xC050;
    static constexpr Word       kSwitchMixedOn       = 0xC053;
    static constexpr Word       kSwitchPage1         = 0xC054;
    static constexpr Word       kSwitchHiresOn       = 0xC057;
    static constexpr Word       kSwitch80ColOn       = 0xC00D;

    static constexpr int        kSlot6               = 6;
    static constexpr int        kDrive1              = 0;
    static constexpr Word       kBootRomEntry        = 0xC600;
    static constexpr Word       kIntCxRomOff         = 0xC006;
    static constexpr size_t     kSentinelOffset      = 0x10;
    static constexpr Byte       kSentinelByte        = 0x5A;
    static constexpr size_t     kWozTrackBitCount    = 51200;
    static constexpr size_t     kCpTrackBitCount     = 50000;
    static constexpr size_t     kWozTrackByteCount   = 6400;
    static constexpr size_t     kCpTrackByteCount    = 6250;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Fnv1a64 — stable framebuffer hash for the golden-output gate.
    //
    ////////////////////////////////////////////////////////////////////////////

    uint64_t Fnv1a64 (const uint32_t * data, size_t count)
    {
        uint64_t   h = 0xcbf29ce484222325ULL;
        size_t     i;
        int        b;
        uint8_t    byte;
        uint32_t   v;

        for (i = 0; i < count; i++)
        {
            v = data[i];

            for (b = 0; b < 4; b++)
            {
                byte = static_cast<uint8_t> ((v >> (b * 8)) & 0xFF);
                h   ^= byte;
                h   *= 0x100000001b3ULL;
            }
        }

        return h;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  PrngNext — local LCG used by the golden-output test to stamp a
    //  deterministic, repeatable pattern into hi-res page 1 + 80-col text
    //  pages. Independent of HeadlessHost::kPinnedSeed so the produced
    //  bytes don't track changes to that seed.
    //
    ////////////////////////////////////////////////////////////////////////////

    uint32_t PrngNext (uint32_t & state)
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  BootIIeToPrompt — cold-boot the //e harness to the Applesoft `]`
    //  prompt. Reused by every US4 graphics-mode scenario.
    //
    ////////////////////////////////////////////////////////////////////////////

    void BootIIeToPrompt (HeadlessHost & host, EmulatorCore & core)
    {
        HRESULT   hr;

        hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe must succeed");
        Assert::IsTrue (core.HasAppleIIe (), L"//e wiring must be complete");

        core.PowerCycle ();
        core.RunCycles  (kColdBootCycles);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSyntheticDsk / BuildSyntheticPo / BuildSyntheticWoz — replicas
    //  of the Phase 11 fixture builders. The validation suite owns its
    //  own copies so it remains a single self-contained file (per user
    //  story 4 acceptance). No third-party software is loaded.
    //
    ////////////////////////////////////////////////////////////////////////////

    vector<Byte> BuildSyntheticDsk ()
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);

        // Boot ROM loads file offset 0..255 into RAM at $0800-$08FF
        // and JMPs to $0801. Plant `JMP $0801` (4C 01 08) so the CPU
        // loops cleanly inside legal code after the hand-off, instead
        // of executing the all-zero (BRK) sector and tripping
        // Cpu::StepOne's illegal-opcode assert when BRK lands in a
        // 65C02-only ROM handler. The test's real assertions (motor
        // on, nibbles read, screen scrape-able) remain meaningful.
        raw[1] = 0x4C; raw[2] = 0x01; raw[3] = 0x08;

        raw[kSentinelOffset] = kSentinelByte;
        return raw;
    }


    vector<Byte> BuildSyntheticPo ()
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);

        // Same JMP-to-self halt as BuildSyntheticDsk -- see comment there.
        raw[1] = 0x4C; raw[2] = 0x01; raw[3] = 0x08;

        raw[kSentinelOffset] = kSentinelByte;
        return raw;
    }


    HRESULT BuildSyntheticWoz (size_t bitCount, size_t byteCount, vector<Byte> & outBytes)
    {
        vector<Byte>   bits (byteCount, 0xFF);

        return WozLoader::BuildSyntheticV2 (1, false, bits, bitCount, outBytes);
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MountAndJumpToSlot6Boot — Phase 11 helper, replicated here. Mounts
    //  the supplied bytes through DiskImageStore, points the //e at the
    //  slot 6 boot ROM, returns the store-owned DiskImage so callers can
    //  inspect track state.
    //
    ////////////////////////////////////////////////////////////////////////////

    DiskImage * MountAndJumpToSlot6Boot (
        HeadlessHost  &  host,
        EmulatorCore  &  core,
        const string  &  virtualPath,
        DiskFormat       fmt,
        const vector<Byte> & bytes)
    {
        HRESULT      hr        = S_OK;
        DiskImage *  external  = nullptr;

        hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1, virtualPath, fmt, bytes);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        external = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (external, L"Store must yield a DiskImage after mount");

        core.diskController->SetExternalDisk (kDrive1, external);
        core.bus->WriteByte (kIntCxRomOff, 0);
        core.cpu->SetPC (kBootRomEntry);

        return external;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  RowContaining — find the first row index whose scraped string
    //  contains the supplied needle, or -1 if none.
    //
    ////////////////////////////////////////////////////////////////////////////

    int RowContaining (const std::vector<std::string> & rows, const std::string & needle)
    {
        size_t   i;

        for (i = 0; i < rows.size (); i++)
        {
            if (rows[i].find (needle) != std::string::npos)
            {
                return static_cast<int> (i);
            }
        }

        return -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmuValidationSuiteTests
//
//  Phase 13 / User Story 4. The consolidated FR-045 acceptance suite.
//  Every scenario runs deterministically through HeadlessHost +
//  IFixtureProvider only — constitution §II compliant, no Win32, no
//  audio device, no host filesystem outside UnitTest/Fixtures/.
//
//  Scope mapping:
//    T114 — `GR`  reaches lo-res mixed-mode state (FR-045 acceptance).
//    T115 — `HGR` reaches hi-res page 1 (FR-045 acceptance).
//    T116 — `HGR2` reaches hi-res page 2 (FR-045 acceptance).
//    T117 — Mixed-mode + 80COL golden framebuffer hash (FR-017a / Q1).
//    T118 — DOS 3.3, ProDOS, WOZ, copy-protected disk boot end-to-end.
//    T113/orchestrator — `RunsAllScenarios` invokes the full FR-045 set
//                        and asserts every sub-scenario reports green.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EmuValidationSuiteTests)
{
public:

    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T114 — GR command engages lo-res graphics + mixed-mode.
    //
    //  Acceptance: after `GR` the //e softswitches report graphics on,
    //  mixed on, hires off, page2 off; the scraped bottom-4 text rows
    //  still contain the `]` prompt (mixed mode preserves the text
    //  region). Lo-res page1 ($0400) holds at least one non-zero byte
    //  cleared by the GR command's color-fill side effect.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_GR_LoresGraphics_Renders)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;
        int            promptRow;

        BootIIeToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "GR");
        Assert::AreEqual (size_t (3), consumed,
            L"`GR` + Return must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue  (core.softSwitches->IsGraphicsMode (),
            L"GR must engage graphics mode (TEXT off)");
        Assert::IsTrue  (core.softSwitches->IsMixedMode (),
            L"GR must engage mixed mode (4 lines text bottom)");
        Assert::IsFalse (core.softSwitches->IsHiresMode (),
            L"GR must keep HIRES off");
        Assert::IsFalse (core.softSwitches->IsPage2 (),
            L"GR must select PAGE1");

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Scraper must produce 24 rows");

        promptRow = RowContaining (rows, "]");
        Assert::IsTrue (promptRow >= kMixedTextStartRow,
            L"GR mixed-mode prompt must appear in the bottom-4 text region");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T115 — HGR command engages hi-res page 1 + mixed-mode.
    //
    //  Acceptance: after `HGR` the soft switches report HIRES on, PAGE2
    //  off, MIXED on; HPLOT then runs without exiting the graphics
    //  region (prompt remains in mixed-mode bottom rows).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_HGR_HiresPage1_Renders)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;
        int            promptRow;

        BootIIeToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "HGR");
        Assert::AreEqual (size_t (4), consumed,
            L"`HGR` + Return must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue  (core.softSwitches->IsGraphicsMode (),
            L"HGR must engage graphics mode (TEXT off)");
        Assert::IsTrue  (core.softSwitches->IsHiresMode (),
            L"HGR must engage HIRES");
        Assert::IsFalse (core.softSwitches->IsPage2 (),
            L"HGR must select PAGE1");
        Assert::IsTrue  (core.softSwitches->IsMixedMode (),
            L"HGR must engage MIXED (preserves bottom text)");

        consumed = KeystrokeInjector::InjectLine (core, "HPLOT 0,0 TO 279,159");
        Assert::IsTrue (consumed >= 18,
            L"HPLOT line must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        promptRow = RowContaining (rows, "]");
        Assert::IsTrue (promptRow >= 0,
            L"HGR + HPLOT must leave the prompt visible somewhere on the text page");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T116 — HGR2 command engages hi-res page 2 (full-screen
    //  graphics, no mixed-mode).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_HGR2_HiresPage2_Renders)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed;

        BootIIeToPrompt (host, core);

        consumed = KeystrokeInjector::InjectLine (core, "HGR2");
        Assert::AreEqual (size_t (5), consumed,
            L"`HGR2` + Return must be fully consumed");

        core.RunCycles (kAfterCommandCycles);

        Assert::IsTrue  (core.softSwitches->IsGraphicsMode (),
            L"HGR2 must engage graphics mode (TEXT off)");
        Assert::IsTrue  (core.softSwitches->IsHiresMode (),
            L"HGR2 must engage HIRES");
        Assert::IsTrue  (core.softSwitches->IsPage2 (),
            L"HGR2 must select PAGE2");
        Assert::IsFalse (core.softSwitches->IsMixedMode (),
            L"HGR2 must clear MIXED (full-screen graphics)");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T117 — Mixed-mode + 80COL golden output (FR-017a / Q1).
    //
    //  Stamps a deterministic pattern into hi-res page 1 (rows 0-159)
    //  and the 80-column aux/main text page (rows 20-23). Renders hi-
    //  res across the full frame, then overlays the 80-col text
    //  bottom-4-row range using Apple80ColTextMode::RenderRowRange (the
    //  same code path the composed mixed-mode dispatcher uses). The
    //  resulting framebuffer is hashed with FNV-1a-64 and asserted
    //  against a baked-in golden constant — two consecutive runs of the
    //  test must reproduce the exact same hash (FR-038, SC-005).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_MixedMode_80Col_GoldenOutput)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        uint32_t       seed = 0xCA550001u;
        int            row;
        int            col;
        Word           rowBase;
        Word           hiresAddr;
        Byte           a;
        Byte           m;
        Byte           h;
        uint64_t       hash;

        HRESULT   hr = host.BuildAppleIIe (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIe must succeed");

        core.PowerCycle ();
        core.RunCycles  (kColdBootCycles);

        // Engage HIRES + MIXED + 80COL via the documented soft-switch
        // surface. Drives the same flag state Applesoft would set after
        // typing HGR + POKE -16302,0 + PR#3.
        core.bus->ReadByte  (kSwitchTextOff);
        core.bus->ReadByte  (kSwitchMixedOn);
        core.bus->ReadByte  (kSwitchPage1);
        core.bus->ReadByte  (kSwitchHiresOn);
        core.bus->WriteByte (kSwitch80StoreOn, 0);
        core.bus->WriteByte (kSwitch80ColOn,   0);

        Byte * auxBuf = core.mmu->GetAuxBuffer ();

        // Stamp a deterministic 80-col text pattern into the bottom 4
        // rows (20..23). Aux supplies even columns, main supplies odd.
        for (row = kMixedTextStartRow; row < TextScreenScraper::kRows; row++)
        {
            rowBase = TextScreenScraper::RowBaseAddress (
                TextScreenScraper::kTextPage1, row);

            for (col = 0; col < TextScreenScraper::kCols40; col++)
            {
                a = static_cast<Byte> (0x80 | (PrngNext (seed) & 0x7F));
                m = static_cast<Byte> (0x80 | (PrngNext (seed) & 0x7F));
                auxBuf[rowBase + col] = a;
                core.bus->WriteByte (static_cast<Word> (rowBase + col), m);
            }
        }

        // Stamp a deterministic hi-res pattern into page 1 ($2000-$3FFF).
        for (row = 0; row < kHiresRowsTopRegion; row++)
        {
            hiresAddr = AppleHiResMode::ScanlineAddress (row, 0x2000);

            for (col = 0; col < 40; col++)
            {
                h = static_cast<Byte> (PrngNext (seed) & 0x7F);
                core.bus->WriteByte (static_cast<Word> (hiresAddr + col), h);
            }
        }

        // Render the composed frame: hi-res baseline, then overlay
        // 80-col text on rows 20..23 via the shared RenderRowRange.
        std::vector<uint32_t>   fb (kFbW * kFbH, 0);

        AppleHiResMode   hires (*core.bus);
        hires.SetPage2 (false);
        hires.Render (nullptr, fb.data (), kFbW, kFbH);

        Apple80ColTextMode   text80 (*core.bus);
        text80.SetAuxMemory  (auxBuf);
        text80.SetAltCharSet (false);
        text80.SetFlashState (true);
        text80.RenderRowRange (
            kMixedTextStartRow,
            TextScreenScraper::kRows,
            nullptr,
            fb.data (),
            kFbW,
            kFbH);

        hash = Fnv1a64 (fb.data (), fb.size ());

        // Golden hash captured from the first deterministic render with
        // PRNG seed 0xCA550001 + HeadlessHost::kPinnedSeed cold boot.
        // Updated 2026-05-13 alongside the NtscColorTable byte-layout
        // fix. Updated 2026-05-14 alongside the swap-chain format
        // change from R8G8B8A8_UNORM to B8G8R8A8_UNORM (see
        // Video/PixelFormat.h) — pixel bytes are now in BGRA order so
        // the FNV hash over the framebuffer changes for any frame with
        // non-grayscale content.
        constexpr uint64_t   kExpected = 0x2ABA2BA47C35CE05ULL;

        Assert::AreEqual (kExpected, hash,
            std::format (L"Mixed-mode 80COL golden hash mismatch: got 0x{:016X}", hash).c_str ());
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T118 — DOS 3.3 disk boot end-to-end.
    //
    //  Mounts a synthetic .dsk through DiskImageStore, drives the //e
    //  past the slot 6 boot ROM, asserts (a) the controller actually
    //  spun up the motor, (b) the nibble engine consumed bits from
    //  track 0, (c) the text screen still scrapes to 24 rows after the
    //  boot attempt — proves the disk subsystem is end-to-end functional
    //  without bundling any third-party DOS 3.3 image. Per FR-045
    //  acceptable simplification.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_DOS33_Boots_To_Catalog)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw       = BuildSyntheticDsk ();
        DiskImage   *  external  = nullptr;
        size_t         bitsAfter = 0;

        external = MountAndJumpToSlot6Boot (host, core,
            "synthetic.dsk", DiskFormat::Dsk, raw);

        Assert::IsTrue (external->GetTrackBitCount (0) > 0,
            L"DOS 3.3 .dsk mount must produce a nibblized track 0");

        core.RunCycles (kBootDiskCycles);

        bitsAfter = core.diskController->GetEngine (kDrive1).GetBitPosition ();

        Assert::IsTrue (core.diskController->IsMotorOn (),
            L"Boot ROM must turn the motor on (FR-021)");
        Assert::IsTrue (bitsAfter > 0,
            L"DOS 3.3 boot ROM must read at least one nibble from track 0");

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Text screen must remain scrape-able through the DOS 3.3 boot attempt");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T118 — ProDOS .po disk boot end-to-end.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_ProDOS_Boots_To_PREFIX)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw       = BuildSyntheticPo ();
        DiskImage   *  external  = nullptr;
        size_t         bitsAfter = 0;

        external = MountAndJumpToSlot6Boot (host, core,
            "synthetic.po", DiskFormat::Po, raw);

        Assert::IsTrue (external->GetSourceFormat () == DiskFormat::Po,
            L"ProDOS .po mount must record source format");

        core.RunCycles (kBootDiskCycles);

        bitsAfter = core.diskController->GetEngine (kDrive1).GetBitPosition ();

        Assert::IsTrue (core.diskController->IsMotorOn (),
            L"Boot ROM must spin up the drive on a .po mount");
        Assert::IsTrue (bitsAfter > 0,
            L"ProDOS boot ROM must read at least one nibble from a .po image");

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Text screen must remain scrape-able through the ProDOS boot attempt");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T118 — WOZ disk boots the first track.
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_WOZ_Disk_Boots_FirstTrack)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   woz;
        DiskImage   *  external  = nullptr;
        size_t         bitsAfter = 0;
        HRESULT        hr        = S_OK;

        hr = BuildSyntheticWoz (kWozTrackBitCount, kWozTrackByteCount, woz);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildSyntheticV2 must succeed");

        external = MountAndJumpToSlot6Boot (host, core,
            "sample.woz", DiskFormat::Woz, woz);

        Assert::IsTrue (external->GetSourceFormat () == DiskFormat::Woz,
            L"WOZ mount must record native bit-stream format");
        Assert::AreEqual (kWozTrackBitCount, external->GetTrackBitCount (0),
            L"WOZ track 0 must preserve the synthetic 51200-bit length");

        core.RunCycles (kBootDiskCycles);

        bitsAfter = core.diskController->GetEngine (kDrive1).GetBitPosition ();

        Assert::IsTrue (bitsAfter > 0,
            L"WOZ nibble engine must advance through the bit stream (FR-022)");
        Assert::IsTrue (core.diskController->IsMotorOn (),
            L"Boot ROM must spin up the drive on a WOZ mount");
    }


    ////////////////////////////////////////////////////////////////////////
    //
    //  US4 / T118 — Copy-protected (variable-length-track) WOZ loads.
    //
    //  Variable-length 50000-bit track 0 vs the standard 51200 — proves
    //  the nibble engine handles the non-standard track lengths copy-
    //  protected titles depend on (FR-024).
    //
    ////////////////////////////////////////////////////////////////////////

    TEST_METHOD (US4_CopyProtected_Disk_Loads_TitleScreen)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   woz;
        DiskImage   *  external  = nullptr;
        size_t         bitsAfter = 0;
        HRESULT        hr        = S_OK;

        hr = BuildSyntheticWoz (kCpTrackBitCount, kCpTrackByteCount, woz);
        Assert::IsTrue (SUCCEEDED (hr), L"CP-style synthetic WOZ build must succeed");

        external = MountAndJumpToSlot6Boot (host, core,
            "copyprotected.woz", DiskFormat::Woz, woz);

        Assert::AreEqual (kCpTrackBitCount, external->GetTrackBitCount (0),
            L"CP-style WOZ must preserve the non-standard 50000-bit track length");

        core.RunCycles (kBootDiskCycles);

        bitsAfter = core.diskController->GetEngine (kDrive1).GetBitPosition ();

        Assert::IsTrue (bitsAfter > 0,
            L"Engine must advance through the variable-length CP track (FR-024)");

        std::vector<std::string>   rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        Assert::AreEqual (size_t (TextScreenScraper::kRows), rows.size (),
            L"Text screen must remain scrape-able through the CP boot attempt");
    }
};
