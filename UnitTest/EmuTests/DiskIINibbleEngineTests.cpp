#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Devices/Disk/DiskIINibbleEngine.h"
#include "Devices/Disk/DiskImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace
{
    static constexpr size_t   kSyntheticTrackBytes = 64;
    static constexpr int      kBitsPerNibble       = 8;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIINibbleEngineTests
//
//  Phase 9 unit-level acceptance for the bit-stream engine, independent
//  of the controller surface.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIINibbleEngineTests)
{
public:

    TEST_METHOD (BitTimingMatches4uSPerBit)
    {
        DiskImage             img;
        DiskIINibbleEngine    eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        eng.Tick (3);
        Assert::AreEqual (size_t (0), eng.GetBitPosition (),
            L"3 cycles is below the 4-cycle bit boundary");

        eng.Tick (1);
        Assert::AreEqual (size_t (1), eng.GetBitPosition (),
            L"4 cumulative cycles must produce one bit advance");

        eng.Tick (DiskIINibbleEngine::kCyclesPerBit * 5);
        Assert::AreEqual (size_t (6), eng.GetBitPosition (),
            L"5 more bits' worth of cycles must advance 5 bits");
    }

    TEST_METHOD (ReadAdvancesPosition)
    {
        DiskImage             img;
        DiskIINibbleEngine    eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);

        eng.Tick (DiskIINibbleEngine::kCyclesPerBit * 16);

        Assert::AreEqual (size_t (16), eng.GetBitPosition (),
            L"Ticking 16 bit-times must advance 16 bits");
    }

    TEST_METHOD (WriteAdvancesPositionAndMarksDirty)
    {
        DiskImage             img;
        DiskIINibbleEngine    eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (true);
        eng.SetWriteMode (true);
        eng.WriteLatch   (0xFF);

        eng.Tick (DiskIINibbleEngine::kCyclesPerBit * 8);

        Assert::IsTrue (img.IsDirty (),
            L"Write-mode tick must mark the image dirty");
        Assert::AreEqual (size_t (8), eng.GetBitPosition (),
            L"Write-mode tick must still advance the bit cursor");
    }

    TEST_METHOD (MotorOffFreezesPosition)
    {
        DiskImage             img;
        DiskIINibbleEngine    eng;

        img.ResizeTrack (0, kSyntheticTrackBytes * kBitsPerNibble);

        eng.SetDiskImage (&img);
        eng.SetMotorOn   (false);

        eng.Tick (DiskIINibbleEngine::kCyclesPerBit * 100);

        Assert::AreEqual (size_t (0), eng.GetBitPosition (),
            L"Motor off must freeze the bit cursor");
    }

    TEST_METHOD (SetCurrentTrackClampsToValidRange)
    {
        DiskIINibbleEngine    eng;

        eng.SetCurrentTrack (-5);
        Assert::AreEqual (DiskIINibbleEngine::kMinTrack, eng.GetCurrentTrack (),
            L"Negative tracks must clamp to kMinTrack");

        eng.SetCurrentTrack (1000);
        Assert::AreEqual (DiskIINibbleEngine::kMaxTrack, eng.GetCurrentTrack (),
            L"Out-of-range tracks must clamp to kMaxTrack");
    }
};
