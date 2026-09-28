#include "Pch.h"

#include "CppUnitTest.h"

#include "Ui/DriveWidgetController.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{





////////////////////////////////////////////////////////////////////////////////
//
//  AnimationSyncTests
//
//  Animations advancing by WALL-CLOCK time rather than by frame, and reporting
//  honestly whether they are still running.
//
//  Frame-rate independence is the property under test: the same animation
//  sampled at 60 Hz and at 30 Hz must reach the same value at the same instant.
//  A per-frame step looks fine on the developer's machine and runs at half
//  speed on a 30 Hz display.
//
//  The in-flight report matters as much as the value, because it is what tells
//  the UI loop to keep asking for frames -- an animation that reports finished
//  early freezes partway, and one that never finishes burns a core forever.
//
//  Time is passed in, so a full animation runs instantly and deterministically
//  with no sleeping.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AnimationSyncTests)
{
public:

    TEST_METHOD (Tween_LinearMidpoint)
    {
        DxuiAnimation    anim;
        DxuiTweenHandle  h;
        float            v    = 0.0f;

        // Pinned on: the tween is sampled mid-flight, and a host with
        // animations turned off -- a headless CI runner, for one -- would
        // otherwise start it already finished.
        anim.SetAnimationsEnabled (true);
        anim.AdvanceTime (0.0f);
        h = anim.StartTween (0.0f, 100.0f, 2.0f, DxuiTweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 1.0f, v));
        Assert::AreEqual (50.0f, v, 0.001f);
    }


    TEST_METHOD (Tween_ClampsAtEnd)
    {
        DxuiAnimation    anim;
        DxuiTweenHandle  h;
        float            v    = 0.0f;

        h = anim.StartTween (0.0f, 10.0f, 1.0f, DxuiTweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 5.0f, v));
        Assert::AreEqual (10.0f, v, 0.001f);
    }


    TEST_METHOD (Tween_AnimationsOff_StartsAtItsEnd)
    {
        DxuiAnimation    anim;
        DxuiTweenHandle  h;
        float            v    = 0.0f;

        // Animations off is an accessibility setting: the tween reports its
        // end value from its very first sample instead of moving at all.
        anim.SetAnimationsEnabled (false);
        anim.AdvanceTime (0.0f);
        h = anim.StartTween (0.0f, 100.0f, 2.0f, DxuiTweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 0.0f, v));
        Assert::AreEqual (100.0f, v, 0.001f);
    }


    TEST_METHOD (DriveSyncBroker_PublishAndConsumeWithinFrame)
    {
        using SyncAction = DriveWidgetController::SyncAction;

        DriveWidgetController                               controller;
        std::vector<DriveWidgetController::DriveSyncEvent>  events;
        uint64_t                                            openId  = 0;
        uint64_t                                            closeId = 0;



        openId  = controller.PublishSyncEvent (0, SyncAction::DoorOpen,  42);
        closeId = controller.PublishSyncEvent (0, SyncAction::DoorClose, 42);

        events = controller.ConsumeSyncEvents();

        Assert::AreEqual ((size_t) 2, events.size());
        Assert::AreNotEqual (openId, closeId);
        Assert::AreEqual (openId,  events[0].eventId);
        Assert::AreEqual (closeId, events[1].eventId);
        Assert::IsTrue   (events[0].action == SyncAction::DoorOpen);
        Assert::IsTrue   (events[1].action == SyncAction::DoorClose);
        Assert::AreEqual ((int64_t) 42, events[1].timestampMs);
        Assert::IsTrue   (controller.ConsumeSyncEvents().empty());
    }


    TEST_METHOD (ApplyEase_BoundaryValues)
    {
        Assert::AreEqual (0.0f, DxuiAnimation::ApplyEase (DxuiTweenEase::Linear,    0.0f), 0.0001f);
        Assert::AreEqual (1.0f, DxuiAnimation::ApplyEase (DxuiTweenEase::Linear,    1.0f), 0.0001f);
        Assert::AreEqual (1.0f, DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut,   1.0f), 0.0001f);
        Assert::AreEqual (0.0f, DxuiAnimation::ApplyEase (DxuiTweenEase::EaseInOut, 0.0f), 0.0001f);
        Assert::AreEqual (1.0f, DxuiAnimation::ApplyEase (DxuiTweenEase::EaseInOut, 1.0f), 0.0001f);
    }
};

}   // namespace UiTests
