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
        Disk2NibbleEngine    eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        eng.Tick (3);
        Assert::AreEqual (size_t (0), eng.GetBitPosition(),
            L"3 cycles is below the 4-cycle bit boundary");

        eng.Tick (1);
        Assert::AreEqual (size_t (1), eng.GetBitPosition(),
            L"4 cumulative cycles must produce one bit advance");

        eng.Tick (Disk2NibbleEngine::kCyclesPerBit * 5);
        Assert::AreEqual (size_t (6), eng.GetBitPosition(),
            L"5 more bits' worth of cycles must advance 5 bits");
    }

    TEST_METHOD (ReadAdvancesPosition)
    {
        DiskImage             img;
        Disk2NibbleEngine    eng;

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
        Disk2NibbleEngine    eng;

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
        Disk2NibbleEngine    eng;

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
    }

    static void TickBits (Disk2NibbleEngine & eng, int bits)
    {
        eng.Tick ((uint32_t) (Disk2NibbleEngine::kCyclesPerBit * bits));
    }


    TEST_METHOD (ConsumeFreshNibble_returnsFalse_onFreshEngine)
    {
        Disk2NibbleEngine    eng;
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
        Disk2NibbleEngine    eng;
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
        Disk2NibbleEngine    eng;
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
        Disk2NibbleEngine    eng;
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
        Disk2NibbleEngine    engA;
        Disk2NibbleEngine    engB;
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
};
