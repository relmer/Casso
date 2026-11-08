#include "Pch.h"

#include "DebuggerWindowRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindowRunKeysTests
//
//  The run, pause, step, breakpoint and run-to-cursor keys of every key scheme,
//  pressed at the debugger window over a real machine, through to what the
//  machine then does.
//
//  A KEY IS PRESSED THE WAY THE WINDOW RECEIVES ONE: to OnKey, then to the key
//  map when OnKey leaves it, as DxuiWindow::DispatchKey hands it on. Its line
//  runs as the shell runs a window line, and the machine runs in slices as the
//  shell's CPU thread runs them, so a run ends only where the run driver ends
//  it. Nothing is installed or forced by the test: a stop the machine makes is
//  a stop the product made.
//
//  Each dialect is chosen from the Dialect drop-down, as a user chooses one,
//  and takes effect from the snapshot after it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static constexpr Word  s_kCall    = WindowRig::kCall;
    static constexpr Word  s_kLoop    = WindowRig::kLoop;
    static constexpr Word  s_kJump    = WindowRig::kJump;
    static constexpr Word  s_kRoutine = WindowRig::kRoutine;
    static constexpr Word  s_kScreen  = WindowRig::kScreen;

    static constexpr const wchar_t *  s_kpszAppleWin     = L"AppleWin";
    static constexpr const wchar_t *  s_kpszMonitor      = L"Monitor";
    static constexpr const wchar_t *  s_kpszGSSquared    = L"GSSquared";
    static constexpr const wchar_t *  s_kpszWinDbg       = L"WinDbg";
    static constexpr const wchar_t *  s_kpszCasso        = L"Casso";
    static constexpr const char    *  s_kpszSchemeApple  = "AppleWin";
    static constexpr const char    *  s_kpszSchemeGs     = "GSSquared";





    ////////////////////////////////////////////////////////////////////////////
    //
    //  RunKeyTests
    //
    //  F5 in the Visual Studio scheme, Enter in the AppleWin and GSSquared
    //  schemes: the machine runs until something stops it.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RunKeyTests)
    {
    public:

        TEST_METHOD (VisualStudio_F5_RunsTheMachineUntilSomethingStopsIt)
        {
            WindowRig  rig;
            uint64_t   started = rig.GetCycles();



            Assert::IsTrue (rig.window.Press (VK_F5), L"the key is taken");

            WindowRig::AssertLines ({ "G" }, rig.TakeSent(), L"the run line");
            Assert::IsTrue  (rig.host.replies.back().status == CommandStatus::Ok);
            Assert::IsTrue  (rig.host.replies.back().text.empty(), L"a run replies when it stops, not when it starts");
            Assert::IsFalse (rig.IsPaused(), L"the machine is let go");

            Assert::AreEqual (8, rig.RunSlices (8), L"nothing stops it");
            rig.window.Frame();

            Assert::IsTrue  (rig.GetCycles() - started >= 8 * 1023, L"it ran every slice");
            Assert::IsTrue  (rig.GetPc() == s_kLoop || rig.GetPc() == s_kJump, L"round the loop");
            Assert::AreEqual ((Byte) 0x41, rig.Peek (s_kScreen), L"after running the call on its way");
            Assert::IsFalse (rig.window.GetSnapshot()->isPaused, L"the window shows it running");
            Assert::IsTrue  (rig.GetSession().GetRunState() == RunState::DebugRun);
        }


        TEST_METHOD (F5_RunsToABreakpointAndStopsThere)
        {
            WindowRig  rig;



            rig.Submit (L"BP 0304");
            (void) rig.TakeSent();

            rig.PressAndSettle (VK_F5);

            WindowRig::AssertLines ({ "G" }, rig.TakeSent(), L"the run line");
            Assert::AreEqual (s_kJump, rig.GetPc(),          L"stopped on the breakpoint");
            Assert::AreEqual ((Byte) 1, rig.GetRegisters().x, L"after the call and one INX");
            Assert::IsTrue   (rig.IsPaused());
            Assert::IsTrue   (rig.window.GetSnapshot()->isPaused);
            Assert::IsTrue   (rig.window.GetSnapshot()->codeViews[0][(size_t) rig.GetCodeRow (s_kJump)].isCurrent, L"the PC's line is marked");
        }


        //  A second run resumes where the first stopped: the call at $0300 is
        //  not run again, so $0400, cleared between the runs, stays clear.
        TEST_METHOD (F5_AfterAPause_ResumesWhereTheMachineStopped)
        {
            WindowRig  rig;
            Word       stoppedAt = 0;
            uint64_t   cycles    = 0;



            rig.window.Press (VK_F5);
            (void) rig.RunSlices (3);
            rig.window.Press (VK_F5, false, false, true);
            rig.Settle();

            stoppedAt = rig.GetPc();
            cycles    = rig.GetCycles();
            rig.Poke (s_kScreen, 0x00);

            rig.window.Press (VK_F5);
            Assert::AreEqual (4, rig.RunSlices (4), L"running again");

            Assert::IsTrue   (stoppedAt == s_kLoop || stoppedAt == s_kJump);
            Assert::IsTrue   (rig.GetCycles() - cycles >= 4 * 1023, L"it ran on");
            Assert::AreEqual ((Byte) 0x00, rig.Peek (s_kScreen), L"from the loop, not from the call");
            Assert::AreEqual ((Byte) 0xFF, rig.GetRegisters().sp, L"no call left anything on the stack");
        }


        //  AppleWin and GSSquared run on Enter, from a command box with nothing
        //  in it: the empty line is not submitted and joins no history.
        TEST_METHOD (AppleWinAndGSSquared_EnterInAnEmptyCommandBox_Runs)
        {
            for (const char * scheme : { s_kpszSchemeApple, s_kpszSchemeGs })
            {
                WindowRig  rig (scheme);



                rig.window.Stroke (VK_RETURN, L'\r');

                WindowRig::AssertLines ({ "G" }, rig.TakeSent(), TextEncoding::NarrowToWide (scheme).c_str());
                Assert::IsFalse  (rig.IsPaused(), L"running");
                Assert::AreEqual (4, rig.RunSlices (4));
                Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty(), L"nothing was typed");

                rig.window.Press (VK_UP);
                Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty(), L"nothing joined the history");
            }
        }


        //  With a line typed, Enter runs the line rather than the machine.
        TEST_METHOD (AppleWinAndGSSquared_EnterInATypedCommandBox_RunsTheLineInstead)
        {
            for (const char * scheme : { s_kpszSchemeApple, s_kpszSchemeGs })
            {
                WindowRig  rig (scheme);



                rig.window.TypeText (L"R A 42");
                rig.window.Stroke (VK_RETURN, L'\r');

                WindowRig::AssertLines ({ "R A 42" }, rig.TakeSent(), TextEncoding::NarrowToWide (scheme).c_str());
                Assert::AreEqual ((Byte) 0x42, rig.GetRegisters().a);
                Assert::IsTrue   (rig.IsPaused(), L"the machine did not run");
                Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty(), L"the box is cleared");
            }
        }


        TEST_METHOD (AppleWin_EnterWithACodeViewFocused_Runs)
        {
            WindowRig  rig (s_kpszSchemeApple);



            rig.SelectCode (s_kJump);
            rig.window.Stroke (VK_RETURN, L'\r');

            WindowRig::AssertLines ({ "G" }, rig.TakeSent(), L"a list does not take Enter");
            Assert::IsFalse (rig.IsPaused());
        }


        //  Visual Studio binds nothing to Enter, so an empty command box
        //  submits its empty line, which is run and answered like any other.
        TEST_METHOD (VisualStudio_EnterInAnEmptyCommandBox_SubmitsTheEmptyLine)
        {
            WindowRig  rig;



            rig.window.Stroke (VK_RETURN, L'\r');
            rig.window.Frame();

            WindowRig::AssertLines ({ "" }, rig.TakeSent(), L"the empty line");
            Assert::IsTrue (rig.IsPaused(), L"nothing ran");

            rig.window.Press (VK_UP);
            Assert::IsTrue (rig.window.GetCommandBox()->GetText().empty(), L"an empty line joins no history");
        }


        //  F5 sends each dialect's own word for a run, which that dialect
        //  runs.
        TEST_METHOD (F5_InEachDialect_SendsThatDialectsRunAndRuns)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kRuns[] =
            {
                { s_kpszAppleWin,  "G"  },
                { s_kpszMonitor,   "/G" },
                { s_kpszGSSquared, "g"  },
                { s_kpszWinDbg,    "g"  },
                { s_kpszCasso,     "G"  },
            };



            for (const auto & [dialect, line] : kRuns)
            {
                WindowRig  rig;



                rig.ChooseDialect (dialect);
                (void) rig.TakeSent();

                rig.window.Press (VK_F5);

                WindowRig::AssertLines ({ line }, rig.TakeSent(), dialect);
                Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok, dialect);
                Assert::AreEqual (4, rig.RunSlices (4), dialect);
                Assert::AreEqual ((Byte) 0x41, rig.Peek (s_kScreen), dialect);
            }
        }


        //  A run key while a run is on is refused by the session, and the run
        //  goes on: the key does not look at whether the bar has Run enabled.
        TEST_METHOD (F5_DuringARun_IsRefusedAndTheRunGoesOn)
        {
            WindowRig  rig;



            rig.window.Press (VK_F5);
            (void) rig.RunSlices (2);
            rig.window.Frame();
            (void) rig.TakeSent();
            (void) rig.TakeConsole();

            Assert::IsFalse (rig.IsEntryEnabled (DebuggerCommands::kRun), L"the bar has Run off while running");
            Assert::IsTrue  (rig.window.Press (VK_F5), L"the key is still taken");
            rig.window.Frame();

            WindowRig::AssertLines ({ "G" }, rig.TakeSent(), L"sent all the same");
            Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Error);
            WindowRig::AssertLines ({ ">G", "Error: already running", "       A run is in progress. Use PAUSE to stop it." },
                                    rig.TakeConsole(), L"the refusal is in the console");
            Assert::AreEqual (4, rig.RunSlices (4), L"the first run goes on");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  PauseKeyTests
    //
    //  Shift+F5 in every scheme: no command line, but the host's pause, which
    //  ends a run at the next slice or stops a free-running machine.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PauseKeyTests)
    {
    public:

        TEST_METHOD (ShiftF5_EndsARunAtTheNextSlice_InEveryScheme)
        {
            for (const char * scheme : { "", s_kpszSchemeApple, s_kpszSchemeGs })
            {
                WindowRig  rig (scheme);
                uint64_t   stopped = 0;



                rig.ClickEntry (DebuggerCommands::kRun);
                (void) rig.RunSlices (2);
                rig.window.Frame();
                (void) rig.TakeSent();
                (void) rig.TakeConsole();

                Assert::IsTrue   (rig.window.Press (VK_F5, false, false, true), L"the key is taken");
                Assert::AreEqual (1, rig.host.pauses, L"the host's pause");
                Assert::IsTrue   (rig.TakeSent().empty(), L"a pause is not a command line");

                Assert::AreEqual (1, rig.RunSlices (8), L"one more slice, and the run is over");
                rig.window.Frame();

                stopped = rig.GetCycles();

                Assert::IsTrue   (rig.IsPaused(), L"the machine is stopped");
                Assert::IsTrue   (rig.GetSession().GetRunState() == RunState::Paused);
                Assert::IsTrue   (rig.window.GetSnapshot()->isPaused, L"and the window shows it stopped");
                Assert::IsTrue   (rig.TakeConsole().empty(), L"nothing is echoed");
                Assert::AreEqual (0, rig.RunSlices (8), L"it stays stopped");
                Assert::AreEqual (stopped, rig.GetCycles());
            }
        }


        //  With no debugger run, the emulator runs freely, and Shift+F5 stops
        //  the machine itself, at once.
        TEST_METHOD (ShiftF5_OnAFreeRunningMachine_StopsIt)
        {
            WindowRig  rig;
            uint64_t   stopped = 0;



            rig.ResumeEmulator();
            Assert::AreEqual (3, rig.RunSlices (3), L"running freely");

            rig.window.Press (VK_F5, false, false, true);
            stopped = rig.GetCycles();

            Assert::IsTrue   (rig.IsPaused(), L"stopped at once");
            Assert::IsTrue   (rig.GetSession().GetRunState() == RunState::Paused);
            Assert::AreEqual (0, rig.RunSlices (4));
            Assert::AreEqual (stopped, rig.GetCycles());
        }


        //  Pause on a stopped machine asks again and changes nothing.
        TEST_METHOD (ShiftF5_WhilePaused_ChangesNothing)
        {
            WindowRig  rig;
            uint64_t   cycles = rig.GetCycles();



            Assert::IsTrue   (rig.window.Press (VK_F5, false, false, true));
            rig.window.Frame();

            Assert::AreEqual (1, rig.host.pauses, L"the host is asked");
            Assert::IsTrue   (rig.TakeSent().empty());
            Assert::IsTrue   (rig.IsPaused());
            Assert::AreEqual (s_kCall, rig.GetPc());
            Assert::AreEqual (cycles, rig.GetCycles());
        }


        //  A function key reaches the scheme from a box with text in it.
        TEST_METHOD (ShiftF5_FromATypedCommandBox_StillPauses)
        {
            WindowRig  rig;



            rig.ClickEntry (DebuggerCommands::kRun);
            (void) rig.RunSlices (1);

            rig.window.Click (rig.window.GetCommandBox());
            rig.window.TypeText (L"BPL");
            rig.window.Press (VK_F5, false, false, true);

            Assert::AreEqual (1, rig.host.pauses);
            Assert::AreEqual (1, rig.RunSlices (4));
            Assert::IsTrue   (rig.IsPaused());
            Assert::AreEqual (std::wstring (L"BPL"), rig.window.GetCommandBox()->GetText(), L"the box keeps its text");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  StepIntoKeyTests
    //
    //  F11, and Space in the AppleWin and GSSquared schemes: one instruction,
    //  into the call.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StepIntoKeyTests)
    {
    public:

        static void AssertSteppedIntoTheCall (WindowRig & rig, const wchar_t * message)
        {
            Assert::AreEqual (s_kRoutine,     rig.GetPc(),            message);
            Assert::AreEqual ((Byte) 0xFD,  rig.GetRegisters().sp,  message);
            Assert::AreEqual ((Byte) 0x03,  rig.Peek (0x01FF),      L"the return address, high byte");
            Assert::AreEqual ((Byte) 0x02,  rig.Peek (0x01FE),      L"and low byte");
            Assert::AreEqual ((Byte) 0x00,  rig.Peek (s_kScreen),     L"nothing of the routine ran");
            Assert::IsTrue   (rig.IsPaused());
            Assert::IsTrue   (rig.window.GetSnapshot()->codeViews[0][(size_t) rig.GetCodeRow (s_kRoutine)].isCurrent, L"the window shows the new PC");
        }


        TEST_METHOD (VisualStudio_F11_StepsIntoTheCall)
        {
            WindowRig  rig;



            rig.PressAndSettle (VK_F11);

            WindowRig::AssertLines ({ "T" }, rig.TakeSent(), L"the step line");
            AssertSteppedIntoTheCall (rig, L"F11");
        }


        //  Space steps from an empty command box, which it leaves empty.
        TEST_METHOD (AppleWinAndGSSquared_SpaceInAnEmptyCommandBox_StepsIntoTheCall)
        {
            for (const char * scheme : { s_kpszSchemeApple, s_kpszSchemeGs })
            {
                WindowRig  rig (scheme);



                rig.window.Stroke (VK_SPACE, L' ');
                rig.Settle();

                WindowRig::AssertLines ({ "T" }, rig.TakeSent(), TextEncoding::NarrowToWide (scheme).c_str());
                AssertSteppedIntoTheCall (rig, TextEncoding::NarrowToWide (scheme).c_str());
                Assert::IsTrue (rig.window.GetCommandBox()->GetText().empty(), L"the space is not typed");
            }
        }


        TEST_METHOD (F11_InEachDialect_SendsThatDialectsStepAndSteps)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kSteps[] =
            {
                { s_kpszAppleWin,  "T"  },
                { s_kpszMonitor,   "/T" },
                { s_kpszGSSquared, "s"  },
                { s_kpszWinDbg,    "t"  },
                { s_kpszCasso,     "T"  },
            };



            for (const auto & [dialect, line] : kSteps)
            {
                WindowRig  rig;



                rig.ChooseDialect (dialect);
                (void) rig.TakeSent();

                rig.PressAndSettle (VK_F11);

                WindowRig::AssertLines ({ line }, rig.TakeSent(), dialect);
                AssertSteppedIntoTheCall (rig, dialect);
            }
        }


        //  Another step key during a step's run is refused by the session.
        TEST_METHOD (F11_BeforeTheLastStepRan_IsRefused)
        {
            WindowRig  rig;



            rig.window.Press (VK_F11);
            rig.window.Press (VK_F11);
            rig.Settle();

            WindowRig::AssertLines ({ "T", "T" }, rig.TakeSent(), L"both sent");
            Assert::IsTrue (rig.host.replies.back().status == CommandStatus::Error, L"the second refused");
            AssertSteppedIntoTheCall (rig, L"one step, not two");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  StepOverKeyTests
    //
    //  F10, AppleWin's Ctrl+Space and GSSquared's O: the call runs to its
    //  return, and the step stops after it.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StepOverKeyTests)
    {
    public:

        static void AssertSteppedOverTheCall (WindowRig & rig, const wchar_t * message)
        {
            Assert::AreEqual (s_kLoop,        rig.GetPc(),            message);
            Assert::AreEqual ((Byte) 0x41,  rig.GetRegisters().a,   L"the routine ran");
            Assert::AreEqual ((Byte) 0x41,  rig.Peek (s_kScreen),     L"all of it");
            Assert::AreEqual ((Byte) 0xFF,  rig.GetRegisters().sp,  L"and returned");
            Assert::AreEqual ((Byte) 0x00,  rig.GetRegisters().x,   L"and nothing after it ran");
            Assert::IsTrue   (rig.IsPaused());
        }


        TEST_METHOD (VisualStudio_F10_StepsOverTheCall)
        {
            WindowRig  rig;



            rig.PressAndSettle (VK_F10);

            WindowRig::AssertLines ({ "P" }, rig.TakeSent(), L"the step-over line");
            AssertSteppedOverTheCall (rig, L"F10");
        }


        //  A Ctrl chord reaches the scheme from a box whatever it holds, and
        //  the space it types is not typed.
        TEST_METHOD (AppleWin_CtrlSpace_StepsOver_EvenFromATypedBox)
        {
            WindowRig  rig (s_kpszSchemeApple);



            rig.window.TypeText (L"BP");
            rig.window.Stroke (VK_SPACE, L' ', true);
            rig.Settle();

            WindowRig::AssertLines ({ "P" }, rig.TakeSent(), L"Ctrl+Space");
            AssertSteppedOverTheCall (rig, L"Ctrl+Space");
            Assert::AreEqual (std::wstring (L"BP"), rig.window.GetCommandBox()->GetText(), L"no space typed");
        }


        //  GSSquared's O is a letter, which a box keeps, so it steps from
        //  anywhere else: here, a code view.
        TEST_METHOD (GSSquared_O_StepsOver_FromACodeView)
        {
            WindowRig  rig (s_kpszSchemeGs);



            rig.SelectCode (s_kCall);
            rig.window.Stroke ('O', L'o');
            rig.Settle();

            WindowRig::AssertLines ({ "P" }, rig.TakeSent(), L"O");
            AssertSteppedOverTheCall (rig, L"O");
        }


        //  Only the O key alone: with Shift or Ctrl it is not the scheme's.
        TEST_METHOD (GSSquared_ShiftOAndCtrlO_DoNothing)
        {
            WindowRig  rig (s_kpszSchemeGs);



            rig.SelectCode (s_kCall);
            Assert::IsFalse (rig.window.Press ('O', false, false, true), L"Shift+O");
            Assert::IsFalse (rig.window.Press ('O', true),               L"Ctrl+O");
            rig.Settle();

            Assert::IsTrue   (rig.TakeSent().empty());
            Assert::AreEqual (s_kCall, rig.GetPc());
        }


        TEST_METHOD (F10_InEachDialect_SendsThatDialectsStepOverAndStepsOver)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kSteps[] =
            {
                { s_kpszAppleWin,  "P"  },
                { s_kpszMonitor,   "/P" },
                { s_kpszGSSquared, "o"  },
                { s_kpszWinDbg,    "p"  },
                { s_kpszCasso,     "P"  },
            };



            for (const auto & [dialect, line] : kSteps)
            {
                WindowRig  rig;



                rig.ChooseDialect (dialect);
                (void) rig.TakeSent();

                //  From the code view: an empty GSSquared command line takes
                //  F10 as its own step, which the console-key tests cover.
                rig.SelectCode (s_kCall);
                rig.PressAndSettle (VK_F10);

                WindowRig::AssertLines ({ line }, rig.TakeSent(), dialect);
                AssertSteppedOverTheCall (rig, dialect);
            }
        }


        //  Anything but a call is one instruction.
        TEST_METHOD (F10_OnAnythingButACall_StepsOneInstruction)
        {
            WindowRig  rig;



            rig.PressAndSettle (VK_F10);
            rig.PressAndSettle (VK_F10);

            Assert::AreEqual (s_kJump,       rig.GetPc(), L"past the INX");
            Assert::AreEqual ((Byte) 0x01, rig.GetRegisters().x);
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  StepOutKeyTests
    //
    //  Shift+F11, AppleWin's Shift+Space and GSSquared's R: the routine runs
    //  to its return.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (StepOutKeyTests)
    {
    public:

        //  Into the routine first, with F11, from wherever the keys are.
        static void StepIntoTheRoutine (WindowRig & rig)
        {
            rig.ClickEntry (DebuggerCommands::kStepInto);
            rig.Settle();
            Assert::AreEqual (s_kRoutine, rig.GetPc(), L"in the routine");
            (void) rig.TakeSent();
        }


        static void AssertSteppedOut (WindowRig & rig, const wchar_t * message)
        {
            Assert::AreEqual (s_kLoop,        rig.GetPc(),            message);
            Assert::AreEqual ((Byte) 0x41,  rig.Peek (s_kScreen),     L"the rest of the routine ran");
            Assert::AreEqual ((Byte) 0xFF,  rig.GetRegisters().sp,  L"and returned");
            Assert::AreEqual ((Byte) 0x00,  rig.GetRegisters().x,   L"and nothing after it ran");
            Assert::IsTrue   (rig.IsPaused());
        }


        TEST_METHOD (VisualStudio_ShiftF11_StepsOutOfTheRoutine)
        {
            WindowRig  rig;



            StepIntoTheRoutine (rig);
            rig.PressAndSettle (VK_F11, false, false, true);

            WindowRig::AssertLines ({ "RTS" }, rig.TakeSent(), L"the step-out line");
            AssertSteppedOut (rig, L"Shift+F11");
        }


        //  From an empty box Shift+Space steps out, and its space is not typed.
        TEST_METHOD (AppleWin_ShiftSpaceInAnEmptyBox_StepsOut)
        {
            WindowRig  rig (s_kpszSchemeApple);



            StepIntoTheRoutine (rig);
            rig.window.Click (rig.window.GetCommandBox());
            rig.window.Stroke (VK_SPACE, L' ', false, false, true);
            rig.Settle();

            WindowRig::AssertLines ({ "RTS" }, rig.TakeSent(), L"Shift+Space");
            AssertSteppedOut (rig, L"Shift+Space");
            Assert::IsTrue (rig.window.GetCommandBox()->GetText().empty(), L"no space typed");
        }


        TEST_METHOD (GSSquared_R_StepsOut_FromACodeView)
        {
            WindowRig  rig (s_kpszSchemeGs);



            StepIntoTheRoutine (rig);
            rig.SelectCode (s_kRoutine);
            rig.window.Stroke ('R', L'r');
            rig.Settle();

            WindowRig::AssertLines ({ "RTS" }, rig.TakeSent(), L"R");
            AssertSteppedOut (rig, L"R");
        }


        TEST_METHOD (ShiftF11_InEachDialect_SendsThatDialectsStepOutAndStepsOut)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kSteps[] =
            {
                { s_kpszAppleWin,  "RTS"  },
                { s_kpszMonitor,   "/RTS" },
                { s_kpszGSSquared, "r"    },
                { s_kpszWinDbg,    "gu"   },
                { s_kpszCasso,     "RTS"  },
            };



            for (const auto & [dialect, line] : kSteps)
            {
                WindowRig  rig;



                rig.ChooseDialect (dialect);
                StepIntoTheRoutine (rig);

                rig.PressAndSettle (VK_F11, false, false, true);

                WindowRig::AssertLines ({ line }, rig.TakeSent(), dialect);
                AssertSteppedOut (rig, dialect);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  ToggleBreakpointKeyTests
    //
    //  F9, in every scheme: a breakpoint set where there is none, and the one
    //  there cleared, on the selected line of the disassembly view last used
    //  or else on the PC's line.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ToggleBreakpointKeyTests)
    {
    public:

        TEST_METHOD (F9_WithNothingSelected_SetsAndClearsAtThePc)
        {
            for (const char * scheme : { "", s_kpszSchemeApple, s_kpszSchemeGs })
            {
                WindowRig  rig (scheme);



                Assert::IsTrue (rig.window.Press (VK_F9));
                rig.window.Frame();

                WindowRig::AssertLines ({ "BP 0300" }, rig.TakeSent(), TextEncoding::NarrowToWide (scheme).c_str());
                WindowRig::AssertLines ({ "Breakpoint #0 set at $0300" }, rig.host.replies.back().text, L"the reply");
                Assert::IsTrue (rig.GetBreakpoints() == std::vector<std::pair<Word, bool>> { { s_kCall, true } });
                Assert::IsTrue (rig.window.GetSnapshot()->codeViews[0][(size_t) rig.GetCodeRow (s_kCall)].hasBreakpoint, L"the gutter shows it");

                rig.window.Press (VK_F9);
                rig.window.Frame();

                WindowRig::AssertLines ({ "BPC 0" }, rig.TakeSent(), L"cleared by its id");
                Assert::IsTrue (rig.GetBreakpoints().empty());
                Assert::IsFalse (rig.window.GetSnapshot()->codeViews[0][(size_t) rig.GetCodeRow (s_kCall)].hasBreakpoint);
            }
        }


        //  The breakpoint the key sets is one the machine stops at.
        TEST_METHOD (F9_OnASelectedLine_SetsABreakpointARunStopsAt)
        {
            WindowRig  rig;



            rig.SelectCode (s_kJump);
            rig.window.Press (VK_F9);
            rig.window.Frame();

            WindowRig::AssertLines ({ "BP 0304" }, rig.TakeSent(), L"the selected line, not the PC's");

            rig.PressAndSettle (VK_F5);

            Assert::AreEqual (s_kJump, rig.GetPc(), L"stopped there");
            Assert::AreEqual ((Byte) 1, rig.GetRegisters().x);
        }


        TEST_METHOD (F9_ClearsADisabledBreakpointTooByItsId)
        {
            WindowRig  rig;



            rig.Submit (L"BP 0300");
            rig.Submit (L"BPD 0");
            Assert::IsTrue (rig.GetBreakpoints() == std::vector<std::pair<Word, bool>> { { s_kCall, false } });
            (void) rig.TakeSent();

            rig.window.Press (VK_F9);
            rig.window.Frame();

            WindowRig::AssertLines ({ "BPC 0" }, rig.TakeSent(), L"a disabled breakpoint is still there to clear");
            Assert::IsTrue (rig.GetBreakpoints().empty());
        }


        //  Only a breakpoint that starts on the line is the line's; a range
        //  that covers it from before gets a second breakpoint beside it, and
        //  the range's own line clears the range.
        TEST_METHOD (F9_OnALineARangeCovers_SetsASecondBreakpoint)
        {
            WindowRig  rig;



            rig.Submit (L"BP 0300:0310");
            (void) rig.TakeSent();

            rig.SelectCode (s_kJump);
            rig.window.Press (VK_F9);
            rig.window.Frame();

            WindowRig::AssertLines ({ "BP 0304" }, rig.TakeSent(), L"a line inside the range");
            Assert::AreEqual ((size_t) 2, rig.GetBreakpoints().size());

            rig.SelectCode (s_kCall);
            rig.window.Press (VK_F9);
            rig.window.Frame();

            WindowRig::AssertLines ({ "BPC 0" }, rig.TakeSent(), L"the range's first line clears the range");
            Assert::IsTrue (rig.GetBreakpoints() == std::vector<std::pair<Word, bool>> { { s_kJump, true } });
        }


        //  The id is written in decimal whatever its size.
        TEST_METHOD (F9_ClearsABreakpointNumberedTenOrMore)
        {
            WindowRig  rig;



            for (int i = 0; i < 10; i++)
            {
                rig.Submit (std::format (L"BP {:04X}", 0x0200 + i));
            }

            (void) rig.TakeSent();

            rig.window.Press (VK_F9);
            rig.window.Frame();
            WindowRig::AssertLines ({ "BP 0300" }, rig.TakeSent(), L"the eleventh");

            rig.window.Press (VK_F9);
            rig.window.Frame();
            WindowRig::AssertLines ({ "BPC 10" }, rig.TakeSent(), L"cleared as number ten");
            Assert::AreEqual ((size_t) 10, rig.GetBreakpoints().size(), L"the other ten stay");
        }


        TEST_METHOD (F9_InEachDialect_SetsAndClearsInThatDialectsWords)
        {
            struct Toggle
            {
                const wchar_t  * dialect;
                const char     * set;
                const char     * clear;
            };

            static constexpr Toggle  kToggles[] =
            {
                { s_kpszAppleWin,  "BP 0300",  "BPC 0"  },
                { s_kpszMonitor,   "/BP 0300", "/BPC 0" },
                { s_kpszGSSquared, "bp 0300",  "nobp 0" },
                { s_kpszWinDbg,    "bp 0300",  "bc 0"   },
                { s_kpszCasso,     "BP 0300",  "BPC 0"  },
            };



            for (const Toggle & each : kToggles)
            {
                WindowRig  rig;



                rig.ChooseDialect (each.dialect);
                (void) rig.TakeSent();

                rig.SelectCode (s_kCall);
                rig.window.Press (VK_F9);
                rig.window.Frame();

                WindowRig::AssertLines ({ each.set }, rig.TakeSent(), each.dialect);
                Assert::IsTrue (rig.host.replies.back().status == CommandStatus::Ok, each.dialect);
                Assert::IsTrue (rig.GetBreakpoints() == std::vector<std::pair<Word, bool>> { { s_kCall, true } }, each.dialect);

                rig.window.Press (VK_F9);
                rig.window.Frame();

                WindowRig::AssertLines ({ each.clear }, rig.TakeSent(), each.dialect);
                Assert::IsTrue (rig.host.replies.back().status == CommandStatus::Ok, each.dialect);
                Assert::IsTrue (rig.GetBreakpoints().empty(), each.dialect);
            }
        }


        //  The selection is a row of the view, so after the view moves, F9
        //  acts on whatever line that row now shows: the one highlighted.
        TEST_METHOD (F9_ActsOnTheLineTheSelectedRowShowsNow)
        {
            WindowRig  rig;
            int        row = 0;
            Word       now = 0;



            rig.SelectCode (s_kJump);
            row = rig.window.GetCodeList (0)->GetSelectedRow();

            //  AppleWin's V moves the view down one instruction.
            rig.Submit (L"V");
            (void) rig.TakeSent();
            rig.window.Frame();

            now = rig.window.GetSnapshot()->codeViews[0][(size_t) row].address;
            Assert::AreEqual (row, rig.window.GetCodeList (0)->GetSelectedRow(), L"the same row is selected");
            Assert::AreNotEqual (s_kJump, now, L"and shows another line now");

            rig.window.Press (VK_F9);
            WindowRig::AssertLines ({ std::format ("BP {:04X}", now) }, rig.TakeSent(), L"the line on the row");
        }


        //  Before the first snapshot there is no line to act on; the key is
        //  still the scheme's and goes nowhere else.
        TEST_METHOD (F9_BeforeTheFirstSnapshot_SendsNothing)
        {
            WindowRig  rig ("", false);



            Assert::IsNull (rig.window.GetSnapshot());
            Assert::IsTrue (rig.window.Press (VK_F9), L"taken");
            Assert::IsTrue (rig.TakeSent().empty(), L"nothing to toggle");
        }


        //  On a machine running freely, with no stop condition, F9 arms one,
        //  and the machine stops on the line.
        TEST_METHOD (F9_OnAFreeRunningMachine_StopsItOnTheLine)
        {
            WindowRig  rig;
            uint64_t   cycles = 0;



            rig.SelectCode (s_kJump);
            rig.ResumeEmulator();
            Assert::AreEqual (2, rig.RunSlices (2), L"running freely");

            rig.window.Press (VK_F9);
            WindowRig::AssertLines ({ "BP 0304" }, rig.TakeSent(), L"set between slices");

            (void) rig.RunSlices (4);
            cycles = rig.GetCycles();

            Assert::AreEqual (s_kJump, rig.GetPc(), L"the machine stops on the line");
            (void) rig.RunSlices (4);
            Assert::AreEqual (cycles, rig.GetCycles(), L"and stays there");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  RunToCursorKeyTests
    //
    //  Ctrl+F10 in the Visual Studio and GSSquared schemes, Ctrl+Down in the
    //  AppleWin scheme: the machine runs until the PC reaches the selected
    //  line of the disassembly view last used.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RunToCursorKeyTests)
    {
    public:

        static void AssertRanToTheJump (WindowRig & rig, const wchar_t * message)
        {
            Assert::AreEqual (s_kJump,       rig.GetPc(),          message);
            Assert::AreEqual ((Byte) 1,    rig.GetRegisters().x, L"through the call and one INX");
            Assert::AreEqual ((Byte) 0x41, rig.Peek (s_kScreen));
            Assert::IsTrue   (rig.IsPaused());
            Assert::IsTrue   (rig.window.GetSnapshot()->isPaused);
        }


        TEST_METHOD (CtrlF10_And_CtrlDown_RunToTheSelectedLine_InTheirSchemes)
        {
            struct Chord
            {
                const char  * scheme;
                WPARAM        vk;
            };

            static constexpr Chord  kChords[] =
            {
                { "",                VK_F10  },
                { s_kpszSchemeApple, VK_DOWN },
                { s_kpszSchemeGs,    VK_F10  },
            };



            for (const Chord & chord : kChords)
            {
                WindowRig  rig (chord.scheme);



                rig.SelectCode (s_kJump);
                rig.PressAndSettle (chord.vk, true);

                WindowRig::AssertLines ({ "G 0304" }, rig.TakeSent(), TextEncoding::NarrowToWide (chord.scheme).c_str());
                AssertRanToTheJump (rig, TextEncoding::NarrowToWide (chord.scheme).c_str());
            }
        }


        //  With no line selected there is nowhere to run to: nothing is sent,
        //  and the key is still taken.
        TEST_METHOD (CtrlF10_WithNothingSelected_SendsNothing)
        {
            WindowRig  rig;



            Assert::IsTrue   (rig.window.Press (VK_F10, true));
            rig.Settle();

            Assert::IsTrue   (rig.TakeSent().empty());
            Assert::AreEqual (s_kCall, rig.GetPc());
        }


        //  Ctrl+Down passes a box, however full: Down alone walks the command
        //  box's history, and a memory window would move its caret.
        TEST_METHOD (AppleWin_CtrlDown_RunsToTheCursor_FromEveryBoxAndTheMemoryWindow)
        {
            for (int where = 0; where < 4; where++)
            {
                WindowRig        rig (s_kpszSchemeApple);
                IDxuiControl   * target = nullptr;



                rig.SelectCode (s_kJump);

                target = (where == 0) ? (IDxuiControl *) rig.window.GetCommandBox()
                       : (where == 1) ? (IDxuiControl *) rig.window.GetMemoryBox()
                       : (where == 2) ? (IDxuiControl *) rig.window.GetPokeBox()
                       :                (IDxuiControl *) rig.window.GetMemoryPane (1)->GetView();

                rig.window.Click (target);
                Assert::IsTrue (rig.window.GetFocusedControl() == target, L"the click focused it");

                if (where < 3)
                {
                    rig.window.TypeText (L"12");
                }

                rig.PressAndSettle (VK_DOWN, true);

                WindowRig::AssertLines ({ "G 0304" }, rig.TakeSent(), std::format (L"place {}", where).c_str());
                AssertRanToTheJump (rig, std::format (L"place {}", where).c_str());
            }
        }


        TEST_METHOD (CtrlF10_InEachDialectButGSSquared_RunsToTheCursor)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kRuns[] =
            {
                { s_kpszAppleWin, "G 0304"  },
                { s_kpszMonitor,  "/G 0304" },
                { s_kpszWinDbg,   "g 0304"  },
                { s_kpszCasso,    "G 0304"  },
            };



            for (const auto & [dialect, line] : kRuns)
            {
                WindowRig  rig;



                rig.ChooseDialect (dialect);
                (void) rig.TakeSent();

                rig.SelectCode (s_kJump);
                rig.PressAndSettle (VK_F10, true);

                WindowRig::AssertLines ({ line }, rig.TakeSent(), dialect);
                AssertRanToTheJump (rig, dialect);
            }
        }


        //  A machine running freely is taken into the run, which stops at the
        //  line as a run from a stopped machine does.
        TEST_METHOD (CtrlF10_OnAFreeRunningMachine_StopsAtTheCursor)
        {
            WindowRig  rig;



            rig.SelectCode (s_kJump);
            rig.ResumeEmulator();
            Assert::AreEqual (2, rig.RunSlices (2));

            rig.PressAndSettle (VK_F10, true);

            Assert::AreEqual (s_kJump, rig.GetPc());
            Assert::IsTrue   (rig.IsPaused(), L"the run ended");
            Assert::IsTrue   (rig.GetSession().GetRunState() == RunState::Paused);
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  CursorViewKeyTests
    //
    //  F9 and run to cursor act on the disassembly view last used: each view
    //  its own selection, and never one that has been closed.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CursorViewKeyTests)
    {
    public:

        //  A second view, opened as the + after the disassembly tabs opens
        //  one, at an address.
        static void OpenSecondView (WindowRig & rig, Word address)
        {
            rig.host.SetDebuggerCodeAddress (address, 1);
            rig.window.Frame();
            Assert::IsTrue (rig.window.GetSnapshot()->codeOpen[1], L"the second view is open");
            Assert::IsTrue (rig.window.GetCodeList (1)->IsVisible(), L"and in front");
        }


        TEST_METHOD (TheKeysActOnTheSelectionOfTheViewLastUsed)
        {
            WindowRig  rig;



            rig.SelectCode (s_kCall);
            OpenSecondView (rig, s_kRoutine);
            rig.SelectCode (s_kRoutine + 2, 1);

            rig.window.Press (VK_F9);
            WindowRig::AssertLines ({ "BP 0312" }, rig.TakeSent(), L"the second view's line");

            rig.window.Press (VK_F10, true);
            WindowRig::AssertLines ({ "G 0312" }, rig.TakeSent(), L"and run to it");
        }


        //  A closed view has no lines; the keys go back to the first view.
        TEST_METHOD (AfterTheLastUsedViewCloses_TheKeysActOnTheFirstView)
        {
            WindowRig  rig;



            rig.SelectCode (s_kJump);
            OpenSecondView (rig, s_kRoutine);
            rig.SelectCode (s_kRoutine, 1);

            rig.host.CloseDebuggerCodeView (1);
            rig.window.Frame();
            Assert::IsFalse (rig.window.GetSnapshot()->codeOpen[1], L"closed");

            rig.window.Press (VK_F9);
            WindowRig::AssertLines ({ "BP 0304" }, rig.TakeSent(), L"the first view's selection, not the PC");

            Assert::IsTrue (rig.IsEntryEnabled (DebuggerCommands::kRunToCursor), L"the first view has a line to run to");
            rig.window.Press (VK_F10, true);
            WindowRig::AssertLines ({ "G 0304" }, rig.TakeSent(), L"run to the first view's line");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  KeyDialectTests
    //
    //  The dialect a key's line is written in is the one in the snapshot the
    //  window last took, which is the session's once the snapshot after a MODE
    //  has arrived.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (KeyDialectTests)
    {
    public:

        //  Before any snapshot, a line is AppleWin's.
        TEST_METHOD (BeforeTheFirstSnapshot_ALineIsAppleWins)
        {
            WindowRig  rig ("", false);



            (void) rig.GetSession().ExecuteLine ("MODE WINDBG", CommandMode::AppleWin);
            rig.window.Press (VK_F10);

            WindowRig::AssertLines ({ "P" }, rig.TakeSent(), L"AppleWin's word, in the absence of a snapshot");
        }


        //  A MODE whose snapshot has not arrived leaves the keys in the words
        //  of the dialect before it.
        TEST_METHOD (UntilTheSnapshotAfterAModeArrives_ALineIsInTheOldDialect)
        {
            WindowRig  rig;



            rig.GetDialectRow (s_kpszWinDbg)->dispatch();
            Assert::IsTrue (rig.GetSession().GetMode() == CommandMode::WinDbg, L"the session changed at once");

            rig.window.Press (VK_F9);
            WindowRig::AssertLines ({ "MODE WINDBG", "BP 0300" }, rig.TakeSent(), L"still AppleWin's word");

            rig.window.Frame();
            rig.window.Press (VK_F9);
            WindowRig::AssertLines ({ "bc 0" }, rig.TakeSent(), L"WinDbg's once the snapshot says so");
        }


        //  WinDbg has no word for the other control lines, and reaches them
        //  through its engine marker.
        TEST_METHOD (WinDbg_TheOtherControlLinesTakeTheEngineMarker)
        {
            WindowRig  rig;



            rig.ChooseDialect (s_kpszWinDbg);
            (void) rig.TakeSent();

            rig.ClickEntry (DebuggerCommands::kTrace);
            rig.window.Frame();

            WindowRig::AssertLines ({ "!HISTORY ON" }, rig.TakeSent(), L"the trace toggle");
            Assert::IsTrue (rig.host.replies.back().status == CommandStatus::Ok);
            Assert::IsTrue (rig.window.GetSnapshot()->trace.isOn, L"and it took");
        }
    };
}
