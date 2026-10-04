#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kSampleWarmupCycles = 60000;
static constexpr size_t    s_kSampleSteps        = 3000;
static constexpr size_t    s_kSampleSliceSteps   = 100;
static constexpr size_t    s_kSampleKeyEvery     = 1000;
static constexpr size_t    s_kSamplePaddleEvery  = 700;
static constexpr uint64_t  s_kSampleCostFrames   = 300;
static constexpr uint32_t  s_kSampleSliceCycles  = 1023;
static constexpr size_t    s_kSampleCallCount    = 1000000;





////////////////////////////////////////////////////////////////////////////////
//
//  SliceInputSampleTests
//
//  Host input sampled at the start of each execution slice. A key the host
//  presses is in the live machine before the guest reads it; sampled at the
//  slice boundary, it is journaled there, so every position between the
//  press and the read replays exactly.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SliceInputSampleTests)
{
public:

    struct Step
    {
        uint64_t  position = 0;
        Word      pc       = 0;
        uint64_t  checksum = 0;
    };


    //  One instruction at a time back over a recording in which keys were
    //  pressed and paddles moved between guest reads, then forward again to
    //  the live end: the whole machine matches the live run at every position.
    TEST_METHOD (StepBackAndForwardAcrossAnUnreadKeyMatchesTheLiveRun)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        std::vector<Step>  steps;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        size_t             index      = 0;
        size_t             presses    = 0;
        size_t             pending    = 0;



        presses = Record (machine, controller, steps, pending);

        Assert::IsTrue (presses > 0, L"the recording must press keys");
        Assert::IsTrue (pending > 0, L"and hold one unread across several positions");

        for (index = steps.size() - 1; index > 0; index--)
        {
            hr = controller.StepBack (result);
            AssertSucceeded (hr, L"StepBack");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            CheckStep (machine, steps[index - 1], L"back");
        }

        for (index = 1; index < steps.size(); index++)
        {
            hr = controller.StepForward (result);
            AssertSucceeded (hr, L"StepForward");

            Assert::IsTrue (result.outcome == ReverseOutcome::Moved);
            CheckStep (machine, steps[index], L"forward");
        }

        Assert::IsFalse (controller.IsInHistory(), L"forward to the end of history makes the machine live");
    }


    //  Each press is journaled once, at the slice boundary, and the guest's
    //  later read of the same latch adds nothing.
    TEST_METHOD (ASampledKeyIsNotRecordedAgainAtTheRead)
    {
        TestMachine           machine    ("Apple2e");
        ReverseController     controller (machine);
        std::vector<Step>     steps;
        size_t                presses    = 0;
        size_t                pending    = 0;
        size_t                sampled    = 0;
        size_t                observed   = 0;
        size_t                i          = 0;
        const InputJournal  & journal    = machine.GetInputJournal();



        presses = Record (machine, controller, steps, pending);

        for (i = journal.GetBeginIndex(); i < journal.GetEndIndex(); i++)
        {
            const InputRecord  & record = journal.GetRecord (i);

            if (record.kind == InputKind::KeyLatch && (record.value & 0x80) != 0)
            {
                sampled  += record.isObserved ? 0 : 1;
                observed += record.isObserved ? 1 : 0;
            }
        }

        Assert::IsTrue    (presses > 0,              L"the recording must press keys");
        Assert::AreEqual  (presses, sampled,         L"one boundary record per press");
        Assert::AreEqual  ((size_t) 0, observed,     L"and none again at the read");
    }


    //  With the journal off, sampling records nothing and changes nothing.
    TEST_METHOD (SamplingWithTheJournalOffRecordsNothing)
    {
        TestMachine  machine ("Apple2e");
        size_t       before  = 0;



        ReverseSessionRig::Prepare (machine);

        before = machine.GetInputJournal().GetEndIndex();

        machine.GetRefs().keyboard->PressKey ('Q');
        machine.SampleHostInputs();

        Assert::AreEqual (before, machine.GetInputJournal().GetEndIndex(), L"no record");
    }


    //  What the slice sample costs: the guest loop run in 1023-cycle slices
    //  without sampling, with it and the journal off, and with it while
    //  recording; and the call alone with the journal off. Logged rather than
    //  asserted; the numbers mean something in Release.
    TEST_METHOD (SampleCostIsLogged)
    {
        TestMachine        plain      ("Apple2e");
        TestMachine        sampledOff ("Apple2e");
        TestMachine        recorded   ("Apple2e");
        ReverseController  controller (recorded);
        double             plainUs    = 0;
        double             offUs      = 0;
        double             onUs       = 0;
        double             callNs     = 0;
        HRESULT            hr         = S_OK;



        ReverseSessionRig::Prepare (plain);
        ReverseSessionRig::Prepare (sampledOff);
        ReverseSessionRig::Prepare (recorded);

        hr = controller.Start (ReverseSettings());
        AssertSucceeded (hr, L"Start");

        plainUs = TimeSlices (plain,      false);
        offUs   = TimeSlices (sampledOff, true);
        onUs    = TimeSlices (recorded,   true);
        callNs  = TimeCalls  (sampledOff) * 1000 / s_kSampleCallCount;

        Assert::AreEqual<uint64_t> (plain.GetPosition(), sampledOff.GetPosition(), L"both ran the same instructions");

        Logger::WriteMessage (std::format ("{} frames in slices: no sampling {:.1f} ms; sampling, journal off {:.1f} ms ({:+.2f}%); sampling while recording {:.1f} ms; one call with the journal off {:.2f} ns, {:.3f} us per frame\n",
                                           s_kSampleCostFrames,
                                           plainUs / 1000,
                                           offUs / 1000,
                                           (offUs - plainUs) * 100 / plainUs,
                                           onUs / 1000,
                                           callNs,
                                           callNs * KeyframeSettings::kFrameCycles / s_kSampleSliceCycles / 1000).c_str());
    }


private:

    //  Warms the guest loop up, then steps it one instruction at a time,
    //  sampling host input every s_kSampleSliceSteps instructions as the CPU
    //  thread does at each slice. A key is pressed and a paddle moved just
    //  before some samples. Returns the number of presses; pending counts the
    //  positions at which a pressed key stood unread.
    static size_t Record (
        TestMachine        & machine,
        ReverseController  & controller,
        std::vector<Step>  & steps,
        size_t             & pending)
    {
        HRESULT  hr       = S_OK;
        Step     step;
        size_t   i        = 0;
        size_t   presses  = 0;
        bool     isUnread = false;



        ReverseSessionRig::Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.GetRefs().iieSoftSwitches->SetPaddle (0, 0);
        machine.RunCycles (s_kSampleWarmupCycles);

        for (i = 0; i <= s_kSampleSteps; i++)
        {
            if (i % s_kSampleSliceSteps == 0)
            {
                if (i % s_kSampleKeyEvery == s_kSampleKeyEvery / 2)
                {
                    machine.GetRefs().keyboard->PressKey (static_cast<Byte> ('a' + i / s_kSampleKeyEvery));
                    presses++;
                }

                if (i % s_kSamplePaddleEvery == 0 && i != 0)
                {
                    machine.GetRefs().iieSoftSwitches->SetPaddle (0, static_cast<Byte> (i / s_kSamplePaddleEvery));
                }

                machine.SampleHostInputs();
            }

            isUnread  = !machine.GetRefs().keyboard->IsStrobeClear();
            pending  += isUnread ? 1 : 0;

            step.position = machine.GetPosition();
            step.pc       = machine.GetCpu()->GetPC();
            step.checksum = ReverseSessionRig::Checksum (machine);
            steps.push_back (step);

            if (i < s_kSampleSteps)
            {
                machine.StepOne();
            }
        }

        return presses;
    }


    static void CheckStep (TestMachine & machine, const Step & expected, const wchar_t * when)
    {
        Assert::AreEqual<uint64_t> (expected.position, machine.GetPosition(),     std::format (L"{}: position", when).c_str());
        Assert::AreEqual<Word>     (expected.pc,       machine.GetCpu()->GetPC(), std::format (L"{}: PC at position {}", when, expected.position).c_str());
        Assert::AreEqual<uint64_t> (expected.checksum, ReverseSessionRig::Checksum (machine),
                                    std::format (L"{}: the whole machine at position {}", when, expected.position).c_str());
    }


    static double TimeSlices (MachineHost & machine, bool isSampling)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  start     = {};
        LARGE_INTEGER  stop      = {};
        uint64_t       target    = s_kSampleCostFrames * KeyframeSettings::kFrameCycles;
        uint64_t       executed  = 0;



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&start);

        while (executed < target)
        {
            if (isSampling)
            {
                machine.SampleHostInputs();
            }

            executed += machine.RunCycles (s_kSampleSliceCycles);
        }

        QueryPerformanceCounter (&stop);

        return static_cast<double> (stop.QuadPart - start.QuadPart) * 1e6 / static_cast<double> (frequency.QuadPart);
    }


    static double TimeCalls (MachineHost & machine)
    {
        LARGE_INTEGER  frequency = {};
        LARGE_INTEGER  start     = {};
        LARGE_INTEGER  stop      = {};
        size_t         i         = 0;



        QueryPerformanceFrequency (&frequency);
        QueryPerformanceCounter   (&start);

        for (i = 0; i < s_kSampleCallCount; i++)
        {
            machine.SampleHostInputs();
        }

        QueryPerformanceCounter (&stop);

        return static_cast<double> (stop.QuadPart - start.QuadPart) * 1e6 / static_cast<double> (frequency.QuadPart);
    }
};
