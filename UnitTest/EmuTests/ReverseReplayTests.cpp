#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Devices/Disk/DiskImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kReplaySliceCycles = 3000;
static constexpr size_t    s_kReplaySlices      = 400;
static constexpr size_t    s_kReplaySampleEvery = 9;
static constexpr size_t    s_kReplayShuffle     = 7;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseReplayTests
//
//  Determinism: a scripted session with keys, paddle moves, disk reads, guest
//  disk writes and Mockingboard writes is recorded live, with the whole
//  machine saved at many instruction boundaries. Seeking back to each of
//  them, in a scattered order, must give back exactly the bytes that were
//  live there: CPU, every RAM bank, every device and the disk's track bits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseReplayTests)
{
public:

    struct Sample
    {
        uint64_t           position = 0;
        uint64_t           cycle    = 0;
        std::vector<Byte>  state;
    };


    TEST_METHOD (SeekingBackGivesTheLiveMachineAtEverySample)
    {
        TestMachine          machine    ("Apple2e");
        ReverseController    controller (machine);
        std::vector<Sample>  samples;
        Sample               last;
        ReverseResult        result;
        HRESULT              hr         = S_OK;
        size_t               count      = 0;
        size_t               i          = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        AssertSucceeded (hr, L"Start");

        samples = RecordSession (machine);
        last    = TakeSample (machine);
        count   = samples.size();

        CheckSessionCoverage (machine, controller);
        Assert::IsTrue (count > 0, L"the session must have taken samples");

        for (i = 0; i < count; i++)
        {
            const Sample  & sample = samples[(i * s_kReplayShuffle) % count];

            hr = controller.SeekToCycle (sample.cycle, result);
            AssertSucceeded (hr, L"SeekToCycle");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"a seek inside history moves");
            Assert::AreEqual<uint64_t> (sample.position, result.position, L"the seek lands on the sampled boundary");
            CheckState (sample.state, ReverseSessionRig::Save (machine), sample.position);
            Assert::IsTrue (controller.IsInHistory(), L"behind the live end, the machine is in history");
        }

        hr = controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");

        Assert::AreEqual<uint64_t> (last.position, result.position, L"a seek past the end stops at the live end");
        CheckState (last.state, ReverseSessionRig::Save (machine), last.position);
        Assert::IsFalse (controller.IsInHistory(), L"at the live end the machine is live again");
    }


    //  Replaying forward from the oldest keyframe to the live end reaches every
    //  keyframe, each checked against its checksum.
    TEST_METHOD (ReplayFromTheFirstKeyframeMatchesEveryKeyframe)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             keyframes  = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        RecordSession (machine);
        keyframes = controller.GetKeyframes().GetCount();

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition oldest");

        hr = controller.SeekToCycle (UINT64_MAX, result);
        AssertSucceeded (hr, L"SeekToCycle live");

        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"no keyframe may diverge");
        Assert::AreEqual (keyframes, controller.GetKeyframes().GetCount(), L"every keyframe survives the replay");
        Assert::IsFalse (controller.IsInHistory());
    }


    //  A keyframe whose stored state is not what the machine had: replaying
    //  forward across it must notice, cut history at the keyframe before it,
    //  and leave the machine live there.
    TEST_METHOD (ACorruptedKeyframeIsCaughtAndHistoryIsCut)
    {
        TestMachine                     machine    ("Apple2e");
        ReverseController               controller (machine);
        KeyframeStore                 & store      = controller.GetKeyframes();
        std::vector<KeyframeInfo>       infos;
        std::vector<std::vector<Byte>>  states;
        ReverseResult                   result;
        HRESULT                         hr         = S_OK;
        size_t                          bad        = 0;
        size_t                          i          = 0;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        RecordSession (machine);

        Assert::IsTrue (store.GetCount() >= 6, L"the session must span several keyframes");

        bad = 2;

        for (i = bad; i < store.GetCount(); i++)
        {
            infos.push_back (store.GetInfo (i));
            states.emplace_back();

            hr = store.Restore (i, states.back());
            AssertSucceeded (hr, L"Restore");
        }

        // Flip one byte of RAM in the bad keyframe, keep the rest intact.
        states[0][states[0].size() / 4] ^= 0x5A;

        hr = store.TruncateAfter (store.GetInfo (bad - 1).cycle);
        AssertSucceeded (hr, L"TruncateAfter");

        for (i = 0; i < infos.size(); i++)
        {
            hr = store.Add (infos[i].position, infos[i].cycle, infos[i].journalIndex, states[i]);
            AssertSucceeded (hr, L"Add");
        }

        // A seek loads the keyframe at or before its target, so it reaches
        // no keyframe; the first step back into the stretch before the bad
        // one replays the whole stretch to build its table, and reaches it.
        hr = controller.SeekToPosition (store.GetInfo (bad).position - 1, result);
        AssertSucceeded (hr, L"SeekToPosition before the bad keyframe");
        Assert::IsTrue (result.outcome == ReverseOutcome::Moved, L"nothing is crossed yet");

        hr = controller.StepBack (result);
        AssertSucceeded (hr, L"StepBack into the stretch before the bad keyframe");

        Assert::IsTrue (result.outcome == ReverseOutcome::HistoryCut, L"the replay must catch the bad keyframe");
        Assert::AreEqual (bad, store.GetCount(), L"history is cut after the last good keyframe");
        Assert::AreEqual<uint64_t> (store.GetInfo (bad - 1).position, machine.GetPosition(), L"the machine is at the last good keyframe");
        Assert::AreEqual (store.GetInfo (bad - 1).journalIndex, machine.GetInputJournal().GetEndIndex(), L"the inputs after it are dropped");
        Assert::IsTrue (store.DoesStateMatch (bad - 1, ReverseSessionRig::Save (machine)), L"in that keyframe's state");
        Assert::IsFalse (controller.IsInHistory(), L"and live there");
    }


    //  What recording costs at full speed: the same guest loop for the same
    //  emulated time without history, then with it at the default settings,
    //  one keyframe every ten frames and nothing per instruction. Logged
    //  rather than asserted; the numbers mean something in Release.
    TEST_METHOD (RecordingCostIsLogged)
    {
        constexpr uint64_t  kFrames    = 300;
        TestMachine         plain      ("Apple2e");
        TestMachine         recorded   ("Apple2e");
        ReverseController   controller (recorded);
        double              plainUs    = 0;
        double              recordedUs = 0;
        double              seconds    = static_cast<double> (kFrames * KeyframeSettings::kFrameCycles) / kClockHz;
        HRESULT             hr         = S_OK;



        ReverseSessionRig::Prepare (plain);
        plainUs = TimeFrames (plain, kFrames);

        ReverseSessionRig::Prepare (recorded);

        hr = controller.Start (ReverseSettings());
        AssertSucceeded (hr, L"Start");

        recordedUs = TimeFrames (recorded, kFrames);

        Assert::AreEqual<uint64_t> (plain.GetPosition(), recorded.GetPosition(), L"both ran the same instructions");

        Logger::WriteMessage (std::format ("{:.2f} emulated seconds without history: {:.1f} ms, {:.1f}x real time; recording: {:.1f} ms, {:.1f}x real time, +{:.1f}%; {} keyframes in {} bytes, {} bytes reserved\n",
                                           seconds,
                                           plainUs / 1000,
                                           seconds * 1e6 / plainUs,
                                           recordedUs / 1000,
                                           seconds * 1e6 / recordedUs,
                                           (recordedUs - plainUs) * 100 / plainUs,
                                           controller.GetKeyframes().GetCount(),
                                           controller.GetKeyframes().GetByteCount(),
                                           controller.GetKeyframes().GetReservedBytes()).c_str());
    }
private:

    static constexpr double  kClockHz = 1022727.0;


    static double TimeFrames (MachineHost & machine, uint64_t frames)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  start     = {};
        LARGE_INTEGER  stop      = {};
        uint64_t       frame     = 0;



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&start);

        for (frame = 0; frame < frames; frame++)
        {
            machine.RunCycles (KeyframeSettings::kFrameCycles);
        }

        QueryPerformanceCounter (&stop);

        return static_cast<double> (stop.QuadPart - start.QuadPart) * 1e6 / static_cast<double> (frequency.QuadPart);
    }


    static std::vector<Sample> RecordSession (MachineHost & machine)
    {
        std::vector<Sample>  samples;
        size_t               slice = 0;



        for (slice = 0; slice < s_kReplaySlices; slice++)
        {
            if (slice % s_kReplaySampleEvery == 0)
            {
                samples.push_back (TakeSample (machine));
            }

            ReverseSessionRig::Feed (machine, slice);
            machine.RunCycles (s_kReplaySliceCycles);
        }

        return samples;
    }


    static Sample TakeSample (MachineHost & machine)
    {
        Sample  sample;



        sample.position = machine.GetPosition();
        sample.cycle    = machine.GetCpu()->GetTotalCycles();
        sample.state    = ReverseSessionRig::Save (machine);

        return sample;
    }


    //  The session must really have exercised what the test claims to cover.
    static void CheckSessionCoverage (MachineHost & machine, const ReverseController & controller)
    {
        const InputJournal  & journal = machine.GetInputJournal();
        size_t                keys    = 0;
        size_t                paddles = 0;
        size_t                i       = 0;



        for (i = journal.GetBeginIndex(); i < journal.GetEndIndex(); i++)
        {
            keys    += journal.GetRecord (i).kind == InputKind::KeyLatch ? 1 : 0;
            paddles += journal.GetRecord (i).kind == InputKind::Paddle   ? 1 : 0;
        }

        Assert::IsTrue (keys > 0,    L"the guest must have read keys");
        Assert::IsTrue (paddles > 0, L"the guest must have read paddle moves");
        Assert::IsTrue (machine.GetDiskStore().GetImage (ReverseSessionRig::kDiskSlot, ReverseSessionRig::kDiskDrive)->IsDirty(), L"the guest must have written the disk");
        Assert::IsTrue (controller.GetKeyframes().GetCount() > 5, L"the session must span several keyframes");

        Logger::WriteMessage (std::format ("{} keyframes in {} bytes, {} journal records ({} keys, {} paddles)\n",
                                           controller.GetKeyframes().GetCount(),
                                           controller.GetKeyframes().GetByteCount(),
                                           journal.GetEndIndex() - journal.GetBeginIndex(),
                                           keys,
                                           paddles).c_str());
    }


    static void CheckState (const std::vector<Byte> & expected, const std::vector<Byte> & actual, uint64_t position)
    {
        size_t  size  = std::min (expected.size(), actual.size());
        size_t  first = 0;



        Assert::AreEqual (expected.size(), actual.size(), std::format (L"state size at position {}", position).c_str());

        while (first < size && expected[first] == actual[first])
        {
            first++;
        }

        Assert::AreEqual (size, first, std::format (L"the replayed state at position {} differs from live first at byte {}", position, first).c_str());
    }
};
