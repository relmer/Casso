#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>

#include "Cpu.h"
#include "Assembler.h"
#include "AssemblerTypes.h"

#include "HeadlessHost.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/DiskIIController.h"
#include "Devices/AppleIIeSoftSwitchBank.h"

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
//  -> DiskImageStore -> DiskIIController -> DiskIINibbleEngine ->
//  Disk2.rom slot 6 boot -> 6502 CPU executing our RWTS -> MMU HGR
//  page-1 writes -> AppleIIeSoftSwitchBank graphics-mode latching.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int     kMaxAncestorWalk     = 10;
    static constexpr size_t  kHgrPayloadSize      = 8192;
    static constexpr size_t  kSectorByteSize      = 256;
    static constexpr int     kSectorsPerTrack     = 16;
    static constexpr Word    kHgrBase             = 0x2000;
    static constexpr Word    kBootEntry           = 0xC600;
    static constexpr Word    kDemoEntry           = 0x0801;
    static constexpr uint64_t  kDemoCycleBudget   = 60'000'000ULL;


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
        fs::path  src      = FindRepoFile ("Apple2/Demos/casso-rocks.a65");
        fs::path  hgrPath  = FindRepoFile ("Apple2/Demos/cassowary.hgr");

        if (src.empty () || hgrPath.empty ())
        {
            Logger::WriteMessage ("SKIPPED: Apple2/Demos/casso-rocks.a65 or "
                                  "cassowary.hgr not found in this checkout.\n");
            return;
        }

        std::string source = ReadFileText (src);
        Assert::IsFalse (source.empty (), L"casso-rocks.a65 must not be empty");

        std::vector<Byte>  hgrPayload = ReadFileBytes (hgrPath);
        Assert::AreEqual (kHgrPayloadSize, hgrPayload.size (),
            L"cassowary.hgr must be exactly 8192 bytes (raw HGR framebuffer)");

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

        Assert::AreEqual (Word (kDemoEntry), asmResult.startAddress,
            L"Demo must be assembled with .org $0801 (boot ROM JMP target)");
        Assert::IsTrue (asmResult.bytes.size () > 0 &&
                        asmResult.bytes.size () <= kSectorByteSize - 1,
            L"Demo code must fit in the remainder of sector 0 ($0801-$08FF)");

        // Build a 143360-byte raw .dsk image.
        //   - File offset 1..N: assembled boot code (loaded by disk2.rom into
        //     $0801+).
        //   - Tracks 1+2 (file offsets $1000..$3000): 8 KB cassowary
        //     framebuffer, but laid out using the DOS 3.3 logical-to-
        //     physical interleave so that when our RWTS reads logical
        //     sector S of track T it gets exactly hgrPayload[((T-1)*16+S)*256..].
        //     The nibblizer puts logical sector S's data at file offset
        //     (T*16 + kDsk_LtoP[S])*256, so we apply the inverse mapping
        //     when stitching the payload in.
        static constexpr int  kDsk_LtoP[16] =
        {
            0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
        };

        std::vector<Byte>  raw (NibblizationLayer::kImageByteSize, 0);

        for (size_t i = 0; i < asmResult.bytes.size (); i++)
        {
            raw[1 + i] = asmResult.bytes[i];
        }

        for (int trackOffset = 0; trackOffset < 2; trackOffset++)
        {
            int  track = 1 + trackOffset;

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
                    raw[fileOffset + i] = hgrPayload[payloadOffset + i];
                }
            }
        }

        HeadlessHost  host;
        EmulatorCore  core;

        HRESULT  hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

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

        // ----- Verify HGR1 soft-switch state -----
        AppleIIeSoftSwitchBank *  ss = core.softSwitches.get ();

        Assert::IsNotNull (ss, L"AppleIIeSoftSwitchBank must be present");
        Assert::IsTrue (ss->IsGraphicsMode (),
            L"Demo must leave the //e in graphics mode (TEXT off)");
        Assert::IsFalse (ss->IsMixedMode (),
            L"Demo must leave MIXED off (full-screen HGR)");
        Assert::IsFalse (ss->IsPage2 (),
            L"Demo must leave PAGE2 off (HGR page 1 visible)");
        Assert::IsTrue (ss->IsHiresMode (),
            L"Demo must leave HIRES on");

        // ----- Verify HGR framebuffer contents -----
        // The 6502 RWTS reads 256 bytes per sector and stores them via
        // ($26),Y. Compare every byte against the source payload so any
        // single-bit error in seek / address-mark scan / 6+2 expansion
        // surfaces immediately.
        size_t  mismatchCount = 0;
        size_t  firstMismatch = 0;
        Byte    expectedAtMismatch = 0;
        Byte    actualAtMismatch   = 0;

        for (size_t i = 0; i < kHgrPayloadSize; i++)
        {
            Byte  expected = hgrPayload[i];
            Byte  actual   = core.bus->ReadByte (
                static_cast<Word> (kHgrBase + i));

            if (actual != expected)
            {
                if (mismatchCount == 0)
                {
                    firstMismatch       = i;
                    expectedAtMismatch  = expected;
                    actualAtMismatch    = actual;
                }
                mismatchCount++;
            }
        }

        if (mismatchCount != 0)
        {
            wchar_t  msg[256] = {};
            swprintf_s (msg, L"HGR framebuffer mismatch: %zu of %zu bytes "
                             L"differ. First at $%04X: expected $%02X, got "
                             L"$%02X.",
                        mismatchCount,
                        kHgrPayloadSize,
                        static_cast<unsigned> (kHgrBase + firstMismatch),
                        static_cast<unsigned> (expectedAtMismatch),
                        static_cast<unsigned> (actualAtMismatch));
            Assert::Fail (msg);
        }

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
};
