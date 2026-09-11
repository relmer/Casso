#include "Pch.h"

#include "Shell/FramePacing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FramePacingTests
//
//  The two decisions between the emulation advancing and a picture reaching
//  the screen.
//
//  Both used to live inside the CPU thread's per-frame callback, where the
//  only way to exercise them was to run the emulator and watch it. Getting
//  either wrong is felt rather than seen -- a stutter, a dropped frame, a
//  screen that will not refresh -- which is exactly the class of bug an eye is
//  bad at catching and a test is good at holding.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FramePacingTests)
{
public:

    TEST_METHOD (BelowMaximumSpeed_EveryFrameIsPublished)
    {
        //  The CPU thread's waitable timer already paces these to one frame
        //  per vsync. Throttling a second time would drop frames the
        //  emulation produced correctly and on time.
        Assert::IsTrue (FramePacing::ShouldPublish (false, 0,     s_kIntervalUs));
        Assert::IsTrue (FramePacing::ShouldPublish (false, 1,     s_kIntervalUs));
        Assert::IsTrue (FramePacing::ShouldPublish (false, 99999, s_kIntervalUs));
    }


    TEST_METHOD (AtMaximumSpeed_PublishWaitsForTheInterval)
    {
        //  Emulation is unthrottled here, so the publish is what holds the
        //  renderer to a cadence a display can actually show.
        Assert::IsFalse (FramePacing::ShouldPublish (true, 0, s_kIntervalUs));
        Assert::IsFalse (FramePacing::ShouldPublish (true, s_kIntervalUs - 1, s_kIntervalUs));
        Assert::IsTrue  (FramePacing::ShouldPublish (true, s_kIntervalUs, s_kIntervalUs));
        Assert::IsTrue  (FramePacing::ShouldPublish (true, s_kIntervalUs * 4, s_kIntervalUs));
    }


    TEST_METHOD (AnUnchangedScreen_DoesNotReRasterize)
    {
        FrameSignature  steady = MakeSignature();

        Assert::IsFalse (FramePacing::NeedsRender (steady, steady),
                         L"a steady screen must cost nothing but the comparisons");
    }


    TEST_METHOD (ChangingAnyOneInput_ForcesARender)
    {
        FrameSignature  last = MakeSignature();

        //  Each of the four on its own. A gate that misses one shows a stale
        //  picture until something else happens to move.
        FrameSignature  dirty = last;
        FrameSignature  mode  = last;
        FrameSignature  flash = last;
        FrameSignature  color = last;

        dirty.videoDirty = true;
        mode.modeSig     = last.modeSig  + 1;
        flash.flashOn    = !last.flashOn;
        color.colorSig   = last.colorSig + 1;

        Assert::IsTrue (FramePacing::NeedsRender (dirty, last), L"a write into a display page");
        Assert::IsTrue (FramePacing::NeedsRender (mode,  last), L"a video mode change");
        Assert::IsTrue (FramePacing::NeedsRender (flash, last), L"the flash phase turning over");
        Assert::IsTrue (FramePacing::NeedsRender (color, last), L"a color treatment change");
    }


    TEST_METHOD (AChangeDuringASkippedFrame_IsNotLost)
    {
        FrameSignature  lastRendered = MakeSignature();
        FrameSignature  changed      = lastRendered;

        //  At Maximum speed most frames never publish. The comparison is
        //  against what was last RENDERED rather than last seen, so a change
        //  that happened while frames were being skipped still forces a render
        //  on the next published one instead of being missed.
        changed.modeSig = lastRendered.modeSig + 1;

        Assert::IsTrue (FramePacing::NeedsRender (changed, lastRendered));
        Assert::IsTrue (FramePacing::NeedsRender (changed, lastRendered),
                        L"and again -- skipping does not clear the difference");
    }


    TEST_METHOD (VideoDirty_ForcesARenderEvenWhenNothingElseMoved)
    {
        FrameSignature  last = MakeSignature();
        FrameSignature  now  = last;

        //  The bus sees writes the soft switches cannot: a program drawing
        //  into the display page changes no mode and no phase.
        now.videoDirty = true;

        Assert::IsTrue (FramePacing::NeedsRender (now, last));
    }


private:

    //  ~60 Hz, the cadence Maximum speed is held to.
    static constexpr int64_t  s_kIntervalUs = 16667;


    static FrameSignature MakeSignature()
    {
        FrameSignature  signature;

        signature.videoDirty = false;
        signature.modeSig    = 0x2A;
        signature.flashOn    = true;
        signature.colorSig   = 0x1234567890ABCDEFull;

        return (signature);
    }
};
