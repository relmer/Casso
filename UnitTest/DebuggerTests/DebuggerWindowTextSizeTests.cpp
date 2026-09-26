#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
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
        using DebuggerWindow::OnMappedCommand;
        using DebuggerWindow::RouteMappedKey;
        using DebuggerWindow::ApplyKeyScheme;
        using DebuggerWindow::AppendConsole;
        using DebuggerWindow::IsFindOpen;
        using DebuggerWindow::GetConsoleView;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::GetFindCaseBox;
        using DebuggerWindow::GetFindStatus;

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
}
