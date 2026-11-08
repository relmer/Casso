#include "Pch.h"

#include "DebuggerWindowRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerWindowBoxKeysTests
//
//  The keys the window's text boxes keep and the ones they pass on: the
//  command box, the Go to box and the poke box, and the memory windows, which
//  count as a box that is never empty. Enter submits each box; Up and Down walk
//  the command box's history; Ctrl+C copies the console's selection; and in
//  GSSquared's dialect an empty command line steps and runs on its own keys.
//
//  Every key goes through the window as DxuiWindow::DispatchKey hands it on, a
//  key-down and then the character a keyboard makes of it, so a key a box
//  claims to keep but leaves unclaimed is caught reaching the key scheme.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static constexpr Word                s_kBoxCall      = WindowRig::kCall;
    static constexpr Word                s_kBoxJump      = WindowRig::kJump;
    static constexpr Word                s_kBoxRoutine   = WindowRig::kRoutine;
    static constexpr const char        * s_kpszBoxApple  = "AppleWin";
    static constexpr const char        * s_kpszBoxGs     = "GSSquared";






    ////////////////////////////////////////////////////////////////////////////
    //
    //  BoxRig
    //
    //  The window rig with a way to reach each box.
    //
    ////////////////////////////////////////////////////////////////////////////

    class BoxRig : public WindowRig
    {
    public:
        //  The boxes, by where a test clicks to type into them.
        enum class Box { Command, GoTo, Poke };


        explicit BoxRig (const std::string & scheme = std::string(), bool publishes = true) :
            WindowRig (scheme, publishes)
        {
        }


        DxuiTextInput *  GetBox (Box box)
        {
            switch (box)
            {
            case Box::GoTo: return window.GetMemoryBox();
            case Box::Poke: return window.GetPokeBox();
            default:        return window.GetCommandBox();
            }
        }


        //  A click into a box, which focuses it.
        DxuiTextInput *  Focus (Box box)
        {
            DxuiTextInput  * target = GetBox (box);



            window.Click (target);
            Assert::IsTrue (window.GetFocusedControl() == target, L"the click focused the box");
            return target;
        }


        static const wchar_t *  GetName (Box box)
        {
            switch (box)
            {
            case Box::GoTo: return L"the Go to box";
            case Box::Poke: return L"the poke box";
            default:        return L"the command box";
            }
        }
    };





    using Box = BoxRig::Box;





    ////////////////////////////////////////////////////////////////////////////
    //
    //  BoxRoutingTests
    //
    //  Space and Enter reach the key scheme from an empty box and are the
    //  box's once it holds text; a letter is always the box's; function keys
    //  and Ctrl chords always reach the scheme.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BoxRoutingTests)
    {
    public:

        //  Space in a box with text in it types a space, in the schemes that
        //  bind Space to a step, and the machine does not move.
        TEST_METHOD (SpaceInATypedBox_TypesASpace_AndStepsNothing)
        {
            for (const char * scheme : { s_kpszBoxApple, s_kpszBoxGs })
            {
                for (Box box : { Box::Command, Box::GoTo, Box::Poke })
                {
                    BoxRig           rig (scheme);
                    DxuiTextInput  * target = rig.Focus (box);



                    rig.window.TypeText (L"12");
                    rig.window.Stroke (VK_SPACE, L' ');
                    rig.window.Stroke (VK_SPACE, L' ', false, false, true);
                    rig.window.TypeText (L"3");
                    rig.Settle();

                    Assert::AreEqual (std::wstring (L"12  3"), target->GetText(), BoxRig::GetName (box));
                    Assert::IsTrue   (rig.TakeSent().empty(), L"nothing stepped");
                    Assert::AreEqual (s_kBoxCall, rig.GetPc(), BoxRig::GetName (box));
                }
            }
        }


        //  GSSquared's O and R are letters, which a box keeps, whether it is
        //  empty or not.
        TEST_METHOD (GSSquared_OAndRInABox_AreTyped_AndStepNothing)
        {
            for (Box box : { Box::Command, Box::GoTo, Box::Poke })
            {
                BoxRig           rig (s_kpszBoxGs);
                DxuiTextInput  * target = rig.Focus (box);



                rig.window.TypeText (L"or nobp load");
                rig.Settle();

                Assert::AreEqual (std::wstring (L"or nobp load"), target->GetText(), BoxRig::GetName (box));
                Assert::IsTrue   (rig.TakeSent().empty(), L"nothing stepped");
                Assert::AreEqual (s_kBoxCall, rig.GetPc());
            }
        }


        //  From an empty box, Space steps, and the character it types is
        //  taken, so the box is still empty; the next one types as usual.
        TEST_METHOD (AppleWin_SpaceInAnEmptyBox_Steps_AndTheBoxStaysEmpty)
        {
            for (Box box : { Box::Command, Box::GoTo, Box::Poke })
            {
                BoxRig           rig (s_kpszBoxApple);
                DxuiTextInput  * target = rig.Focus (box);



                rig.window.Stroke (VK_SPACE, L' ');
                rig.Settle();

                WindowRig::AssertLines ({ "T" }, rig.TakeSent(), BoxRig::GetName (box));
                Assert::AreEqual (s_kBoxRoutine, rig.GetPc(), L"stepped into the call");
                Assert::IsTrue   (target->GetText().empty(), L"no space typed");

                rig.window.TypeText (L"x ");
                Assert::AreEqual (std::wstring (L"x "), target->GetText(), L"only the one character was taken");
            }
        }


        TEST_METHOD (AppleWin_CtrlSpaceAndShiftSpaceInAnEmptyBox_StepOverAndOut)
        {
            BoxRig  rig (s_kpszBoxApple);



            rig.window.Stroke (VK_SPACE, L' ', true);
            rig.Settle();

            WindowRig::AssertLines ({ "P" }, rig.TakeSent(), L"Ctrl+Space");
            Assert::AreEqual (WindowRig::kLoop, rig.GetPc());
            Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty());

            rig.window.Stroke (VK_SPACE, L' ', false, false, true);
            WindowRig::AssertLines ({ "RTS" }, rig.TakeSent(), L"Shift+Space");
            Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty());
        }


        //  Enter in an empty Go to or poke box runs, in the schemes that bind
        //  it; nothing is submitted.
        TEST_METHOD (AppleWinAndGSSquared_EnterInAnEmptyGoToOrPokeBox_Runs)
        {
            for (const char * scheme : { s_kpszBoxApple, s_kpszBoxGs })
            {
                for (Box box : { Box::GoTo, Box::Poke })
                {
                    BoxRig  rig (scheme);



                    (void) rig.Focus (box);
                    rig.window.Stroke (VK_RETURN, L'\r');

                    WindowRig::AssertLines ({ "G" }, rig.TakeSent(), BoxRig::GetName (box));
                    Assert::IsTrue   (rig.host.goTos.empty(), L"no Go to");
                    Assert::AreEqual (4, rig.RunSlices (4), L"running");
                }
            }
        }


        //  Visual Studio binds neither Space nor Enter, so an empty box types
        //  a space, and an empty Go to box goes nowhere.
        TEST_METHOD (VisualStudio_SpaceAndEnterInAnEmptyBox_AreTheBoxs)
        {
            for (Box box : { Box::Command, Box::GoTo, Box::Poke })
            {
                BoxRig           rig;
                DxuiTextInput  * target = rig.Focus (box);



                rig.window.Stroke (VK_SPACE, L' ');
                Assert::AreEqual (std::wstring (L" "), target->GetText(), BoxRig::GetName (box));
                Assert::IsTrue   (rig.TakeSent().empty());
            }
        }


        //  A Space that reached the scheme from a list, where no box has the
        //  keys, types nothing into a box.
        TEST_METHOD (ASpaceThatSteppedFromACodeView_TypesNothing)
        {
            BoxRig  rig (s_kpszBoxApple);



            rig.SelectCode (s_kBoxCall);
            rig.window.Stroke (VK_SPACE, L' ');
            rig.Settle();

            WindowRig::AssertLines ({ "T" }, rig.TakeSent(), L"stepped");
            Assert::IsTrue (rig.window.GetCommandBox()->GetText().empty(), L"the command box took no space");
            Assert::IsTrue (rig.window.GetMemoryBox()->GetText().empty());
            Assert::IsTrue (rig.window.GetPokeBox()->GetText().empty());
        }


        //  Function keys and Ctrl chords reach the scheme from every box,
        //  whatever it holds, and leave the box as it was.
        TEST_METHOD (FunctionKeysAndChords_ReachTheSchemeFromEveryBox)
        {
            for (Box box : { Box::Command, Box::GoTo, Box::Poke })
            {
                BoxRig           rig;
                DxuiTextInput  * target = rig.Focus (box);



                rig.window.TypeText (L"12");
                rig.window.Press (VK_F10);
                rig.Settle();

                WindowRig::AssertLines ({ "P" }, rig.TakeSent(), BoxRig::GetName (box));
                Assert::AreEqual (WindowRig::kLoop, rig.GetPc());
                Assert::AreEqual (std::wstring (L"12"), target->GetText());
            }
        }


        //  Characters typed while a list has the keys go into no box.
        TEST_METHOD (CharactersTypedAtAList_ReachNoBox)
        {
            BoxRig  rig;



            rig.SelectCode (s_kBoxCall);
            rig.window.TypeText (L"bpl");

            Assert::IsTrue (rig.window.GetCommandBox()->GetText().empty());
            Assert::IsTrue (rig.window.GetMemoryBox()->GetText().empty());
            Assert::IsTrue (rig.window.GetPokeBox()->GetText().empty());
            Assert::IsTrue (rig.TakeSent().empty());
        }


        //  A box holds 256 characters; more are not typed.
        TEST_METHOD (ABoxHoldsTwoHundredFiftySixCharacters)
        {
            for (Box box : { Box::Command, Box::GoTo, Box::Poke })
            {
                BoxRig           rig;
                DxuiTextInput  * target = rig.Focus (box);



                rig.window.TypeText (std::wstring (260, L'7'));
                Assert::AreEqual ((size_t) 256, target->GetText().size(), BoxRig::GetName (box));
            }
        }


        //  Escape is not a box's: it neither clears the command box nor ends a
        //  walk through its history.
        TEST_METHOD (EscapeInTheCommandBox_ChangesNothing)
        {
            BoxRig  rig;



            rig.Submit (L"BPL");
            rig.Submit (L"WL");
            rig.window.Press (VK_UP);
            rig.window.Stroke (VK_ESCAPE, 0x1B);
            Assert::AreEqual (std::wstring (L"WL"), rig.window.GetCommandBox()->GetText());

            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"BPL"), rig.window.GetCommandBox()->GetText(), L"the walk went on");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  MemoryWindowRoutingTests
    //
    //  Every character typed into a memory window is an edit, so it keeps
    //  Space and Enter as a box with text in it does; a Ctrl chord still
    //  reaches the scheme.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MemoryWindowRoutingTests)
    {
    public:

        static DxuiHexView * FocusMemory (WindowRig & rig)
        {
            DxuiHexView  * view   = rig.window.GetMemoryPane (1)->GetView();
            RECT           bounds = view->GetBounds();



            //  On the hex of the second row.
            rig.window.ClickAt (POINT { bounds.left + 70, bounds.top + 20 });
            Assert::IsTrue (rig.window.GetFocusedControl() == view, L"the click focused the memory window");
            Assert::IsTrue (view->GetActiveColumn() == DxuiHexView::Column::Hex, L"in the hex");
            return view;
        }


        TEST_METHOD (AppleWin_EnterAndSpaceInAMemoryWindow_AreItsOwn)
        {
            WindowRig      rig (s_kpszBoxApple);
            DxuiHexView  * view = FocusMemory (rig);
            uint64_t       at   = 0;



            rig.window.Stroke (VK_RETURN, L'\r');
            Assert::IsTrue   (rig.TakeSent().empty(), L"Enter ran nothing");

            rig.window.Stroke (VK_TAB, L'\t');
            Assert::IsTrue   (view->GetActiveColumn() == DxuiHexView::Column::Text, L"Tab went to the text column");

            at = view->GetCaret();
            rig.window.Stroke (VK_SPACE, L' ');
            rig.Settle();

            WindowRig::AssertLines ({ std::format ("PATCH {:04X} A0", (Word) at) }, rig.TakeSent(), L"the space was an edit, not a step");
            Assert::AreEqual ((Byte) 0xA0, rig.Peek ((Word) at), L"written to the machine");
            Assert::AreEqual (s_kBoxCall, rig.GetPc(), L"and nothing stepped");
        }


        TEST_METHOD (GSSquared_OAndRInAMemoryWindow_AreItsOwn)
        {
            WindowRig      rig (s_kpszBoxGs);
            DxuiHexView  * view = FocusMemory (rig);



            rig.window.Stroke ('O', L'o');
            rig.window.Stroke ('R', L'r');
            Assert::IsTrue (rig.TakeSent().empty(), L"not hex digits, and not steps");

            rig.window.Stroke (VK_TAB, L'\t');
            rig.window.Stroke ('O', L'o');
            rig.Settle();

            WindowRig::AssertLines ({ std::format ("PATCH {:04X} EF", (Word) (view->GetCaret() - 1)) }, rig.TakeSent(), L"an o in the text column");
            Assert::AreEqual (s_kBoxCall, rig.GetPc());
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  ListKeyTests
    //
    //  A list takes no key but Space on selected checkbox rows, so the scheme's
    //  keys act from any pane; on the breakpoints pane Space turns the selected
    //  breakpoints on and off instead of stepping.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ListKeyTests)
    {
    public:

        TEST_METHOD (SpaceOnASelectedBreakpoint_TurnsItOffAndOn_InsteadOfStepping)
        {
            for (const char * scheme : { "", s_kpszBoxApple, s_kpszBoxGs })
            {
                WindowRig  rig (scheme);



                rig.Submit (L"BP 0304");
                (void) rig.TakeSent();

                Assert::IsTrue (rig.window.ClickRow (rig.window.GetBreakpointList(), 0), L"the breakpoint's row");
                rig.window.Stroke (VK_SPACE, L' ');
                rig.window.Frame();

                WindowRig::AssertLines ({ "BPD 0" }, rig.TakeSent(), TextEncoding::NarrowToWide (scheme).c_str());
                Assert::IsTrue (rig.GetBreakpoints() == std::vector<std::pair<Word, bool>> { { s_kBoxJump, false } });

                rig.window.Stroke (VK_SPACE, L' ');
                rig.window.Frame();

                WindowRig::AssertLines ({ "BPE 0" }, rig.TakeSent(), L"and on again");
                Assert::IsTrue (rig.GetBreakpoints() == std::vector<std::pair<Word, bool>> { { s_kBoxJump, true } });
                Assert::AreEqual (s_kBoxCall, rig.GetPc(), L"nothing stepped");
            }
        }


        //  A disabled breakpoint does not stop the machine.
        TEST_METHOD (ABreakpointTurnedOffWithSpace_DoesNotStopARun)
        {
            WindowRig  rig;



            rig.Submit (L"BP 0304");
            rig.window.ClickRow (rig.window.GetBreakpointList(), 0);
            rig.window.Stroke (VK_SPACE, L' ');
            rig.window.Frame();

            rig.window.Press (VK_F5);
            Assert::AreEqual (6, rig.RunSlices (6), L"the run passes the breakpoint");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  CommandBoxEnterTests
    //
    //  Enter in the command box runs the line as typed, in the session's
    //  dialect, then clears the box and keeps the line in the history.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CommandBoxEnterTests)
    {
    public:

        TEST_METHOD (EnterRunsTheLineAsTyped_AndShowsItsReply)
        {
            WindowRig  rig;



            (void) rig.TakeConsole();
            rig.Submit (L"R A 42");

            WindowRig::AssertLines ({ "R A 42" }, rig.TakeSent(), L"sent as typed");
            Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok);
            Assert::AreEqual ((Byte) 0x42, rig.GetRegisters().a, L"and run");
            Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty(), L"the box is cleared");

            {
                std::vector<std::string>  console  = rig.TakeConsole();
                std::vector<std::string>  expected = { ">R A 42" };

                expected.insert (expected.end(), rig.host.replies.back().text.begin(), rig.host.replies.back().text.end());
                WindowRig::AssertLines (expected, console, L"the console shows the line and its reply");
            }
        }


        //  The typed line is not put into the dialect's words: it already is
        //  in them. Each dialect's own form reaches the session unchanged.
        TEST_METHOD (EnterSendsATypedLineUnchanged_InEveryDialect)
        {
            struct Typed
            {
                const wchar_t  * dialect;
                const wchar_t  * line;
            };

            static constexpr Typed  kTyped[] =
            {
                { L"Monitor",   L"300:EA"    },
                { L"GSSquared", L"0300: EA"  },
                { L"WinDbg",    L"eb 300 ea" },
                { L"Casso",     L"MEB 300 EA" },
            };



            for (const Typed & each : kTyped)
            {
                WindowRig  rig;



                rig.ChooseDialect (each.dialect);
                (void) rig.TakeSent();

                rig.Submit (each.line);

                WindowRig::AssertLines ({ TextEncoding::WideToNarrow (each.line) }, rig.TakeSent(), each.dialect);
                Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok, each.dialect);
                Assert::AreEqual ((Byte) 0xEA, rig.Peek (s_kBoxCall), each.dialect);
            }
        }


        //  Running a line brings the console back to its end, however far
        //  back it was scrolled.
        TEST_METHOD (EnterShowsTheEndOfTheConsole)
        {
            WindowRig        rig;
            DxuiTextView   * console = rig.window.GetConsoleView();



            rig.Submit (L"HELP");
            console->SetTopLine (0);
            Assert::IsTrue (console->GetTopLine() + console->GetLineCap() < console->GetLineCount(), L"scrolled back");

            rig.window.Click (rig.window.GetCommandBox());
            rig.window.TypeText (L"BPL");
            Logger::WriteMessage (std::format ("a top {} cap {} count {}\n", console->GetTopLine(), console->GetLineCap(), console->GetLineCount()).c_str());
            rig.window.Stroke (VK_RETURN, L'\r');
            Logger::WriteMessage (std::format ("b top {} cap {} count {}\n", console->GetTopLine(), console->GetLineCap(), console->GetLineCount()).c_str());
            rig.window.TakeHostUpdate();
            Logger::WriteMessage (std::format ("c top {} cap {} count {} rows {}\n", console->GetTopLine(), console->GetLineCap(), console->GetLineCount(), console->GetRows().size()).c_str());
            rig.window.Frame();
            Logger::WriteMessage (std::format ("d top {} cap {} count {} rows {}\n", console->GetTopLine(), console->GetLineCap(), console->GetLineCount(), console->GetRows().size()).c_str());
            Assert::IsTrue (console->GetTopLine() + console->GetLineCap() >= console->GetLineCount(), L"at its end");
        }


        //  A Monitor R or W with no file asks for one: open for R, save for W.
        TEST_METHOD (MonitorW_WithNoFile_AsksWhereToSave_AndWritesThere)
        {
            WindowRig    rig;
            std::string  saved;
            HRESULT      hr    = S_OK;



            rig.ChooseDialect (L"Monitor");
            (void) rig.TakeSent();
            rig.host.dialogs.path = L"C:\\Work\\out.bin";

            rig.Submit (L"300.303W");

            Assert::AreEqual (1, rig.host.dialogs.saves);
            Assert::AreEqual (0, rig.host.dialogs.opens);
            Assert::AreEqual ((size_t) 1, rig.host.dialogs.lastSpec.filters.size());
            Assert::AreEqual (std::wstring (L"All files"), rig.host.dialogs.lastSpec.filters[0].name);
            Assert::AreEqual (std::wstring (L"*.*"),       rig.host.dialogs.lastSpec.filters[0].pattern);
            WindowRig::AssertLines ({ "300.303W\"C:\\Work\\out.bin\"" }, rig.TakeSent(), L"the line with the file");
            Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok);

            hr = rig.rig.files.ReadAllText (L"C:\\Work\\out.bin", saved);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (std::string ("\x20\x10\x03\xE8", 4), saved, L"the four bytes");

            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"300.303W"), rig.window.GetCommandBox()->GetText(), L"the history keeps what was typed");
        }


        TEST_METHOD (MonitorR_WithNoFile_AsksWhatToOpen_AndReadsIt)
        {
            WindowRig  rig;
            HRESULT    hr = S_OK;



            hr = rig.rig.files.WriteAllText (L"C:\\Work\\in.bin", std::string ("\xA9\x07\x60", 3));
            Assert::AreEqual (S_OK, hr);

            rig.ChooseDialect (L"Monitor");
            rig.host.dialogs.path = L"C:\\Work\\in.bin";

            rig.Submit (L"800.802R");

            Assert::AreEqual (1, rig.host.dialogs.opens);
            Assert::AreEqual (0, rig.host.dialogs.saves);
            Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok);
            Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x0800));
            Assert::AreEqual ((Byte) 0x07, rig.Peek (0x0801));
            Assert::AreEqual ((Byte) 0x60, rig.Peek (0x0802));
        }


        //  Backing out of the picker keeps the line in the box, unrun, and out
        //  of the history.
        TEST_METHOD (BackingOutOfThePicker_LeavesTheLineUnrun)
        {
            WindowRig  rig;



            rig.ChooseDialect (L"Monitor");
            (void) rig.TakeSent();
            rig.host.dialogs.picks = false;

            rig.Submit (L"300.303W");

            Assert::AreEqual (1, rig.host.dialogs.saves);
            Assert::IsTrue   (rig.TakeSent().empty(), L"nothing ran");
            Assert::AreEqual (std::wstring (L"300.303W"), rig.window.GetCommandBox()->GetText(), L"the line waits");
            Assert::IsFalse  (rig.rig.files.Exists (L"C:\\Work\\out.bin"));

            rig.window.Stroke (VK_BACK, 0x08);
            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"300.303"), rig.window.GetCommandBox()->GetText(), L"nothing joined the history");
        }


        //  AppleWin's R is its registers, not a file.
        TEST_METHOD (OutsideTheMonitor_NoLineAsksForAFile)
        {
            WindowRig  rig;



            rig.Submit (L"R");

            Assert::AreEqual (0, rig.host.dialogs.opens + rig.host.dialogs.saves);
            WindowRig::AssertLines ({ "R" }, rig.TakeSent(), L"run as it is");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  HistoryKeyTests
    //
    //  Up and Down in the command box: the lines run from it, newest first,
    //  and what was being typed when the walk began at the end of it.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HistoryKeyTests)
    {
    public:

        static std::wstring Text (WindowRig & rig) { return rig.window.GetCommandBox()->GetText(); }


        TEST_METHOD (UpWalksBackNewestFirst_AndStopsAtTheOldest)
        {
            WindowRig  rig;



            rig.Submit (L"BPL");
            rig.Submit (L"WL");
            rig.Submit (L"R");

            Assert::IsTrue   (rig.window.Press (VK_UP));
            Assert::AreEqual (std::wstring (L"R"), Text (rig));
            Assert::AreEqual ((size_t) 1, rig.window.GetCommandBox()->GetSelectionStart(), L"the caret at the end");
            Assert::AreEqual ((size_t) 1, rig.window.GetCommandBox()->GetSelectionEnd());

            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"WL"), Text (rig));
            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"BPL"), Text (rig));
            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"BPL"), Text (rig), L"the oldest holds");
            Assert::IsTrue   (rig.TakeSent().size() == 3, L"a walk runs nothing");
        }


        TEST_METHOD (DownWalksForward_ToWhatWasBeingTyped)
        {
            WindowRig  rig;



            rig.Submit (L"BPL");
            rig.Submit (L"WL");
            rig.window.TypeText (L"MD 30");

            rig.window.Press (VK_UP);
            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"BPL"), Text (rig));

            rig.window.Press (VK_DOWN);
            Assert::AreEqual (std::wstring (L"WL"), Text (rig));
            rig.window.Press (VK_DOWN);
            Assert::AreEqual (std::wstring (L"MD 30"), Text (rig), L"what was being typed comes back");

            Assert::IsTrue   (rig.window.Press (VK_DOWN), L"taken, past the end");
            Assert::AreEqual (std::wstring (L"MD 30"), Text (rig), L"and nothing newer");
        }


        //  Shift still walks; Ctrl and Alt are not the history's.
        TEST_METHOD (OnlyShiftAmongTheModifiersWalks)
        {
            WindowRig  rig;



            rig.Submit (L"BPL");

            rig.window.Press (VK_UP, true);
            Assert::IsTrue   (Text (rig).empty(), L"Ctrl+Up");
            rig.window.Press (VK_UP, false, true);
            Assert::IsTrue   (Text (rig).empty(), L"Alt+Up");

            rig.window.Press (VK_UP, false, false, true);
            Assert::AreEqual (std::wstring (L"BPL"), Text (rig), L"Shift+Up");
        }


        TEST_METHOD (UpAndDownInTheGoToAndPokeBoxes_WalkNothing)
        {
            for (Box box : { Box::GoTo, Box::Poke })
            {
                BoxRig           rig;
                DxuiTextInput  * target = nullptr;



                rig.Submit (L"BPL");
                target = rig.Focus (box);
                rig.window.TypeText (L"12");

                rig.window.Press (VK_UP);
                rig.window.Press (VK_DOWN);
                Assert::AreEqual (std::wstring (L"12"), target->GetText(), BoxRig::GetName (box));
                Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty());
            }
        }


        TEST_METHOD (UpWithNoHistory_IsTakenAndChangesNothing)
        {
            WindowRig  rig;



            rig.window.TypeText (L"ab");

            Assert::IsTrue   (rig.window.Press (VK_UP));
            Assert::AreEqual (std::wstring (L"ab"), Text (rig));
        }


        TEST_METHOD (ALineRunAgain_MovesToTheNewestPlace)
        {
            WindowRig  rig;



            rig.Submit (L"BPL");
            rig.Submit (L"WL");
            rig.Submit (L"BPL");

            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"BPL"), Text (rig));
            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"WL"), Text (rig));
            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"WL"), Text (rig), L"one BPL, not two");
        }


        //  A recalled line that is edited and walked away from is not kept:
        //  the walk goes on through the history as it was.
        TEST_METHOD (EditingARecalledLine_ThenWalking_LeavesTheHistoryAsItWas)
        {
            WindowRig  rig;



            rig.Submit (L"BPL");
            rig.Submit (L"WL");

            rig.window.Press (VK_UP);
            rig.window.TypeText (L"X");
            Assert::AreEqual (std::wstring (L"WLX"), Text (rig));

            rig.window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"BPL"), Text (rig));
            rig.window.Press (VK_DOWN);
            Assert::AreEqual (std::wstring (L"WL"), Text (rig), L"the edit is not kept");
            rig.window.Press (VK_DOWN);
            Assert::IsTrue   (Text (rig).empty(), L"what was typed before the walk");
        }


        //  Lines the keys and the controls send are not the user's typing.
        TEST_METHOD (LinesSentByKeysAndControls_AreNotInTheHistory)
        {
            WindowRig  rig;



            rig.window.Press (VK_F9);
            rig.ClickEntry (DebuggerCommands::kStepInto);
            rig.Settle();
            Assert::AreEqual ((size_t) 2, rig.TakeSent().size());

            rig.window.Click (rig.window.GetCommandBox());
            rig.window.Press (VK_UP);
            Assert::IsTrue (Text (rig).empty());
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  CopyKeyTests
    //
    //  A press in the console's output selects there and leaves the keys in
    //  the command box; Ctrl+C there, with nothing selected in the box, copies
    //  the output's selection.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CopyKeyTests)
    {
    public:

        //  A drag across the console's first line, as the mouse makes one.
        static void SelectInTheConsole (TestDebuggerWindow & window)
        {
            RECT            bounds = window.GetConsoleView()->GetBounds();
            DxuiMouseEvent  ev;



            ev.button      = DxuiMouseButton::Left;
            ev.kind        = DxuiMouseEventKind::Down;
            ev.positionDip = POINT { bounds.left + 2, bounds.top + 4 };
            (void) window.OnMouse (ev);

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = POINT { bounds.left + 200, bounds.top + 4 };
            (void) window.OnMouse (ev);

            ev.kind        = DxuiMouseEventKind::Up;
            (void) window.OnMouse (ev);
        }


        TEST_METHOD (APressInTheConsole_LeavesTheKeysInTheCommandBox)
        {
            WindowRig  rig;



            rig.SelectCode (s_kBoxCall);
            Assert::IsTrue (rig.window.GetFocusedControl() == rig.window.GetCodeList (0));

            rig.window.Click (rig.window.GetConsoleView());
            Assert::IsTrue (rig.window.GetFocusedControl() == rig.window.GetCommandBox(), L"the command box has the keys");

            rig.window.TypeText (L"bpl");
            Assert::AreEqual (std::wstring (L"bpl"), rig.window.GetCommandBox()->GetText());
        }


        TEST_METHOD (CtrlC_CopiesTheConsolesSelection)
        {
            WindowRig  rig;



            rig.Submit (L"BP 0304");
            SelectInTheConsole (rig.window);
            Assert::IsTrue (rig.window.GetConsoleView()->HasSelection(), L"the drag selected");

            Assert::IsTrue   (rig.window.Press ('C', true), L"taken");
            Assert::AreEqual (1, rig.window.copies);
            Assert::AreEqual (rig.window.GetConsoleView()->GetSelectionText(), rig.window.copied);
            Assert::IsFalse  (rig.window.copied.empty());
        }


        TEST_METHOD (CtrlC_WithNothingSelectedInTheConsole_CopiesNothing)
        {
            WindowRig  rig;



            rig.Submit (L"BP 0304");
            Assert::IsFalse (rig.window.GetConsoleView()->HasSelection());

            rig.window.Press ('C', true);
            Assert::AreEqual (0, rig.window.copies);
        }


        //  With text selected in the command box, Ctrl+C is the box's own: the
        //  console's selection is left alone.
        TEST_METHOD (CtrlC_WithASelectionInTheBox_IsNotTheConsoles)
        {
            WindowRig  rig;
            RECT       box = {};



            rig.Submit (L"BP 0304");
            SelectInTheConsole (rig.window);
            rig.window.TypeText (L"abc");

            box = rig.window.GetCommandBox()->GetBounds();

            for (int click = 0; click < 3; click++)
            {
                rig.window.ClickAt (POINT { box.left + 20, (box.top + box.bottom) / 2 });
            }

            Assert::AreEqual ((size_t) 0, rig.window.GetCommandBox()->GetSelectionStart(), L"a triple click selects it all");
            Assert::AreEqual ((size_t) 3, rig.window.GetCommandBox()->GetSelectionEnd());

            rig.window.Press ('C', true);
            Assert::AreEqual (0, rig.window.copies);
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  GSSquaredConsoleKeyTests
    //
    //  In GSSquared's dialect, with the command line empty, Space and F10 step
    //  and Return runs, as in GSSquared's own window, whatever the key scheme.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GSSquaredConsoleKeyTests)
    {
    public:

        TEST_METHOD (AnEmptyCommandLine_SpaceAndF10Step_ReturnRuns_InEveryScheme)
        {
            for (const char * scheme : { "", s_kpszBoxApple, s_kpszBoxGs })
            {
                WindowRig  rig (scheme);



                rig.ChooseDialect (L"GSSquared");
                (void) rig.TakeSent();

                rig.window.Stroke (VK_SPACE, L' ');
                rig.Settle();
                WindowRig::AssertLines ({ "s" }, rig.TakeSent(), L"Space steps into");
                Assert::AreEqual (s_kBoxRoutine, rig.GetPc());
                Assert::IsTrue   (rig.window.GetCommandBox()->GetText().empty(), L"no space typed");

                rig.PressAndSettle (VK_F10);
                WindowRig::AssertLines ({ "s" }, rig.TakeSent(), L"F10 steps into, even where F10 steps over");
                Assert::AreEqual ((Word) (s_kBoxRoutine + 2), rig.GetPc());

                rig.window.Stroke (VK_RETURN, L'\r');
                WindowRig::AssertLines ({ "g" }, rig.TakeSent(), L"Return runs");
                Assert::AreEqual (4, rig.RunSlices (4));
            }
        }


        TEST_METHOD (WithSomethingTyped_TheKeysAreTheBoxsAndTheSchemes)
        {
            WindowRig  rig;



            rig.ChooseDialect (L"GSSquared");
            (void) rig.TakeSent();

            rig.window.TypeText (L"bp");
            rig.window.Stroke (VK_SPACE, L' ');
            Assert::AreEqual (std::wstring (L"bp "), rig.window.GetCommandBox()->GetText());
            Assert::IsTrue   (rig.TakeSent().empty());

            rig.PressAndSettle (VK_F10);
            WindowRig::AssertLines ({ "o" }, rig.TakeSent(), L"the scheme's step over");
            Assert::AreEqual (WindowRig::kLoop, rig.GetPc());
        }


        TEST_METHOD (WithAModifier_TheKeysAreNotGSSquareds)
        {
            WindowRig  rig;



            rig.ChooseDialect (L"GSSquared");
            (void) rig.TakeSent();

            rig.window.Stroke (VK_SPACE, L' ', true);
            rig.window.Press  (VK_F10, false, true);
            Assert::IsTrue   (rig.TakeSent().empty(), L"Ctrl+Space and Alt+F10");
            Assert::AreEqual (std::wstring (L" "), rig.window.GetCommandBox()->GetText(), L"Ctrl+Space types its space, as an edit box does");

            rig.window.Stroke (VK_BACK, 0x08);

            //  With a modifier, Return is the box's Enter: the empty line is
            //  submitted rather than a run.
            rig.window.Press (VK_RETURN, false, true);
            WindowRig::AssertLines ({ "" }, rig.TakeSent(), L"Alt+Return");

            rig.window.Press (VK_F10, false, false, true);
            Assert::IsTrue   (rig.TakeSent().empty(), L"Shift+F10");
            Assert::AreEqual ((size_t) 1, rig.window.menus.size(), L"Shift+F10 is Dock To");
        }


        TEST_METHOD (InTheOtherDialects_TheKeysAreTheSchemes)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kDialects[] =
            {
                { L"AppleWin", "P"  },
                { L"Monitor",  "/P" },
                { L"WinDbg",   "p"  },
                { L"Casso",    "P"  },
            };



            for (const auto & [dialect, stepOver] : kDialects)
            {
                WindowRig  rig;



                rig.ChooseDialect (dialect);
                (void) rig.TakeSent();

                rig.window.Stroke (VK_SPACE, L' ');
                Assert::AreEqual (std::wstring (L" "), rig.window.GetCommandBox()->GetText(), dialect);
                Assert::IsTrue   (rig.TakeSent().empty(), dialect);

                rig.window.Stroke (VK_BACK, 0x08);
                rig.window.Press (VK_F10);
                WindowRig::AssertLines ({ stepOver }, rig.TakeSent(), dialect);
            }
        }


        TEST_METHOD (InTheGoToAndPokeBoxes_TheKeysAreNotGSSquareds)
        {
            for (Box box : { Box::GoTo, Box::Poke })
            {
                BoxRig           rig;
                DxuiTextInput  * target = nullptr;



                rig.ChooseDialect (L"GSSquared");
                (void) rig.TakeSent();
                target = rig.Focus (box);

                rig.window.Stroke (VK_SPACE, L' ');
                Assert::AreEqual (std::wstring (L" "), target->GetText(), BoxRig::GetName (box));
                Assert::IsTrue   (rig.TakeSent().empty());

                rig.window.Press (VK_F10);
                WindowRig::AssertLines ({ "o" }, rig.TakeSent(), L"the scheme's step over");
            }
        }


        //  The dialect is read from the snapshot, so before the first one the
        //  keys are not GSSquared's, whatever the session is in.
        TEST_METHOD (BeforeTheFirstSnapshot_TheKeysAreNotGSSquareds)
        {
            WindowRig  rig ("", false);



            (void) rig.GetSession().ExecuteLine ("MODE GSSQUARED", CommandMode::AppleWin);

            rig.window.Stroke (VK_SPACE, L' ');
            Assert::AreEqual (std::wstring (L" "), rig.window.GetCommandBox()->GetText());
            Assert::IsTrue   (rig.TakeSent().empty());
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  GoToBoxTests
    //
    //  Enter in the Go to box: a hex address moves the active memory window at
    //  once; a register, a name or an operand is resolved where the registers
    //  are, and the window moves when the answer arrives.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GoToBoxTests)
    {
    public:

        static void GoTo (BoxRig & rig, const std::wstring & text)
        {
            DxuiTextInput  * box = rig.Focus (Box::GoTo);



            while (!box->GetText().empty())
            {
                rig.window.Stroke (VK_BACK, 0x08);
            }

            rig.window.TypeText (text);
            rig.window.Stroke (VK_RETURN, L'\r');
        }


        TEST_METHOD (AHexAddressMovesTheWindowAtOnce_WithOrWithoutAPrefix)
        {
            for (const wchar_t * text : { L"0300", L"$0300", L"0x300", L" 300 ", L"300" })
            {
                BoxRig  rig;



                GoTo (rig, text);
                Assert::AreEqual ((Word) 0x0300, rig.window.GetMemoryPane (1)->GetTopAddress(), text);
                Assert::IsTrue   (rig.host.goTos.empty(), L"nothing to resolve");
                Assert::AreEqual (std::wstring (text), rig.window.GetMemoryBox()->GetText(), L"the box keeps its text");

                rig.window.Frame();
                Assert::AreEqual ((Word) 0x0300, rig.window.GetSnapshot()->memoryWindows[0].first, L"and the window's bytes are read from there");
                Assert::AreEqual (rig.Peek (0x0300), *rig.window.GetSnapshot()->memoryWindows[0].bytes[0]);
            }
        }


        //  Four hex digits are an address before they could be a name.
        TEST_METHOD (FourHexDigitsAreAnAddress)
        {
            BoxRig  rig;



            GoTo (rig, L"BEEF");
            Assert::AreEqual ((Word) 0xBEEF, rig.window.GetMemoryPane (1)->GetTopAddress());
            Assert::IsTrue   (rig.host.goTos.empty());
        }


        //  A register goes where the register points: A, X and Y on the zero
        //  page, S on the stack, PC to the code. A is the register, never the
        //  hex digit.
        TEST_METHOD (ARegisterGoesWhereItPoints)
        {
            struct Place
            {
                const wchar_t  * text;
                Word             address;
            };

            static constexpr Place  kPlaces[] =
            {
                { L"A",  0x0041 },
                { L"a",  0x0041 },
                { L"X",  0x0042 },
                { L"Y",  0x0043 },
                { L"SP", 0x01FF },
                { L"S",  0x01FF },
                { L"pc", 0x0300 },
            };



            for (const Place & place : kPlaces)
            {
                BoxRig  rig;



                rig.Submit (L"R A 41");
                rig.Submit (L"R X 42");
                rig.Submit (L"R Y 43");

                GoTo (rig, place.text);
                Assert::AreEqual ((size_t) 1, rig.host.goTos.size(), place.text);
                Assert::AreEqual (1, rig.host.goTos[0].first, L"for the active window");
                Assert::AreEqual (TextEncoding::WideToNarrow (place.text), rig.host.goTos[0].second);

                rig.window.Frame();
                Assert::AreEqual (place.address, rig.window.GetMemoryPane (1)->GetTopAddress(), place.text);
            }
        }


        TEST_METHOD (AnOperandIsResolvedAgainstTheRegistersAndMemory)
        {
            BoxRig  rig;



            rig.Submit (L"MEB 3E 00 04");
            rig.Submit (L"R Y 2");

            GoTo (rig, L"(3E),Y");
            rig.window.Frame();

            Assert::AreEqual ((Word) 0x0402, rig.window.GetMemoryPane (1)->GetTopAddress(), L"the pointer at $3E, plus Y");
        }


        TEST_METHOD (TextThatIsNothing_SaysSoAndMovesNothing)
        {
            BoxRig  rig;
            Word    before = rig.window.GetMemoryPane (1)->GetTopAddress();



            (void) rig.TakeConsole();
            GoTo (rig, L"KBD");
            rig.window.Frame();

            WindowRig::AssertLines ({ "Go to: \"KBD\" is not an address, a register or a 6502 operand." }, rig.TakeConsole(), L"why");
            Assert::AreEqual (before, rig.window.GetMemoryPane (1)->GetTopAddress());
        }


        TEST_METHOD (AnEmptyGoToBox_GoesNowhere)
        {
            BoxRig  rig;
            Word    before = rig.window.GetMemoryPane (1)->GetTopAddress();



            (void) rig.TakeConsole();
            GoTo (rig, L"");
            GoTo (rig, L"  ");
            rig.window.Frame();

            Assert::IsTrue   (rig.host.goTos.empty(), L"nothing to resolve");
            Assert::IsTrue   (rig.TakeConsole().empty(), L"and nothing to report");
            Assert::AreEqual (before, rig.window.GetMemoryPane (1)->GetTopAddress());
        }


        //  The active window is the one last clicked.
        TEST_METHOD (GoToMovesTheWindowLastClicked)
        {
            BoxRig  rig;



            rig.host.SetDebuggerMemoryWindow (2, 0x0800);
            rig.window.Frame();
            Assert::IsTrue (rig.window.GetMemoryPane (2)->GetView()->IsVisible(), L"window 2 is in front");

            rig.window.Click (rig.window.GetMemoryPane (2)->GetView());

            GoTo (rig, L"0300");
            Assert::AreEqual ((Word) 0x0300, rig.window.GetMemoryPane (2)->GetTopAddress(), L"window 2 moved");
            Assert::AreNotEqual ((Word) 0x0300, rig.window.GetMemoryPane (1)->GetTopAddress(), L"window 1 did not");
        }
    };





    ////////////////////////////////////////////////////////////////////////////
    //
    //  PokeBoxTests
    //
    //  Enter in the poke box: an address and a byte, split at a space, a comma
    //  or an equals sign, written with MEB in the session's dialect.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PokeBoxTests)
    {
    public:

        static void Poke (BoxRig & rig, const std::wstring & text)
        {
            DxuiTextInput  * box = rig.Focus (Box::Poke);



            while (!box->GetText().empty())
            {
                rig.window.Stroke (VK_BACK, 0x08);
            }

            rig.window.TypeText (text);
            rig.window.Stroke (VK_RETURN, L'\r');
        }


        TEST_METHOD (AnAddressAndAByte_WriteTheByte_WhateverSeparatesThem)
        {
            struct Entry
            {
                const wchar_t  * text;
                const char     * line;
                Word             address;
                Byte             value;
            };

            static constexpr Entry  kEntries[] =
            {
                { L"0300 A9",   "MEB 0300 A9", 0x0300, 0xA9 },
                { L"$301,$AA",  "MEB 0301 AA", 0x0301, 0xAA },
                { L"302=ab",    "MEB 0302 AB", 0x0302, 0xAB },
                { L"0x303 0",   "MEB 0303 00", 0x0303, 0x00 },
                { L"BFFF FF",   "MEB BFFF FF", 0xBFFF, 0xFF },
                { L"0 1",       "MEB 0000 01", 0x0000, 0x01 },
            };



            for (const Entry & entry : kEntries)
            {
                BoxRig  rig;



                Poke (rig, entry.text);

                WindowRig::AssertLines ({ entry.line }, rig.TakeSent(), entry.text);
                Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok, entry.text);
                Assert::AreEqual (entry.value, rig.Peek (entry.address), entry.text);
                Assert::IsTrue   (rig.window.GetPokeBox()->GetText().empty(), L"the box is cleared");
            }
        }


        TEST_METHOD (TextThatIsNotAnAddressAndAByte_IsLeftToCorrect)
        {
            for (const wchar_t * text : { L"0300 1FF", L"0300", L"XYZ 12", L"10000 12", L"0300 ", L"0300 G1" })
            {
                BoxRig  rig;



                Poke (rig, text);

                Assert::IsTrue   (rig.TakeSent().empty(), text);
                Assert::AreEqual (std::wstring (text), rig.window.GetPokeBox()->GetText(), L"left as typed");
            }
        }


        TEST_METHOD (ThePokeIsWrittenInTheDialectsWords)
        {
            static constexpr std::pair<const wchar_t *, const char *>  kLines[] =
            {
                { L"AppleWin",  "MEB 0300 A9"  },
                { L"Monitor",   "/MEB 0300 A9" },
                { L"GSSquared", "0300: A9"     },
                { L"WinDbg",    "eb 0300 A9"   },
                { L"Casso",     "MEB 0300 A9"  },
            };



            for (const auto & [dialect, line] : kLines)
            {
                BoxRig  rig;



                rig.ChooseDialect (dialect);
                (void) rig.TakeSent();

                Poke (rig, L"0300 A9");

                WindowRig::AssertLines ({ line }, rig.TakeSent(), dialect);
                Assert::IsTrue   (rig.host.replies.back().status == CommandStatus::Ok, dialect);
                Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x0300), dialect);
            }
        }
    };
}
