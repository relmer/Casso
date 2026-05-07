#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Video/VideoTiming.h"
#include "Devices/AppleIIeKeyboard.h"
#include "Devices/AppleKeyboard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoTimingTests
//
//  Phase 5 / FR-033 / audit §1.2 M (RDVBLBAR). Locks down the //e
//  video timing model:
//    - 65 cycles/scanline x 262 scanlines = 17,030 cycles/frame
//    - VBL window = scanlines 192..261 (cycle-in-frame >= 12,480)
//    - $C019 (RDVBLBAR) bit 7 inverted relative to IsInVblank
//
//  Constitution §II: tests construct their own scaffolding directly;
//  no host state, no Win32, no fixture I/O.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VideoTimingTests)
{
public:

    TEST_METHOD (TickAdvancesScanline)
    {
        VideoTiming     vt;

        Assert::AreEqual (static_cast<uint32_t> (0),  vt.GetCycleInFrame    ());
        Assert::AreEqual (static_cast<uint32_t> (0),  vt.GetCurrentScanline ());
        Assert::AreEqual (static_cast<uint32_t> (0),  vt.GetHorizontalPos   ());

        vt.Tick (65);

        Assert::AreEqual (static_cast<uint32_t> (65), vt.GetCycleInFrame    (),
            L"Tick(65) advances cycle-in-frame to 65");
        Assert::AreEqual (static_cast<uint32_t> (1),  vt.GetCurrentScanline (),
            L"Tick(65) advances exactly one scanline");
        Assert::AreEqual (static_cast<uint32_t> (0),  vt.GetHorizontalPos   (),
            L"Tick(65) lands at horizontal position 0 of scanline 1");

        vt.Tick (10);

        Assert::AreEqual (static_cast<uint32_t> (75), vt.GetCycleInFrame    ());
        Assert::AreEqual (static_cast<uint32_t> (1),  vt.GetCurrentScanline ());
        Assert::AreEqual (static_cast<uint32_t> (10), vt.GetHorizontalPos   ());
    }


    TEST_METHOD (OneFullFrameIs17030Cycles)
    {
        VideoTiming     vt;

        // Tick exactly one frame; counter must wrap to 0.
        vt.Tick (VideoTiming::kCyclesPerFrame);

        Assert::AreEqual (static_cast<uint32_t> (0), vt.GetCycleInFrame (),
            L"After 17,030 cycles the cycle-in-frame counter wraps to 0");
        Assert::AreEqual (static_cast<uint32_t> (17030), VideoTiming::kCyclesPerFrame,
            L"NTSC frame must be exactly 65*262 = 17,030 cycles");

        // Tick 17,030 + 5; counter must read 5.
        vt.Tick (VideoTiming::kCyclesPerFrame + 5);

        Assert::AreEqual (static_cast<uint32_t> (5), vt.GetCycleInFrame (),
            L"Tick wraps any multiple of kCyclesPerFrame back to the residual");
    }


    TEST_METHOD (VblankRegionIsLines192Through261)
    {
        VideoTiming     vt;

        // Scanline 0..191 — display
        vt.Tick (VideoTiming::kVblankStartCycle - 1);
        Assert::IsFalse (vt.IsInVblank (),
            L"cycle 12479 (last cycle of scanline 191) is in display region");

        // Scanline 192 — vblank starts
        vt.Tick (1);
        Assert::IsTrue (vt.IsInVblank (),
            L"cycle 12480 (first cycle of scanline 192) is in vblank");
        Assert::AreEqual (static_cast<uint32_t> (192), vt.GetCurrentScanline ());

        // Last cycle of scanline 261 — still vblank
        VideoTiming     vt2;
        vt2.Tick (VideoTiming::kCyclesPerFrame - 1);
        Assert::IsTrue (vt2.IsInVblank (),
            L"cycle 17029 (last cycle of scanline 261) is in vblank");
        Assert::AreEqual (static_cast<uint32_t> (261), vt2.GetCurrentScanline ());

        // Wrap back to scanline 0 — display again
        vt2.Tick (1);
        Assert::IsFalse (vt2.IsInVblank (),
            L"cycle 0 (start of scanline 0) is in display");
        Assert::AreEqual (static_cast<uint32_t> (0), vt2.GetCurrentScanline ());
    }


    TEST_METHOD (IsInVblankTransitionsAtLine192Boundary)
    {
        VideoTiming     vt;

        // Step exactly to scanline 191's last cycle.
        vt.Tick (192 * 65 - 1);
        Assert::IsFalse (vt.IsInVblank (), L"cycle 12479 still display");

        // One more cycle crosses into scanline 192.
        vt.Tick (1);
        Assert::IsTrue  (vt.IsInVblank (), L"cycle 12480 now vblank");
        Assert::AreEqual (static_cast<uint32_t> (192), vt.GetCurrentScanline ());
    }


    TEST_METHOD (RDVBLBAR_BitInvertedRelativeToIsInVblank)
    {
        // The RDVBLBAR convention: bit 7 = 1 during display, 0 during vblank.
        // Verify the polarity at the AppleIIeKeyboard $C019 read site.
        VideoTiming         vt;
        AppleIIeKeyboard    kbd;

        kbd.SetVideoTiming (&vt);

        // Display region — bit 7 should be set.
        vt.Tick (100);
        Assert::IsFalse (vt.IsInVblank ());

        Byte    val = kbd.Read (0xC019);
        Assert::IsTrue ((val & 0x80) != 0,
            L"$C019 bit 7 = 1 during display (RDVBLBAR is inverted)");

        // VBL region — bit 7 should be clear.
        vt.Tick (VideoTiming::kVblankStartCycle);
        Assert::IsTrue (vt.IsInVblank ());

        val = kbd.Read (0xC019);
        Assert::IsTrue ((val & 0x80) == 0,
            L"$C019 bit 7 = 0 during vblank (RDVBLBAR is inverted)");
    }


    TEST_METHOD (SpinWaitOnC019TerminatesWithinOneFrame)
    {
        // Simulate a software spin-loop polling $C019 looking for the
        // VBL transition. Each iteration ticks a typical instruction's
        // worth of cycles (5 — LDA $C019; BPL); the loop must observe
        // bit 7 going from 1 -> 0 within at most one frame's worth.
        VideoTiming         vt;
        AppleIIeKeyboard    kbd;
        Byte                val             = 0;
        uint32_t            spunCycles      = 0;
        uint32_t            previousBit7    = 0xFF;
        bool                sawTransition   = false;

        kbd.SetVideoTiming (&vt);

        previousBit7 = kbd.Read (0xC019) & 0x80;

        for (uint32_t i = 0; i < VideoTiming::kCyclesPerFrame; i += 5)
        {
            vt.Tick (5);
            spunCycles += 5;

            val = kbd.Read (0xC019);

            if ((val & 0x80) != previousBit7)
            {
                sawTransition = true;
                break;
            }
        }

        Assert::IsTrue (sawTransition,
            L"$C019 bit 7 must transition within one full frame");
        Assert::IsTrue (spunCycles <= VideoTiming::kCyclesPerFrame,
            L"Transition must occur within 17,030 cycles");
    }


    TEST_METHOD (FrameWrapsCleanlyAcrossMultipleFrames)
    {
        VideoTiming     vt;

        // Tick 5 frames + 100 cycles.
        for (int i = 0; i < 5; ++i)
        {
            vt.Tick (VideoTiming::kCyclesPerFrame);
        }
        vt.Tick (100);

        Assert::AreEqual (static_cast<uint32_t> (100), vt.GetCycleInFrame (),
            L"Five wraps + 100 lands at cycle 100");
        Assert::IsFalse (vt.IsInVblank (), L"cycle 100 is display");
    }


    TEST_METHOD (ResetClearsCycleCounter)
    {
        VideoTiming     vt;

        vt.Tick (5000);
        Assert::AreEqual (static_cast<uint32_t> (5000), vt.GetCycleInFrame ());

        vt.Reset ();

        Assert::AreEqual (static_cast<uint32_t> (0), vt.GetCycleInFrame (),
            L"Reset clears the cycle counter");
        Assert::IsFalse (vt.IsInVblank (), L"After Reset, IsInVblank is false");
    }
};
