#include "Pch.h"

#include "ControllerRig.h"
#include "Debugger/DebuggerController.h"
#include "Debugger/DebugSession.h"
#include "EmuTests/FixtureRomSource.h"
#include "InMemoryPipeTransport.h"
#include "Shell/EmulatorShell.h"
#include "Shell/ShellDebugger.h"
#include "Shell/HeadlessMachineFactory.h"
#include "Shell/MachineBuilder.h"
#include "UiTests/InMemoryFileSystem.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ShellDebuggerViewStateTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ViewRig
    //
    //  A shell over a power-cycled //e with a debugger open on a stopped
    //  machine and its window shown, over a transport of the test's.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ViewRig
    {
    public:
        static constexpr int  kViewWaitMs     = 5000;
        static constexpr int  kBuildPassingMs = (int) DebuggerViewState::kBuildIntervalMs * 3;

        //  The shell's clock, which PassLater moves on.
        uint64_t                         nowMs = 1000000;

        InMemoryPipeTransport            transport;
        InMemoryFileSystem               files;
        std::unique_ptr<EmulatorShell>   shell = std::make_unique<EmulatorShell>();



        ViewRig()
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
            shell->GetDebugger().PrepareFramebuffers();
            shell->GetDebugger().SetClock ([this] () { return nowMs; });

            controller = std::make_unique<DebuggerController> (shell->GetMachine(), ControllerRig::Paused (shell->GetCpuManager()), transport, files, nullptr, 1);

            hr = controller->Open();
            AssertSucceeded (hr, L"the debugger opens");

            shell->GetDebugger().AttachDebugger (std::move (controller));
            shell->GetDebugger().SetDebugWindowShown (true);
        }



        ~ViewRig()
        {
            shell->GetDebugger().SetDebugWindowShown (false);

            if (shell->GetDebugger().IsDebuggerOpen())
            {
                shell->GetDebugger().CloseDebugger();
            }

            shell.reset();
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
                isTaken = shell->GetDebugger().TakeDebuggerUpdate (snapshot, lines) && snapshot != nullptr;

                if (!isTaken)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
                }
            }
            while (!isTaken && std::chrono::steady_clock::now() < until);

            return isTaken;
        }



        //  A pass once the next build is due.
        //  A pass of the shell's loop a few build intervals on, by its clock.
        void PassLater()
        {
            nowMs += kBuildPassingMs;
            shell->GetDebugger().ServiceDebugger();
        }


        //  Whether a pass sent a view to be built, found at once from the
        //  shell's count rather than after waiting out a timeout.
        bool PassBuilds (bool isLater)
        {
            uint64_t  before = shell->GetDebugger().GetBuildsSubmitted();



            if (isLater)
            {
                PassLater();
            }
            else
            {
                shell->GetDebugger().ServiceDebugger();
            }

            return shell->GetDebugger().GetBuildsSubmitted() != before;
        }



        //  The first view taken, then passes until a stopped machine builds
        //  no more.
        void Settle()
        {
            shell->GetDebugger().ServiceDebugger();
            Assert::IsTrue (TryTakeView (kViewWaitMs), L"the window's first view");

            //  A second view comes only while something is rebuilt.
            if (PassBuilds (true))
            {
                Assert::IsTrue (TryTakeView (kViewWaitMs), L"the view a second pass built");
            }

            Assert::IsFalse (PassBuilds (true), L"a stopped machine doesn't build again");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShellDebuggerViewStateTests
    //
    //  The window's view state setters on the shell: each one builds a stopped
    //  machine's view again, and each one changes the state under the lock a
    //  build reads it under.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ShellDebuggerViewStateTests)
    {
    public:

        using Setter = std::function<void (ShellDebugger &)>;



        static std::vector<std::pair<const wchar_t *, Setter>> GetSetters()
        {
            return
            {
                { L"SetDebugHeatColumnsShown", [] (ShellDebugger & debugger) { debugger.SetDebugHeatColumnsShown (true); } },
                { L"SetDebugHeatMapShown",     [] (ShellDebugger & debugger) { debugger.SetDebugHeatMapShown (true); } },
                { L"SetDebugSoundMuted",       [] (ShellDebugger & debugger) { debugger.SetDebugSoundMuted (0, true); } },
                { L"SetDebugBasicShown",       [] (ShellDebugger & debugger) { debugger.SetDebugBasicShown (true); } },
                { L"SetDebugHeatMapOptions",   [] (ShellDebugger & debugger) { debugger.SetDebugHeatMapOptions (HeatMapOptions().ToText()); } },
                { L"SetDebugHeatMapHover",     [] (ShellDebugger & debugger) { debugger.SetDebugHeatMapHover (Word (0x0300)); } },
            };
        }


        //  A change the window makes is shown at once, though the machine is
        //  stopped and its view was built.
        TEST_METHOD (EachSetterBuildsAStoppedMachinesViewAgain)
        {
            for (const auto & [label, setter] : GetSetters())
            {
                ViewRig  rig;



                rig.Settle();

                setter (rig.shell->GetDebugger());
                rig.PassLater();

                Assert::IsTrue (rig.TryTakeView (ViewRig::kViewWaitMs), label);
            }
        }


        //  A step un-pauses the machine until it lands, and a view built in
        //  between would have a running machine's rows, without the trace's
        //  next instructions, before the stopped view put them back: the
        //  trace pane's flash on a step (T770), and the code views drawn at
        //  the PC inside a call a step over runs. A pass while the step is
        //  under way builds nothing, a build interval later as well; one past
        //  the hold builds, so a long trace run refreshes as a run does.
        TEST_METHOD (APassWhileAStepIsUnderWayBuildsNothing)
        {
            ViewRig  rig;



            rig.Settle();

            Assert::IsTrue (rig.shell->GetDebugger().StepSession(), L"the step starts");
            Assert::IsFalse (rig.shell->GetCpuManager().IsPaused(), L"and runs");

            Assert::IsFalse (rig.PassBuilds (false), L"no view while the step is under way");

            Assert::IsFalse (rig.PassBuilds (true), L"nor a build interval later");

            rig.nowMs += DebuggerViewState::kStepHoldMs;
            rig.shell->GetDebugger().ServiceDebugger();
            Assert::IsTrue (rig.TryTakeView (ViewRig::kViewWaitMs), L"a view once the hold passes");
        }


        //  Each setter waits for the view state's lock, so a build reading
        //  the state never sees it half changed.
        TEST_METHOD (EachSetterTakesTheViewStateLock)
        {
            static constexpr int  kBlockedMs = 50;



            for (const auto & [label, setter] : GetSetters())
            {
                ViewRig             rig;
                std::atomic<bool>   isDone     = false;
                bool                wasBlocked = false;
                std::thread         thread;



                {
                    std::lock_guard<std::mutex>  held (rig.shell->GetDebugger().GetViewStateLock());



                    thread = std::thread ([&] { setter (rig.shell->GetDebugger()); isDone = true; });
                    std::this_thread::sleep_for (std::chrono::milliseconds (kBlockedMs));

                    wasBlocked = !isDone.load();
                }

                //  Joined before any assertion, which would otherwise
                //  destroy a running thread.
                thread.join();

                Assert::IsTrue (wasBlocked,    label);
                Assert::IsTrue (isDone.load(), label);
            }
        }
    };
}
