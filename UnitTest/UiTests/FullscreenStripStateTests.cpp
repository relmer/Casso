#include "Pch.h"
#include "../EhmTestHelper.h"

#include "Ui/Scene/FullscreenStripState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripStateTests
//
//  The data-model's four invariants, property-style over scripted input
//  sequences: edge-reveal is host-pointer-only, Hidden is unreachable while
//  pinned, a released capture restores exactly once, and the activity
//  indicator exists only while Hidden.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullscreenStripStateTests)
{
public:

    // Advances the FSM in kStepMs ticks with constant inputs, collecting
    // effects; returns the number of restores fired and the last effects.
    static int RunFor (FullscreenStripState & fsm, StripInputs inputs,
                       int64_t & nowMs, int64_t durationMs,
                       GuestPointerMode * outRestored = nullptr)
    {
        constexpr int64_t   kStepMs  = 30;
        int                 restores = 0;

        for (int64_t end = nowMs + durationMs; nowMs <= end; nowMs += kStepMs)
        {
            StripInputs   step    = inputs;
            StripEffects  effects;

            step.nowMs  = nowMs;
            step.hotkey = false;               // hotkeys are injected separately
            effects     = fsm.Tick (step);

            if (effects.restoreCapture != GuestPointerMode::None)
            {
                restores++;

                if (outRestored != nullptr)
                {
                    *outRestored = effects.restoreCapture;
                }
            }
        }

        return restores;
    }

    TEST_METHOD (Edge_Reveal_Fires_Only_While_The_Host_Owns_The_Pointer)
    {
        FullscreenStripState  fsm;
        StripInputs           inputs;
        int64_t               now = 1000;



        // Guest owns the pointer: bottom-edge dwell must NOT summon.
        inputs.pointerAtBottomEdge = true;
        inputs.guestPointer        = GuestPointerMode::Mouse;
        RunFor (fsm, inputs, now, 2000);
        Assert::IsTrue (fsm.Mode() == StripMode::Hidden);

        // Host owns it: the same dwell reveals.
        inputs.guestPointer = GuestPointerMode::None;
        RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        Assert::IsTrue (fsm.Mode() == StripMode::Shown);
    }

    TEST_METHOD (Pinned_Blocks_Hidden_Until_Released)
    {
        FullscreenStripState  fsm;
        StripInputs           inputs;
        int64_t               now = 1000;



        inputs.pointerAtBottomEdge = true;
        RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        Assert::IsTrue (fsm.Mode() == StripMode::Shown);

        // Pointer leaves but a tooltip pins the strip: it must never reach
        // Hidden, however long the wait.
        inputs.pointerAtBottomEdge = false;
        inputs.pinned              = true;
        RunFor (fsm, inputs, now, 10000);
        Assert::IsFalse (fsm.Mode() == StripMode::Hidden);

        // Unpinned: the auto-hide path completes.
        inputs.pinned = false;
        RunFor (fsm, inputs, now, FullscreenStripState::kAutoHideGraceMs +
                                  FullscreenStripState::kSlideMs * 3);
        Assert::IsTrue (fsm.Mode() == StripMode::Hidden);
    }

    TEST_METHOD (Hotkey_Under_Capture_Releases_And_Restores_Exactly_Once)
    {
        FullscreenStripState  fsm;
        StripInputs           inputs;
        StripEffects          effects;
        GuestPointerMode      restored = GuestPointerMode::None;
        int64_t               now      = 1000;
        int                   restores = 0;



        // Summon by hotkey while paddle-captured: the capture releases NOW.
        inputs.nowMs        = now;
        inputs.hotkey       = true;
        inputs.guestPointer = GuestPointerMode::Paddle;
        effects             = fsm.Tick (inputs);
        Assert::IsTrue (effects.releaseCapture);
        Assert::IsTrue (effects.restoreCapture == GuestPointerMode::None);

        // Interact a while (capture released -> guest no longer owns it),
        // then walk away; the strip hides and restores the PADDLE capture
        // exactly once.
        inputs.hotkey       = false;
        inputs.guestPointer = GuestPointerMode::None;
        restores            = RunFor (fsm, inputs, now, 20000, &restored);

        Assert::IsTrue  (fsm.Mode() == StripMode::Hidden);
        Assert::AreEqual (1, restores);
        Assert::IsTrue  (restored == GuestPointerMode::Paddle);

        // A later ordinary reveal/hide cycle must NOT restore again.
        inputs.pointerAtBottomEdge = true;
        RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        inputs.pointerAtBottomEdge = false;
        restores = RunFor (fsm, inputs, now, 20000);
        Assert::AreEqual (0, restores);
    }

    TEST_METHOD (Indicator_Shows_Only_While_Hidden)
    {
        FullscreenStripState  fsm;
        StripInputs           inputs;
        int64_t               now = 1000;



        inputs.anyDriveActive = true;
        RunFor (fsm, inputs, now, 200);
        Assert::IsTrue (fsm.ActivityIndicator());

        // Revealed: the drives themselves are visible; no indicator.
        inputs.pointerAtBottomEdge = true;
        RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        Assert::IsTrue  (fsm.Mode() == StripMode::Shown);
        Assert::IsFalse (fsm.ActivityIndicator());

        // Idle drives while hidden: no indicator either.
        inputs.pointerAtBottomEdge = false;
        inputs.anyDriveActive      = false;
        RunFor (fsm, inputs, now, 20000);
        Assert::IsTrue  (fsm.Mode() == StripMode::Hidden);
        Assert::IsFalse (fsm.ActivityIndicator());
    }

    TEST_METHOD (Reengaging_While_Hiding_Reverses_Without_Restoring)
    {
        FullscreenStripState  fsm;
        StripInputs           inputs;
        StripEffects          effects;
        int64_t               now      = 1000;
        int                   restores = 0;



        // Hotkey summon under mouse capture, then wander off: wait until the
        // strip is mid-Hiding.
        inputs.nowMs        = now;
        inputs.hotkey       = true;
        inputs.guestPointer = GuestPointerMode::Mouse;
        effects             = fsm.Tick (inputs);
        Assert::IsTrue (effects.releaseCapture);

        inputs.hotkey       = false;
        inputs.guestPointer = GuestPointerMode::None;
        RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        Assert::IsTrue (fsm.Mode() == StripMode::Shown);

        // Refresh the engagement so the grace clock starts from a known
        // point, then leave for exactly grace + half a slide: mid-Hiding.
        inputs.pointerOverStrip = true;
        RunFor (fsm, inputs, now, 60);
        inputs.pointerOverStrip = false;

        restores = RunFor (fsm, inputs, now,
                           FullscreenStripState::kAutoHideGraceMs +
                           FullscreenStripState::kSlideMs / 2);
        Assert::IsTrue   (fsm.Mode() == StripMode::Hiding);
        Assert::AreEqual (0, restores);

        // Pointer returns mid-slide: reverse to Shown, restore still owed.
        inputs.pointerOverStrip = true;
        restores = RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        Assert::IsTrue   (fsm.Mode() == StripMode::Shown);
        Assert::AreEqual (0, restores);

        // Leave for good: one restore, at Hidden.
        inputs.pointerOverStrip = false;
        restores = RunFor (fsm, inputs, now, 20000);
        Assert::IsTrue   (fsm.Mode() == StripMode::Hidden);
        Assert::AreEqual (1, restores);
    }

    TEST_METHOD (Slide_Progress_Tracks_The_Mode)
    {
        FullscreenStripState  fsm;
        StripInputs           inputs;
        int64_t               now = 1000;



        Assert::AreEqual (0.0f, fsm.SlideProgress (now));

        inputs.pointerAtBottomEdge = true;
        inputs.nowMs               = now;
        fsm.Tick (inputs);
        Assert::IsTrue (fsm.Mode() == StripMode::Revealing);

        // Mid-slide sits strictly between the endpoints.
        {
            float  mid = fsm.SlideProgress (now + FullscreenStripState::kSlideMs / 2);

            Assert::IsTrue (mid > 0.0f && mid < 1.0f);
        }

        RunFor (fsm, inputs, now, FullscreenStripState::kSlideMs * 3);
        Assert::AreEqual (1.0f, fsm.SlideProgress (now));
    }

};
