#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/CallStackHistory.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "Debugger/DebugSessionView.h"
#include "EmuTests/FixtureRomSource.h"
#include "EmuTests/InlineWorkQueue.h"
#include "InMemoryPipeTransport.h"
#include "resource.h"
#include "Shell/CpuCommandDispatcher.h"
#include "Shell/EmulatorShell.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/MachineBuilder.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "UiTests/InMemoryFileSystem.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CallHistoryShellTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CallHistoryShell
    //
    //  The shell, with what a test drives the debugger's call history through
    //  made public.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class CallHistoryShell : public EmulatorShell
    {
    public:
        using EmulatorShell::AttachDebugger;
        using EmulatorShell::CloseDebugger;
        using EmulatorShell::GetCallReplayer;
        using EmulatorShell::IsDebuggerOpen;
        using EmulatorShell::PrepareFramebuffers;
        using EmulatorShell::ServiceDebugger;
        using EmulatorShell::SetDebugWindowShown;
        using EmulatorShell::TakeDebuggerUpdate;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShellRig
    //
    //  A shell over a //e built as the headless factory builds one and power
    //  cycled, keeping its history from the start, with three nested calls
    //  made and the innermost looping by the time the debugger opens, over a
    //  transport of the test's own. The call record's rebuilds wait on a
    //  queue the test runs, and every command reaches the shell as the CPU
    //  thread hands it one.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ShellRig
    {
    public:
        static constexpr Word      kStart    = 0x0800;
        static constexpr Word      kOuter    = 0x0810;
        static constexpr Word      kMiddle   = 0x0820;
        static constexpr Word      kInner    = 0x0830;
        static constexpr uint64_t  kOpenAt   = 5000;
        static constexpr int       kBudgetMb = 16;

        InMemoryPipeTransport               transport;
        InMemoryFileSystem                  files;
        InlineWorkQueue                     queue;
        std::unique_ptr<CallHistoryShell>   shell    = std::make_unique<CallHistoryShell>();
        DebuggerController                * debugger = nullptr;



        ShellRig()
        {
            MachineBuildServices                  services;
            MachineBuilder                        builder    (shell->GetMachine(), services);
            FixtureRomSource                      source;
            std::string                           error;
            std::unique_ptr<DebuggerController>   controller;
            HRESULT                               hr         = S_OK;



            hr = HeadlessMachineFactory::Build (shell->GetMachine(), builder, source, "Apple2e", TestMachine::Slots::Empty, TestMachine::kSeed, error);
            AssertSucceeded (hr, L"the shell's machine builds");

            shell->GetMachine().PowerCycle();
            LoadProgram (shell->GetMachine());

            shell->PrepareFramebuffers();
            shell->GetCallReplayer().SetWorkQueue (&queue);

            Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (true, kBudgetMb));

            RunTo (kOpenAt);

            controller = std::make_unique<DebuggerController> (shell->GetMachine(), ControllerRig::Paused (shell->GetCpuManager()), transport, files, nullptr, 1);

            hr = controller->Open();
            AssertSucceeded (hr, L"the debugger opens");

            debugger = controller.get();
            shell->AttachDebugger (std::move (controller));
        }



        ~ShellRig()
        {
            if (shell->IsDebuggerOpen())
            {
                shell->CloseDebugger();
            }

            shell.reset();
        }



        //  $0800: LDX #$FF / TXS / JSR outer, outer JSR middle, middle JSR
        //  inner, and inner counting at $0300 forever.
        static void LoadProgram (MachineHost & machine)
        {
            static constexpr Byte  kMain[]     = { 0xA2, 0xFF, 0x9A, 0x20, 0x10, 0x08, 0x4C, 0x06, 0x08 };  // LDX #$FF / TXS / JSR outer / JMP *
            static constexpr Byte  kOuterAt[]  = { 0x20, 0x20, 0x08, 0x60 };                                // JSR middle / RTS
            static constexpr Byte  kMiddleAt[] = { 0x20, 0x30, 0x08, 0x60 };                                // JSR inner / RTS
            static constexpr Byte  kInnerAt[]  = { 0xEE, 0x00, 0x03, 0x4C, 0x30, 0x08 };                    // INC $0300 / JMP inner



            WriteBytes (machine, kStart,  kMain,     sizeof (kMain));
            WriteBytes (machine, kOuter,  kOuterAt,  sizeof (kOuterAt));
            WriteBytes (machine, kMiddle, kMiddleAt, sizeof (kMiddleAt));
            WriteBytes (machine, kInner,  kInnerAt,  sizeof (kInnerAt));

            machine.GetCpu()->SetPC (kStart);
        }



        static void WriteBytes (MachineHost & machine, Word at, const Byte * bytes, size_t count)
        {
            for (size_t i = 0; i < count; i++)
            {
                machine.GetMemoryBus().WriteByte ((Word) (at + i), bytes[i]);
            }
        }



        //  A command as the CPU thread hands one to the shell.
        void Dispatch (WORD id, const std::string & payload)
        {
            EmulatorCommand  command;



            command.id      = id;
            command.payload = payload;

            shell->DispatchCpuCommand (command);
        }



        void RunTo (uint64_t position)
        {
            while (shell->GetMachine().GetPosition() < position)
            {
                shell->GetMachine().StepOne();
            }
        }



        //  The pass that requests the rebuild, the rebuild run, and the pass
        //  that takes it in.
        void Rebuild()
        {
            shell->ServiceDebugger();
            queue.WaitAll();
            shell->ServiceDebugger();

            Assert::IsFalse (debugger->GetCallHistory().IsRebuilding(), L"the rebuild was taken in");
        }



        CallRecord GetRecord()
        {
            DebugSessionView  view;



            debugger->GetSession().TakeView (view);
            return view.callRecord;
        }



        uint64_t GetGeneration()
        {
            return debugger->GetSession().GetCallRecordGeneration();
        }



        //  The next view the window would take, waiting at most waitMs for a
        //  build under way.
        bool TryTakeView (int waitMs)
        {
            std::shared_ptr<const DebuggerViewSnapshot>  snapshot;
            std::vector<std::string>                     lines;
            auto                                         until    = std::chrono::steady_clock::now() + std::chrono::milliseconds (waitMs);
            bool                                         isTaken  = false;



            do
            {
                isTaken = shell->TakeDebuggerUpdate (snapshot, lines) && snapshot != nullptr;

                if (!isTaken)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
                }
            }
            while (!isTaken && std::chrono::steady_clock::now() < until);

            return isTaken;
        }



        static bool HasCalls (const CallRecord & record)
        {
            auto  hasTarget = [&record] (Word target)
            {
                return std::ranges::any_of (record.frames, [target] (const CallStackFrame & frame) { return frame.target == target; });
            };



            return hasTarget (kOuter) && hasTarget (kMiddle) && hasTarget (kInner);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CallHistoryShellTests
    //
    //  The shell's part in rebuilding the debugger's call record from
    //  history: each pass of the CPU thread looks after the rebuild, a
    //  reverse command starts the record again where the machine lands unless
    //  it left the machine where it was, turning history off behind live
    //  leaves a record whose bottom is TrackingRestarted, not HistoryBegan,
    //  closing the debugger drops the rebuild under way, and the debugger's
    //  view is built again while a stopped machine waits on one.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CallHistoryShellTests)
    {
    public:

        //  How long a test waits for a view built on the pool, and how long
        //  it lets pass for the next one to be due.
        static constexpr int  kViewWaitMs     = 5000;
        static constexpr int  kNoViewWaitMs   = 200;
        static constexpr int  kBuildPassingMs = (int) DebuggerViewState::kBuildIntervalMs * 3;



        //  The debugger opened mid-run: a pass of the CPU thread requests the
        //  rebuild, and a later one takes it in, with the calls made before
        //  the debugger opened.
        TEST_METHOD (EachPassLooksAfterTheRebuild)
        {
            ShellRig  rig;



            Assert::IsFalse (ShellRig::HasCalls (rig.GetRecord()), L"opened mid-run: the calls before are not recorded");

            rig.shell->ServiceDebugger();

            Assert::IsTrue   (rig.debugger->GetCallHistory().IsRebuilding(), L"a pass requests the rebuild");
            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(),      L"on the rebuilder's worker");

            rig.queue.WaitAll();
            rig.shell->ServiceDebugger();

            Assert::IsFalse (rig.debugger->GetCallHistory().IsRebuilding(), L"the next pass takes it in");
            Assert::IsTrue  (ShellRig::HasCalls (rig.GetRecord()),          L"with the calls made before the debugger opened");
        }


        //  A reverse command that moves the machine starts the record again
        //  where it lands, and the next pass rebuilds it there.
        TEST_METHOD (AReverseCommandStartsTheRecordAgainWhereTheMachineLands)
        {
            uint64_t   landing    = ShellRig::kOpenAt + ShellRig::kOpenAt / 2;
            ShellRig   rig;
            uint64_t   generation = 0;



            rig.Rebuild();
            rig.RunTo (ShellRig::kOpenAt * 2);

            generation = rig.GetGeneration();

            rig.Dispatch (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (ReverseCommand::Seek, landing));

            Assert::AreEqual    (landing, rig.shell->GetMachine().GetPosition(), L"the seek lands at the position given");
            Assert::AreNotEqual (generation, rig.GetGeneration(),                L"and the record starts again there");
            Assert::IsFalse     (ShellRig::HasCalls (rig.GetRecord()),           L"holding no call before it");

            rig.shell->ServiceDebugger();

            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"the next pass requests its rebuild");

            rig.queue.WaitAll();
            rig.shell->ServiceDebugger();

            Assert::IsTrue (ShellRig::HasCalls (rig.GetRecord()), L"which brings back the calls");
        }


        //  A reverse command that cannot run -- history was turned off after
        //  it was posted -- leaves the machine and the record as they were.
        TEST_METHOD (AReverseCommandThatCannotRunKeepsTheRecord)
        {
            ShellRig    rig;
            uint64_t    position   = 0;
            uint64_t    generation = 0;
            CallRecord  before;



            rig.Rebuild();
            rig.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (false, ShellRig::kBudgetMb));

            position   = rig.shell->GetMachine().GetPosition();
            generation = rig.GetGeneration();
            before     = rig.GetRecord();

            rig.Dispatch (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (ReverseCommand::Seek, ShellRig::kOpenAt / 2));

            Assert::AreEqual (position,   rig.shell->GetMachine().GetPosition(), L"the machine did not move");
            Assert::AreEqual (generation, rig.GetGeneration(),                  L"and the record did not start again");
            Assert::IsTrue   (ShellRig::HasCalls (rig.GetRecord()),              L"it still holds the calls");
            Assert::AreEqual (before.frames.size(), rig.GetRecord().frames.size(), L"every one of them");
        }


        //  Turning history off while the machine is behind live makes it live
        //  first, which starts the record again where it lands, and then no
        //  rebuild can follow: the record keeps what it holds, and its bottom
        //  marks only that the calls before it are not available, not that
        //  history starts there.
        TEST_METHOD (TurningHistoryOffBehindLiveMarksTheBottomTrackingRestarted)
        {
            uint64_t                         landing = ShellRig::kOpenAt + ShellRig::kOpenAt / 2;
            uint64_t                         end     = ShellRig::kOpenAt * 2;
            ShellRig                         rig;
            std::optional<CallStackBreak>    bottom;
            std::vector<CallStackPane::Row>  rows;



            rig.Rebuild();
            rig.RunTo (end);

            rig.Dispatch (IDM_DEBUG_REVERSE, CpuCommandDispatcher::FormatReversePayload (ReverseCommand::Seek, landing));
            rig.Rebuild();

            Assert::AreEqual (landing, rig.shell->GetMachine().GetPosition(), L"behind live, where the seek landed");

            rig.Dispatch (IDM_DEBUG_REVERSE_OPTIONS, CpuCommandDispatcher::FormatReverseOptionsPayload (false, ShellRig::kBudgetMb));
            rig.shell->ServiceDebugger();

            bottom = rig.debugger->GetSession().GetCallRecordBottom();
            rows   = CallStackPane::GetRows (rig.debugger->GetSession().GetCallStack());

            Assert::AreEqual (end, rig.shell->GetMachine().GetPosition(),        L"live again, where history ended");
            Assert::IsFalse  (rig.debugger->GetCallHistory().IsRebuilding(),     L"with no rebuild");
            Assert::IsTrue   (bottom.has_value() && bottom->kind == CallBreakKind::TrackingRestarted,
                              L"the bottom marks only that the calls before it are not available");

            Assert::IsTrue (std::ranges::any_of (rows, [&bottom] (const CallStackPane::Row & row)
            {
                return row.isNote && row.routine == CallStackPane::GetUnavailableNote (bottom->pc);
            }), L"and the pane's note matches");
        }


        //  Closing the debugger drops the rebuild under way: it never runs,
        //  and no result is left behind for a debugger opened later.
        TEST_METHOD (ClosingTheDebuggerDropsTheRebuild)
        {
            ShellRig                rig;
            CallStackRebuildResult  result;
            bool                    isLeft = false;



            rig.shell->ServiceDebugger();

            Assert::AreEqual ((size_t) 1, rig.queue.GetPendingCount(), L"a rebuild was requested");

            rig.shell->CloseDebugger();
            rig.debugger = nullptr;

            Assert::IsFalse (rig.shell->IsDebuggerOpen(), L"the debugger is gone");

            rig.queue.WaitAll();

            isLeft = rig.shell->GetCallReplayer().TryTakeResult (result);

            Assert::IsFalse (isLeft, L"the rebuild was dropped, not run");
        }


        //  While the window shows a stopped machine whose call record is being
        //  rebuilt, its view is built again a frame apart, so the rebuilt
        //  record shows when it comes in; once it is in, a stopped machine
        //  builds no more.
        TEST_METHOD (TheViewIsBuiltAgainWhileTheRecordIsRebuilt)
        {
            ShellRig  rig;



            rig.shell->SetDebugWindowShown (true);

            rig.shell->ServiceDebugger();

            Assert::IsTrue (rig.debugger->GetCallHistory().IsRebuilding(), L"the rebuild is under way");
            Assert::IsTrue (rig.TryTakeView (kViewWaitMs),                L"the window's first view");

            std::this_thread::sleep_for (std::chrono::milliseconds (kBuildPassingMs));
            rig.shell->ServiceDebugger();

            Assert::IsTrue (rig.TryTakeView (kViewWaitMs), L"built again while the rebuild runs");

            rig.queue.WaitAll();
            std::this_thread::sleep_for (std::chrono::milliseconds (kBuildPassingMs));
            rig.shell->ServiceDebugger();

            Assert::IsFalse (rig.debugger->GetCallHistory().IsRebuilding(), L"the rebuild was taken in");
            Assert::IsTrue  (rig.TryTakeView (kViewWaitMs),                L"and built again with it");

            std::this_thread::sleep_for (std::chrono::milliseconds (kBuildPassingMs));
            rig.shell->ServiceDebugger();

            Assert::IsFalse (rig.TryTakeView (kNoViewWaitMs), L"then a stopped machine builds no more");

            rig.shell->SetDebugWindowShown (false);
        }
    };
}
