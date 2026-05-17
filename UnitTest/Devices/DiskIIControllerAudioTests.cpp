#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/DiskIIController.h"
#include "Audio/IDriveAudioSink.h"

// DiskIIController carries two DiskImage instances; per-test heap
// allocation would otherwise blow the C6262 stack-frame budget on
// some tests.
#pragma warning (disable: 6262)

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingSink
//
//  Captures an ordered log of audio events for assertion.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingSink : public IDriveAudioSink
{
public:
    enum class Event
    {
        MotorStart,
        MotorStop,
        HeadStep,
        HeadBump,
        DiskInserted,
        DiskEjected,
    };

    struct LogEntry
    {
        Event  event;
        int    arg;
    };

    vector<LogEntry>  log;

    void OnMotorStart   () override                  { log.push_back ({ Event::MotorStart,   0 }); }
    void OnMotorStop    () override                  { log.push_back ({ Event::MotorStop,    0 }); }
    void OnHeadStep     (int newQt) override         { log.push_back ({ Event::HeadStep,     newQt }); }
    void OnHeadBump     () override                  { log.push_back ({ Event::HeadBump,     0 }); }
    void OnDiskInserted() override                  { log.push_back ({ Event::DiskInserted, 0 }); }
    void OnDiskEjected  () override                  { log.push_back ({ Event::DiskEjected,  0 }); }

    int CountOf (Event ev) const
    {
        int  n = 0;

        for (auto & e : log)
        {
            if (e.event == ev)
            {
                n++;
            }
        }

        return n;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIControllerAudioTests
//
//  Verifies controller event-firing semantics (spec FR-001..FR-004).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskIIControllerAudioTests)
{
public:

    TEST_METHOD (MotorOnSoftSwitch_firesOnMotorStart_exactlyOnce)
    {
        DiskIIController  ctrl (6);
        RecordingSink     sink;

        ctrl.SetAudioSink (&sink);

        ctrl.Write (0xC0E9, 0x00);
        ctrl.Write (0xC0E9, 0x00);

        Assert::AreEqual (1, sink.CountOf (RecordingSink::Event::MotorStart));
    }

    TEST_METHOD (MotorOffThenSpindownTick_firesOnMotorStop)
    {
        DiskIIController  ctrl (6);
        RecordingSink     sink;

        ctrl.SetAudioSink (&sink);

        ctrl.Write (0xC0E9, 0x00);    // motor on
        ctrl.Write (0xC0E8, 0x00);    // request motor off; arms spindown

        // Spindown is 1,000,000 cycles. Tick past the timer.
        ctrl.Tick (1100000);

        Assert::AreEqual (1, sink.CountOf (RecordingSink::Event::MotorStop));
    }

    TEST_METHOD (MotorOffThenMotorOnWithinSpindown_doesNotFireOnMotorStop)
    {
        DiskIIController  ctrl (6);
        RecordingSink     sink;

        ctrl.SetAudioSink (&sink);

        ctrl.Write (0xC0E9, 0x00);    // motor on
        ctrl.Write (0xC0E8, 0x00);    // motor off (spindown armed)
        ctrl.Tick  (500000);          // partway through spindown
        ctrl.Write (0xC0E9, 0x00);    // motor on again -- cancels spindown
        ctrl.Tick  (1100000);

        // Critical FR-001: brief intra-sector toggle did NOT trip a
        // motor-stop event, AND did NOT fire a redundant motor-start.
        Assert::AreEqual (0, sink.CountOf (RecordingSink::Event::MotorStop));
        Assert::AreEqual (1, sink.CountOf (RecordingSink::Event::MotorStart));
    }

    TEST_METHOD (PhaseChange_noMovement_firesNothing)
    {
        DiskIIController  ctrl (6);
        RecordingSink     sink;

        ctrl.SetAudioSink (&sink);

        // Energize phase 0 with no other phases set: rot=0,
        // neighbors not energized, qtDelta == 0.
        ctrl.Write (0xC0E1, 0x00);

        Assert::AreEqual (0, sink.CountOf (RecordingSink::Event::HeadStep));
        Assert::AreEqual (0, sink.CountOf (RecordingSink::Event::HeadBump));
    }

    TEST_METHOD (PhaseChange_pastTrack0_firesOnHeadBump_notOnHeadStep)
    {
        DiskIIController  ctrl (6);
        RecordingSink     sink;

        ctrl.SetAudioSink (&sink);

        // Head starts at quarter-track 0. Energizing phase 3 with the
        // head at rot=0 pulls toward "decrement" direction -- pre-
        // clamp delta is negative, clamped to 0 == bump.
        ctrl.Write (0xC0E7, 0x00);

        Assert::AreEqual (0, sink.CountOf (RecordingSink::Event::HeadStep));
        Assert::AreEqual (1, sink.CountOf (RecordingSink::Event::HeadBump));
    }

    TEST_METHOD (PhaseChange_oneQuarterStep_firesOnHeadStep_withCorrectQt)
    {
        DiskIIController  ctrl (6);
        RecordingSink     sink;

        ctrl.SetAudioSink (&sink);

        // Head at QT=0, rot=0. Adjacent phase 1 pulls forward
        // (direction=+1, single-magnet => qtDelta=+2).
        ctrl.Write (0xC0E3, 0x00);

        Assert::AreEqual (1, sink.CountOf (RecordingSink::Event::HeadStep));
        Assert::AreEqual (0, sink.CountOf (RecordingSink::Event::HeadBump));
        Assert::AreEqual (2, ctrl.GetQuarterTrack());

        // Verify the qt argument matches the post-step position.
        for (auto & e : sink.log)
        {
            if (e.event == RecordingSink::Event::HeadStep)
            {
                Assert::AreEqual (2, e.arg);
            }
        }
    }
};
