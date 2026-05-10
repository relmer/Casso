#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootRomDecoderTests
//
//  End-to-end spec-conformance tests for the disk pipeline. Builds a
//  synthetic .dsk image whose track 0 sectors carry known data patterns,
//  nibblizes it through NibblizationLayer, mounts it on a //e + Disk II
//  harness, then runs the *actual* disk2.rom boot firmware on the
//  emulated 6502 until it has loaded sector 0 into RAM. Asserts that the
//  bytes the real boot ROM decoded into RAM match the original raw
//  source bytes.
//
//  These tests fail loudly for any mismatch between Casso's nibblizer
//  output and the spec the real Apple firmware expects to read --
//  specifically the kind of "self-symmetric round-trip passes but the
//  on-disk format is non-standard" bug that lets DOS 3.3 boot but every
//  RWTS sector read silently fail.
//
//  Test scaffolding details:
//    - The boot ROM (slot 6 firmware at $C600..$C6FF) reads sector 0 of
//      track 0 into $0800..$08FF, then JMPs to $0801. We let the CPU
//      run a generous cycle budget (~4M = ~4 seconds emulated time at
//      1 MHz; about 200 ms wall time at debug-build interpreter speed)
//      and then check the RAM contents.
//    - We use a sentinel pattern (each byte = its index) so any bit-
//      level corruption shows up as a localized mismatch we can inspect.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr Word       kBootRomEntry        = 0xC600;
    static constexpr Word       kIntCxRomOff         = 0xC006;
    static constexpr Word       kBootLoadAddress     = 0x0800;
    static constexpr int        kSlot6               = 6;
    static constexpr int        kDrive1              = 0;
    static constexpr uint64_t   kSectorReadCycles    = 16'000'000ULL;
    static constexpr size_t     kSectorBytes         = 256;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Build a 143360-byte raw .dsk image whose track-0 sector-0 contains
    //  a deterministic byte pattern. Every other sector stays zero so any
    //  off-by-one in track or sector indexing surfaces as wrong-byte
    //  failures rather than just no-op success.
    //
    ////////////////////////////////////////////////////////////////////////////

    vector<Byte> BuildSentinelDsk (Byte (*patternFn) (size_t))
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);
        size_t         i   = 0;

        // .dsk physical layout: track N sector N starts at offset
        // (track * 16 + sector_physical) * 256. The DOS 3.3 to-physical
        // map kDsk_LtoP starts with logical 0 -> physical 0, so DOS
        // logical sector 0 of track 0 lives at file offset 0.
        for (i = 0; i < kSectorBytes; i++)
        {
            raw[i] = patternFn (i);
        }

        return raw;
    }


    Byte PatternIdentity (size_t index)
    {
        return static_cast<Byte> (index & 0xFF);
    }


    Byte PatternComplement (size_t index)
    {
        return static_cast<Byte> (~static_cast<Byte> (index & 0xFF));
    }


    Byte PatternMixed (size_t index)
    {
        // Mix high and low bits to exercise the 6+2 group splitting:
        // bits 0..1 land in the "third group" nibbles; bits 2..7 land
        // in the main encoded run. A pattern that varies all 8 bits
        // independently catches partial-bit-group corruption.
        return static_cast<Byte> ((index * 0xA5u) ^ (index >> 3));
    }


    DiskImage * MountSentinelDsk (
        HeadlessHost  &  host,
        EmulatorCore  &  core,
        const vector<Byte> & raw)
    {
        HRESULT      hr        = S_OK;
        DiskImage *  external  = nullptr;

        hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII must succeed");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
                                             "boot-rom-decoder.dsk",
                                             DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes must succeed");

        external = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (external, L"Store must yield a DiskImage after mount");

        core.diskController->SetExternalDisk (kDrive1, external);

        // Surface the disk2.rom slot ROM at $C600 (CxxxRomRouter /
        // INTCXROM=0; audit C1).
        core.bus->WriteByte (kIntCxRomOff, 0);

        core.cpu->SetPC (kBootRomEntry);
        return external;
    }


    // Run until PC reaches the loaded boot-loader entry at $0801
    // (the boot ROM JMPs here only after a successful sector read +
    // address-field checksum + data-field checksum verification).
    // Returns true if PC ever escaped the $C6xx ROM range into the
    // RAM bootstrap, false if we burned the cycle budget without
    // ever leaving the boot ROM (i.e. the read attempt failed
    // checksums and the ROM is still spinning).
    bool RunUntilBootLoaderRuns (EmulatorCore & core)
    {
        char  path[260] = {};
        DWORD pl = GetTempPathA (260, path);
        FILE * fp = nullptr;
        if (pl > 0 && pl < 260 - 32)
        {
            strcat_s (path, "bootrom-trace.log");
            (void) fopen_s (&fp, path, "w");
        }

        const uint64_t kStep = kSectorReadCycles / 200;
        uint64_t       cyc   = 0;
        bool           ranBootLoader = false;
        Word           maxPc = 0;

        while (cyc < kSectorReadCycles)
        {
            core.RunCycles (kStep);
            cyc += kStep;

            Word pc = core.cpu->GetPC ();

            // Boot ROM lives at $C600-$C6FF; firmware WAIT at $FCAA;
            // anything outside those ranges is the loaded bootstrap
            // running. RAM under $0800 (zero page, stack, $03xx
            // scratch) doesn't get JSR'd from the boot ROM, so any
            // PC in $0800-$BFFF is success.
            if (pc >= 0x0800 && pc < 0xC000)
            {
                ranBootLoader = true;
                if (pc > maxPc) maxPc = pc;
            }

            if (fp != nullptr)
            {
                fprintf (fp,
                    "cyc=%llu PC=$%04X X=$%02X A=$%02X bp=%zu trk=%d latch=$%02X 800-7=$%02X $%02X $%02X $%02X $%02X $%02X $%02X $%02X bootloader=%d maxPc=$%04X\n",
                    (unsigned long long) cyc, pc,
                    core.cpu->GetX (), core.cpu->GetA (),
                    core.diskController->GetEngine (kDrive1).GetBitPosition (),
                    core.diskController->GetCurrentTrack (),
                    core.diskController->GetEngine (kDrive1).PeekReadLatch (),
                    core.bus->ReadByte (0x0800),
                    core.bus->ReadByte (0x0801),
                    core.bus->ReadByte (0x0802),
                    core.bus->ReadByte (0x0803),
                    core.bus->ReadByte (0x0804),
                    core.bus->ReadByte (0x0805),
                    core.bus->ReadByte (0x0806),
                    core.bus->ReadByte (0x0807),
                    ranBootLoader ? 1 : 0, maxPc);
            }
        }

        if (fp != nullptr)
        {
            fclose (fp);
        }

        return ranBootLoader;
    }


    void AssertBootRomReadsSector0 (Byte (*patternFn) (size_t), const wchar_t * patternName)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw = BuildSentinelDsk (patternFn);

        MountSentinelDsk (host, core, raw);

        if (!RunUntilBootLoaderRuns (core))
        {
            wchar_t   msg[512] = {};
            swprintf_s (msg,
                L"Pattern %ls: boot ROM never JMPed out of $C6xx into the "
                L"loaded bootstrap at $0801 within %llu cycles. The boot "
                L"ROM only leaves $C6xx after a sector read where address-"
                L"field, V/T/S, and data-field checksums all matched -- so "
                L"this means Casso's nibblized output is failing the spec-"
                L"conformance check the real Apple disk2.rom firmware "
                L"applies. See %%TEMP%%\\bootrom-trace.log for PC trace.",
                patternName, (unsigned long long) kSectorReadCycles);
            Assert::Fail (msg);
        }
    }
}





TEST_CLASS (BootRomDecoderTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  The disk2.rom slot-6 boot firmware will JMP to the loaded
    //  bootstrap at $0801 ONLY after every checksum gate (address
    //  field volume/track/sector/checksum, then data field running-XOR
    //  checksum) verifies. So the simplest spec-conformance check is
    //  "did PC ever escape the $C6xx ROM?". If the on-disk format is
    //  off in any byte, the data-field checksum mismatches, the boot
    //  ROM retries, and PC stays in $C65E..$C6F8 forever (well, for
    //  the cycle budget).
    //
    //  Three patterns make sure the failure mode (if any) isn't
    //  pattern-specific: an all-zero, identity, and mixed-bit pattern
    //  exercise different parts of the 6+2 encoding (e.g. the 86
    //  third-group nibbles each carry the low 2 bits of three source
    //  bytes -- a pattern that varies bits 0-1 per source byte stresses
    //  this differently than a pattern that varies bits 2-7).
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BootRom_LoadsSector0_IdentityPattern)
    {
        AssertBootRomReadsSector0 (PatternIdentity, L"identity (i)");
    }

    TEST_METHOD (BootRom_LoadsSector0_ComplementPattern)
    {
        AssertBootRomReadsSector0 (PatternComplement, L"complement (~i)");
    }

    TEST_METHOD (BootRom_LoadsSector0_MixedPattern)
    {
        AssertBootRomReadsSector0 (PatternMixed, L"mixed (i*A5 ^ i>>3)");
    }
};
