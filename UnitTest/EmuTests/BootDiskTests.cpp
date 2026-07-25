#include "Pch.h"
#include <filesystem>
#include <fstream>

#include "Cpu.h"
#include "Assembler.h"
#include "AssemblerTypes.h"

#include "HeadlessHost.h"
#include "KeystrokeInjector.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk2Controller.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Video/AppleHiResMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  BootDiskTests
//
//  End-to-end gate for the full Apple //e disk-boot pipeline using a
//  100% in-house demo disk -- no third-party software, no copyrighted
//  content. Source for the demo lives at Apple2/Demos/casso-rocks.a65;
//  the framebuffer payload at Apple2/Demos/cassowary.hgr is generated
//  by scripts/HgrPreprocess.py from one of the cassowary photos under
//  Assets/. This test assembles the .a65 via Casso's own AS65-compatible
//  assembler, stitches the HGR payload behind the boot sector at file
//  offset $1000 (track 1 sector 0), mounts the resulting .dsk through
//  DiskImageStore, lets the //e boot ROM pick it up, and verifies the
//  demo:
//
//    - reads the 8 KB framebuffer off tracks 1+2 (32 sectors) into
//      $2000-$3FFF using a from-scratch 6502 RWTS,
//    - flips the soft switches into HGR1 mode (TEXT off, MIXED off,
//      PAGE2 off, HIRES on),
//    - leaves the cassowary on screen.
//
//  Exercises: HgrPreprocess.py output -> Assembler -> NibblizationLayer
//  -> DiskImageStore -> Disk2Controller -> Disk2NibbleEngine ->
//  Disk2.rom slot 6 boot -> 6502 CPU executing our RWTS -> MMU HGR
//  page-1 writes -> Apple2eSoftSwitchBank graphics-mode latching.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int     kMaxAncestorWalk     = 10;
    static constexpr size_t  kHgrPayloadSize      = 8192;
    static constexpr size_t  kLoresPayloadSize    = 1024;
    static constexpr size_t  kSectorByteSize      = 256;
    static constexpr int     kSectorsPerTrack     = 16;
    static constexpr Word    kHgrBase             = 0x2000;
    static constexpr Word    kBootEntry           = 0xC600;
    static constexpr Word    kDemoEntry           = 0x0801;
    static constexpr Word    kStage2Entry         = 0x1000;
    static constexpr uint64_t  kDemoCycleBudget   = 10'000'000ULL;  // 10M cycles ≈ 9.8 sec emulated; ample for 9 disk tracks (~2 sec real time)


    fs::path FindRepoFile (const std::string & relPath)
    {
        std::error_code ec;
        fs::path        cursor = fs::current_path (ec);
        if (ec) return fs::path ();

        for (int i = 0; i < kMaxAncestorWalk; i++)
        {
            fs::path candidate = cursor / relPath;
            if (fs::exists (candidate, ec)) return candidate;
            if (!cursor.has_parent_path () || cursor == cursor.parent_path ())
            {
                break;
            }
            cursor = cursor.parent_path ();
        }
        return fs::path ();
    }


    std::string ReadFileText (const fs::path & path)
    {
        std::ifstream f (path);
        if (!f) return std::string ();
        return std::string ((std::istreambuf_iterator<char> (f)),
                            std::istreambuf_iterator<char> ());
    }


    std::vector<Byte> ReadFileBytes (const fs::path & path)
    {
        std::ifstream f (path, std::ios::binary);
        if (!f) return std::vector<Byte> ();
        return std::vector<Byte> ((std::istreambuf_iterator<char> (f)),
                                  std::istreambuf_iterator<char> ());
    }
}


TEST_CLASS (BootDiskTests)
{
public:

    TEST_METHOD (CassoRocks_DemoDisk_DisplaysHgrCassowary)
    {
        fs::path  src           = FindRepoFile ("Apple2/Demos/casso-rocks.a65");
        fs::path  stage2Src     = FindRepoFile ("Apple2/Demos/casso-rocks-stage2.a65");
        fs::path  hgrPath       = FindRepoFile ("Apple2/Demos/cassowary.hgr");
        fs::path  bandsPath     = FindRepoFile ("Apple2/Demos/test-bands.hgr");
        fs::path  loresPath     = FindRepoFile ("Apple2/Demos/lores-bars.lores");
        fs::path  dhgrAuxPath   = FindRepoFile ("Apple2/Demos/dhgr-cassowary-aux.bin");
        fs::path  dhgrMainPath  = FindRepoFile ("Apple2/Demos/dhgr-cassowary-main.bin");

        if (src.empty () || stage2Src.empty () ||
            hgrPath.empty () || bandsPath.empty () ||
            loresPath.empty () ||
            dhgrAuxPath.empty () || dhgrMainPath.empty ())
        {
            Logger::WriteMessage ("SKIPPED: one or more demo-disk source "
                                  "files (casso-rocks*.a65, cassowary.hgr, "
                                  "test-bands.hgr, lores-bars.lores, "
                                  "dhgr-cassowary-{aux,main}.bin) not "
                                  "found in this checkout.\n");
            return;
        }

        std::string source       = ReadFileText (src);
        std::string stage2Source = ReadFileText (stage2Src);
        Assert::IsFalse (source.empty (), L"casso-rocks.a65 must not be empty");
        Assert::IsFalse (stage2Source.empty (),
            L"casso-rocks-stage2.a65 must not be empty");

        std::vector<Byte>  hgrPayload      = ReadFileBytes (hgrPath);
        std::vector<Byte>  bandsPayload    = ReadFileBytes (bandsPath);
        std::vector<Byte>  loresPayload    = ReadFileBytes (loresPath);
        std::vector<Byte>  dhgrAuxPayload  = ReadFileBytes (dhgrAuxPath);
        std::vector<Byte>  dhgrMainPayload = ReadFileBytes (dhgrMainPath);
        Assert::AreEqual (kHgrPayloadSize, hgrPayload.size (),
            L"cassowary.hgr must be exactly 8192 bytes");
        Assert::AreEqual (kHgrPayloadSize, bandsPayload.size (),
            L"test-bands.hgr must be exactly 8192 bytes");
        Assert::AreEqual (kLoresPayloadSize, loresPayload.size (),
            L"lores-bars.lores must be exactly 1024 bytes");
        Assert::AreEqual (kHgrPayloadSize, dhgrAuxPayload.size (),
            L"dhgr-cassowary-aux.bin must be exactly 8192 bytes");
        Assert::AreEqual (kHgrPayloadSize, dhgrMainPayload.size (),
            L"dhgr-cassowary-main.bin must be exactly 8192 bytes");

        Cpu             cpu;
        Assembler       assembler (cpu.GetInstructionSet ());

        AssemblyResult  asmResult = assembler.Assemble (source);
        if (!asmResult.success)
        {
            wchar_t  msg[256] = {};
            const char *  firstError = asmResult.errors.empty ()
                ? "(no error message)"
                : asmResult.errors[0].message.c_str ();
            swprintf_s (msg, L"casso-rocks.a65 must assemble cleanly. First "
                             L"error: %hs", firstError);
            Assert::Fail (msg);
        }

        AssemblyResult  stage2Result = assembler.Assemble (stage2Source);
        if (!stage2Result.success)
        {
            wchar_t  msg[256] = {};
            const char *  firstError = stage2Result.errors.empty ()
                ? "(no error message)"
                : stage2Result.errors[0].message.c_str ();
            swprintf_s (msg, L"casso-rocks-stage2.a65 must assemble cleanly. "
                             L"First error: %hs", firstError);
            Assert::Fail (msg);
        }

        Assert::AreEqual (Word (kDemoEntry), asmResult.startAddress,
            L"Stage 1 must be assembled with .org $0801 (boot ROM JMP target)");
        Assert::IsTrue (asmResult.bytes.size () > 0 &&
                        asmResult.bytes.size () <= kSectorByteSize - 1,
            L"Stage 1 code must fit in the remainder of sector 0 ($0801-$08FF)");
        Assert::AreEqual (Word (kStage2Entry), stage2Result.startAddress,
            L"Stage 2 must be assembled with .org $0A00");
        Assert::IsTrue (stage2Result.bytes.size () > 0 &&
                        stage2Result.bytes.size () <= kSectorByteSize,
            L"Stage 2 code must fit in a single 256-byte sector");

        // Build a 143360-byte raw .dsk image:
        //   - File offset 1..N (track 0 sector 0 minus the first byte):
        //     stage 1 boot code.
        //   - Tracks 1+2: 8 KB DHGR aux pattern (loaded by stage 1
        //     into main $6000-$7FFF, then copied to aux $2000 by
        //     enter_dhgr).
        //   - Track 3 logical sector 0: stage 2 code (lands at $1000).
        //     Track 3 logical sectors 1..4: 1 KB LoRes test pattern
        //     (lands at $1100-$14FF, copied into text page 1 in
        //     mode_lores).
        //   - Tracks 4+5: 8 KB DHGR main pattern (loaded by stage 2
        //     init into main $8000-$9FFF, then copied to main $2000
        //     by enter_dhgr).
        //   - Tracks 6+7: 8 KB HGR1 cassowary (loaded by stage 2
        //     background phase directly into its stash location at
        //     main $A000-$BFFF; mode_hgr1 memcpys to $2000 on demand).
        //   - Tracks 8+9: 8 KB HGR2 bands (loaded by stage 2
        //     background phase to main $4000-$5FFF, the final HGR2
        //     framebuffer destination).
        //
        //   Disk layout reorder vs prior versions: DHGR data lives on
        //   the FIRST tracks so the demo can show DHGR after only ~5
        //   disk reads instead of waiting for all 9. HGR1+HGR2 load
        //   in the background after first frame is up.
        //
        //   The HGR payloads use the DOS 3.3 logical-to-physical
        //   interleave so that when our RWTS reads logical sector S
        //   of track T it gets exactly payload[((T-startTrack)*16+S)*256..].
        static constexpr int  kDsk_LtoP[16] =
        {
            0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
        };

        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);

        for (size_t i = 0; i < asmResult.bytes.size (); i++)
        {
            raw[1 + i] = asmResult.bytes[i];
        }

        // Track 3 logical sector 0 -> stage 2 code at $1000.
        // Track 3 logical sectors 1-4 -> LoRes pattern at $1100-$14FF.
        // Use the LtoP mapping so the on-disk physical layout the
        // nibblizer produces matches what stage 1's RWTS expects to
        // read out by logical address.
        auto StampTrack3Sector = [&] (int logicalSector,
                                      const Byte * data, size_t len)
        {
            size_t  fileOffset = static_cast<size_t> (
                3 * kSectorsPerTrack + kDsk_LtoP[logicalSector])
                * kSectorByteSize;
            for (size_t i = 0; i < len; i++)
            {
                raw[fileOffset + i] = data[i];
            }
        };

        StampTrack3Sector (0, stage2Result.bytes.data (),
                              stage2Result.bytes.size ());
        for (int sector = 0; sector < 4; sector++)
        {
            StampTrack3Sector (1 + sector,
                               loresPayload.data () + sector * kSectorByteSize,
                               kSectorByteSize);
        }

        // Stitch a payload across 2 tracks starting at startTrack.
        auto StitchPayload = [&raw] (int startTrack,
                                     const std::vector<Byte> & payload)
        {
            for (int trackOffset = 0; trackOffset < 2; trackOffset++)
            {
                int  track = startTrack + trackOffset;

                for (int sector = 0; sector < kSectorsPerTrack; sector++)
                {
                    size_t  fileOffset =
                        static_cast<size_t> (track * kSectorsPerTrack + kDsk_LtoP[sector])
                        * kSectorByteSize;
                    size_t  payloadOffset =
                        static_cast<size_t> (trackOffset * kSectorsPerTrack + sector)
                        * kSectorByteSize;

                    for (size_t i = 0; i < kSectorByteSize; i++)
                    {
                        raw[fileOffset + i] = payload[payloadOffset + i];
                    }
                }
            }
        };

        StitchPayload (1, dhgrAuxPayload);        // tracks 1+2 -> DHGR aux @ main $6000
        StitchPayload (4, dhgrMainPayload);       // tracks 4+5 -> DHGR main @ main $8000
        StitchPayload (6, hgrPayload);            // tracks 6+7 -> HGR1 cassowary @ main $A000
        StitchPayload (8, bandsPayload);          // tracks 8+9 -> HGR2 bands @ main $4000

        HeadlessHost  host;
        EmulatorCore  core;

        HRESULT  hr = host.BuildApple2eWithDisk2 (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2eWithDisk2 must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (6, 0, "casso-rocks.dsk",
                                             DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        DiskImage *  img = core.diskStore->GetImage (6, 0);
        Assert::IsNotNull (img);
        core.diskController->SetExternalDisk (0, img);

        core.bus->WriteByte (0xC006, 0);  // INTCXROM=0

        core.cpu->SetPC (kBootEntry);

        core.RunCycles (kDemoCycleBudget);

        // ----- Verify boot landing soft-switch state (mode 0 = DHGR) -----
        Apple2eSoftSwitchBank *   ss = core.softSwitches.get ();

        Assert::IsNotNull (ss, L"Apple2eSoftSwitchBank must be present");
        Assert::IsTrue (ss->IsGraphicsMode (),
            L"Demo must leave the //e in graphics mode (TEXT off)");
        Assert::IsFalse (ss->IsMixedMode (),
            L"Demo must leave MIXED off (full-screen graphics)");
        Assert::IsFalse (ss->IsPage2 (),
            L"Mode 0 (DHGR) must select PAGE1 before any keystroke");
        Assert::IsTrue (ss->IsHiresMode (),
            L"Mode 0 (DHGR) must enable HIRES");

        // ----- Verify framebuffer contents at boot landing -----
        // Stage 2 init reads HGR2 bands -> $4000, DHGR aux scratch
        // -> $6000, DHGR main scratch -> $8000, then memcpys
        // cassowary $2000 -> $A000 (stash), then enters DHGR mode
        // (which copies $8000 -> $2000 main and $6000 -> $2000 aux).
        // So at boot landing: $2000=DHGR main half, $4000=bands,
        // $6000=DHGR aux scratch (still resident), $8000=DHGR main
        // scratch (still resident), $A000=stashed cassowary, and
        // aux $2000 = DHGR aux half.
        auto VerifyMemRange = [&] (Word baseAddr,
                                   const std::vector<Byte> & expected,
                                   const wchar_t * label)
        {
            size_t  mismatchCount      = 0;
            size_t  firstMismatch      = 0;
            Byte    expectedAtMismatch = 0;
            Byte    actualAtMismatch   = 0;

            for (size_t i = 0; i < kHgrPayloadSize; i++)
            {
                Byte  e = expected[i];
                Byte  actual = core.bus->ReadByte (
                    static_cast<Word> (baseAddr + i));

                if (actual != e)
                {
                    if (mismatchCount == 0)
                    {
                        firstMismatch       = i;
                        expectedAtMismatch  = e;
                        actualAtMismatch    = actual;
                    }
                    mismatchCount++;
                }
            }

            if (mismatchCount != 0)
            {
                wchar_t  msg[256] = {};
                swprintf_s (msg, L"%ls memory mismatch: %zu of %zu bytes "
                                 L"differ. First at $%04X: expected $%02X, "
                                 L"got $%02X.",
                            label,
                            mismatchCount,
                            kHgrPayloadSize,
                            static_cast<unsigned> (baseAddr + firstMismatch),
                            static_cast<unsigned> (expectedAtMismatch),
                            static_cast<unsigned> (actualAtMismatch));
                Assert::Fail (msg);
            }
        };

        VerifyMemRange (0x2000, dhgrMainPayload,
            L"DHGR main half at boot landing (main $2000)");
        VerifyMemRange (0x4000, bandsPayload,
            L"HGR2 bands (main $4000)");
        VerifyMemRange (0x6000, dhgrAuxPayload,
            L"DHGR aux scratch (main $6000)");
        VerifyMemRange (0x8000, dhgrMainPayload,
            L"DHGR main scratch (main $8000)");
        VerifyMemRange (0xA000, hgrPayload,
            L"Stashed HGR1 cassowary (main $A000)");

        // The DHGR aux half is at aux $2000 — read via MMU aux buffer.
        Byte *  auxBuf = core.mmu->GetAuxBuffer ();
        Assert::IsNotNull (auxBuf, L"MMU aux buffer must be available");
        {
            size_t  m = 0;
            for (size_t i = 0; i < kHgrPayloadSize; i++)
            {
                if (auxBuf[0x2000 + i] != dhgrAuxPayload[i]) { m++; }
            }
            Assert::AreEqual (size_t (0), m,
                L"DHGR aux half at boot landing must match payload");
        }

        // ----- Cycle through the 4 display modes with keystrokes -----
        Assert::IsNotNull (core.keyboard.get (), L"AppleKeyboard must be present");

        // Keystroke 1 -> mode 1 (HGR1 cassowary). Restores cassowary
        // from main $A000 stash to main $2000, disables DHGR-specific
        // soft switches, returns to vanilla HGR.
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (300'000ULL);
        Assert::IsTrue (ss->IsHiresMode (),
            L"Mode 1 (HGR1) must keep HIRES on");
        Assert::IsFalse (ss->IsPage2 (),
            L"Mode 1 (HGR1) must select PAGE1");
        VerifyMemRange (0x2000, hgrPayload,
            L"HGR1 cassowary restored to main $2000 in mode 1");

        // Keystroke 2 -> mode 2 (HGR2 bands).
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (200'000ULL);
        Assert::IsTrue (ss->IsPage2 (),
            L"Mode 2 (HGR2) must enable PAGE2");
        Assert::IsTrue (ss->IsHiresMode (),
            L"Mode 2 (HGR2) must keep HIRES on");

        // Keystroke 3 -> mode 3 (LoRes).
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (200'000ULL);
        Assert::IsFalse (ss->IsHiresMode (),
            L"Mode 3 (LoRes) must clear HIRES");
        Assert::IsTrue (ss->IsGraphicsMode (),
            L"Mode 3 (LoRes) must keep TEXT off");
        Assert::IsFalse (ss->IsPage2 (),
            L"Mode 3 (LoRes) must clear PAGE2");

        // Spot-check the LoRes pattern landed in text page 1.
        for (size_t i = 0; i < kLoresPayloadSize; i++)
        {
            Byte  actual = core.bus->ReadByte (
                static_cast<Word> (0x0400 + i));
            Byte  e      = loresPayload[i];
            if (actual != e)
            {
                wchar_t  msg[256] = {};
                swprintf_s (msg, L"LoRes pattern copy mismatch at $%04X: "
                                 L"expected $%02X, got $%02X.",
                            static_cast<unsigned> (0x0400 + i),
                            static_cast<unsigned> (e),
                            static_cast<unsigned> (actual));
                Assert::Fail (msg);
            }
        }

        // Keystroke 4 -> past last mode -> JMP ($FFFC) -> //e RESET.MGR
        // -> Applesoft. Just assert we're executing in ROM.
        core.keyboard->KeyPressRaw (' ');
        core.RunCycles (50'000ULL);
        Assert::IsTrue (core.cpu->GetPC () >= 0xD000,
            L"After cycling past last mode, demo must JMP into ROM "
            L"($D000+, typically the Applesoft cold start at $E000)");

        // Side effect: emit the .dsk alongside the source so the demo can
        // also be booted in the GUI without re-running the test. Best-
        // effort -- silent failure on read-only checkouts (CI).
        fs::path  dskOut = src.parent_path () / "casso-rocks.dsk";
        std::ofstream  out (dskOut, std::ios::binary);
        if (out)
        {
            out.write (reinterpret_cast<const char *> (raw.data ()),
                       static_cast<std::streamsize> (raw.size ()));
        }
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Applesoft hi-res color-mask sweep
    //
    //  Companion gate to the demo-disk boot test above, covering the far
    //  end of the same hi-res pipeline: the Applesoft ROM's own hi-res
    //  package rather than a hand-written RWTS.
    //
    //  The Applesoft one-liner
    //
    //      HGR : FOR J=0 TO 255 : POKE 228,J : HPLOT 0,0 : CALL -3082 : NEXT
    //
    //  walks all 256 raw color masks. `HCOLOR=` can only reach the eight
    //  masks in the ROM table at $F6F6 (00 2A 55 7F 80 AA D5 FF); poking
    //  228 ($E4) reaches every one, which makes the sweep an exhaustive
    //  pass over the hi-res renderer's whole input space -- each 7-bit
    //  pixel pattern against both palette (high-bit) states -- presented
    //  as stable, full-screen, uniform fields where a wrong artifact
    //  color is unmissable.
    //
    //  Two ROM details the sweep depends on, both asserted below:
    //
    //    - $F450 is the ONLY read of location 228 in the entire 16 KB
    //      ROM. It copies $E4 into the working color at $1C, and the
    //      fill routine paints from $1C. The HPLOT is therefore not
    //      decorative -- it is the sole bridge from the poke to the
    //      screen. Without it the sweep paints 256 identical black
    //      frames.
    //    - x = 0 lands in byte 0 of the scanline, an even byte, so the
    //      phase-advance branch at $F454 is not taken and $1C receives
    //      the mask verbatim instead of phase-flipped.
    //
    ////////////////////////////////////////////////////////////////////////////

    // Applesoft hi-res ROM entry points (Apple //e ROM image, $C000-$FFFF).
    static constexpr Word      kRomHgr              = 0xF3E2;   // HGR: page 1, MIXED on
    static constexpr Word      kRomHplot0           = 0xF457;   // plot (X/Y, A) using $E4
    static constexpr Word      kRomBkgnd            = 0xF3F6;   // CALL -3082: fill from $1C

    // Applesoft hi-res zero page.
    static constexpr Word      kZpWorkColor         = 0x001C;   // the byte BKGND paints
    static constexpr Word      kZpColorMask         = 0x00E4;   // 228: what HCOLOR= writes
    static constexpr Word      kZpHiresPage         = 0x00E6;   // $20 page 1, $40 page 2

    static constexpr Word      kHiresPage1Base      = 0x2000;
    static constexpr Word      kHiresPage1LastByte  = 0x3FFF;
    static constexpr Byte      kHiresPage1High      = 0x20;
    static constexpr size_t    kHiresPageBytes      = 8192;
    static constexpr size_t    kColorMaskCount      = 256;
    static constexpr Byte      kLastColorMask       = 0xFF;

    // The phase-advance helper at $F47C/$F47E inverts the seven pixel
    // bits between bytes only when the mask's pixel bits land in this
    // range -- exactly the masks whose pattern does not tile evenly
    // across a 7-pixel byte. The range is symmetric under 7-bit
    // complement, so a qualifying mask toggles stably rather than
    // drifting.
    static constexpr Byte      kMaskPixelBits       = 0x7F;
    static constexpr Byte      kFlipRangeFirst      = 0x20;
    static constexpr Byte      kFlipRangeLast       = 0x5F;

    // Harness stubs, planted in the $0300 free space below the //e's
    // page-3 vectors.
    static constexpr Word      kSweepStubBase       = 0x0300;
    static constexpr Word      kSweepStubMaskAt     = 0x0301;   // patched per mask
    static constexpr Word      kSweepStubDone       = 0x0310;
    static constexpr Word      kHgrStubBase         = 0x0320;
    static constexpr Word      kHgrStubDone         = 0x0323;

    static constexpr Byte      kOpLdaImm            = 0xA9;
    static constexpr Byte      kOpStaZp             = 0x85;
    static constexpr Byte      kOpLdxImm            = 0xA2;
    static constexpr Byte      kOpLdyImm            = 0xA0;
    static constexpr Byte      kOpJsrAbs            = 0x20;
    static constexpr Byte      kOpJmpAbs            = 0x4C;

    static constexpr Word      kByteMask            = 0x00FF;
    static constexpr int       kByteShift           = 8;

    // 1.0205 MHz: the //e's effective CPU rate once the long-cycle
    // stretch is folded in. Used only to report the sweep in seconds.
    static constexpr double    kCpuClockHz          = 1020484.0;

    static constexpr uint64_t  kSweepColdBootCycles = 5'000'000ULL;
    static constexpr uint64_t  kOneFillCycleBudget  = 500'000ULL;
    static constexpr uint64_t  kSweepPollChunk      = 100'000ULL;
    static constexpr uint64_t  kSweepCycleCeiling   = 200'000'000ULL;

    static constexpr int       kFbWidth             = 560;
    static constexpr int       kFbHeight            = 384;

    // Golden hash over all 256 rendered color fields, captured from the
    // first deterministic run. Any change to NtscColorTable, the hi-res
    // scanline walk, or the framebuffer pixel layout moves it.
    static constexpr uint64_t  kSweepRenderHash     = 0xD883834A016C9325ULL;

    static constexpr const char *  kSweepOneLiner =
        "HGR : FOR J=0 TO 255 : POKE 228,J : HPLOT 0,0 : CALL -3082 : NEXT";


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ExpectedFillByte
    //
    //  Independent model of what BKGND ($F3F6) paints, derived from the
    //  routine rather than captured from it. BKGND writes the working
    //  color at every byte of the 8 KB page and calls the phase-advance
    //  helper after each one, so a mask whose pixel bits fall in
    //  $20-$5F alternates mask / mask^$7F down the page and every other
    //  mask paints uniformly. Byte 0 always gets the mask itself.
    //
    ////////////////////////////////////////////////////////////////////////////

    static Byte ExpectedFillByte (Byte mask, size_t byteIndex)
    {
        Byte   pixelBits  = static_cast<Byte> (mask & kMaskPixelBits);
        bool   alternates = (pixelBits >= kFlipRangeFirst) &&
                            (pixelBits <= kFlipRangeLast);



        if (alternates && ((byteIndex & 1) != 0))
        {
            return static_cast<Byte> (mask ^ kMaskPixelBits);
        }

        return mask;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  RunUntilPc
    //
    //  Steps the CPU until the program counter reaches `stopPc` or the
    //  cycle budget is exhausted, mirroring EmulatorCore::RunCycles'
    //  step/accumulate pairing. Returns the cycles actually consumed so
    //  callers can time an individual ROM call.
    //
    ////////////////////////////////////////////////////////////////////////////

    static uint64_t RunUntilPc (EmulatorCore & core, Word stopPc, uint64_t cycleBudget)
    {
        uint64_t   startCycles = core.cpu->GetTotalCycles();
        uint64_t   spent       = 0;
        uint32_t   stepCycles  = 0;



        while (spent < cycleBudget)
        {
            if (core.cpu->GetPC() == stopPc)
            {
                break;
            }

            core.cpu->StepOne();
            stepCycles = core.cpu->GetLastInstructionCycles();
            core.cpu->AddCycles (stepCycles);
            spent = core.cpu->GetTotalCycles() - startCycles;
        }

        return spent;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  PlantBytes
    //
    ////////////////////////////////////////////////////////////////////////////

    static void PlantBytes (EmulatorCore & core, Word base, const Byte * code, size_t len)
    {
        size_t   i = 0;



        for (i = 0; i < len; i++)
        {
            core.bus->WriteByte (static_cast<Word> (base + i), code[i]);
        }
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  LowByte / HighByte
    //
    ////////////////////////////////////////////////////////////////////////////

    static Byte LowByte  (Word value) { return static_cast<Byte> (value & kByteMask); }
    static Byte HighByte (Word value) { return static_cast<Byte> (value >> kByteShift); }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  PlantHgrStub — JSR $F3E2 then park on a self-JMP the test can poll.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void PlantHgrStub (EmulatorCore & core)
    {
        const Byte   code[] =
        {
            kOpJsrAbs, LowByte (kRomHgr),      HighByte (kRomHgr),
            kOpJmpAbs, LowByte (kHgrStubDone), HighByte (kHgrStubDone)
        };



        PlantBytes (core, kHgrStubBase, code, sizeof (code));
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  PlantSweepStub
    //
    //  One iteration of the Applesoft loop body, in 6502: store the mask
    //  into 228, HPLOT 0,0 (A = Y coordinate, X/Y = 16-bit X coordinate),
    //  then CALL -3082. Parks on a self-JMP so the test can detect the
    //  return without guessing a cycle count.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void PlantSweepStub (EmulatorCore & core)
    {
        const Byte   code[] =
        {
            kOpLdaImm, 0,                                           // patched per mask
            kOpStaZp,  LowByte (kZpColorMask),
            kOpLdaImm, 0,                                           // hi-res Y coordinate
            kOpLdxImm, 0,                                           // X coordinate low
            kOpLdyImm, 0,                                           // X coordinate high
            kOpJsrAbs, LowByte (kRomHplot0),     HighByte (kRomHplot0),
            kOpJsrAbs, LowByte (kRomBkgnd),      HighByte (kRomBkgnd),
            kOpJmpAbs, LowByte (kSweepStubDone), HighByte (kSweepStubDone)
        };



        PlantBytes (core, kSweepStubBase, code, sizeof (code));
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Fnv1a64 — stable framebuffer hash for the golden-render gate.
    //
    ////////////////////////////////////////////////////////////////////////////

    static uint64_t Fnv1a64 (uint64_t seed, const uint32_t * data, size_t count)
    {
        uint64_t   h = seed;
        size_t     i = 0;
        int        b = 0;
        uint32_t   v = 0;



        for (i = 0; i < count; i++)
        {
            v = data[i];

            for (b = 0; b < 4; b++)
            {
                h ^= static_cast<uint8_t> ((v >> (b * 8)) & kByteMask);
                h *= 0x100000001b3ULL;
            }
        }

        return h;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  VerifySweptPage — compares the whole hi-res page against the model
    //  for one mask, failing once with a located diff rather than 8192
    //  times.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void VerifySweptPage (EmulatorCore & core, Byte mask)
    {
        size_t   byteIndex       = 0;
        size_t   mismatchCount   = 0;
        size_t   firstMismatch   = 0;
        Byte     expected        = 0;
        Byte     actual          = 0;
        Byte     expectedAtFirst = 0;
        Byte     actualAtFirst   = 0;
        wchar_t  msg[256]        = {};



        for (byteIndex = 0; byteIndex < kHiresPageBytes; byteIndex++)
        {
            expected = ExpectedFillByte (mask, byteIndex);
            actual   = core.bus->ReadByte (
                static_cast<Word> (kHiresPage1Base + byteIndex));

            if (actual == expected)
            {
                continue;
            }

            if (mismatchCount == 0)
            {
                firstMismatch   = byteIndex;
                expectedAtFirst = expected;
                actualAtFirst   = actual;
            }

            mismatchCount++;
        }

        if (mismatchCount != 0)
        {
            swprintf_s (msg, L"Mask $%02X: %zu of %zu page bytes differ from "
                             L"the BKGND model. First at $%04X: expected "
                             L"$%02X, got $%02X.",
                        static_cast<unsigned> (mask),
                        mismatchCount,
                        kHiresPageBytes,
                        static_cast<unsigned> (kHiresPage1Base + firstMismatch),
                        static_cast<unsigned> (expectedAtFirst),
                        static_cast<unsigned> (actualAtFirst));
            Assert::Fail (msg);
        }
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Drives the real ROM routines over all 256 color masks and checks
    //  every byte of the hi-res page against the independent model.
    //
    //  Exercises: Apple2e.rom Applesoft hi-res package -> 6502 CPU ->
    //  Apple2eMmu page-1 writes -> Apple2eSoftSwitchBank mode latching.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Applesoft_HgrColorSweep_AllMasksMatchRomFill)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        uint64_t       fillCycles      = 0;
        uint64_t       totalFillCycles = 0;
        uint64_t       minFillCycles   = UINT64_MAX;
        uint64_t       maxFillCycles   = 0;
        size_t         maskIndex       = 0;
        Byte           mask            = 0;

        HRESULT   hr = host.BuildApple2e (core);



        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2e must succeed");
        Assert::IsTrue (core.HasApple2e(), L"//e wiring must be complete");

        core.PowerCycle();
        core.RunCycles  (kSweepColdBootCycles);

        // ----- HGR: select page 1, clear it, latch the display mode -----
        PlantHgrStub (core);
        core.cpu->SetPC (kHgrStubBase);
        fillCycles = RunUntilPc (core, kHgrStubDone, kOneFillCycleBudget);

        Assert::AreEqual (Word (kHgrStubDone), core.cpu->GetPC(),
            L"HGR ($F3E2) must return within the cycle budget");
        Assert::AreEqual (kHiresPage1High, core.bus->ReadByte (kZpHiresPage),
            L"HGR must set HPAG ($E6) to $20 (hi-res page 1)");
        Assert::IsTrue  (core.softSwitches->IsGraphicsMode(),
            L"HGR must clear TEXT");
        Assert::IsTrue  (core.softSwitches->IsHiresMode(),
            L"HGR must set HIRES");
        Assert::IsTrue  (core.softSwitches->IsMixedMode(),
            L"HGR (unlike HGR2) must leave MIXED on -- $F3E7 touches $C053, "
            L"so the bottom four text rows stay visible");
        Assert::IsFalse (core.softSwitches->IsPage2(),
            L"HGR must select PAGE1");

        // ----- Sweep every raw color mask -----
        PlantSweepStub (core);

        for (maskIndex = 0; maskIndex < kColorMaskCount; maskIndex++)
        {
            mask = static_cast<Byte> (maskIndex);

            core.bus->WriteByte (kSweepStubMaskAt, mask);
            core.cpu->SetPC (kSweepStubBase);

            fillCycles = RunUntilPc (core, kSweepStubDone, kOneFillCycleBudget);

            Assert::AreEqual (Word (kSweepStubDone), core.cpu->GetPC(),
                L"HPLOT + BKGND must return within the cycle budget");

            totalFillCycles += fillCycles;

            if (fillCycles < minFillCycles) { minFillCycles = fillCycles; }
            if (fillCycles > maxFillCycles) { maxFillCycles = fillCycles; }

            // HPLOT copies 228 into the working color; 8192 phase flips
            // is an even number, so $1C lands back on the mask itself.
            Assert::AreEqual (mask, core.bus->ReadByte (kZpColorMask),
                L"The fill must not disturb the poked mask at 228 ($E4)");
            Assert::AreEqual (mask, core.bus->ReadByte (kZpWorkColor),
                L"HPLOT 0,0 must copy 228 into the working color at $1C "
                L"verbatim (byte 0 is even, so no phase flip)");

            VerifySweptPage (core, mask);
        }

        Logger::WriteMessage (std::format (
            "HGR color sweep: {} masks, {} ROM cycles total "
            "({:.2f} s emulated), per fill min {} / max {} cycles "
            "({:.3f} / {:.3f} s).\n",
            kColorMaskCount,
            totalFillCycles,
            static_cast<double> (totalFillCycles) / kCpuClockHz,
            minFillCycles,
            maxFillCycles,
            static_cast<double> (minFillCycles) / kCpuClockHz,
            static_cast<double> (maxFillCycles) / kCpuClockHz).c_str());
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Renders all 256 modeled color-mask fields through AppleHiResMode
    //  and folds every frame into one FNV-1a-64 hash. The fields come
    //  from the model (validated against the real ROM by the test above),
    //  so this isolates the NTSC artifact renderer: any palette
    //  inversion, half-dot-shift error, or byte-boundary phase bug moves
    //  the hash.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Applesoft_HgrColorSweep_RendersDeterministicFrames)
    {
        HeadlessHost            host;
        EmulatorCore            core;
        std::vector<uint32_t>   fb (static_cast<size_t> (kFbWidth) * kFbHeight, 0);
        uint64_t                hash      = 0xcbf29ce484222325ULL;
        size_t                  maskIndex = 0;
        size_t                  byteIndex = 0;
        Byte                    mask      = 0;

        HRESULT   hr = host.BuildApple2e (core);



        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2e must succeed");
        Assert::IsTrue (core.HasApple2e(), L"//e wiring must be complete");

        core.PowerCycle();
        core.RunCycles  (kSweepColdBootCycles);

        AppleHiResMode   hires (*core.bus);

        hires.SetPage2 (false);

        for (maskIndex = 0; maskIndex < kColorMaskCount; maskIndex++)
        {
            mask = static_cast<Byte> (maskIndex);

            for (byteIndex = 0; byteIndex < kHiresPageBytes; byteIndex++)
            {
                core.bus->WriteByte (
                    static_cast<Word> (kHiresPage1Base + byteIndex),
                    ExpectedFillByte (mask, byteIndex));
            }

            hires.Render (nullptr, fb.data(), kFbWidth, kFbHeight);
            hash = Fnv1a64 (hash, fb.data(), fb.size());
        }

        Logger::WriteMessage (std::format (
            "HGR color sweep render hash: 0x{:016X}\n", hash).c_str());

        Assert::AreEqual (kSweepRenderHash, hash, std::format (
            L"Color-sweep render hash mismatch: got 0x{:016X}", hash).c_str());
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Types the actual Applesoft one-liner at the `]` prompt and runs it
    //  to completion, measuring how long the full 256-step sweep takes on
    //  a real //e. Covers the path the ROM-driven test above skips:
    //  Applesoft tokenizing, FOR/NEXT, POKE and CALL argument
    //  evaluation, and the HPLOT statement handler.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Applesoft_HgrColorSweep_OneLinerRunsToCompletion)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        size_t         consumed    = 0;
        bool           returnTaken = false;
        uint64_t       startCycles = 0;
        uint64_t       elapsed     = 0;

        HRESULT   hr = host.BuildApple2e (core);



        Assert::IsTrue (SUCCEEDED (hr), L"BuildApple2e must succeed");
        Assert::IsTrue (core.HasApple2e(), L"//e wiring must be complete");

        core.PowerCycle();
        core.RunCycles  (kSweepColdBootCycles);

        consumed = KeystrokeInjector::InjectString (core, kSweepOneLiner);
        Assert::AreEqual (strlen (kSweepOneLiner), consumed,
            L"The whole one-liner must be consumed by the ROM input routine");

        returnTaken = KeystrokeInjector::InjectKey (
            core, KeystrokeInjector::kAppleReturn);
        Assert::IsTrue (returnTaken, L"Return must be consumed");

        startCycles = core.cpu->GetTotalCycles();

        // Completion signal: the loop has reached its last mask AND that
        // mask's fill has written the final byte of the page. $FF at an
        // odd page byte is unique to mask $FF -- the only other mask that
        // could place it there is $80, whose pixel bits are $00 and so
        // never phase-flips.
        while (elapsed < kSweepCycleCeiling)
        {
            core.RunCycles (kSweepPollChunk);
            elapsed = core.cpu->GetTotalCycles() - startCycles;

            if (core.bus->ReadByte (kZpColorMask) == kLastColorMask &&
                core.bus->ReadByte (kHiresPage1LastByte) == kLastColorMask)
            {
                break;
            }
        }

        Assert::IsTrue (elapsed < kSweepCycleCeiling,
            L"The one-liner must finish all 256 steps within the ceiling");

        Assert::IsTrue (core.softSwitches->IsHiresMode(),
            L"The sweep must leave the //e in hi-res");
        Assert::IsTrue (core.softSwitches->IsMixedMode(),
            L"HGR leaves MIXED on, so the text window stays visible");

        VerifySweptPage (core, kLastColorMask);

        Logger::WriteMessage (std::format (
            "Applesoft one-liner: 256 steps in {} cycles "
            "({:.2f} s emulated at {:.4f} MHz, {:.2f} fills/sec).\n",
            elapsed,
            static_cast<double> (elapsed) / kCpuClockHz,
            kCpuClockHz / 1e6,
            static_cast<double> (kColorMaskCount) /
                (static_cast<double> (elapsed) / kCpuClockHz)).c_str());
    }
};
