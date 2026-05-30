#include "Pch.h"
#include "Audio/Disk2AudioSource.h"
#include "Audio/IDriveAudioEventSink.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingAudioEventSink
//
//  Captures an ordered log of IDriveAudioEventSink callbacks fired by
//  Disk2AudioSource at each audio-decision moment (spec-006 FR-022 /
//  FR-025). Headless: no file I/O, no audio rendering required.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingAudioEventSink : public IDriveAudioEventSink
{
public:
    enum class Kind
    {
        Started,
        Restarted,
        Continued,
        Silent,
        LoopStarted,
        LoopStopped,
    };

    struct Entry
    {
        Kind          which;
        SoundKind     sound;
        int           drive;
        SilentReason  reason;   // valid only when which == Silent
    };

    vector<Entry>  log;

    void OnAudioStarted (SoundKind k, int d) override
    {
        log.push_back ({ Kind::Started, k, d, SilentReason::BufferMissing });
    }

    void OnAudioRestarted (SoundKind k, int d) override
    {
        log.push_back ({ Kind::Restarted, k, d, SilentReason::BufferMissing });
    }

    void OnAudioContinued (SoundKind k, int d) override
    {
        log.push_back ({ Kind::Continued, k, d, SilentReason::BufferMissing });
    }

    void OnAudioSilent (SoundKind k, int d, SilentReason r) override
    {
        log.push_back ({ Kind::Silent, k, d, r });
    }

    void OnAudioLoopStarted (SoundKind k, int d) override
    {
        log.push_back ({ Kind::LoopStarted, k, d, SilentReason::BufferMissing });
    }

    void OnAudioLoopStopped (SoundKind k, int d) override
    {
        log.push_back ({ Kind::LoopStopped, k, d, SilentReason::BufferMissing });
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AudioSourceEventSinkTests
//
//  Spec-006 T039: each test wires a recording sink to Disk2AudioSource
//  and exercises a single controller-event path. The
//  SetSampleBufferForTest seam keeps the host filesystem out of scope
//  (constitution Principle II). With NO sink attached, audio output is
//  byte-identical to the pre-feature path -- that invariant is covered
//  by the pre-existing DriveAudioMixer / Disk2AudioSource* test suites
//  acting as the regression gate.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Disk2AudioSourceEventSinkTests)
{
public:

    TEST_METHOD (FirstHeadStepAfterLongQuiet_firesAudioStarted)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (16, 0.5f));
        src.SetAudioEventSink (&sink);

        src.Tick       (100000);
        src.OnHeadStep (1);

        Assert::AreEqual (size_t (1), sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[0].sound == SoundKind::HeadStep);
        Assert::AreEqual (0, sink.log[0].drive);
    }

    TEST_METHOD (TwoCloseHeadSteps_firesStartedThenRestarted)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        // Long HeadStep buffer so the first shot is still playing
        // when the second step arrives.
        src.SetSampleBufferForTest (L"HeadStep", vector<float> (1024, 0.5f));
        src.SetAudioEventSink (&sink);

        src.Tick       (100000);
        src.OnHeadStep (1);

        // Second step 2,000 cycles later: inside kSeekThresholdCycles
        // (16,368), but no prior seek mode at entry -> Restarted.
        src.Tick       (102000);
        src.OnHeadStep (2);

        Assert::AreEqual (size_t (2),                                     sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[1].which == RecordingAudioEventSink::Kind::Restarted);
    }

    TEST_METHOD (HeadStepPastSeekThreshold_doesNotEnterSeekContinuePath)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (1024, 0.5f));
        src.SetAudioEventSink (&sink);

        src.Tick       (100000);
        src.OnHeadStep (1);

        // Beyond kSeekThresholdCycles (16,368) the source should start
        // a new event rather than restart/continue seek behavior.
        src.Tick       (116500);
        src.OnHeadStep (2);

        Assert::AreEqual (size_t (2), sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[1].which == RecordingAudioEventSink::Kind::Restarted);
        Assert::IsFalse  (src.IsSeekMode());
    }

    TEST_METHOD (HeadStepDuringSeekMode_firesAudioContinued)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (1024, 0.5f));
        src.SetAudioEventSink (&sink);

        // Two close steps put the source into seek mode by the third.
        src.Tick       (100000);
        src.OnHeadStep (1);
        src.Tick       (102000);
        src.OnHeadStep (2);
        Assert::IsTrue (src.IsSeekMode());

        // Third close step: entry m_seekMode == true -> Continued.
        src.Tick       (104000);
        src.OnHeadStep (3);

        Assert::AreEqual (size_t (3),                                       sink.log.size());
        Assert::IsTrue   (sink.log[2].which == RecordingAudioEventSink::Kind::Continued);
        Assert::IsTrue   (sink.log[2].sound == SoundKind::HeadStep);
    }

    TEST_METHOD (MissingHeadStepBuffer_firesAudioSilentBufferMissing)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        // No HeadStep buffer injected -> empty.
        src.SetAudioEventSink (&sink);

        src.Tick       (100000);
        src.OnHeadStep (1);

        Assert::AreEqual (size_t (1),                                       sink.log.size());
        Assert::IsTrue   (sink.log[0].which  == RecordingAudioEventSink::Kind::Silent);
        Assert::IsTrue   (sink.log[0].sound  == SoundKind::HeadStep);
        Assert::IsTrue   (sink.log[0].reason == SilentReason::BufferMissing);
    }

    TEST_METHOD (MotorEngaged_firesAudioLoopStarted_andDisengagedFiresLoopStopped)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"MotorLoop",  vector<float> (32, 1.0f));
        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.5f));
        src.OnDiskInserted();
        src.SetAudioEventSink (&sink);

        src.OnMotorEngaged();
        src.OnMotorDisengaged();

        Assert::AreEqual (size_t (2),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::LoopStarted);
        Assert::IsTrue   (sink.log[0].sound == SoundKind::MotorLoop);
        Assert::IsTrue   (sink.log[1].which == RecordingAudioEventSink::Kind::LoopStopped);
        Assert::IsTrue   (sink.log[1].sound == SoundKind::MotorLoop);
    }

    TEST_METHOD (MotorEngagedWithMissingBuffer_firesAudioSilentBufferMissing)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetAudioEventSink (&sink);

        src.OnMotorEngaged();

        Assert::AreEqual (size_t (1),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].which  == RecordingAudioEventSink::Kind::Silent);
        Assert::IsTrue   (sink.log[0].sound  == SoundKind::MotorLoop);
        Assert::IsTrue   (sink.log[0].reason == SilentReason::BufferMissing);
    }

    TEST_METHOD (HeadBump_firesAudioStarted_thenRestartedWhenStillPlaying)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"HeadStop", vector<float> (1024, 0.9f));
        src.SetAudioEventSink (&sink);

        src.OnHeadBump();
        src.OnHeadBump();

        Assert::AreEqual (size_t (2),                                       sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[0].sound == SoundKind::HeadStop);
        Assert::IsTrue   (sink.log[1].which == RecordingAudioEventSink::Kind::Restarted);
        Assert::IsTrue   (sink.log[1].sound == SoundKind::HeadStop);
    }

    TEST_METHOD (DiskInsertedAndEjected_fireAudioStartedForDoorSounds)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (16, 0.5f));
        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.5f));
        src.SetAudioEventSink (&sink);

        src.OnDiskInserted();
        src.OnDiskEjected();

        Assert::AreEqual (size_t (2),                                       sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[0].sound == SoundKind::DoorClose);
        Assert::IsTrue   (sink.log[1].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[1].sound == SoundKind::DoorOpen);
    }

    TEST_METHOD (DiskInsertedWithMissingBuffer_firesAudioSilentBufferMissing)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetAudioEventSink (&sink);

        src.OnDiskInserted();

        Assert::AreEqual (size_t (1),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].which  == RecordingAudioEventSink::Kind::Silent);
        Assert::IsTrue   (sink.log[0].sound  == SoundKind::DoorClose);
        Assert::IsTrue   (sink.log[0].reason == SilentReason::BufferMissing);
    }

    TEST_METHOD (NoSinkAttached_isNoopAndDoesNotCrash)
    {
        Disk2AudioSource  src;

        src.SetSampleBufferForTest (L"MotorLoop", vector<float> (16, 1.0f));
        src.SetSampleBufferForTest (L"HeadStep",  vector<float> (16, 0.5f));

        src.OnMotorEngaged();
        src.OnHeadStep (1);
        src.OnHeadBump();
        src.OnDiskInserted();
        src.OnDiskEjected();
        src.OnMotorDisengaged();

        Assert::IsFalse (src.IsMotorRunning());
    }


    ////////////////////////////////////////////////////////////////////
    //
    //  Spec-006 bug 14a -- motor loop is gated on disk presence.
    //
    ////////////////////////////////////////////////////////////////////

    TEST_METHOD (MotorEngagedWithNoDisk_firesAudioSilentNoDiskPresent)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"MotorLoop", vector<float> (32, 1.0f));
        src.SetAudioEventSink (&sink);

        src.OnMotorEngaged();

        Assert::AreEqual (size_t (1),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].which  == RecordingAudioEventSink::Kind::Silent);
        Assert::IsTrue   (sink.log[0].sound  == SoundKind::MotorLoop);
        Assert::IsTrue   (sink.log[0].reason == SilentReason::NoDiskPresent);
    }

    TEST_METHOD (EjectWhileMotorOn_firesLoopStoppedAndSilentNoDiskPresent)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"MotorLoop",  vector<float> (32, 1.0f));
        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.5f));
        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (16, 0.5f));

        src.OnDiskInserted();
        src.OnMotorEngaged();
        src.SetAudioEventSink (&sink);

        src.OnDiskEjected();

        // Expected ordered log entries:
        //   [0] LoopStopped (MotorLoop)
        //   [1] Silent      (MotorLoop, NoDiskPresent)
        //   [2] Started     (DoorOpen)
        Assert::AreEqual (size_t (3),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].which  == RecordingAudioEventSink::Kind::LoopStopped);
        Assert::IsTrue   (sink.log[0].sound  == SoundKind::MotorLoop);
        Assert::IsTrue   (sink.log[1].which  == RecordingAudioEventSink::Kind::Silent);
        Assert::IsTrue   (sink.log[1].sound  == SoundKind::MotorLoop);
        Assert::IsTrue   (sink.log[1].reason == SilentReason::NoDiskPresent);
        Assert::IsTrue   (sink.log[2].which  == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[2].sound  == SoundKind::DoorOpen);
        Assert::IsFalse  (src.IsDiskPresent());
    }

    TEST_METHOD (InsertWhileMotorOn_firesRestartedAndDoorClose)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"MotorLoop",  vector<float> (32, 1.0f));
        src.SetSampleBufferForTest (L"DoorClose", vector<float> (16, 0.5f));

        // Motor on while drive bay empty -- silent.
        src.SetAudioEventSink (&sink);
        src.OnMotorEngaged();

        Assert::AreEqual (size_t (1),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].reason == SilentReason::NoDiskPresent);

        sink.log.clear();

        src.OnDiskInserted();

        // Expected:
        //   [0] Restarted (MotorLoop)
        //   [1] Started   (DoorClose)
        Assert::AreEqual (size_t (2),                                        sink.log.size());
        Assert::IsTrue   (sink.log[0].which == RecordingAudioEventSink::Kind::Restarted);
        Assert::IsTrue   (sink.log[0].sound == SoundKind::MotorLoop);
        Assert::IsTrue   (sink.log[1].which == RecordingAudioEventSink::Kind::Started);
        Assert::IsTrue   (sink.log[1].sound == SoundKind::DoorClose);
        Assert::IsTrue   (src.IsDiskPresent());
    }

    TEST_METHOD (MotorOnWithDisk_thenEjectThenReinsert_mixOutputGatedCorrectly)
    {
        Disk2AudioSource   src;
        float              out[8] = {};

        src.SetSampleBufferForTest (L"MotorLoop",  vector<float> (16, 1.0f));
        src.SetSampleBufferForTest (L"DoorOpen",  vector<float> (1,  0.0f));
        src.SetSampleBufferForTest (L"DoorClose", vector<float> (1,  0.0f));

        src.OnDiskInserted();
        src.OnMotorEngaged();
        src.GeneratePCM (out, 8);

        Assert::IsTrue (out[0] > 0.0f, L"Motor audible with disk present");

        memset (out, 0, sizeof (out));
        src.OnDiskEjected();
        src.GeneratePCM (out, 8);

        for (int i = 0; i < 8; i++)
        {
            Assert::AreEqual (0.0f, out[i], L"Motor silent after eject");
        }

        memset (out, 0, sizeof (out));
        src.OnDiskInserted();
        src.GeneratePCM (out, 8);

        Assert::IsTrue (out[0] > 0.0f, L"Motor audible again after re-insert");
    }


    ////////////////////////////////////////////////////////////////////
    //
    //  Boot-recalibrate ratchet -- a rapid run of consecutive head bumps
    //  (head pinned at the track-0 stop) renders as a grouped
    //  [thunk, pause, click, click] cadence rather than a steady stream
    //  of identical thunks (the "fast buzz" regression). Models the real
    //  controller stream: an isolated first bump, then bumps every
    //  ~19,690 cycles -- comfortably inside kHeadIdleCycles (51,150).
    //
    ////////////////////////////////////////////////////////////////////

    TEST_METHOD (RapidBumpBurst_rendersMachineGunRatchetNotSteadyThunks)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;
        uint64_t                  cycle = 236309;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (1024, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (1024, 0.9f));
        src.SetAudioEventSink (&sink);

        // Nine consecutive bumps, each 19,690 cycles after the previous.
        for (int n = 0; n < 9; n++)
        {
            src.Tick (cycle);
            src.OnHeadBump();
            cycle += 19690;
        }

        // Two of the nine bumps land on the silent ratchet slot, so only
        // seven produce an audio onset. The expected sound sequence is
        // [thunk, click, click, thunk, click, click, thunk].
        SoundKind  expected[7] =
        {
            SoundKind::HeadStop,
            SoundKind::HeadStep,
            SoundKind::HeadStep,
            SoundKind::HeadStop,
            SoundKind::HeadStep,
            SoundKind::HeadStep,
            SoundKind::HeadStop,
        };

        Assert::AreEqual (size_t (7), sink.log.size());

        for (int i = 0; i < 7; i++)
        {
            Assert::IsTrue (sink.log[i].sound == expected[i]);
        }
    }

    TEST_METHOD (IsolatedBumpsFarApart_eachRenderAsFirmThunk)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (16, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (16, 0.9f));
        src.SetAudioEventSink (&sink);

        // Two bumps separated by more than kHeadIdleCycles (51,150) are
        // not a ratchet -- each is a discrete wall-bang thunk.
        src.Tick (100000);
        src.OnHeadBump();
        src.Tick (100000 + 60000);
        src.OnHeadBump();

        Assert::AreEqual (size_t (2), sink.log.size());
        Assert::IsTrue   (sink.log[0].sound == SoundKind::HeadStop);
        Assert::IsTrue   (sink.log[1].sound == SoundKind::HeadStop);
    }

    TEST_METHOD (StepBetweenBumps_reArmsRatchetSoNextBumpIsThunk)
    {
        Disk2AudioSource          src;
        RecordingAudioEventSink   sink;

        src.SetSampleBufferForTest (L"HeadStep", vector<float> (16, 0.5f));
        src.SetSampleBufferForTest (L"HeadStop", vector<float> (16, 0.9f));

        // Drive a short ratchet so the slot counter advances off zero.
        src.Tick (100000);
        src.OnHeadBump();
        src.Tick (119690);
        src.OnHeadBump();

        // A real step (head left the wall) must re-arm the pattern.
        src.Tick (139380);
        src.OnHeadStep (1);
        Assert::AreEqual (uint32_t (0), src.GetRatchetSlot());

        // The next bump after the step is therefore an isolated thunk.
        src.SetAudioEventSink (&sink);
        src.Tick (159070);
        src.OnHeadBump();

        Assert::AreEqual (size_t (1), sink.log.size());
        Assert::IsTrue   (sink.log[0].sound == SoundKind::HeadStop);
    }
};
