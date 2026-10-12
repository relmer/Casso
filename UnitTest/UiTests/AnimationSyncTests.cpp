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


    //  Two threads publish while the frame drains, as the CPU thread's disk
    //  changes do while the window draws: every event arrives once, and in
    //  the order its id was given out.
    TEST_METHOD (DriveSyncBroker_ConcurrentPublishAndConsume_LosesNothingAndKeepsOrder)
    {
        using SyncAction = DriveWidgetController::SyncAction;
        using SyncEvents = std::vector<DriveWidgetController::DriveSyncEvent>;

        constexpr int  kPerProducer = 100'000;
        constexpr int  kTotal       = 2 * kPerProducer;

        DriveWidgetController  controller;
        std::atomic<int>       running    { 2 };
        std::vector<bool>      seen       (kTotal + 1, false);
        std::array<int, 2>     perDrive   {};
        SyncEvents             batch;
        size_t                 consumed   = 0;
        size_t                 repeats    = 0;
        size_t                 outOfOrder = 0;
        uint64_t               lastId     = 0;
        bool                   isLast     = false;

        auto  produce = [&controller, &running] (int drive)
        {
            for (int i = 0; i < kPerProducer; i++)
            {
                controller.PublishSyncEvent (drive, SyncAction::DoorReinsert, i);
            }

            running--;
        };



        std::thread  first  (produce, 0);
        std::thread  second (produce, 1);

        while (!isLast)
        {
            isLast = running.load() == 0;
            batch  = controller.ConsumeSyncEvents();

            for (const auto & evt : batch)
            {
                bool  isInRange = evt.eventId >= 1 && evt.eventId <= (uint64_t) kTotal;

                outOfOrder += (evt.eventId <= lastId) ? 1 : 0;
                repeats    += (isInRange && seen[evt.eventId]) ? 1 : 0;
                lastId      = evt.eventId;

                if (isInRange)
                {
                    seen[evt.eventId] = true;
                }

                perDrive[evt.driveId & 1]++;
                consumed++;
            }
        }

        first.join();
        second.join();

        Assert::AreEqual<size_t> (kTotal, consumed,   L"every event was consumed");
        Assert::AreEqual<size_t> (0,      repeats,    L"none twice");
        Assert::AreEqual<size_t> (0,      outOfOrder, L"in increasing id order");
        Assert::IsTrue (std::find (seen.begin() + 1, seen.end(), false) == seen.end(), L"every id from 1 on arrived");
        Assert::AreEqual (kPerProducer, perDrive[0], L"drive 1's events");
        Assert::AreEqual (kPerProducer, perDrive[1], L"drive 2's events");
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
