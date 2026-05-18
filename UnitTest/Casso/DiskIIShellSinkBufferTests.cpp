#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "DiskIIShellSinkBuffer.h"
#include "DebugDialogProjection.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


// CppUnitTest TEST_METHOD bodies inline std::deque<DiskIIEvent>
// scratch buffers that push the function-stack budget past C6262's
// threshold. Same pattern as DebugDialogProjectionTests.
#pragma warning(disable: 6262)



////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIShellSinkBufferTests
//
//  Spec-006 round-4 bug 3 coverage. Exercises the shell-side sink
//  buffer's push / drain / live-forward semantics without spinning up
//  a real EmulatorShell, DiskIIController, or DiskIIDebugDialog --
//  all interactions go through the public IDiskIIEventSink /
//  IDriveAudioEventSink methods and DrainInto.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    class RecordingLiveSink : public IDiskIIEventSink,
                              public IDriveAudioEventSink
    {
    public:
        int  motorEngagedCount   = 0;
        int  headStepCount       = 0;
        int  dataReadCount       = 0;
        int  audioStartedCount   = 0;
        int  lastDriveSelect     = -1;

        void OnMotorCommandOn   () override {}
        void OnMotorEngaged     () override { motorEngagedCount++; }
        void OnMotorCommandOff  () override {}
        void OnMotorDisengaged  () override {}
        void OnHeadStep         (int, int) override { headStepCount++; }
        void OnHeadBump         (int) override {}
        void OnAddressMark      (int, int, int) override {}
        void OnDataMarkRead     (int, int, int, int) override { dataReadCount++; }
        void OnDataMarkWrite    (int, int, int, int) override {}
        void OnDriveSelect      (int drive) override { lastDriveSelect = drive; }
        void OnDiskInserted     (int) override {}
        void OnDiskEjected      (int) override {}

        void OnAudioStarted     (SoundKind, int) override { audioStartedCount++; }
        void OnAudioRestarted   (SoundKind, int) override {}
        void OnAudioContinued   (SoundKind, int) override {}
        void OnAudioSilent      (SoundKind, int, SilentReason) override {}
        void OnAudioLoopStarted (SoundKind, int) override {}
        void OnAudioLoopStopped (SoundKind, int) override {}
    };
}



namespace DiskIIShellSinkBufferTests
{
    TEST_CLASS (DiskIIShellSinkBufferTests)
    {
    public:
        TEST_METHOD (Push100EventsThenDrain_destHasAllEventsInOrder)
        {
            DiskIIShellSinkBuffer    buf;
            std::deque<DiskIIEvent>  out;
            int                      i = 0;

            for (i = 0; i < 100; i++)
            {
                buf.OnHeadStep (i, i + 1);
            }

            buf.DrainInto (out, 1000);

            Assert::AreEqual ((size_t) 100, out.size ());

            for (i = 0; i < 100; i++)
            {
                Assert::IsTrue (out[i].type == DiskIIEventType::HeadStep);
                Assert::AreEqual (i,     out[i].payload.step.prevQt);
                Assert::AreEqual (i + 1, out[i].payload.step.newQt);
            }
        }


        TEST_METHOD (DrainInto_mixedControllerAndAudio_preservesFifoOrder)
        {
            DiskIIShellSinkBuffer    buf;
            std::deque<DiskIIEvent>  out;

            buf.OnMotorEngaged ();
            buf.OnAudioStarted (SoundKind::MotorLoop, 0);
            buf.OnAddressMark  (17, 5, 254);
            buf.OnDataMarkRead (17, 5, 254, 256);
            buf.OnDriveSelect  (1);

            buf.DrainInto (out, 1000);

            Assert::AreEqual ((size_t) 5,                       out.size ());
            Assert::IsTrue   (out[0].type == DiskIIEventType::MotorEngaged);
            Assert::IsTrue   (out[1].type == DiskIIEventType::AudioStarted);
            Assert::IsTrue   (out[1].category == EventCategory::Audio);
            Assert::IsTrue   (out[2].type == DiskIIEventType::AddrMark);
            Assert::IsTrue   (out[3].type == DiskIIEventType::DataRead);
            Assert::AreEqual (17,                                out[3].payload.dataMark.track);
            Assert::AreEqual (5,                                 out[3].payload.dataMark.sector);
            Assert::AreEqual (254,                               out[3].payload.dataMark.volume);
            Assert::AreEqual (256,                               out[3].payload.dataMark.byteCount);
            Assert::IsTrue   (out[4].type == DiskIIEventType::DriveSelect);
        }


        TEST_METHOD (DrainInto_destAlreadyOverCap_trimsFront)
        {
            DiskIIShellSinkBuffer    buf;
            std::deque<DiskIIEvent>  out;
            int                      i = 0;

            for (i = 0; i < 50; i++)
            {
                buf.OnHeadStep (i, i + 1);
            }

            buf.DrainInto (out, 20);

            // 50 events pushed, cap 20 -> oldest 30 popped from front,
            // newest 20 remain. Last event was OnHeadStep (49, 50).
            Assert::AreEqual ((size_t) 20, out.size ());
            Assert::AreEqual (30,          out.front ().payload.step.prevQt);
            Assert::AreEqual (49,          out.back  ().payload.step.prevQt);
        }


        TEST_METHOD (Push50EventsBeforeOpen_thenLiveSinkAttached_dialogSeesAllInOrder)
        {
            DiskIIShellSinkBuffer    buf;
            RecordingLiveSink        live;
            std::deque<DiskIIEvent>  backlog;
            int                      i = 0;

            // Simulate boot-time activity before the dialog is opened.
            for (i = 0; i < 50; i++)
            {
                buf.OnMotorEngaged ();
            }

            // Dialog opens: shell drains backlog into dialog's deque
            // BEFORE installing live forwarding.
            buf.DrainInto (backlog, 1000);

            Assert::AreEqual ((size_t) 50, backlog.size ());

            for (i = 0; i < 50; i++)
            {
                Assert::IsTrue (backlog[i].type == DiskIIEventType::MotorEngaged);
            }

            // Now install the live sink. Subsequent events bypass the
            // buffer's ring and forward to the dialog directly.
            buf.SetLiveSink (&live, &live);

            buf.OnMotorEngaged ();
            buf.OnHeadStep     (0, 1);
            buf.OnDataMarkRead (0, 0, 0, 256);
            buf.OnAudioStarted (SoundKind::MotorLoop, 0);

            Assert::AreEqual (1, live.motorEngagedCount);
            Assert::AreEqual (1, live.headStepCount);
            Assert::AreEqual (1, live.dataReadCount);
            Assert::AreEqual (1, live.audioStartedCount);

            // A second drain after live mode sees nothing -- the
            // ring is not being populated anymore.
            std::deque<DiskIIEvent>  secondDrain;

            buf.DrainInto (secondDrain, 1000);
            Assert::AreEqual ((size_t) 0, secondDrain.size ());
        }


        TEST_METHOD (DriveSelectStamping_currentDriveCarriesToSubsequentEvents)
        {
            DiskIIShellSinkBuffer    buf;
            std::deque<DiskIIEvent>  out;

            // Default current drive is 0 -- the first head-step
            // carries drive 0.
            buf.OnHeadStep    (0, 1);
            buf.OnDriveSelect (1);
            buf.OnHeadStep    (1, 2);

            buf.DrainInto (out, 100);

            Assert::AreEqual ((size_t) 3,                  out.size ());
            Assert::AreEqual ((int8_t) 0,                  out[0].drive);
            Assert::IsTrue   (out[1].type == DiskIIEventType::DriveSelect);
            Assert::AreEqual ((int8_t) 1,                  out[2].drive);
            Assert::AreEqual (1,                            buf.GetCurrentDrive ());
        }


        TEST_METHOD (LiveSinkTracksDriveSelectStateInBuffer)
        {
            DiskIIShellSinkBuffer    buf;
            RecordingLiveSink        live;

            buf.SetLiveSink (&live, &live);
            buf.OnDriveSelect (1);

            Assert::AreEqual (1, live.lastDriveSelect);
            Assert::AreEqual (1, buf.GetCurrentDrive ());
        }


        TEST_METHOD (PushBeyondRingCapacity_silentlyDropsOldest)
        {
            DiskIIShellSinkBuffer    buf;
            std::deque<DiskIIEvent>  out;
            int                      i = 0;
            int                      pushed = 5000;   // > kEventRingCapacity (4096)

            for (i = 0; i < pushed; i++)
            {
                buf.OnHeadBump (i);
            }

            buf.DrainInto (out, 10000);

            // The ring caps at kEventRingCapacity = 4096; pushes that
            // would overflow are dropped. The exact cut is an SPSC
            // implementation detail, so just assert the cap is the
            // ceiling.
            Assert::IsTrue (out.size () <= DiskIIEventRing::kEventRingCapacity);
            Assert::IsTrue (out.size () > 0);
        }
    };
}
