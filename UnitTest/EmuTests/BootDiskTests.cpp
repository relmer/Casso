#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include <filesystem>
#include <fstream>

#include "Cpu.h"
#include "Assembler.h"
#include "AssemblerTypes.h"

#include "HeadlessHost.h"
#include "TextScreenScraper.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/DiskIIController.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;





////////////////////////////////////////////////////////////////////////////////
//
//  BootDiskTests
//
//  End-to-end gate for the full Apple //e disk-boot pipeline using a
//  100% in-house demo disk -- no third-party software, no copyrighted
//  content. Source for the demo lives at Apple2/Demos/casso-rocks.a65;
//  this test assembles it via Casso's own AS65-compatible assembler,
//  builds a synthetic .dsk image with the resulting machine code at
//  sector 0, mounts it through DiskImageStore, lets the //e boot ROM
//  pick it up, and verifies the demo's "CASSO ROCKS!" banner appears
//  on the rendered text screen.
//
//  Exercises: Assembler -> NibblizationLayer -> DiskImageStore ->
//  DiskIIController -> DiskIINibbleEngine -> disk2.rom slot 6 boot ->
//  6502 CPU executing our code -> MMU text-page write -> renderer's
//  text-screen scraper.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int  kMaxAncestorWalk = 10;


    // Walk up from CWD to find the directory containing a given relative
    // path, so the test stays filesystem-portable across CI vs local.
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
}


TEST_CLASS (BootDiskTests)
{
public:

    TEST_METHOD (CassoRocks_DemoDisk_PrintsBanner)
    {
        fs::path src = FindRepoFile ("Apple2/Demos/casso-rocks.a65");
        if (src.empty ())
        {
            Logger::WriteMessage ("SKIPPED: Apple2/Demos/casso-rocks.a65 not "
                                  "found in this checkout.\n");
            return;
        }

        std::string source = ReadFileText (src);
        Assert::IsFalse (source.empty (), L"casso-rocks.a65 must not be empty");

        Cpu        cpu;
        Assembler  assembler (cpu.GetInstructionSet ());
        AssemblyResult  asmResult = assembler.Assemble (source);

        if (!asmResult.success)
        {
            wchar_t msg[256] = {};
            const char * firstError = asmResult.errors.empty ()
                ? "(no error message)"
                : asmResult.errors[0].message.c_str ();
            swprintf_s (msg, L"casso-rocks.a65 must assemble cleanly. First "
                             L"error: %hs", firstError);
            Assert::Fail (msg);
        }

        Assert::AreEqual (Word (0x0801), asmResult.startAddress,
            L"Demo must be assembled with .org $0801 (boot ROM JMP target)");
        Assert::IsTrue (asmResult.bytes.size () > 0 &&
                        asmResult.bytes.size () <= 256 - 1,
            L"Demo code must fit in the remainder of sector 0 ($0801-$08FF)");

        // Build a synthetic 143360-byte raw .dsk image. The boot ROM
        // loads file offset 0..255 into RAM at $0800-$08FF and JMPs to
        // $0801, so place the demo bytes at file offset 1.
        std::vector<Byte> raw (NibblizationLayer::kImageByteSize, 0);
        for (size_t i = 0; i < asmResult.bytes.size (); i++)
        {
            raw[1 + i] = asmResult.bytes[i];
        }

        HeadlessHost   host;
        EmulatorCore   core;

        HRESULT hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (6, 0, "casso-rocks.dsk",
                                             DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        DiskImage * img = core.diskStore->GetImage (6, 0);
        Assert::IsNotNull (img);
        core.diskController->SetExternalDisk (0, img);

        core.bus->WriteByte (0xC006, 0);  // INTCXROM=0
        core.cpu->SetPC (0xC600);

        // Boot ROM seek + sector read + JMP to $0801 + 1024-byte HOME +
        // 12-byte banner copy completes well under 10M cycles.
        core.RunCycles (10'000'000ULL);

        std::vector<std::string> rows = TextScreenScraper::Scrape40 (
            *core.bus, TextScreenScraper::kTextPage1);

        int  foundRow = -1;
        for (int r = 0; r < static_cast<int> (rows.size ()); r++)
        {
            if (rows[r].find ("CASSO ROCKS!") != std::string::npos)
            {
                foundRow = r;
                break;
            }
        }

        if (foundRow < 0)
        {
            wchar_t msg[2048] = {};
            wcscpy_s (msg, L"Boot disk did not display 'CASSO ROCKS!'. Screen:\n");
            for (size_t r = 0; r < rows.size () && wcslen (msg) < 1900; r++)
            {
                wchar_t line[128] = {};
                swprintf_s (line, L"%2zu: |%hs|\n", r, rows[r].c_str ());
                wcscat_s (msg, line);
            }
            Assert::Fail (msg);
        }

        Assert::AreEqual (12, foundRow,
            L"Banner must appear on row 12 (centered vertically)");

        // Side effect: emit a .dsk file alongside the source so the
        // demo can also be booted in the GUI without re-running the
        // assembler. Best-effort -- failures are silent so this works
        // on CI runners with no write access to the repo.
        fs::path dskOut = src.parent_path () / "casso-rocks.dsk";
        std::ofstream out (dskOut, std::ios::binary);
        if (out)
        {
            out.write (reinterpret_cast<const char *> (raw.data ()),
                       static_cast<std::streamsize> (raw.size ()));
        }
    }
};
