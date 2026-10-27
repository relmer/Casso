#pragma once

#include "Config/HistorySettings.h"
#include "Core/MachineConfig.h"
#include "Debugger/Reverse/ReverseHost.h"
#include "EmuTests/TestMachine.h"
#include "resource.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/EmulatorShell.h"
#include "Shell/MachineBuilder.h"
#include "Shell/ShellDebugger.h"
#include "Shell/Components/ShellHistory.h"
#include "HResultAssert.h"





namespace ShellHistoryTests
{
    static constexpr Word      kStart       = 0x0800;
    static constexpr uint64_t  kFrameCycles = 17'030;
    static constexpr int       kBudgetMb    = 16;
    static constexpr int       kFrames      = 30;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShellRig
    //
    //  A shell over a //e running a loop, with history on, driven as the CPU
    //  thread drives it. The debugger can be detached from history, so a
    //  test can check that history works with no debugger at all.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ShellRig
    {
    public:
        std::unique_ptr<EmulatorShell>  shell = std::make_unique<EmulatorShell>();



        explicit ShellRig (bool isDebuggerAttached)
        {
            static constexpr Byte  kLoop[] = { 0xE8, 0x4C, 0x00, 0x08 };   // INX; JMP $0800
            MachineHost          & machine = shell->GetMachine();
            MachineBuildServices   services;
            MachineBuilder         builder (machine, services);
            MachineConfig          config;
            HRESULT                hr      = S_OK;
            size_t                 i       = 0;



            TestMachine::LoadConfig ("Apple2e", config);

            //  No cards: the loop uses none, and a Disk II requires services
            //  the bare build does not supply.
            config.slots.clear();

            machine.SetPrng               (std::make_unique<Prng> (TestMachine::kSeed));
            machine.SetVideoTiming        (std::make_unique<VideoTiming>());
            machine.SetCurrentMachineName (L"Apple2e");
            machine.GetConfig() = config;

            hr = builder.Build (config);
            AssertSucceeded (hr, L"the shell's machine builds");

            machine.PowerCycle();

            for (i = 0; i < sizeof (kLoop); i++)
            {
                machine.GetMemoryBus().WriteByte (static_cast<Word> (kStart + i), kLoop[i]);
            }

            machine.GetCpu()->SetPC (kStart);

            shell->GetDebugger().PrepareFramebuffers();

            if (!isDebuggerAttached)
            {
                shell->GetHistory().SetClient (nullptr);
            }

            Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (true, kBudgetMb));
        }



        //  A command, dispatched as the CPU thread dispatches one.
        void Dispatch (WORD id, const std::string & payload)
        {
            EmulatorCommand  command;



            command.id      = id;
            command.payload = payload;

            shell->DispatchCpuCommand (command);
        }



        void RunFrames (int count)
        {
            int  i = 0;



            for (i = 0; i < count; i++)
            {
                shell->GetHistory().RecordFrame();
                shell->GetMachine().RunCycles (kFrameCycles);
            }
        }



        void Reverse (ReverseCommand command, uint64_t argument = 0)
        {
            Dispatch (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (command, argument));
        }
    };
}
