#include "Pch.h"

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


        TEST_METHOD (TheHistoryMenuItemsAreEnabledByTheirRules)
        {
            //  Rewind and step back: on, and past the oldest point.
            Assert::IsTrue  (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_REWIND,    true,  false, false), L"rewind, live");
            Assert::IsTrue  (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_REWIND,    true,  true,  false), L"rewind, behind live");
            Assert::IsFalse (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_REWIND,    true,  true,  true),  L"rewind, at the oldest point");
            Assert::IsFalse (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_REWIND,    false, false, false), L"rewind, history off");
            Assert::IsTrue  (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_STEP_BACK, true,  false, false), L"step back, live");
            Assert::IsFalse (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_STEP_BACK, true,  true,  true),  L"step back, at the oldest point");
            Assert::IsFalse (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_STEP_BACK, false, false, false), L"step back, history off");

            //  Go live: behind live.
            Assert::IsTrue  (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_GO_LIVE,   true,  true,  false), L"go live, behind live");
            Assert::IsFalse (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_GO_LIVE,   true,  false, false), L"go live, live");

            //  History on: always.
            Assert::IsTrue  (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_TOGGLE,    true,  false, false), L"history on, on");
            Assert::IsTrue  (ShellHistory::IsMenuItemEnabled (IDM_HISTORY_TOGGLE,    false, false, false), L"history on, off");
        }


        TEST_METHOD (CancelingTurningHistoryOffKeepsItOn)
        {
            ShellRig        rig (false);
            ShellHistory &  history = rig.shell->GetHistory();
            int             asked   = 0;
            bool            isSaved = false;



            history.SetConfirmTurnOff ([&asked] { asked++; return false; });

            isSaved = history.SaveOptions (false, kBudgetMb, HistorySettings::kDefaultFrames);

            Assert::AreEqual (1, asked, L"the confirmation is shown");
            Assert::IsFalse  (isSaved, L"the change is canceled");
            Assert::IsTrue   (history.GetSettings().isRecording, L"and history stays on");
        }


        TEST_METHOD (ConfirmingTurnsHistoryOffAndOnAsksNothing)
        {
            ShellRig        rig (false);
            ShellHistory &  history = rig.shell->GetHistory();
            int             asked   = 0;
            bool            isSaved = false;



            history.SetConfirmTurnOff ([&asked] { asked++; return true; });

            isSaved = history.SaveOptions (false, kBudgetMb, HistorySettings::kDefaultFrames);

            Assert::AreEqual (1, asked, L"the confirmation is shown");
            Assert::IsTrue   (isSaved, L"the change is made");
            Assert::IsFalse  (history.GetSettings().isRecording, L"history is off");

            isSaved = history.SaveOptions (true, kBudgetMb, HistorySettings::kDefaultFrames);

            Assert::AreEqual (1, asked, L"turning it on shows no confirmation");
            Assert::IsTrue   (isSaved && history.GetSettings().isRecording, L"history is on");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RewindLoopTests
    //
    //  The rewind key held: each frame steps the machine back a frame's cycles
    //  in place of running one, paced at the video rate; the first frame after
    //  the release pauses the machine where it is; and a rewind that lands on
    //  the oldest point pauses there.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RewindLoopTests)
    {
    public:

        TEST_METHOD (EachFrameStepsBackAFramesCycles)
        {
            ShellRig       rig (false);
            ShellHistory & history = rig.shell->GetHistory();
            uint64_t       before  = 0;
            uint64_t       after   = 0;



            rig.RunFrames (kFrames);
            before = rig.shell->GetMachine().GetCpu()->GetHistoryCycles();

            history.StartRewind();

            Assert::IsTrue (history.RunRewindFrame(), L"the frame is a rewind frame");

            after = rig.shell->GetMachine().GetCpu()->GetHistoryCycles();

            Assert::IsTrue (history.IsBehindLive(), L"the machine is behind live");
            Assert::IsTrue (after >= before - kFrameCycles && after < before - kFrameCycles + 8, L"one frame's cycles back, to an instruction boundary");
            Assert::IsTrue (rig.shell->GetCpuManager().IsPaceForced(), L"paced at the video rate");
            Assert::IsFalse (rig.shell->GetCpuManager().IsPaused(), L"and running");
        }


        TEST_METHOD (TheFrameAfterTheReleasePausesWhereTheMachineStands)
        {
            ShellRig       rig (false);
            ShellHistory & history = rig.shell->GetHistory();
            uint64_t       landed  = 0;



            rig.RunFrames (kFrames);

            history.StartRewind();
            Assert::IsTrue (history.RunRewindFrame(), L"a first frame back");
            Assert::IsTrue (history.RunRewindFrame(), L"a second");

            landed = rig.shell->GetMachine().GetPosition();
            history.StopRewind();

            Assert::IsTrue   (history.RunRewindFrame(), L"the frame after the release finishes the rewind");
            Assert::AreEqual (landed, rig.shell->GetMachine().GetPosition(), L"without stepping back again");
            Assert::IsTrue   (rig.shell->GetCpuManager().IsPaused(), L"paused");
            Assert::IsFalse  (rig.shell->GetCpuManager().IsPaceForced(), L"paced as the speed sets again");
            Assert::IsFalse  (history.RunRewindFrame(), L"and the next frame runs as usual");
        }


        TEST_METHOD (ARewindStopsPausedAtTheOldestPoint)
        {
            constexpr int  kMostFrames = 1000;

            ShellRig       rig (false);
            ShellHistory & history = rig.shell->GetHistory();
            uint64_t       oldest  = 0;
            int            frames  = 0;



            rig.RunFrames (kFrames);
            oldest = history.GetReverseHost()->GetController().GetKeyframes().GetInfo (0).position;

            history.StartRewind();

            while (frames < kMostFrames && history.RunRewindFrame() && !rig.shell->GetCpuManager().IsPaused())
            {
                frames++;
            }

            Assert::IsTrue   (frames < kMostFrames, L"the rewind ends");
            Assert::IsTrue   (rig.shell->GetCpuManager().IsPaused(), L"paused");
            Assert::AreEqual (oldest, rig.shell->GetMachine().GetPosition(), L"at the oldest point");
            Assert::IsFalse  (history.RunRewindFrame(), L"the key held down rewinds no further");
        }


        TEST_METHOD (APausedMachineRunsWhileRewinding)
        {
            ShellRig       rig (false);
            ShellHistory & history = rig.shell->GetHistory();



            rig.RunFrames (kFrames);
            rig.shell->GetCpuManager().SetPaused (true);

            history.StartRewind();

            Assert::IsFalse (rig.shell->GetCpuManager().IsPaused(), L"the machine runs");
            Assert::IsTrue  (history.RunRewindFrame(), L"and steps back");
        }


        TEST_METHOD (NoRewindWithHistoryOff)
        {
            ShellRig       rig (false);
            ShellHistory & history = rig.shell->GetHistory();



            rig.RunFrames (kFrames);
            rig.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (false, kBudgetMb));

            history.StartRewind();

            Assert::IsFalse (history.RunRewindFrame(), L"the frame runs as usual");
            Assert::IsFalse (history.IsBehindLive(), L"live");
        }
    };
}
