#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextView.h"
#include "Widgets/DxuiToolbar.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuietDebuggerHost
    //
    //  A host that keeps nothing and answers every question with nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class QuietDebuggerHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { commands.push_back (line); }
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string & text)            override { goTos.push_back (text); }
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  goTos;
        std::vector<std::string>  commands;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TextSizeWindow
    //
    //  The debugger window with its controls built and no HWND: OnCreate makes
    //  every pane's control, docked or not, and a text-size change is applied
    //  as the keys apply it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TextSizeWindow : public DebuggerWindow
    {
    public:
        TextSizeWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::ApplyTextZoom;
        using DebuggerWindow::StepTextZoom;
        using DebuggerWindow::GetTextZoom;
        using DebuggerWindow::SubmitMemoryBox;
        using DebuggerWindow::GetMemoryBox;
        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::OnMappedCommand;
        using DebuggerWindow::RouteMappedKey;
        using DebuggerWindow::ApplyKeyScheme;
        using DebuggerWindow::AppendConsole;
        using DebuggerWindow::IsFindOpen;
        using DebuggerWindow::GetConsoleView;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::GetFindCaseBox;
        using DebuggerWindow::GetFindWordBox;
        using DebuggerWindow::GetFindStatus;
        using DebuggerWindow::BeginWatchEdit;
        using DebuggerWindow::EndWatchEdit;
        using DebuggerWindow::GetFocused;
        using DebuggerWindow::GetWatchList;
        using DebuggerWindow::GetPokeBox;
        using DebuggerWindow::GetMenuCommands;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetPaneOfControl;
        using DebuggerWindow::GetPaneOfFocus;
        using DebuggerWindow::GetMemoryButtons;
        using DebuggerWindow::GetMemoryMoreButton;
        using DebuggerWindow::GetMemoryOverflow;
        using DebuggerWindow::GetWatchEditor;

        //  A key as the window's message handling delivers it: to the window
        //  first, then to the key scheme if nothing took it.
        bool  Press (WPARAM vk, bool ctrl = false, bool shift = false)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, shift, ctrl, false };

            return OnKey (ev) || RouteMappedKey (ev);
        }

        void  Type (const std::wstring & text)
        {
            for (wchar_t ch : text)
            {
                DxuiKeyEvent  down  = { DxuiKeyEventKind::Down, (WPARAM) (ch == L' ' ? VK_SPACE : towupper (ch)), false, false, false, false };
                DxuiKeyEvent  typed = { DxuiKeyEventKind::Char, (WPARAM) ch, false, false, false, false };

                (void) (OnKey (down) || RouteMappedKey (down));
                (void) OnKey (typed);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowTextSizeTests
    //
    //  Ctrl+Plus, Ctrl+Minus and Ctrl+0 size the text of every content pane and
    //  nothing else. The controls are found by walking the window's children
    //  rather than through the list the size change itself uses, so a pane that
    //  list leaves out is caught. A floating pane is not floated here -- its
    //  window needs an HWND -- but it is the same control in another host, and
    //  the walk reaches it all the same.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowTextSizeTests)
    {
    public:

        TEST_METHOD (ATextSizeChangeReachesEveryContentPaneAndNothingElse)
        {
            static constexpr float  kZoom    = 1.5f;
            CassoTheme              theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost       host;
            TextSizeWindow          window   (theme, host);
            DxuiDpiScaler           scaler;
            DxuiToolbar           * bar      = nullptr;
            RECT                    barRect  = {};
            RECT                    barAfter = {};
            int                     lists    = 0;
            std::vector<float>      before;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            for (size_t i = 0; i < window.GetChildCount(); i++)
            {
                IDxuiControl  * child = window.GetChild (i);

                if (auto * list = dynamic_cast<DxuiListView *> (child))
                {
                    before.push_back (list->GetFontSizeDip());
                }
                else if (auto * toolbar = dynamic_cast<DxuiToolbar *> (child))
                {
                    bar     = toolbar;
                    barRect = toolbar->GetBounds();
                }
            }

            Assert::IsNotNull (bar, L"the command bar is one of the window's controls");

            window.ApplyTextZoom (kZoom);

            for (size_t i = 0; i < window.GetChildCount(); i++)
            {
                IDxuiControl  * child = window.GetChild (i);

                if (auto * list = dynamic_cast<DxuiListView *> (child))
                {
                    Assert::AreEqual (before[(size_t) lists] * kZoom, list->GetFontSizeDip(), 0.01f,
                                      std::format (L"list {} of the window's lists grew", lists).c_str());
                    lists++;
                }
                else if (auto * hex = dynamic_cast<DxuiHexView *> (child))
                {
                    Assert::AreEqual (kZoom, hex->GetZoom(), 0.001f, L"every memory window, open or not");
                }
                else if (auto * text = dynamic_cast<DxuiTextView *> (child))
                {
                    Assert::AreEqual (kZoom, text->GetZoom(), 0.001f, L"the source and the console");
                }
            }

            barAfter = bar->GetBounds();

            Assert::IsTrue (lists > 10,                                L"the code views, the panes and the device panels are all lists");
            Assert::IsTrue (EqualRect (&barRect, &barAfter) != FALSE, L"the command bar keeps its size");
        }





        TEST_METHOD (StepsBackFromALimitLandOnTheUsualSizes)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            for (int i = 0; i < 40; i++) { window.StepTextZoom (+1); }
            for (int i = 0; i < 20; i++) { window.StepTextZoom (-1); }

            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f, L"up past the largest size and back down");

            for (int i = 0; i < 20; i++) { window.StepTextZoom (-1); }
            for (int i = 0; i < 5;  i++) { window.StepTextZoom (+1); }

            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f, L"down past the smallest size and back up");
        }





        TEST_METHOD (EachStepIsTenPercentagePointsFromHalfToThreeTimes)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            window.StepTextZoom (+1);
            Assert::AreEqual (1.1f, window.GetTextZoom(), 0.001f, L"one step up");

            window.StepTextZoom (+1);
            Assert::AreEqual (1.2f, window.GetTextZoom(), 0.001f, L"two steps up add, not multiply");

            for (int i = 0; i < 40; i++) { window.StepTextZoom (+1); }

            Assert::AreEqual (3.0f, window.GetTextZoom(), 0.001f, L"the largest size");

            for (int i = 0; i < 40; i++) { window.StepTextZoom (-1); }

            Assert::AreEqual (0.5f, window.GetTextZoom(), 0.001f, L"the smallest size");

            window.StepTextZoom (+1);
            Assert::AreEqual (0.6f, window.GetTextZoom(), 0.001f, L"one step up from the smallest");
        }





        TEST_METHOD (CtrlPlusMinusAndZeroStepAndReset)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiKeyEvent        plus   = { DxuiKeyEventKind::Down, VK_OEM_PLUS,  false, false, true, false };
            DxuiKeyEvent        minus  = { DxuiKeyEventKind::Down, VK_OEM_MINUS, false, false, true, false };
            DxuiKeyEvent        zero   = { DxuiKeyEventKind::Down, '0',          false, false, true, false };



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            Assert::IsTrue (window.OnKey (plus));
            Assert::IsTrue (window.OnKey (plus));
            Assert::IsTrue (window.OnKey (plus));
            Assert::AreEqual (1.3f, window.GetTextZoom(), 0.001f, L"three steps up with Ctrl+Plus");

            Assert::IsTrue (window.OnKey (minus));
            Assert::AreEqual (1.2f, window.GetTextZoom(), 0.001f, L"one back with Ctrl+Minus");

            Assert::IsTrue (window.OnKey (zero));
            Assert::AreEqual (1.0f, window.GetTextZoom(), 0.001f, L"Ctrl+0 back to the usual size");
        }





        TEST_METHOD (GoToSendsRegisterAForResolutionAndIgnoresABlankBox)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            window.OnCreate();

            window.GetMemoryBox()->SetText (L"  ");
            window.SubmitMemoryBox();

            Assert::AreEqual ((size_t) 0, host.goTos.size(), L"a blank box goes nowhere");

            window.GetMemoryBox()->SetText (L"a");
            window.SubmitMemoryBox();

            Assert::AreEqual ((size_t) 1, host.goTos.size(), L"A is the register, resolved against the machine");
            Assert::AreEqual (std::string ("a"), host.goTos[0]);
        }





        static std::shared_ptr<const DebuggerViewSnapshot> MakeCodeSnapshot (Word first)
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();



            for (Word i = 0; i < 10; i++)
            {
                DebuggerViewSnapshot::CodeLine  line;



                line.address = (Word) (first + i);
                snapshot->codeViews[0].push_back (line);
            }

            return snapshot;
        }





        TEST_METHOD (TheSelectedCodeRowFollowsItsAddressAcrossNewRows)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            window.OnCreate();
            window.ApplyCodeSnapshot (MakeCodeSnapshot (0x0300), 0);
            window.GetCodeList (0)->ClickRow (5, false, false);

            window.ApplyCodeSnapshot (MakeCodeSnapshot (0x0302), 0);
            Assert::AreEqual (3, window.GetCodeList (0)->GetSelectedRow(), L"$0305 moved up two rows");

            window.ApplyCodeSnapshot (MakeCodeSnapshot (0x0400), 0);
            Assert::AreEqual (-1, window.GetCodeList (0)->GetSelectedRow(), L"$0305 is off the pane");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowFindTests
    //
    //  Find in the console, driven by the keys as the window receives them: the
    //  window first, then the key scheme. Typing a search goes to the find box
    //  and never to the scheme, even in one that steps on Space or a letter.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowFindTests)
    {
    public:

        static void  Build (TextSizeWindow & window, DebuggerKeyScheme scheme)
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.ApplyKeyScheme (scheme);
            window.AppendConsole ({ "LDA #$00", "STA $C030", "sta $0300", "ORA #$01" });
        }


        TEST_METHOD (CtrlFOpensTheBarAndTypingStaysInIt)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window, DebuggerKeyScheme::AppleWin);

            Assert::IsFalse  (window.IsFindOpen());
            Assert::IsTrue   (window.Press ('F', true), L"Ctrl+F");
            Assert::IsTrue   (window.IsFindOpen());
            Assert::IsTrue   (window.GetFindBox()->IsFocused(), L"the keys go to the find box");

            window.Type (L"sta ");

            Assert::AreEqual (std::wstring (L"sta "), window.GetFindBox()->GetText());
            Assert::AreEqual ((size_t) 0, host.commands.size(), L"AppleWin steps on Space, but not from the find box");

            Assert::IsTrue   (window.Press (VK_RETURN), L"Enter finds");
            Assert::AreEqual (std::wstring (L"STA "), window.GetConsoleView()->GetSelectionText(), L"case does not matter by default");
            Assert::AreEqual ((size_t) 0, host.commands.size(), L"and Enter runs nothing");
        }


        TEST_METHOD (F3AndShiftF3StepThroughTheMatchesAndGoRound)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window, DebuggerKeyScheme::VisualStudio);

            window.Press ('F', true);
            window.Type  (L"sta");

            Assert::IsTrue   (window.Press (VK_F3), L"F3 from the find box");
            Assert::AreEqual (std::wstring (L"STA"), window.GetConsoleView()->GetSelectionText());

            Assert::IsTrue   (window.Press (VK_F3));
            Assert::AreEqual (std::wstring (L"sta"), window.GetConsoleView()->GetSelectionText(), L"the next one, on the last line");
            Assert::AreEqual (std::wstring(),        window.GetFindStatus());

            Assert::IsTrue   (window.Press (VK_F3));
            Assert::AreEqual (std::wstring (L"STA"), window.GetConsoleView()->GetSelectionText(), L"round to the first again");
            Assert::AreEqual (std::wstring (L"Continued from the top"), window.GetFindStatus());

            Assert::IsTrue   (window.Press (VK_F3, false, true), L"Shift+F3");
            Assert::AreEqual (std::wstring (L"sta"), window.GetConsoleView()->GetSelectionText(), L"back round to the last");
            Assert::AreEqual (std::wstring (L"Continued from the bottom"), window.GetFindStatus());
        }


        TEST_METHOD (MatchCaseSkipsTheOtherCase)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window, DebuggerKeyScheme::GSSquared);

            window.Press ('F', true);
            window.Type  (L"sta");
            window.GetFindCaseBox()->SetChecked (true);

            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"sta"), window.GetConsoleView()->GetSelectionText(), L"only the lowercase line");

            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"Continued from the top"), window.GetFindStatus(), L"the only match, found again by going round");

            window.GetFindBox()->SetText (L"");
            window.Type  (L"ora");

            Assert::AreEqual (std::wstring (L"ora"), window.GetFindBox()->GetText());
            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"No matches"), window.GetFindStatus(), L"only ORA, in capitals");
            Assert::AreEqual ((size_t) 0, host.commands.size(), L"GSSquared steps on O and R, but not from the find box");
        }


        TEST_METHOD (MatchWholeWordSkipsPartOfALongerWord)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window, DebuggerKeyScheme::VisualStudio);

            window.Press ('F', true);
            window.Type  (L"c03");

            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"C03"), window.GetConsoleView()->GetSelectionText(), L"part of C030, while whole words are off");

            Assert::IsTrue   (window.GetFindWordBox()->IsVisible(), L"the whole-word box shows with the bar");
            window.GetFindWordBox()->SetChecked (true);

            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"No matches"), window.GetFindStatus(), L"C030 goes on past C03");

            window.Type  (L"0");

            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"C030"), window.GetConsoleView()->GetSelectionText(), L"the whole word, between $ and the line's end");
        }


        TEST_METHOD (NoMatchSaysSoAndEscapeCloses)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window, DebuggerKeyScheme::VisualStudio);

            window.OnMappedCommand (DebuggerCommands::kFind);
            Assert::IsTrue   (window.IsFindOpen(), L"the command bar's Find opens the bar as Ctrl+F does");

            window.Type  (L"jmp");

            Assert::IsTrue   (window.Press (VK_RETURN));
            Assert::AreEqual (std::wstring (L"No matches"), window.GetFindStatus());
            Assert::IsFalse  (window.GetConsoleView()->HasSelection());

            Assert::IsTrue   (window.Press (VK_ESCAPE));
            Assert::IsFalse  (window.IsFindOpen());
            Assert::IsFalse  (window.GetFindBox()->IsVisible(), L"the bar is hidden");
            Assert::IsFalse  (window.GetFindBox()->IsFocused(), L"and the keys go back to the command line");
        }


        TEST_METHOD (F3WithNothingToFindOpensTheBar)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window, DebuggerKeyScheme::AppleWin);

            Assert::IsTrue   (window.Press (VK_F3));
            Assert::IsTrue   (window.IsFindOpen());
            Assert::IsTrue   (window.GetFindBox()->IsFocused());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowBoxKeyTests
    //
    //  A key a text box keeps goes to the box and never to the key scheme,
    //  even when the box does nothing with the key-down itself.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowBoxKeyTests)
    {
    public:

        TEST_METHOD (SpaceInANonEmptyCommandLineTypesAndDoesNotStep)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            DebuggerWindowFindTests::Build (window, DebuggerKeyScheme::AppleWin);
            window.FocusControl (window.GetCommandBox());

            window.Type (L"a b");

            Assert::AreEqual (std::wstring (L"a b"), window.GetCommandBox()->GetText());
            Assert::AreEqual ((size_t) 0, host.commands.size(), L"AppleWin steps on Space only from an empty line");
        }


        TEST_METHOD (GSSquaredLettersTypedIntoABoxDoNotStep)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            DebuggerWindowFindTests::Build (window, DebuggerKeyScheme::GSSquared);
            window.FocusControl (window.GetMemoryBox());

            window.Type (L"or");

            Assert::AreEqual (std::wstring (L"or"), window.GetMemoryBox()->GetText());
            Assert::AreEqual ((size_t) 0, host.commands.size(), L"o and r step only outside a box");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowKeyTests
    //
    //  Keys that move focus, walk the command history and edit a watch, sent
    //  as the window's message handling sends them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowKeyTests)
    {
    public:

        static void  Build (TextSizeWindow & window)
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
        }


        static void  CollectShown (IDxuiControl * control, std::set<IDxuiControl *> & out)
        {
            if (control == nullptr || !control->IsVisible() || !control->IsEnabled())
            {
                return;
            }

            if (control->IsFocusable() && control->GetTabIndex() != IDxuiControl::kTabIndexExcluded)
            {
                out.insert (control);
            }

            for (size_t i = 0; i < control->GetChildCount(); i++)
            {
                CollectShown (control->GetChild (i), out);
            }
        }


        TEST_METHOD (TabReachesEveryShownControlAndNoHiddenOne)
        {
            CassoTheme                theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost         host;
            TextSizeWindow            window  (theme, host);
            std::set<IDxuiControl *>  shown;
            std::set<IDxuiControl *>  reached;



            Build (window);

            for (size_t i = 0; i < window.GetChildCount(); i++)
            {
                CollectShown (window.GetChild (i), shown);
            }

            for (size_t i = 0; i < shown.size() * 2; i++)
            {
                window.Press (VK_TAB);
                reached.insert (window.GetFocused());
            }

            Assert::IsTrue (shown.size() > 3, L"the window shows several controls Tab can reach");

            for (IDxuiControl * control : reached)
            {
                Assert::IsTrue (shown.contains (control), L"Tab never lands on a hidden control");
            }

            Assert::AreEqual (shown.size(), reached.size(), L"Tab reaches every shown control");
        }


        TEST_METHOD (DownAfterEditingARecalledLineKeepsTheEdit)
        {
            CassoTheme          theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window  (theme, host);
            DxuiTextInput     * box     = nullptr;



            Build (window);

            box = dynamic_cast<DxuiTextInput *> (window.GetFocused());
            Assert::IsNotNull (box, L"the command line has the keys");

            window.Type  (L"R");
            window.Press (VK_RETURN);
            window.Type  (L"T");
            window.Press (VK_RETURN);

            window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"T"), box->GetText());

            window.Type  (L"X");
            window.Press (VK_DOWN);
            Assert::AreEqual (std::wstring (L"TX"), box->GetText(), L"Down keeps an edited line");

            window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"T"), box->GetText(), L"Up walks back from the most recent line");

            window.Press (VK_DOWN);
            Assert::AreEqual (std::wstring (L"TX"), box->GetText(), L"and the edit comes back at the end of the walk");
        }


        TEST_METHOD (AnAutomaticWatchEditGoesToTheWatchItBeganOn)
        {
            CassoTheme                              theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                       host;
            TextSizeWindow                          window (theme, host);
            auto                                    before = std::make_shared<DebuggerViewSnapshot>();
            auto                                    after  = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::AutoWatchLine     reg;
            DebuggerViewSnapshot::AutoWatchLine     mem;



            reg.key   = "R:A";
            reg.label = "A";
            reg.value = "00";
            mem.key   = "M:0010";
            mem.label = "$0010";
            mem.value = "00";

            before->autoWatches = { reg, mem };
            after->autoWatches  = { mem, reg };

            Build (window);
            window.TakeSnapshot (before);

            //  Row 0 is the heading; row 1 is register A.
            window.BeginWatchEdit (1, 1);
            window.TakeSnapshot (after);
            window.Type (L"42");
            window.EndWatchEdit (true);

            Assert::AreEqual ((size_t) 1, host.commands.size(), L"one line runs");
            Assert::AreEqual (std::string ("R A 42"), host.commands.front(), L"the value goes to A, where the edit began");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowFocusTests
    //
    //  Where the keys go: to the control the window's focus is on, and to no
    //  control that is hidden or belongs to another window.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowFocusTests)
    {
    public:

        static void  Build (TextSizeWindow & window)
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);
        }


        static std::shared_ptr<const DebuggerViewSnapshot> MakeSnapshot (const std::string & machine, bool memoryOpen)
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();

            snapshot->machine  = machine;
            snapshot->isPaused = true;
            snapshot->watches  = { { 1, 0x0300, "12" } };

            if (memoryOpen)
            {
                DebuggerViewSnapshot::MemoryWindow  window;

                window.id = 1;
                snapshot->memoryWindows.push_back (window);
            }

            return snapshot;
        }


        static void  EditWatch (TextSizeWindow & window)
        {
            window.FocusControl (window.GetWatchList());
            window.GetWatchList()->ClickRow (0, false, false);
            window.Press (VK_F2);
            window.Type  (L"34");
            window.Press (VK_RETURN);
        }


        TEST_METHOD (WatchUndoIsDroppedWithTheMachine)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            size_t              sent   = 0;



            Build (window);
            window.TakeSnapshot (MakeSnapshot ("Apple2e", true));

            EditWatch (window);
            Assert::AreEqual ((size_t) 1, host.commands.size(), L"the edit writes the watch");

            window.TakeSnapshot (MakeSnapshot ("Apple2e", true));
            window.FocusControl (window.GetWatchList());
            Assert::IsTrue   (window.Press ('Z', true));
            Assert::AreEqual ((size_t) 2, host.commands.size(), L"on the same machine Ctrl+Z puts it back");

            EditWatch (window);
            sent = host.commands.size();

            window.TakeSnapshot (MakeSnapshot ("Apple2Plus", true));
            window.FocusControl (window.GetWatchList());
            window.Press ('Z', true);
            Assert::AreEqual (sent, host.commands.size(), L"another machine's value is not written into this one");
        }


        TEST_METHOD (CtrlASelectsAllOfAFocusedTextView)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window);
            window.AppendConsole ({ "LDA #$00", "STA $C030" });
            window.FocusControl  (window.GetConsoleView());

            Assert::IsTrue   (window.OnKey (DxuiKeyEvent { DxuiKeyEventKind::Down, 'A', false, false, true, false }), L"Ctrl+A");
            Assert::IsTrue   (window.GetConsoleView()->HasSelection(), L"the whole text is selected, as its Edit menu does");
        }


        TEST_METHOD (TypingGoesOnlyToTheFocusedBox)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window);
            window.TakeSnapshot (MakeSnapshot ("Apple2e", true));

            //  The Go to box is focused in a floating window, as SetFocusedControl
            //  leaves it, and a click then focuses a list in the main window.
            window.FocusControl (window.GetWatchList());
            window.GetMemoryBox()->OnFocusChanged (true);

            window.Type (L"12");

            Assert::AreEqual (std::wstring(), window.GetMemoryBox()->GetText(), L"the keys go to the focused list, not the box");
        }


        TEST_METHOD (AHiddenMemoryBarGivesUpTheFocus)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;



            Build (window);
            window.TakeSnapshot (MakeSnapshot ("Apple2e", true));
            scaler.SetDpi (96);
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            window.FocusControl (window.GetPokeBox());
            Assert::IsTrue   (window.GetPokeBox()->IsVisible(), L"the poke box shows in the memory window's bar");

            window.TakeSnapshot (MakeSnapshot ("Apple2e", false));
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            Assert::IsFalse  (window.GetPokeBox()->IsVisible(), L"no memory window, no bar");
            Assert::IsTrue   (window.GetFocused() == window.GetCommandBox(), L"the keys go back to the command line");

            window.Type (L"12");
            Assert::AreEqual (std::wstring(), window.GetPokeBox()->GetText(), L"and nothing is typed into the hidden box");
        }


        //  A disassembly view is docked as a frame over its lines, so the
        //  focused list is not itself what the dock site places.
        TEST_METHOD (AFocusedListInsideAFrameMarksItsPane)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            Build (window);
            window.FocusControl (window.GetCodeList (0));

            Assert::AreEqual (DebuggerLayout::GetCodePaneId (0), window.GetPaneOfFocus(), L"the disassembly pane has the focus border");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowStateTests
    //
    //  What the window keeps across a scheme change, a closed view and a
    //  floated pane.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowStateTests)
    {
    public:

        TEST_METHOD (TheKeysDropDownChecksTheSchemeInForceFromTheStart)
        {
            CassoTheme          theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window  (theme, host);
            std::wstring        appleWin = DebuggerKeySchemes::GetMap (DebuggerKeyScheme::AppleWin).GetName();
            std::wstring        vs       = DebuggerKeySchemes::GetMap (DebuggerKeyScheme::VisualStudio).GetName();
            std::optional<bool> appleWinChecked;
            std::optional<bool> vsChecked;



            //  As Create does: the controls first, then the saved scheme.
            window.OnCreate();
            window.ApplyKeyScheme (DebuggerKeyScheme::AppleWin);

            //  The Keys rows come after the dialect rows, which share the
            //  AppleWin and GSSquared labels, so the last row of each wins.
            for (const std::shared_ptr<DxuiCommand> & command : window.GetMenuCommands())
            {
                if (command->label == appleWin)
                {
                    appleWinChecked = command->IsChecked();
                }
                else if (command->label == vs)
                {
                    vsChecked = command->IsChecked();
                }
            }

            Assert::IsTrue  (appleWinChecked.has_value() && vsChecked.has_value());
            Assert::IsTrue  (*appleWinChecked, L"the saved scheme is the checked one");
            Assert::IsFalse (*vsChecked,       L"the default is not checked when another is in force");
        }


        TEST_METHOD (F9AfterASecondViewClosesActsOnTheFirstView)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;
            auto                both   = std::make_shared<DebuggerViewSnapshot>();
            auto                first  = std::make_shared<DebuggerViewSnapshot>();



            both->pc = 0x0500;

            for (Word i = 0; i < 10; i++)
            {
                DebuggerViewSnapshot::CodeLine  line;

                line.address = (Word) (0x0300 + i);
                both->codeViews[0].push_back (line);

                line.address = (Word) (0x0400 + i);
                both->codeViews[1].push_back (line);
            }

            both->codeOpen[0] = true;
            both->codeOpen[1] = true;

            *first             = *both;
            first->codeOpen[1] = false;
            first->codeViews[1].clear();

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);

            window.TakeSnapshot (both);
            window.GetCodeList (0)->ClickRow (3, false, false);
            window.GetCodeList (1)->ClickRow (2, false, false);

            window.TakeSnapshot (first);
            host.commands.clear();

            Assert::IsTrue   (window.Press (VK_F9));
            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands[0].find ("0303") != std::string::npos, L"the first view's selected row, not the PC");
        }


        TEST_METHOD (TheWatchEditorGoesWithTheWatchPane)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);



            window.OnCreate();

            //  A floated pane takes its controls with it; the editor opens
            //  over the watch list, so it has to be one of them.
            Assert::AreEqual (std::wstring (DebuggerLayout::kWatches), window.GetPaneOfControl (window.GetWatchEditor()));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowPaneBoundsTests
    //
    //  A pane's controls stay inside the pane: each paints inside a clip of
    //  the pane's bounds, and a bar button with no room goes into the bar's
    //  overflow menu rather than past the pane's edge.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowPaneBoundsTests)
    {
    public:

        TEST_METHOD (MemoryButtonsWithNoRoomMoveToTheOverflowMenu)
        {
            static constexpr LONG  kWidths[] = { 760, 900, 1100, 1400, 1800 };

            CassoTheme          theme       = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window      (theme, host);
            DxuiDpiScaler       scaler;
            bool                overflowed  = false;



            DebuggerWindowFocusTests::Build (window);
            window.TakeSnapshot (DebuggerWindowFocusTests::MakeSnapshot ("Apple2e", true));
            scaler.SetDpi (96);

            for (LONG width : kWidths)
            {
                RECT    bar    = {};
                size_t  shown  = 0;

                window.Layout (RECT { 0, 0, width, 840 }, scaler);

                Assert::IsTrue (window.TryGetChildClip (window.GetMemoryBox(), bar), L"the memory bar's controls are clipped to the bar");

                for (DxuiButton * button : window.GetMemoryButtons())
                {
                    if (button->IsVisible())
                    {
                        shown++;
                        Assert::IsTrue (button->GetBounds().right <= bar.right, std::format (L"a shown button stays inside the pane at width {}", width).c_str());
                    }
                }

                Assert::AreEqual (window.GetMemoryButtons().size(), shown + window.GetMemoryOverflow().size(), L"every button is shown or in the overflow menu");
                Assert::AreEqual (!window.GetMemoryOverflow().empty(), window.GetMemoryMoreButton()->IsVisible(), L"the overflow button shows only with something in it");

                if (window.GetMemoryMoreButton()->IsVisible())
                {
                    overflowed = true;
                    Assert::IsTrue (window.GetMemoryMoreButton()->GetBounds().right <= bar.right, L"the overflow button stays inside the pane");
                }
            }

            Assert::IsTrue (overflowed, L"some width leaves too little room for every button");
        }


        TEST_METHOD (APaneControlPaintsInsideItsPane)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            RECT                clip   = {};
            RECT                list   = {};



            DebuggerWindowFocusTests::Build (window);

            Assert::IsTrue (window.TryGetChildClip (window.GetCodeList (0), clip), L"the disassembly list has a clip");

            list = window.GetCodeList (0)->GetBounds();

            Assert::IsTrue (clip.left <= list.left && clip.top <= list.top && list.right <= clip.right && list.bottom <= clip.bottom,
                            L"the clip is its pane, which holds the list");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowConsoleScrollTests
    //
    //  The console follows its output while at the bottom, stays where the
    //  user scrolled it, and follows again once scrolled back to the bottom.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowConsoleScrollTests)
    {
    public:

        static std::vector<std::string>  MakeLines (int count)
        {
            std::vector<std::string>  lines;

            for (int i = 0; i < count; i++)
            {
                lines.push_back (std::format ("line {}", i));
            }

            return lines;
        }


        static bool  IsAtBottom (DxuiTextView * view)
        {
            return view->GetTopLine() + view->GetLineCap() >= view->GetLineCount();
        }


        TEST_METHOD (OutputKeepsAScrolledConsoleWhereItWas)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            TextSizeWindow      window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiTextView      * view   = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            view = window.GetConsoleView();
            view->SetCellSize (8, 16);
            view->Layout (RECT { 0, 0, 400, 300 }, scaler);
            window.AppendConsole (MakeLines (200));

            Assert::IsTrue   (IsAtBottom (view), L"follows the output");
            Assert::IsTrue   (view->GetTopLine() > 10, L"the output overflows the view");

            view->SetTopLine (10);
            window.AppendConsole ({ "SRC OFF" });
            Assert::AreEqual (10, view->GetTopLine(), L"stays where scrolled");

            view->SetTopLine (view->GetLineCount());
            window.AppendConsole ({ "more" });
            Assert::IsTrue   (IsAtBottom (view), L"follows again from the bottom");
        }
    };
}
