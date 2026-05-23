#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/Animation.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (AnimationSyncTests)
{
public:

    TEST_METHOD (Tween_LinearMidpoint)
    {
        Animation    anim;
        TweenHandle  h;
        float        v = 0.0f;

        anim.AdvanceTime (0.0f);
        h = anim.StartTween (0.0f, 100.0f, 2.0f, TweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 1.0f, v));
        Assert::AreEqual (50.0f, v, 0.001f);
    }


    TEST_METHOD (Tween_ClampsAtEnd)
    {
        Animation    anim;
        TweenHandle  h;
        float        v = 0.0f;

        h = anim.StartTween (0.0f, 10.0f, 1.0f, TweenEase::Linear);

        Assert::IsTrue   (anim.SampleTween (h, 5.0f, v));
        Assert::AreEqual (10.0f, v, 0.001f);
    }


    TEST_METHOD (DriveSyncBroker_PublishAndConsumeWithinFrame)
    {
        Animation             anim;
        DriveSyncBrokerEvent  visual;
        DriveSyncBrokerEvent  audio;

        visual.driveIndex  = 0;
        visual.tag         = 1;
        visual.frameTimeMs = 42;
        audio.driveIndex   = 0;
        audio.tag          = 2;
        audio.frameTimeMs  = 42;

        anim.PublishSyncEvent (visual);
        anim.PublishSyncEvent (audio);

        std::vector<DriveSyncBrokerEvent>  events = anim.ConsumePendingEvents();

        Assert::AreEqual ((size_t) 2, events.size());
        Assert::AreEqual (visual.frameTimeMs, events[0].frameTimeMs);
        Assert::AreEqual (audio.frameTimeMs,  events[1].frameTimeMs);
        Assert::IsTrue   (anim.ConsumePendingEvents().empty());
    }


    TEST_METHOD (ApplyEase_BoundaryValues)
    {
        Assert::AreEqual (0.0f, Animation::ApplyEase (TweenEase::Linear,    0.0f), 0.0001f);
        Assert::AreEqual (1.0f, Animation::ApplyEase (TweenEase::Linear,    1.0f), 0.0001f);
        Assert::AreEqual (1.0f, Animation::ApplyEase (TweenEase::EaseOut,   1.0f), 0.0001f);
        Assert::AreEqual (0.0f, Animation::ApplyEase (TweenEase::EaseInOut, 0.0f), 0.0001f);
        Assert::AreEqual (1.0f, Animation::ApplyEase (TweenEase::EaseInOut, 1.0f), 0.0001f);
    }
};

}   // namespace UiTests
