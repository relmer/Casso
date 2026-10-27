#include "Pch.h"

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

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





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





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShellHistoryTests
    //
    //  The machine's history is the shell's, not the debugger's: recording,
    //  moves and the return to live work with no debugger attached, and the
    //  debugger attached follows the same history.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ShellHistoryTests)
    {
    public:

        TEST_METHOD (HistoryRecordsWithNoDebuggerAttached)
        {
            ShellRig  rig (false);



            rig.RunFrames (kFrames);

            Assert::IsTrue (rig.shell->GetHistory().IsRecording(), L"history is recorded with no debugger attached");
            Assert::IsTrue (rig.shell->GetHistory().GetReverseHost()->GetController().GetKeyframes().GetCount() > 0, L"and takes keyframes");
        }


        TEST_METHOD (AStepBackAndGoLiveWorkWithNoDebuggerAttached)
        {
            ShellRig  rig (false);
            uint64_t  live = 0;



            rig.RunFrames (kFrames);
            live = rig.shell->GetMachine().GetPosition();

            rig.Reverse (ReverseCommand::StepBack);

            Assert::IsTrue (rig.shell->GetHistory().IsBehindLive(), L"a step back moves the machine into history");
            Assert::AreEqual<uint64_t> (live - 1, rig.shell->GetMachine().GetPosition(), L"one instruction back");

            rig.shell->GetHistory().UpdateReplayCaption();
            Assert::IsFalse (rig.shell->GetHistory().GetReplayCaption().empty(), L"the caption notes the replay");

            rig.Reverse (ReverseCommand::GoLive);

            Assert::IsFalse (rig.shell->GetHistory().IsBehindLive(), L"go live returns to the live end");
            Assert::AreEqual<uint64_t> (live, rig.shell->GetMachine().GetPosition(), L"where it was");
        }


        TEST_METHOD (TheDebuggerFollowsTheShellsHistory)
        {
            ShellRig  rig (true);



            rig.RunFrames (kFrames);
            rig.Reverse (ReverseCommand::StepBack);

            Assert::IsTrue (rig.shell->GetHistory().IsBehindLive(), L"the shell's history moved");
            Assert::IsTrue (rig.shell->GetDebugger().GetSliceHooks().isBehindLive, L"and the debugger reads the same position");
        }


        TEST_METHOD (TurningHistoryOffStopsRecordingAndClearsTheCaption)
        {
            ShellRig  rig (false);



            rig.RunFrames (kFrames);
            rig.Reverse (ReverseCommand::StepBack);
            rig.shell->GetHistory().UpdateReplayCaption();

            rig.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (false, kBudgetMb));

            Assert::IsFalse (rig.shell->GetHistory().IsRecording(), L"recording stops");
            Assert::IsFalse (rig.shell->GetHistory().IsBehindLive(), L"the machine is live");
            Assert::IsTrue  (rig.shell->GetHistory().GetReplayCaption().empty(), L"the caption note is gone");
        }
    };
}
