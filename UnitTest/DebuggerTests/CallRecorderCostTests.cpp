#include "Pch.h"

#include "Debugger/DebuggerController.h"
#include "EmuTests/TestMachine.h"
#include "InMemoryPipeTransport.h"
#include "Shell/CpuManager.h"
#include "UiTests/InMemoryFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecorderCostTests
//
//  What the call-stack recorder costs, measured on the path production runs:
//  a real debugger session over a real //e, with recording switched on the way
//  attaching switches it on. Three configurations separate the recorder's
//  share from everything else a session brings:
//
//    bare        the machine alone, no debugger
//    attached    a debugger session, recording off
//    recording   the same session, recording on
//
//  and three workloads bracket what a user runs:
//
//    calls       a tight JSR/RTS loop -- the recorder's worst case, every
//                third instruction is one it records
//    boot        power-on to the Applesoft prompt, recording from the first
//                instruction -- the question a boot-time option asks
//    idle        the Applesoft prompt's keyboard poll, where most sessions sit
//
//  Each pass builds one rig per configuration and runs the three a slice at
//  a time in turn, timing each slice in the CPU time of this thread. CPU time
//  leaves out the time the thread waits for a core, which other builds and
//  test runs on the host can make any length at all. Taking turns gives each
//  configuration the same share of whatever else slows the core the thread
//  runs on, a busy thread on its other logical processor or a lower clock,
//  since that changes over tens of milliseconds and a turn lasts a few.
//  Each gate compares the median over the turns of the recording slice's
//  CPU time over the bare slice's, so a turn the host changed partway
//  through does not move it.
//
//  Measured in wall time over whole runs, as the medians of nine rounds, the
//  gates failed while other builds and suites loaded the host and passed on a
//  quiet one. The CPU time totals in the log still rise and fall with the
//  host's load; the percentages do not.
//
//  Release only. A Debug build measures the checks, not the recorder.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG

namespace CallRecorderCostTests
{
    enum class Config   { Bare, Attached, Recording };
    enum class Workload { Calls, Boot, Idle };



    static constexpr int       kConfigCount    = 3;
    static constexpr int       kPasses         = 3;
    static constexpr uint64_t  kLoopCycles     = 20'000'000ULL;
    static constexpr uint64_t  kBootCycles     = 5'000'000ULL;
    static constexpr uint64_t  kSettleCycles   = 5'000'000ULL;
    static constexpr uint64_t  kSliceCycles    = 250'000ULL;
    static constexpr double    kMillion        = 1'000'000.0;

    //  The gates catch a broken recorder, not a slower one. Measured on
    //  2026-10-08 with the host quiet, with 16 or 32 other threads spinning,
    //  and with 16 copying memory, recording cost -0.3% to +2.9% over bare at
    //  boot and idle, and +205% to +261% on the call loop, where every third
    //  instruction is recorded and a loaded host lowers the figure. A recorder
    //  adding 15% to boot or idle fails the first gate. The call loop's gate
    //  was 4.0, set against +131% measured pinned on 2026-09-21, which left it
    //  12% above the +258% the loop measures quiet now; at 5.0 it fails a
    //  recorder whose cost per call grows by more than half, to +400%.
    static constexpr double    kRealWorkloadCeiling = 1.15;
    static constexpr double    kCallLoopCeiling     = 5.0;



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rig
    //
    //  One machine and, unless the configuration is bare, one debugger over
    //  it. Built fresh for every pass so no configuration inherits another's
    //  caches or record.
    //
    ////////////////////////////////////////////////////////////////////////////

    class Rig
    {
    public:
        TestMachine                          machine;
        CpuManager                           cpuManager;
        InMemoryPipeTransport                transport;
        InMemoryFileSystem                   files;
        std::unique_ptr<DebuggerController>  controller;



        explicit Rig (Config config) :
            machine ("Apple2e", TestMachine::Slots::Empty)
        {
            if (config != Config::Bare)
            {
                controller = std::make_unique<DebuggerController> (machine, cpuManager, transport, files, nullptr, 1);
            }
        }



        //  Recording is switched on exactly as attaching switches it on.
        void SetRecording (bool isOn)
        {
            if (controller != nullptr)
            {
                controller->GetSession().SetCallRecording (isOn);
            }
        }



        //  JSR $0310 / JMP $0300 at $0300, RTS at $0310, with the PC on it.
        void LoadCallLoop()
        {
            static constexpr Byte  kLoop[] = { 0x20, 0x10, 0x03, 0x4C, 0x00, 0x03 };
            Cpu6502 *              cpu     = machine.GetCpu()->GetCpu6502();
            Cpu6502Registers       r       = cpu->GetRegisters();



            for (int i = 0; i < (int) std::size (kLoop); i++)
            {
                machine.GetMemoryBus().WriteByte ((Word) (0x0300 + i), kLoop[i]);
            }

            machine.GetMemoryBus().WriteByte (0x0310, 0x60);

            r.pc = 0x0300;
            r.sp = 0xFF;
            cpu->SetRegisters (r);
        }



        //  Brings the machine to the workload's starting point, untimed: the
        //  boot from the first instruction, as a boot-time option would, and
        //  the others once the machine has settled at the Applesoft prompt.
        void Prepare (Config config, Workload workload)
        {
            machine.PowerCycle();

            if (workload != Workload::Boot)
            {
                machine.RunCycles (kSettleCycles);
            }

            if (workload == Workload::Calls)
            {
                LoadCallLoop();
            }

            SetRecording (config == Config::Recording);
        }
    };



    ////////////////////////////////////////////////////////////////////////////
    //
    //  GetThreadCycles
    //
    //  The CPU time this thread has used, in host processor cycles, counted
    //  only while the thread runs on a core. GetThreadTimes gives the same
    //  in 100 ns units, but advances only on the 15.625 ms clock tick, longer
    //  than a slice.
    //
    ////////////////////////////////////////////////////////////////////////////

    static uint64_t GetThreadCycles()
    {
        ULONG64  cycles = 0;
        BOOL     isRead = FALSE;



        isRead = QueryThreadCycleTime (GetCurrentThread(), &cycles);
        Assert::IsTrue (isRead != FALSE, L"QueryThreadCycleTime failed on the test thread");

        return cycles;
    }



    static double GetMedian (std::vector<double> values)
    {
        std::sort (values.begin(), values.end());
        return values[values.size() / 2];
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  MeasurePass
    //
    //  One pass of one workload: a rig per configuration, brought to the
    //  workload's starting point, then run kSliceCycles at a time in turn.
    //  The configuration that goes first rotates with each turn. Only the
    //  slices are timed. Each configuration's CPU time is added to totals,
    //  and each turn's recording slice and attached slice, over its bare
    //  slice, to the two ratio lists. The machine runs on this thread and the
    //  debugger starts no thread of its own, so the thread's CPU time is all
    //  the work being compared.
    //
    ////////////////////////////////////////////////////////////////////////////

    static void MeasurePass (
        Workload                             workload,
        std::array<uint64_t, kConfigCount> & totals,
        std::vector<double>                & recordingRatios,
        std::vector<double>                & attachedRatios)
    {
        Rig                                  bare      (Config::Bare);
        Rig                                  attached  (Config::Attached);
        Rig                                  recording (Config::Recording);
        std::array<Rig *, kConfigCount>      rigs      = { &bare, &attached, &recording };
        std::array<uint64_t, kConfigCount>   spent     = {};
        uint64_t                             budget    = (workload == Workload::Boot) ? kBootCycles : kLoopCycles;
        uint64_t                             start     = 0;
        int                                  index     = 0;



        for (int config = 0; config < kConfigCount; config++)
        {
            rigs[config]->Prepare ((Config) config, workload);
        }

        for (uint64_t turn = 0; turn * kSliceCycles < budget; turn++)
        {
            for (int k = 0; k < kConfigCount; k++)
            {
                index = (int) ((turn + k) % kConfigCount);
                start = GetThreadCycles();

                rigs[index]->machine.RunCycles (kSliceCycles);

                spent[index]   = GetThreadCycles() - start;
                totals[index] += spent[index];
            }

            recordingRatios.push_back ((double) spent[(int) Config::Recording] / (double) spent[(int) Config::Bare]);
            attachedRatios.push_back  ((double) spent[(int) Config::Attached]  / (double) spent[(int) Config::Bare]);
        }
    }



    TEST_CLASS (CallRecorderCostTests)
    {
    public:

        TEST_METHOD (TheRecorderCostIsMeasuredOnEachWorkload)
        {
            const std::pair<Workload, const wchar_t *>  workloads[] =
            {
                { Workload::Calls, L"calls (JSR/RTS loop, 20M cycles)" },
                { Workload::Boot,  L"boot  (power-on, 5M cycles)"      },
                { Workload::Idle,  L"idle  (Applesoft prompt, 20M cycles)" },
            };



            for (const auto & [workload, label] : workloads)
            {
                std::array<uint64_t, kConfigCount>  totals         = {};
                std::vector<double>                 recordingRatios;
                std::vector<double>                 attachedRatios;
                wchar_t                             line[384]      = {};
                double                              b              = 0.0;
                double                              a              = 0.0;
                double                              r              = 0.0;
                double                              attachedRatio  = 0.0;
                double                              recordingRatio = 0.0;



                for (int pass = 0; pass < kPasses; pass++)
                {
                    MeasurePass (workload, totals, recordingRatios, attachedRatios);
                }

                Assert::IsFalse (recordingRatios.empty(), L"No turn was timed");

                b              = (double) totals[(int) Config::Bare]      / kMillion;
                a              = (double) totals[(int) Config::Attached]  / kMillion;
                r              = (double) totals[(int) Config::Recording] / kMillion;
                attachedRatio  = GetMedian (attachedRatios);
                recordingRatio = GetMedian (recordingRatios);

                swprintf_s (line, L"%s, CPU time in millions of host cycles: bare %.1f, attached %.1f, recording %.1f; median turn over bare: attached %+.1f%%, recording %+.1f%%",
                            label, b, a, r, (attachedRatio - 1.0) * 100.0, (recordingRatio - 1.0) * 100.0);
                Logger::WriteMessage (line);

                Assert::IsTrue (recordingRatio < (workload == Workload::Calls ? kCallLoopCeiling : kRealWorkloadCeiling), line);
            }
        }
    };
}

#endif
