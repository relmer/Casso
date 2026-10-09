#include "Pch.h"

#include "Debugger/CallStackHistory.h"
#include "Debugger/Handlers/CallStackHandlers.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/Replayer.h"
#include "Debugger/Reverse/ReverseController.h"
#include "EmuTests/ReverseSessionRig.h"
#include "HandlerTestRig.h"
#include "Shell/ScratchCallReplayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackRebuildCostTests
//
//  Not a pass or fail: what rebuilding the call record from history costs,
//  written to the test log as milliseconds per million cycles of machine
//  time replayed, so the cost of a rebuild is the figure times the length of
//  history. Each workload records history for kHistoryCycles on a //e, then
//  replays it all from its oldest keyframe twice on a second machine: bare,
//  through the replayer alone, and as the debugger rebuilds the record, with
//  a call recorder watching (ScratchCallReplayer, its machine already
//  built). Three workloads bracket what a user runs:
//
//    idle      power-on to the Applesoft prompt, then its keyboard poll
//    calls     a tight JSR/RTS loop, the recorder's worst case
//    session   the reverse rig: keys, paddles, a disk and the Mockingboard
//
//  Each figure is the median of kRounds. Release only: a Debug build
//  measures the checks, not the replay.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG

namespace CallStackRebuildCostTests
{
    enum class Workload { Idle, Calls, Session };



    static constexpr int       kRounds        = 5;
    static constexpr uint64_t  kHistoryCycles = 30'000'000ULL;
    static constexpr uint64_t  kSliceCycles   = KeyframeSettings::kFrameCycles;
    static constexpr double    kMillion       = 1'000'000.0;



    using MachineRig = MachineHandlerRig<CallStackHandlers>;



    static double Now()
    {
        LARGE_INTEGER  counter = {};
        LARGE_INTEGER  freq    = {};



        QueryPerformanceCounter   (&counter);
        QueryPerformanceFrequency (&freq);

        return (double) counter.QuadPart * 1000.0 / (double) freq.QuadPart;
    }



    static TestMachine::Slots GetSlots (Workload workload)
    {
        return (workload == Workload::Session) ? TestMachine::Slots::AsShipped : TestMachine::Slots::Empty;
    }



    //  The workload's machine at its start: power-on, a JSR $0310 / JMP $0300
    //  loop at $0300 with RTS at $0310, or the reverse rig's session.
    static void Prepare (TestMachine & machine, Workload workload)
    {
        static constexpr Byte  kLoop[] = { 0x20, 0x10, 0x03, 0x4C, 0x00, 0x03 };
        Cpu6502              * cpu     = nullptr;
        Cpu6502Registers       r;



        if (workload == Workload::Session)
        {
            ReverseSessionRig::Prepare (machine);
            return;
        }

        machine.PowerCycle();

        if (workload != Workload::Calls)
        {
            return;
        }

        cpu = machine.GetCpu()->GetCpu6502();

        for (int i = 0; i < (int) std::size (kLoop); i++)
        {
            machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), kLoop[i]);
        }

        machine.GetMemoryBus().WriteByte (0x0310, 0x60);

        r    = cpu->GetRegisters();
        r.pc = 0x0300;
        r.sp = 0xFF;
        cpu->SetRegisters (r);
    }



    //  Records history on the rig's machine for kHistoryCycles, feeding the
    //  session's inputs where the workload has them.
    static void RecordHistory (MachineRig & rig, ReverseController & controller, Workload workload)
    {
        HRESULT  hr    = S_OK;
        size_t   slice = 0;



        Prepare (rig.machine, workload);

        hr = controller.Start (ReverseSessionRig::MakeSettings (KeyframeSettings::kDefaultFrames));
        AssertSucceeded (hr, L"Start");

        while (rig.machine.GetCpu()->GetTotalCycles() < kHistoryCycles)
        {
            if (workload == Workload::Session)
            {
                ReverseSessionRig::Feed (rig.machine, slice++);
            }

            rig.machine.RunCycles (kSliceCycles);
        }

        hr = controller.GetKeyframes().WaitForPending();
        AssertSucceeded (hr, L"WaitForPending");
    }



    //  History replayed from its oldest keyframe to its end through the
    //  replayer alone, on a machine of the workload's own, in milliseconds.
    static double TimeBareReplay (MachineRig & rig, ReverseController & controller, Workload workload)
    {
        TestMachine               scratch  ("Apple2e", GetSlots (workload));
        KeyframeStore             none;
        Replayer                  replayer (scratch, none);
        const KeyframeInfo      & oldest   = controller.GetKeyframes().GetInfo (0);
        std::vector<Byte>         state;
        std::vector<InputRecord>  inputs;
        ReplayTarget              target;
        ReplayReport              report;
        HRESULT                   hr       = S_OK;
        bool                      isCopied = false;
        double                    start    = 0.0;
        double                    elapsed  = 0.0;



        Prepare (scratch, workload);

        hr = controller.GetKeyframes().Restore (0, state);
        AssertSucceeded (hr, L"Restore");

        isCopied = rig.machine.GetInputJournal().TryCopyRecords (oldest.journalIndex, rig.machine.GetPosition(), inputs);
        Assert::IsTrue (isCopied, L"the journal holds history's inputs");

        scratch.GetInputJournal().LoadRecords (oldest.journalIndex, inputs);
        replayer.SetOverMountedMedia (workload == Workload::Session);

        hr = replayer.LoadFrom (state, oldest.position, oldest.journalIndex);
        AssertSucceeded (hr, L"LoadFrom");

        target.position = rig.machine.GetPosition();

        start = Now();
        hr    = replayer.RunTo (target, target.position, nullptr, report);

        elapsed = Now() - start;

        AssertSucceeded (hr, L"RunTo");
        Assert::AreEqual<uint64_t> (rig.machine.GetCpu()->GetTotalCycles(), scratch.GetCpu()->GetTotalCycles(), L"the bare replay reaches the end of history");

        return elapsed;
    }



    //  History replayed as the debugger rebuilds the record from it, its
    //  second machine built already, in milliseconds.
    static double TimeRebuild (MachineRig & rig, ReverseController & controller, ScratchCallReplayer & replayer, size_t & outFrames)
    {
        CallStackHistory        history (rig.machine, rig.session);
        CallStackRebuildJob     job;
        CallStackRebuildResult  result;
        bool                    hasJob  = false;
        HRESULT                 hr      = S_OK;



        history.Attach (&controller, &replayer);

        hr = history.MakeJob (false, 0, 0, rig.machine.GetPosition(), job, hasJob);
        AssertSucceeded (hr, L"MakeJob");
        Assert::IsTrue (hasJob, L"history holds the run");

        job.generation = 1;

        hr = replayer.Rebuild (job, result);
        AssertSucceeded (hr, L"Rebuild");
        Assert::AreEqual<uint64_t> (rig.machine.GetPosition(), result.position, L"the rebuild reaches the end of history");
        Assert::AreEqual<uint64_t> (rig.machine.GetCpu()->GetTotalCycles(), result.cycle, L"on the same cycle");

        history.Attach (nullptr, nullptr);

        outFrames = result.record.frames.size();
        return result.ms;
    }



    TEST_CLASS (CallStackRebuildCostTests)
    {
    public:

        TEST_METHOD (RebuildCostIsLogged)
        {
            const std::pair<Workload, const char *>  workloads[] =
            {
                { Workload::Idle,    "idle   " },
                { Workload::Calls,   "calls  " },
                { Workload::Session, "session" },
            };
            std::string  log;



            log += std::format ("call record rebuilt from history, {} M cycles each, median of {}, ms per million cycles replayed\n",
                                (double) kHistoryCycles / kMillion, kRounds);

            for (const auto & [workload, label] : workloads)
            {
                std::vector<double>  bare;
                std::vector<double>  rebuilt;
                double               cycles  = 0.0;
                double               warmup  = 0.0;
                size_t               frames  = 0;



                for (int round = 0; round < kRounds; round++)
                {
                    MachineRig           rig        ("Apple2e", GetSlots (workload));
                    ReverseController    controller (rig.machine);
                    ScratchCallReplayer  replayer;



                    RecordHistory (rig, controller, workload);

                    replayer.SetMachine (rig.machine.GetConfig(), rig.machine.GetCurrentMachineName());

                    //  The first rebuild builds the second machine; only the
                    //  one after it is timed.
                    warmup = TimeRebuild (rig, controller, replayer, frames);
                    IGNORE_RETURN_VALUE (warmup, 0.0);

                    cycles = (double) (rig.machine.GetCpu()->GetTotalCycles() - controller.GetKeyframes().GetInfo (0).cycle) / kMillion;

                    bare.push_back    (TimeBareReplay (rig, controller, workload) / cycles);
                    rebuilt.push_back (TimeRebuild    (rig, controller, replayer, frames) / cycles);
                }

                std::ranges::sort (bare);
                std::ranges::sort (rebuilt);

                log += std::format ("  {}: bare replay {:6.2f}, rebuilding the record {:6.2f} ({:+.1f}%), {} frames at the end\n",
                                    label, bare[bare.size() / 2], rebuilt[rebuilt.size() / 2],
                                    (rebuilt[rebuilt.size() / 2] / bare[bare.size() / 2] - 1.0) * 100.0, frames);
            }

            Logger::WriteMessage (log.c_str());
        }
    };
}

#endif
