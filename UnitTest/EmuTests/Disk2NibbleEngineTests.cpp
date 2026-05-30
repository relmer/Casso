#include "Pch.h"
#include "Devices/Disk/Disk2NibbleEngine.h"
#include "Devices/Disk/DiskImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr size_t   kSyntheticTrackBytes = 64;
    static constexpr int      kBitsPerNibble       = 8;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleEngineTests
//
//  Phase 9 unit-level acceptance for the bit-stream engine, independent
//  of the controller surface.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2NibbleEngineTests)
{
public:

    TEST_METHOD (BitTimingMatches4uSPerBit)
    {
        DiskImage             img;
        Disk2NibbleEngine     eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        // The Logic State Sequencer runs at 2 MHz (two LSS clocks per CPU
        // cycle) and advances the head one bit per eight LSS clocks --
        // i.e. one bit every kCyclesPerBit (4) CPU cycles, the standard
        // ~250 kbps data rate. A full bit cell's worth of cycles advances
        // exactly one bit.
        eng.Tick (Disk2NibbleEngine::kCyclesPerBit);
        Assert::AreEqual (size_t (1), eng.GetBitPosition(),
            L"One bit cell (4 cycles) must produce one bit advance");

        eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 5);
        Assert::AreEqual (size_t (6), eng.GetBitPosition(),
            L"5 more bits' worth of cycles must advance 5 bits");

        // Sub-bit-cell ticks accumulate across calls: the LSS clock is
        // retained between Tick calls, so two half-cell ticks make one
        // whole bit advance rather than being rounded away.
        eng.Tick (Disk2NibbleEngine::kCyclesPerBit / 2);
        eng.Tick (Disk2NibbleEngine::kCyclesPerBit / 2);
        Assert::AreEqual (size_t (7), eng.GetBitPosition(),
            L"Two half-cell ticks must sum to one bit advance");
    }

    TEST_METHOD (ReadAdvancesPosition)
    {
        DiskImage             img;
        Disk2NibbleEngine     eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 16);

        Assert::AreEqual (size_t (16), eng.GetBitPosition(),
            L"Ticking 16 bit-times must advance 16 bits");
    }

    TEST_METHOD (WriteAdvancesPositionAndMarksDirty)
    {
        DiskImage             img;
        Disk2NibbleEngine     eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);
        eng.SetWriteMode (true);
        eng.WriteLatch   (0xFF);

        eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 8);

        Assert::IsTrue (img.IsDirty(),
            L"Write-mode tick must mark the image dirty");
        Assert::AreEqual (size_t (8), eng.GetBitPosition(),
            L"Write-mode tick must still advance the bit cursor");
    }

    TEST_METHOD (MotorOffFreezesPosition)
    {
        DiskImage             img;
        Disk2NibbleEngine     eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (false);

        eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 100);

        Assert::AreEqual (size_t (0), eng.GetBitPosition(),
            L"Motor off must freeze the bit cursor");
    }

    TEST_METHOD (SetCurrentTrackClampsToValidRange)
    {
        Disk2NibbleEngine    eng;

        eng.SetCurrentTrack (-5);
        Assert::AreEqual (Disk2NibbleEngine::kMinTrack, eng.GetCurrentTrack(),
            L"Negative tracks must clamp to kMinTrack");

        eng.SetCurrentTrack (1000);
        Assert::AreEqual (Disk2NibbleEngine::kMaxTrack, eng.GetCurrentTrack(),
            L"Out-of-range tracks must clamp to kMaxTrack");
    }

    TEST_METHOD (ResetClearsLifetimeNibbleCounters)
    {
        // Regression for the Ctrl+Shift+R PowerCycle path: the status-bar
        // tooltip reads GetReadNibbles / GetWriteNibbles, so a power cycle
        // that did not zero them left the tooltip showing stale pre-cycle
        // counts. Reset() is invoked from Disk2Controller::PowerCycle
        // (via Disk2Controller::Reset), so clearing the counters here is
        // what restores the tooltip after a manual cold boot.
        Disk2NibbleEngine    eng;

        eng.WriteLatch (0xAA);
        eng.WriteLatch (0xAA);
        eng.WriteLatch (0xAA);

        Assert::AreEqual (uint64_t (3), eng.GetWriteNibbles(),
            L"Pre-condition: WriteLatch must bump the lifetime write counter");

        eng.Reset();

        Assert::AreEqual (uint64_t (0), eng.GetReadNibbles(),
            L"Reset must zero the lifetime read-nibble counter");
        Assert::AreEqual (uint64_t (0), eng.GetWriteNibbles(),
            L"Reset must zero the lifetime write-nibble counter");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  ConsumeFreshNibble helpers
    //
    //  Lay down a byte-aligned MSB-set stream on track 0 of a
    //  synthetic DiskImage and tick the engine bit-by-bit so the
    //  LSS sees the deterministic pattern these tests need.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void WriteByteAt (DiskImage & img, size_t bitOffset, uint8_t value)
    {
        int   i = 0;

        for (i = 0; i < 8; i++)
        {
            uint8_t  bit = (uint8_t) ((value >> (7 - i)) & 1);

            img.WriteBit (0, bitOffset + (size_t) i, bit);
        }
    }

    static void PrepareSingleByteStream (DiskImage & img, Disk2NibbleEngine & eng, uint8_t value)
    {
        size_t   i = 0;

        img.ResizeTrack (0, 256);

        for (i = 0; i < 32; i++)
        {
            WriteByteAt (img, i * 8, value);
        }

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        // Drain the MC3470 head-amplifier's one-cell pipeline delay so
        // subsequent "TickBits (eng, 8)" calls assemble a full nibble
        // matching the documented "8 bits per nibble" model.
        eng.Tick (Disk2NibbleEngine::kCyclesPerBit);
    }

    static void TickBits (Disk2NibbleEngine & eng, int bits)
    {
        eng.Tick ((uint32_t) (Disk2NibbleEngine::kCyclesPerBit * bits));
    }


    TEST_METHOD (ConsumeFreshNibble_returnsFalse_onFreshEngine)
    {
        Disk2NibbleEngine     eng;
        uint8_t               out = 0xCC;

        Assert::IsFalse (eng.ConsumeFreshNibble (out),
            L"A brand-new engine has no fresh nibble to consume");
        Assert::AreEqual ((int) 0xCC, (int) out,
            L"ConsumeFreshNibble must not write outNibble when returning false");
    }


    TEST_METHOD (ConsumeFreshNibble_returnsFalse_whenLatchIsStale)
    {
        // After one assembled nibble is consumed, a subsequent
        // call without any further bit advancement must return
        // false (no new "byte ready" rising edge has occurred).
        DiskImage             img;
        Disk2NibbleEngine     eng;
        uint8_t               out = 0;

        PrepareSingleByteStream (img, eng, 0xFF);

        TickBits (eng, 8);

        Assert::IsTrue (eng.ConsumeFreshNibble (out),
            L"First assembly must produce a fresh nibble");
        Assert::AreEqual ((int) 0xFF, (int) out,
            L"Consumed value must match the assembled byte");

        out = 0xCC;
        Assert::IsFalse (eng.ConsumeFreshNibble (out),
            L"Second ConsumeFreshNibble with no further bits must return false");
        Assert::AreEqual ((int) 0xCC, (int) out,
            L"Stale ConsumeFreshNibble must not write outNibble");
    }


    TEST_METHOD (ConsumeFreshNibble_returnsTrue_onlyOnceAfterAssembly)
    {
        // Advance bits until the next nibble assembles, verify
        // ConsumeFreshNibble returns true exactly once, then
        // returns false until the NEXT assembly.
        DiskImage             img;
        Disk2NibbleEngine     eng;
        uint8_t               out = 0;

        PrepareSingleByteStream (img, eng, 0xFF);

        TickBits (eng, 8);
        Assert::IsTrue  (eng.ConsumeFreshNibble (out), L"First assembly");
        Assert::IsFalse (eng.ConsumeFreshNibble (out), L"Immediate re-consume");

        TickBits (eng, 8);
        Assert::IsTrue  (eng.ConsumeFreshNibble (out), L"Second assembly");
        Assert::IsFalse (eng.ConsumeFreshNibble (out), L"Immediate re-consume");

        TickBits (eng, 8);
        Assert::IsTrue  (eng.ConsumeFreshNibble (out), L"Third assembly");
    }


    TEST_METHOD (ConsumeFreshNibble_returnsFalse_whenMsbClear)
    {
        // After an MSB-set assembly we keep ticking without
        // consuming. The LSS overwrites m_readLatch with
        // partial-assembly values whose MSB is clear (latch-delay
        // expires before the next "byte ready"). ConsumeFreshNibble
        // MUST refuse those even though the fresh flag has not been
        // cleared yet by a prior consume.
        DiskImage             img;
        Disk2NibbleEngine     eng;
        uint8_t               out = 0xCC;

        PrepareSingleByteStream (img, eng, 0xFF);

        TickBits (eng, 8);
        Assert::AreEqual ((int) 0xFF, (int) eng.PeekReadLatch(),
            L"Pre-condition: latch holds the full assembled nibble");

        // Two more bits run the latch-delay out and reload the
        // latch with a partial (MSB-clear) value.
        TickBits (eng, 2);

        Assert::AreEqual (0, (int) (eng.PeekReadLatch() & 0x80),
            L"Pre-condition: latch MSB has been cleared by partial reassembly");

        Assert::IsFalse (eng.ConsumeFreshNibble (out),
            L"ConsumeFreshNibble must reject MSB-clear latch even when fresh flag is set");
        Assert::AreEqual ((int) 0xCC, (int) out,
            L"Rejected ConsumeFreshNibble must not write outNibble");
    }


    TEST_METHOD (ConsumeFreshNibble_doesNotAffectCpuVisibleReadLatch)
    {
        // Two engines fed the same bit stream. One has
        // ReadLatch+ConsumeFreshNibble interleaved on every bit
        // (controller pattern); the other only ReadLatch. The
        // ReadLatch return sequences MUST be identical -- the
        // passive-watcher side channel must not perturb the
        // CPU-visible byte.
        DiskImage             imgA;
        DiskImage             imgB;
        Disk2NibbleEngine     engA;
        Disk2NibbleEngine     engB;
        int                   i        = 0;
        uint8_t               sink     = 0;

        PrepareSingleByteStream (imgA, engA, 0xD5);
        PrepareSingleByteStream (imgB, engB, 0xD5);

        for (i = 0; i < 256; i++)
        {
            TickBits (engA, 1);
            TickBits (engB, 1);

            uint8_t  a = engA.ReadLatch();
            uint8_t  b = engB.ReadLatch();

            Assert::AreEqual ((int) a, (int) b,
                L"Interleaved ConsumeFreshNibble must not change the CPU-visible ReadLatch");

            engA.ConsumeFreshNibble (sink);
        }
    }


    TEST_METHOD (WeakBits_UnformattedTrackProducesMixedBitsViaHeadAmpFloat)
    {
        // MC3470 read-amplifier model: when the sliding 4-bit head
        // window is all zeros (no signal locked to the surface), the
        // amplifier "floats" and the output bit is randomized with
        // ~30% probability of a 1. This is the behavior WOZ-2.0
        // copy-protection schemes (Karateka RWTS18, Lode Runner) key
        // off to detect bit-exact copies that have replaced the
        // floating region with deterministic zeros or ones.
        //
        // Drive the engine across a fully-zero track and count the
        // 1-bits in the working shift register over a large sample.
        // The empirical rate must land in a band around 30% --
        // statistically wide enough to be insensitive to the exact
        // LCG constants but tight enough to fail if the weak-bit
        // path is silently bypassed.
        DiskImage             img;
        Disk2NibbleEngine     eng;
        const int             kSampleBits = 100000;
        int                   onesCount   = 0;
        int                   i           = 0;
        double                rate        = 0.0;

        // 4096-bit track of all zeros. Engine will only ever see
        // window == 0 once the initial seed bits drain, so every
        // subsequent output bit must come from the weak-bit RNG.
        img.ResizeTrack (0, 4096);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        for (i = 0; i < kSampleBits; i++)
        {
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit);

            // PeekReadLatch reflects the latest assembled state.
            // We count by sampling the latch's LSB each cell -- a
            // proxy for the most-recently-shifted-in bit.
            if ((eng.PeekReadLatch () & 0x01) != 0)
            {
                onesCount++;
            }
        }

        rate = (double) onesCount / (double) kSampleBits;

        // ~30% target ± wide tolerance. If the weak-bit path is
        // disabled entirely, rate collapses to 0.0 and this fails.
        // If the threshold drifts catastrophically, rate moves out
        // of the 0.20..0.40 band.
        Assert::IsTrue (rate > 0.20 && rate < 0.40,
            L"Weak-bit rate must be in the ~30% band over unformatted track");
    }


    TEST_METHOD (WeakBits_FormattedTrackIsDeterministicAcrossRuns)
    {
        // Sanity guard: weak-bit randomization MUST NOT leak into
        // tracks with real signal. A formatted track (any non-zero
        // pattern in every 4-cell window) keeps the head window
        // non-zero, taking the deterministic branch every cell.
        // Two engines fed the same formatted bit stream must produce
        // byte-identical latch sequences.
        DiskImage             imgA;
        DiskImage             imgB;
        Disk2NibbleEngine     engA;
        Disk2NibbleEngine     engB;
        int                   i        = 0;

        // 0xFF pattern -- every bit is 1, so the head window never
        // empties and the weak-bit RNG never fires.
        PrepareSingleByteStream (imgA, engA, 0xFF);
        PrepareSingleByteStream (imgB, engB, 0xFF);

        for (i = 0; i < 256; i++)
        {
            engA.Tick (Disk2NibbleEngine::kCyclesPerBit);
            engB.Tick (Disk2NibbleEngine::kCyclesPerBit);

            Assert::AreEqual ((int) engA.PeekReadLatch (), (int) engB.PeekReadLatch (),
                L"Formatted-track latch sequence must be deterministic across engines");
        }
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  Self-sync framing helpers
    //
    //  Lay down genuine 10-bit self-sync FF bytes (1111111100), the gap
    //  pattern DOS 3.3 / ProDOS write between fields. Unlike an all-ones
    //  stream, the two trailing zeros force the LSS through its re-align
    //  path every byte -- the mechanism that makes every reader converge
    //  on the same byte framing regardless of where it started reading.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void WriteSelfSyncByte (DiskImage & img, size_t & bitPos)
    {
        int   i = 0;

        for (i = 0; i < 8; i++)
        {
            img.WriteBit (0, bitPos++, 1);
        }

        img.WriteBit (0, bitPos++, 0);
        img.WriteBit (0, bitPos++, 0);
    }


    TEST_METHOD (SelfSyncStreamFramesAsFFWithoutDrift)
    {
        // Port of apple2js disk2.spec.ts "reads an FF sync byte" /
        // "reads several FF sync bytes". A run of 10-bit self-sync FF
        // bytes must decode as a steady stream of 0xFF nibbles. Any
        // dropped or doubled bit cell would show up as a non-FF nibble
        // once the framing slipped -- the exact failure signature seen
        // when a protected loader stalls hunting for a prologue.
        DiskImage             img;
        Disk2NibbleEngine     eng;
        const int             kSyncBytes = 64;
        size_t                bitPos     = 0;
        int                   b          = 0;
        int                   t          = 0;
        int                   freshReads = 0;
        int                   nonFF      = 0;
        uint8_t               nib        = 0;

        img.ResizeTrack (0, (size_t) kSyncBytes * 10);

        for (b = 0; b < kSyncBytes; b++)
        {
            WriteSelfSyncByte (img, bitPos);
        }

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);



        // Tick one bit cell at a time across several full revolutions,
        // harvesting every freshly assembled nibble. Skip the first few
        // assemblies while the sequencer locks onto self-sync.
        for (t = 0; t < kSyncBytes * 10 * 4; t++)
        {
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit);

            if (eng.ConsumeFreshNibble (nib))
            {
                freshReads++;

                if (freshReads > 4 && nib != 0xFF)
                {
                    nonFF++;
                }
            }
        }

        Assert::IsTrue (freshReads > 16,
            L"Self-sync stream must assemble a steady run of nibbles");
        Assert::AreEqual (0, nonFF,
            L"Every framed self-sync nibble must be 0xFF -- no bit-slip drift");
    }


    static void WriteDataByte (DiskImage & img, size_t & bitPos, uint8_t value)
    {
        int   i = 0;

        for (i = 0; i < 8; i++)
        {
            img.WriteBit (0, bitPos++, (uint8_t) ((value >> (7 - i)) & 1));
        }
    }


    static void WriteZeroRun (DiskImage & img, size_t & bitPos, int count)
    {
        int   i = 0;

        for (i = 0; i < count; i++)
        {
            img.WriteBit (0, bitPos++, 0);
        }
    }


    TEST_METHOD (LssReSyncsAfterLongZeroGap)
    {
        // The boundary case protected loaders depend on: a long zero
        // run (an intentional weak-bit / "fake bit" region, exactly the
        // 234 runs of 4+ zeros measured on Choplifter track 0) is
        // immediately followed by self-sync and a fresh prologue. The
        // weak region randomizes, but self-sync is self-correcting:
        // once enough FF sync bytes pass, framing MUST re-lock so the
        // post-gap D5 AA 96 prologue decodes cleanly. If the LSS could
        // not re-lock after a gap, the prologue after every weak region
        // would be unreadable -- which is what a stalled loader looks
        // like.
        DiskImage             img;
        Disk2NibbleEngine     eng;
        const int             kLeadSync  = 24;
        const int             kGapZeros  = 200;
        const int             kTailSync  = 32;
        size_t                bitPos     = 0;
        int                   b          = 0;
        int                   t          = 0;
        int                   prologues  = 0;
        uint8_t               nib        = 0;
        std::vector<uint8_t>  harvested;

        img.ResizeTrack (0, 4096);

        for (b = 0; b < kLeadSync; b++)
        {
            WriteSelfSyncByte (img, bitPos);
        }

        WriteDataByte (img, bitPos, 0xD5);
        WriteDataByte (img, bitPos, 0xAA);
        WriteDataByte (img, bitPos, 0x96);

        WriteZeroRun (img, bitPos, kGapZeros);

        for (b = 0; b < kTailSync; b++)
        {
            WriteSelfSyncByte (img, bitPos);
        }

        WriteDataByte (img, bitPos, 0xD5);
        WriteDataByte (img, bitPos, 0xAA);
        WriteDataByte (img, bitPos, 0x96);

        img.SetTrackBitCount (0, bitPos);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);



        harvested.reserve (1024);

        for (t = 0; t < (int) bitPos * 3; t++)
        {
            eng.Tick (Disk2NibbleEngine::kCyclesPerBit);

            if (eng.ConsumeFreshNibble (nib))
            {
                harvested.push_back (nib);
            }
        }

        for (b = 0; b + 2 < (int) harvested.size (); b++)
        {
            if (harvested[b] == 0xD5 && harvested[b + 1] == 0xAA && harvested[b + 2] == 0x96)
            {
                prologues++;
            }
        }

        // Both prologues -- the one before the gap and the one after --
        // must frame. Across ~3 revolutions each is seen multiple times;
        // the floor of 2 proves the post-gap prologue re-locked at least
        // once rather than being lost to permanent bit-slip.
        Assert::IsTrue (prologues >= 2,
            L"LSS must re-lock self-sync framing after a long zero/weak gap");
    }
};
