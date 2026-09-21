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
//  Configurations alternate within each round and each figure is the median
//  of the rounds, so drift in the host lands on all three alike. Run it pinned
//  to one core: unpinned A/B figures on this machine swing more than the
//  effect being measured.
//
//  Release only. A Debug build measures the checks, not the recorder.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG

namespace CallRecorderCostTests
{
    enum class Config   { Bare, Attached, Recording };
    enum class Workload { Calls, Boot, Idle };



    static constexpr int       kRounds         = 9;
    static constexpr uint64_t  kLoopCycles     = 20'000'000ULL;
    static constexpr uint64_t  kBootCycles     = 5'000'000ULL;
    static constexpr uint64_t  kSettleCycles   = 5'000'000ULL;

    //  Gates that catch a broken recorder without flaking on a shared, unpinned
    //  machine. Measured pinned on 2026-09-21: +1.3% at boot, +1.6% at idle,
    //  +131% on the call loop, where every third instruction is recorded.
    static constexpr double    kRealWorkloadCeiling = 1.15;
    static constexpr double    kCallLoopCeiling     = 4.0;



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rig
    //
    //  One machine and, unless the configuration is bare, one debugger over
    //  it. Built fresh for every sample so no configuration inherits another's
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
    };



    static double Now()
    {
        LARGE_INTEGER  counter = {};
        LARGE_INTEGER  freq    = {};



        QueryPerformanceCounter   (&counter);
        QueryPerformanceFrequency (&freq);

        return (double) counter.QuadPart * 1000.0 / (double) freq.QuadPart;
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Sample
    //
    //  One timed run of one workload under one configuration, in milliseconds.
    //  Only the workload itself is timed; building the rig and reaching the
    //  workload's starting point are not.
    //
    ////////////////////////////////////////////////////////////////////////////

    static double Sample (Config config, Workload workload)
    {
        Rig     rig (config);
        double  start = 0.0;
        double  end   = 0.0;



        rig.machine.PowerCycle();

        switch (workload)
        {
        case Workload::Boot:
            //  From the first instruction, as a boot-time option would.
            rig.SetRecording (config == Config::Recording);
            start = Now();
            rig.machine.RunCycles (kBootCycles);
            end   = Now();
            break;

        case Workload::Idle:
            rig.machine.RunCycles (kSettleCycles);
            rig.SetRecording (config == Config::Recording);
            start = Now();
            rig.machine.RunCycles (kLoopCycles);
            end   = Now();
            break;

        case Workload::Calls:
            rig.machine.RunCycles (kSettleCycles);
            rig.LoadCallLoop();
            rig.SetRecording (config == Config::Recording);
            start = Now();
            rig.machine.RunCycles (kLoopCycles);
            end   = Now();
            break;
        }

        return end - start;
    }



    static double Median (std::vector<double> values)
    {
        std::sort (values.begin(), values.end());
        return values[values.size() / 2];
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
                std::vector<double>  bare;
                std::vector<double>  attached;
                std::vector<double>  recording;
                wchar_t              line[256] = {};
                double               b         = 0.0;
                double               a         = 0.0;
                double               r         = 0.0;



                for (int round = 0; round < kRounds; round++)
                {
                    bare.push_back      (Sample (Config::Bare,      workload));
                    attached.push_back  (Sample (Config::Attached,  workload));
                    recording.push_back (Sample (Config::Recording, workload));
                }

                b = Median (bare);
                a = Median (attached);
                r = Median (recording);

                swprintf_s (line, L"%s: bare %.2f ms, attached %.2f ms (%+.1f%%), recording %.2f ms (%+.1f%% over attached, %+.1f%% over bare)",
                            label, b, a, (a / b - 1.0) * 100.0, r, (r / a - 1.0) * 100.0, (r / b - 1.0) * 100.0);
                Logger::WriteMessage (line);

                Assert::IsTrue (r < b * (workload == Workload::Calls ? kCallLoopCeiling : kRealWorkloadCeiling), line);
            }
        }
    };
}

#endif
