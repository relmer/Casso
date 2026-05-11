#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "HeadlessHost.h"
#include "Devices/Disk/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskReadbackTests
//
//  End-to-end "Casso writes a disk image, then Casso's DiskIIController
//  + LSS engine reads it back through the bus" tests. No boot ROM, no
//  CPU -- just the disk subsystem driven via direct controller API
//  calls and bus reads of $C0E0-$C0EF (slot 6).
//
//  This complements BootRomDecoderTests: those exercise the on-disk
//  format against the real disk2.rom firmware as a spec oracle. These
//  exercise the runtime path -- phase stepping, multi-track reads,
//  multi-sector reads, motor on/off cycles -- in a way the boot ROM
//  tests can't (the boot ROM only reads sector 0 of track 0 with
//  the head already at home position).
//
//  Each disk is built with a unique sentinel byte at the start of every
//  (track, sector) so any byte-swap / off-by-one in the indexing
//  surfaces as a clear "T17S00 expected $11, got $XX" message.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr int        kSlot6               = 6;
    static constexpr int        kDrive1              = 0;
    static constexpr int        kSectorBytes         = 256;
    static constexpr int        kSectorsPerTrack     = 16;
    static constexpr int        kTrackCount          = 35;

    // I/O offsets within the slot 6 soft-switch range $C0E0-$C0EF.
    static constexpr Word       kSlotIoBase          = 0xC0E0;
    static constexpr Word       kPhase0Off           = kSlotIoBase + 0x0;
    static constexpr Word       kPhase0On            = kSlotIoBase + 0x1;
    static constexpr Word       kPhase1Off           = kSlotIoBase + 0x2;
    static constexpr Word       kPhase1On            = kSlotIoBase + 0x3;
    static constexpr Word       kPhase2Off           = kSlotIoBase + 0x4;
    static constexpr Word       kPhase2On            = kSlotIoBase + 0x5;
    static constexpr Word       kPhase3Off           = kSlotIoBase + 0x6;
    static constexpr Word       kPhase3On            = kSlotIoBase + 0x7;
    static constexpr Word       kMotorOff            = kSlotIoBase + 0x8;
    static constexpr Word       kMotorOn             = kSlotIoBase + 0x9;
    static constexpr Word       kSelectDrive1        = kSlotIoBase + 0xA;
    static constexpr Word       kSelectDrive2        = kSlotIoBase + 0xB;
    static constexpr Word       kReadLatch           = kSlotIoBase + 0xC;
    static constexpr Word       kQ6On                = kSlotIoBase + 0xD;
    static constexpr Word       kQ7Off               = kSlotIoBase + 0xE;
    static constexpr Word       kQ7On                = kSlotIoBase + 0xF;

    static constexpr Byte       kAddrProlog0         = 0xD5;
    static constexpr Byte       kAddrProlog1         = 0xAA;
    static constexpr Byte       kAddrProlog2         = 0x96;
    static constexpr Byte       kDataProlog2         = 0xAD;

    // Generous tick budget per attempted byte-fetch -- one revolution
    // is ~50,000 bits = ~12,500 nibbles, plus margin for the latch
    // delay window. 64 cycles gets us a guaranteed advance of one
    // bit-cell-pair through the LSS regardless of the current latch
    // state, so 8 calls per nibble worst case.
    static constexpr uint32_t   kTicksPerLatchPoll   = 4;
    static constexpr uint64_t   kMaxNibblesPerSector = 100'000ULL;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Synthesizers / decoders
    //
    ////////////////////////////////////////////////////////////////////////////

    // Each (track, sector)'s first byte = (track << 4) | sector_logical.
    // Subsequent bytes scale the same value by index for further
    // discrimination.
    Byte SectorSentinel (int track, int sector, int byteOffset)
    {
        Byte base = static_cast<Byte> ((track << 4) | (sector & 0x0F));
        return static_cast<Byte> (base ^ static_cast<Byte> (byteOffset));
    }


    // DOS 3.3 nibblizes physical sector P with the source bytes from
    // logical sector kDsk_LtoP[P]. So when we read physical sector P,
    // the byte we get is whatever was stamped for logical kDsk_LtoP[P].
    int PhysicalToLogicalDosSector (int physical)
    {
        static constexpr int kDsk_LtoP[16] = {
            0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
        };
        return kDsk_LtoP[physical];
    }


    Byte ExpectedSectorByte (int track, int physicalSector, int byteOffset)
    {
        int logical = PhysicalToLogicalDosSector (physicalSector);
        return SectorSentinel (track, logical, byteOffset);
    }


    vector<Byte> BuildSentinelDisk ()
    {
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);
        int            track  = 0;
        int            sector = 0;
        int            i      = 0;

        for (track = 0; track < kTrackCount; track++)
        {
            for (sector = 0; sector < kSectorsPerTrack; sector++)
            {
                size_t offset = static_cast<size_t> (track * kSectorsPerTrack + sector)
                              * kSectorBytes;

                for (i = 0; i < kSectorBytes; i++)
                {
                    raw[offset + i] = SectorSentinel (track, sector, i);
                }
            }
        }

        return raw;
    }


    DiskImage * MountAndSpinUp (HeadlessHost & host, EmulatorCore & core,
                                const vector<Byte> & raw)
    {
        HRESULT      hr        = S_OK;
        DiskImage *  external  = nullptr;

        hr = host.BuildAppleIIeWithDiskII (core);
        Assert::IsTrue (SUCCEEDED (hr), L"BuildAppleIIeWithDiskII");

        core.PowerCycle ();

        hr = core.diskStore->MountFromBytes (kSlot6, kDrive1,
                                             "readback.dsk",
                                             DiskFormat::Dsk, raw);
        Assert::IsTrue (SUCCEEDED (hr), L"MountFromBytes");

        external = core.diskStore->GetImage (kSlot6, kDrive1);
        Assert::IsNotNull (external, L"GetImage");

        core.diskController->SetExternalDisk (kDrive1, external);

        // Spin up: select drive 1, motor on, set Q7=0/Q6=0 (read mode).
        core.bus->ReadByte (kSelectDrive1);
        core.bus->ReadByte (kMotorOn);
        core.bus->ReadByte (kQ6On);     // ensures Q6=1 first
        core.bus->ReadByte (kSlotIoBase + 0xC); // Q6=0
        core.bus->ReadByte (kQ7Off);

        return external;
    }


    // Walk the head to a target full track via the controller's phase-
    // magnet API. Issues the phase-on/phase-off pulses that DOS RWTS
    // and the boot ROM use; stepping is two quarter-tracks per full
    // track and follows the standard "energize next phase, de-energize
    // previous" sequence.
    void SeekToTrack (EmulatorCore & core, int targetTrack)
    {
        int currentTrack = core.diskController->GetCurrentTrack ();

        while (currentTrack != targetTrack)
        {
            int currentQt   = core.diskController->GetQuarterTrack ();
            int targetQt    = targetTrack * 4;
            int direction   = (targetQt > currentQt) ? +1 : -1;

            int currentPhase = (currentQt / 2) & 3;
            int nextPhase    = (currentPhase + direction) & 3;

            // Energize next phase, then de-energize current. Two quarter-
            // tracks per full step.
            core.bus->ReadByte (static_cast<Word> (kSlotIoBase + 0x1 + nextPhase * 2));
            core.bus->ReadByte (static_cast<Word> (kSlotIoBase + 0x0 + currentPhase * 2));

            // Pump engine cycles so the head physically settles before
            // the next pulse.
            core.diskController->Tick (16);

            currentTrack = core.diskController->GetCurrentTrack ();

            // Safety: don't loop forever.
            if (currentTrack == targetTrack) break;
        }

        // De-energize all phases.
        for (int p = 0; p < 4; p++)
        {
            core.bus->ReadByte (static_cast<Word> (kSlotIoBase + 0x0 + p * 2));
        }
        core.diskController->Tick (16);
    }


    // Pull the next valid (MSB-set) nibble from the controller's read
    // latch. After capturing one nibble, advances the engine ~10 cycles
    // (past the LSS latch-hold window) so the subsequent call returns
    // a fresh nibble rather than the same captured value again.
    Byte ReadNextNibble (EmulatorCore & core, uint64_t & ticksOut)
    {
        Byte    latch = 0;
        const uint64_t budget = 500'000ULL;
        uint64_t i      = 0;

        for (i = 0; i < budget; i += kTicksPerLatchPoll)
        {
            core.diskController->Tick (kTicksPerLatchPoll);
            latch = core.bus->ReadByte (kReadLatch);

            if (latch & 0x80)
            {
                // Step past the latch-hold window so the next call sees
                // the following nibble. The LSS holds a complete nibble
                // for ~2 bit cells (8 cycles) after MSB-set; ticking 12
                // here guarantees we're shifting bits into a fresh
                // working register before the caller polls again.
                core.diskController->Tick (12);
                ticksOut += i + kTicksPerLatchPoll + 12;
                return latch;
            }
        }

        ticksOut += budget;
        return 0;
    }


    Byte Decode44 (Byte odd, Byte even)
    {
        return static_cast<Byte> (((odd << 1) | 1) & even);
    }


    static const Byte sg_kWriteTranslate[64] =
    {
        0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
        0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
        0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
        0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
        0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
        0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
        0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
        0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
    };


    Byte InverseTranslate (Byte nib)
    {
        for (int i = 0; i < 64; i++)
        {
            if (sg_kWriteTranslate[i] == nib) return static_cast<Byte> (i);
        }
        return 0xFF;
    }


    // Spin the bus reading the latch until we find an address-field
    // matching the requested logical sector on the current track.
    // Returns true on success; the caller can then call
    // ReadDataFieldAtCursor to harvest the 256 decoded bytes.
    bool FindAddressField (EmulatorCore & core, int wantTrack, int wantSector,
                           int & outVolume)
    {
        Byte    n0    = 0, n1 = 0, n2 = 0;
        uint64_t spent = 0;
        // Bound: ~3 disk revolutions worth of cycles.
        const uint64_t kMaxSpent = 1'500'000ULL;

        while (spent < kMaxSpent)
        {
            n0 = ReadNextNibble (core, spent);
            if (n0 == 0) return false;        // hit the per-nibble timeout
            if (n0 != kAddrProlog0) continue;

            n1 = ReadNextNibble (core, spent);
            if (n1 != kAddrProlog1) continue;

            n2 = ReadNextNibble (core, spent);
            if (n2 != kAddrProlog2) continue;

            Byte vOdd  = ReadNextNibble (core, spent);
            Byte vEven = ReadNextNibble (core, spent);
            Byte tOdd  = ReadNextNibble (core, spent);
            Byte tEven = ReadNextNibble (core, spent);
            Byte sOdd  = ReadNextNibble (core, spent);
            Byte sEven = ReadNextNibble (core, spent);

            int  vol  = Decode44 (vOdd, vEven);
            int  trk  = Decode44 (tOdd, tEven);
            int  sec  = Decode44 (sOdd, sEven);

            if (trk == wantTrack && sec == wantSector)
            {
                outVolume = vol;
                // skip the checksum nibble pair + epilogue (3 nibbles)
                ReadNextNibble (core, spent);
                ReadNextNibble (core, spent);
                ReadNextNibble (core, spent);
                ReadNextNibble (core, spent);
                ReadNextNibble (core, spent);
                return true;
            }
        }

        return false;
    }


    // After FindAddressField succeeds, spin until the data prologue
    // (D5 AA AD) appears, then decode the 343 nibbles and unpack
    // back into 256 bytes per the standard 6-and-2 inverse.
    bool ReadDataFieldAtCursor (EmulatorCore & core, vector<Byte> & outData)
    {
        Byte n0 = 0, n1 = 0, n2 = 0;
        uint64_t spent = 0;
        const uint64_t kMaxSpent = 500'000ULL;

        while (spent < kMaxSpent)
        {
            n0 = ReadNextNibble (core, spent);
            if (n0 == 0) return false;
            if (n0 != kAddrProlog0) continue;
            n1 = ReadNextNibble (core, spent);
            if (n1 != kAddrProlog1) continue;
            n2 = ReadNextNibble (core, spent);
            if (n2 == kDataProlog2) break;
        }

        if (spent >= kMaxSpent) return false;

        // 342 raw + 1 checksum
        Byte encoded[342] = {};
        Byte prev = 0;

        for (int i = 0; i < 342; i++)
        {
            Byte nib  = ReadNextNibble (core, spent);
            Byte raw  = static_cast<Byte> (InverseTranslate (nib) ^ prev);
            encoded[i] = raw;
            prev = raw;
        }

        // Verify checksum
        Byte chkNib = ReadNextNibble (core, spent);
        Byte chk    = static_cast<Byte> (InverseTranslate (chkNib) ^ prev);
        if (chk != 0) return false;

        // Unpack: bytes 86..341 carry high 6 bits; bytes 0..85 each
        // carry 2 low bits for three source bytes.
        outData.assign (kSectorBytes, 0);

        for (int i = 0; i < kSectorBytes; i++)
        {
            int  idx, shift;
            if (i < 86)        { idx = i;           shift = 0; }
            else if (i < 172)  { idx = i - 86;      shift = 2; }
            else               { idx = i - 172;     shift = 4; }

            Byte high = static_cast<Byte> (encoded[86 + i] << 2);
            Byte b0   = static_cast<Byte> ((encoded[idx] >> (shift + 1)) & 1);
            Byte b1   = static_cast<Byte> ((encoded[idx] >> (shift + 0)) & 1);
            outData[i] = static_cast<Byte> (high | (b0 << 0) | (b1 << 1));
        }

        return true;
    }
}





TEST_CLASS (DiskReadbackTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  Read every (track, sector) from a sentinel disk and verify each
    //  decoded sector matches what the nibblizer wrote. This exercises
    //  the whole disk subsystem -- nibblization, controller phase-step,
    //  engine bit-streaming, LSS latch -- in the same access pattern
    //  RWTS uses to read the catalog. No CPU, no boot ROM.
    //
    //  If the boot ROM tests pass but this fails, the bug is in the
    //  runtime engine path (track stepping, latch state across track
    //  changes, motor cycling). If both fail, it's the on-disk format.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (DumpFirstNibblesOnTrack0)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw = BuildSentinelDisk ();

        MountAndSpinUp (host, core, raw);

        // Dump the first 64 nibbles to %TEMP%\readback-trace.log so we
        // can see what the engine actually presents to the bus.
        char  path[260] = {};
        DWORD pl = GetTempPathA (260, path);
        FILE * fp = nullptr;
        if (pl > 0 && pl < 260 - 32)
        {
            strcat_s (path, "readback-trace.log");
            (void) fopen_s (&fp, path, "w");
        }

        uint64_t spent = 0;
        int      validCount = 0;
        for (int i = 0; i < 64; i++)
        {
            Byte n = ReadNextNibble (core, spent);
            if (fp != nullptr)
            {
                fprintf (fp, "[%2d] %02X (after %llu ticks)\n",
                         i, n, (unsigned long long) spent);
            }
            if (n != 0) validCount++;
        }

        if (fp != nullptr) fclose (fp);

        wchar_t msg[128] = {};
        swprintf_s (msg, L"Got %d/64 valid nibbles (see readback-trace.log)",
                    validCount);
        Assert::IsTrue (validCount > 32, msg);
    }


    TEST_METHOD (ReadAllSectorsOnTrack0_FromSentinelDsk)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw = BuildSentinelDisk ();

        MountAndSpinUp (host, core, raw);

        wstring  failures;
        int      successCount = 0;
        int      trk          = 0;

        for (int sec = 0; sec < kSectorsPerTrack; sec++)
        {
            int           volume = 0;
            vector<Byte>  decoded;

            if (!FindAddressField (core, trk, sec, volume))
            {
                wchar_t msg[128] = {};
                swprintf_s (msg, L"T%dS%d address-field not found. ", trk, sec);
                failures += msg;
                continue;
            }

            if (!ReadDataFieldAtCursor (core, decoded))
            {
                wchar_t msg[128] = {};
                swprintf_s (msg, L"T%dS%d data-field decode failed. ", trk, sec);
                failures += msg;
                continue;
            }

            Byte expected = ExpectedSectorByte (trk, sec, 0);
            if (decoded[0] != expected)
            {
                wchar_t msg[128] = {};
                swprintf_s (msg, L"T%dS%d byte0 expected $%02X got $%02X. ",
                            trk, sec, expected, decoded[0]);
                failures += msg;
                continue;
            }

            successCount++;
        }

        if (!failures.empty ())
        {
            wchar_t  banner[128] = {};
            swprintf_s (banner, L"%d/%d sector reads succeeded. Failures: ",
                        successCount, kSectorsPerTrack);
            wstring full = banner;
            full += failures;
            Assert::Fail (full.c_str ());
        }
    }


    // Verifies the disk pipeline (nibblization + LSS) round-trips a
    // FULL byte-range pattern -- where every encoded byte differs by
    // a controlled amount from the next -- in addition to the simple
    // sentinel sequence. The complement pattern is chosen because it
    // breaks the analogous boot-ROM test, which lets us isolate
    // whether the pipeline produces correct bytes for that data
    // (then the boot-ROM bug is in the polling-timing interaction)
    // versus mis-frames the LSS for that data (then the pipeline is
    // wrong).
    TEST_METHOD (ReadComplementPatternSector_FromCustomDsk)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw (NibblizationLayer::kImageByteSize, 0);

        for (int i = 0; i < 256; i++)
        {
            raw[i] = static_cast<Byte> (~static_cast<Byte> (i));
        }

        MountAndSpinUp (host, core, raw);

        int           volume = 0;
        vector<Byte>  decoded;

        Assert::IsTrue (FindAddressField (core, 0, 0, volume),
                        L"T0S0 address-field not found");
        Assert::IsTrue (ReadDataFieldAtCursor (core, decoded),
                        L"T0S0 data-field decode failed");

        wstring  failures;
        for (int i = 0; i < 256; i++)
        {
            Byte expected = static_cast<Byte> (~static_cast<Byte> (i));
            if (decoded[i] != expected)
            {
                wchar_t msg[64] = {};
                swprintf_s (msg, L"byte[%d] expected $%02X got $%02X. ",
                            i, expected, decoded[i]);
                failures += msg;
                if (failures.size () > 800) { failures += L"...truncated"; break; }
            }
        }

        if (!failures.empty ())
        {
            Assert::Fail (failures.c_str ());
        }
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Read every (track, sector) including the cross-track seeks RWTS
    //  uses for VTOC + catalog access. Slow (35 tracks * 16 sectors,
    //  ~50 ms each on debug) but the diagnostic ground truth.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (ReadEveryTrackAndSector_FromSentinelDsk)
    {
        HeadlessHost   host;
        EmulatorCore   core;
        vector<Byte>   raw = BuildSentinelDisk ();

        MountAndSpinUp (host, core, raw);

        wstring  failures;
        int      successCount = 0;

        for (int trk = 0; trk < kTrackCount; trk++)
        {
            SeekToTrack (core, trk);

            int actualTrack = core.diskController->GetCurrentTrack ();
            if (actualTrack != trk)
            {
                wchar_t  msg[128] = {};
                swprintf_s (msg, L"Seek to T%d landed at T%d. ", trk, actualTrack);
                failures += msg;
                continue;
            }

            for (int sec = 0; sec < kSectorsPerTrack; sec++)
            {
                int           volume = 0;
                vector<Byte>  decoded;

                if (!FindAddressField (core, trk, sec, volume))
                {
                    wchar_t msg[128] = {};
                    swprintf_s (msg, L"T%dS%d address-field not found. ", trk, sec);
                    failures += msg;
                    continue;
                }

                if (!ReadDataFieldAtCursor (core, decoded))
                {
                    wchar_t msg[128] = {};
                    swprintf_s (msg, L"T%dS%d data-field decode failed. ", trk, sec);
                    failures += msg;
                    continue;
                }

                Byte expected = ExpectedSectorByte (trk, sec, 0);
                if (decoded[0] != expected)
                {
                    wchar_t msg[128] = {};
                    swprintf_s (msg, L"T%dS%d byte0 expected $%02X got $%02X. ",
                                trk, sec, expected, decoded[0]);
                    failures += msg;
                    continue;
                }

                successCount++;
            }
        }

        if (!failures.empty ())
        {
            wchar_t  banner[128] = {};
            swprintf_s (banner, L"%d/%d sector reads succeeded. Failures: ",
                        successCount, kTrackCount * kSectorsPerTrack);
            wstring full = banner;
            full += failures;
            Assert::Fail (full.c_str ());
        }
    }
};
