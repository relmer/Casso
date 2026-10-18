#include "Pch.h"

#include "DebuggerTests/HandlerTestRig.h"
#include "Debugger/Handlers/BasicHandlers.h"
#include "Debugger/Reverse/ReverseStopTest.h"
#include "EmuTests/GuestSession.h"
#include "EmuTests/KeystrokeInjector.h"
#include "EmuTests/MachineIdle.h"
#include "EmuTests/ReverseSessionRig.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBasicBreakpointTests
//
//  Applesoft's breakpoints in history. An enhanced //e runs a short loop
//  with history recording, one instruction at a time, and whether the
//  reverse stop test fires is stored for each position; reverse
//  continue must then land on those positions, latest first, counting
//  nothing. A keyframe every thousand frames puts the whole run in one
//  stretch, so each replay passes every boundary the forward run did.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseBasicBreakpointTests)
{
public:

    static constexpr uint64_t  kBootCycles      = 20'000'000;
    static constexpr uint64_t  kFramesPerKey    = 1000;
    static constexpr size_t    kRunSteps        = 200'000;
    static constexpr Word      kCurLin          = 0x75;

    struct Live
    {
        uint64_t  position = 0;
        bool      isStop   = false;
        Word      line     = 0;
    };

    using Rig = MachineHandlerRig<BasicHandlers>;

    struct BasicRig
    {
        Rig                rig        { "Apple2eEnhanced", TestMachine::Slots::Empty };
        ReverseController  controller { rig.machine };
        ReverseStopTest    stopTest   { rig.session };
        std::vector<Live>  live;

        Word Line()
        {
            Byte  low  = 0;
            Byte  high = 0;



            rig.target.TryPeek (kCurLin,              low);
            rig.target.TryPeek ((Word) (kCurLin + 1), high);
            return (Word) (low | (high << 8));
        }

        //  Types the program and RUN, starts history and sets the breakpoints.
        void Prepare (std::initializer_list<const char *> lines, std::initializer_list<const char *> breakpoints)
        {
            MachineIdle::RunUntilIdle (rig.machine, kBootCycles);

            for (const char * line : lines)
            {
                GuestSession::TypeAndCollect (rig.machine, line);
            }

            Assert::AreEqual ((size_t) 3, KeystrokeInjector::InjectString (rig.machine, "RUN"), L"RUN typed");
            rig.machine.GetRefs().keyboard->PressKey (KeystrokeInjector::kAppleReturn);

            AssertSucceeded (controller.Start (ReverseSessionRig::MakeSettings (kFramesPerKey)), L"Start");

            for (const char * line : breakpoints)
            {
                rig.RunOk (line);
            }
        }

        //  Steps live, recording at each position whether the stop test
        //  fires, with the debugger's hook off so its own breakpoints stop
        //  nothing; then the hook is put back.
        void Record()
        {
            DebugHook  * hook = rig.machine.GetDebugHook();



            rig.machine.SetDebugHook (nullptr);

            while (live.size() < kRunSteps)
            {
                Live  step;



                step.position = rig.machine.GetPosition();
                step.isStop   = stopTest.ShouldStopBefore (rig.machine, rig.machine.GetCpu()->GetPC());
                step.line     = Line();
                live.push_back (step);
                rig.machine.StepOne();
            }

            rig.machine.SetDebugHook (hook);
            rig.session.OnStopConditionsChanged();
        }

        std::vector<Live> GetHits() const
        {
            std::vector<Live>  hits;



            std::ranges::copy_if (live, std::back_inserter (hits), [] (const Live & step) { return step.isStop; });
            return hits;
        }

        uint32_t GetHitCount()
        {
            uint32_t  count = 0;



            for (const Breakpoint & entry : rig.session.GetBreakpoints().GetAll())
            {
                count += entry.hits;
            }

            return count;
        }
    };


    //  Line 20 begins three times and A changes three times, each change
    //  stopping as line 30 begins; reverse continue lands on all six,
    //  latest first.
    TEST_METHOD (ReverseContinueLandsWhereAForwardRunStops)
    {
        static constexpr Word  kLines[] = { 30, 20, 30, 20, 30, 20 };
        BasicRig               basic;
        std::vector<Live>      hits;
        ReverseResult          result;



        basic.Prepare ({ "10 FOR I = 1 TO 3", "20 A = A + 1", "30 NEXT I", "40 END" }, { "BPBASIC 20", "BPBASICVAR A" });
        basic.Record();
        hits = basic.GetHits();

        Assert::AreEqual (std::size (kLines), hits.size(), std::format (L"{} hits", hits.size()).c_str());

        for (size_t at = hits.size(); at > 0; at--)
        {
            AssertSucceeded (basic.controller.ReverseContinue (basic.stopTest, result), L"ReverseContinue");

            Assert::IsTrue             (result.outcome == ReverseOutcome::Moved, std::format (L"hit {}", at - 1).c_str());
            Assert::AreEqual<uint64_t> (hits[at - 1].position, result.position, std::format (L"hit {}", at - 1).c_str());
            Assert::AreEqual<Word>     (kLines[hits.size() - at], basic.Line(), std::format (L"hit {}", at - 1).c_str());
        }

        AssertSucceeded (basic.controller.ReverseContinue (basic.stopTest, result), L"ReverseContinue past the first hit");
        Assert::IsTrue   (result.outcome != ReverseOutcome::Moved, L"no hit before the first");
        Assert::AreEqual (0u, basic.GetHitCount(), L"reverse continue counted nothing");
        Assert::IsTrue   (basic.rig.sink.stops.empty());
    }

    //  An error stop lands before ERROR, in history as live.
    TEST_METHOD (ReverseContinueLandsOnAnError)
    {
        BasicRig           basic;
        std::vector<Live>  hits;
        ReverseResult      result;



        basic.Prepare ({ "10 A = 1", "20 B = 1 / 0" }, { "BRKBASIC ON" });
        basic.Record();
        hits = basic.GetHits();

        Assert::AreEqual ((size_t) 1, hits.size());

        AssertSucceeded (basic.controller.ReverseContinue (basic.stopTest, result), L"ReverseContinue");
        Assert::AreEqual<uint64_t> (hits[0].position, result.position);
        Assert::AreEqual<Word>     (BasicStopMonitor::kError, basic.rig.machine.GetCpu()->GetPC());
        Assert::AreEqual<Word>     (20, basic.Line());
    }
};
