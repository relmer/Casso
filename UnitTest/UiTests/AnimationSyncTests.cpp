#include "Pch.h"

#include "CppUnitTest.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (AnimationSyncTests)
{
public:

    TEST_METHOD (Tween_LinearMidpoint)
    {
        DxuiAnimation    anim;
        DxuiTweenHandle  h;
        float        v = 0.0f;

        anim.AdvanceTime (0.0f);
        h = anim.StartTween (0.0f, 100.0f, 2.0f, DxuiTweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 1.0f, v));
        Assert::AreEqual (50.0f, v, 0.001f);
    }


    TEST_METHOD (Tween_ClampsAtEnd)
    {
        DxuiAnimation    anim;
        DxuiTweenHandle  h;
        float        v = 0.0f;

        h = anim.StartTween (0.0f, 10.0f, 1.0f, DxuiTweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 5.0f, v));
        Assert::AreEqual (10.0f, v, 0.001f);
    }


    TEST_METHOD (DriveSyncBroker_PublishAndConsumeWithinFrame)
    {
        DxuiAnimation             anim;
        DxuiDriveSyncBrokerEvent  visual;
        DxuiDriveSyncBrokerEvent  audio;

        visual.driveIndex  = 0;
        visual.tag         = 1;
        visual.frameTimeMs = 42;
        audio.driveIndex   = 0;
        audio.tag          = 2;
        audio.frameTimeMs  = 42;

        anim.PublishSyncEvent (visual);
        anim.PublishSyncEvent (audio);

        std::vector<DxuiDriveSyncBrokerEvent>  events = anim.ConsumePendingEvents();

        Assert::AreEqual ((size_t) 2, events.size());
        Assert::AreEqual (visual.frameTimeMs, events[0].frameTimeMs);
        Assert::AreEqual (audio.frameTimeMs,  events[1].frameTimeMs);
        Assert::IsTrue   (anim.ConsumePendingEvents().empty());
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
