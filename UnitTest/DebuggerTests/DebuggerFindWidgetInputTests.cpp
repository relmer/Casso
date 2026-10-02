#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFindWidgetInputTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InputHost
    //
    ////////////////////////////////////////////////////////////////////////////////

    class InputHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  InputWindow
    //
    //  The debugger window with no HWND, driven by the pointer and the keys
    //  as its message handling drives it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class InputWindow : public DebuggerWindow
    {
    public:
        InputWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::GetConsoleView;
        using DebuggerWindow::GetCursorForPoint;
        using DebuggerWindow::GetFindBarTip;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::GetFindControls;
        using DebuggerWindow::GetFindStatus;
        using DebuggerWindow::GetFindWidgetBounds;
        using DebuggerWindow::AppendConsole;
        using DebuggerWindow::OpenFindIn;

        void  Build()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
            ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);
            AppendConsole ({ "LDA #$00", "STA $C030" });
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

        void  Press (WPARAM vk)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, false, false, false };

            (void) (OnKey (ev) || RouteMappedKey (ev));
        }

        void  Click (const IDxuiControl * control)
        {
            DxuiMouseEvent  ev = {};

            ev.positionDip = GetCenter (control);
            ev.button      = DxuiMouseButton::Left;
            ev.kind        = DxuiMouseEventKind::Down;
            (void) OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            (void) OnMouse (ev);
        }

        static POINT  GetCenter (const IDxuiControl * control)
        {
            RECT  rc = control->GetBounds();

            return POINT { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
        }

        //  In GetFindControls' order: the box, the three toggles, previous,
        //  next, find in selection and Close.
        IDxuiControl *  GetFindControl (size_t i) const { return GetFindControls()[i]; }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerFindWidgetInputTests
    //
    //  The find widget answers the pointer as its buttons say it will, and
    //  searches as the text is typed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerFindWidgetInputTests)
    {
    public:

        TEST_METHOD (ClickingNextFindsTheNextMatch)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            InputHost    host;
            InputWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.GetFindBox()->SetText (L"$");

            window.Click (window.GetFindControl (5));
            Assert::AreEqual (std::wstring (L"1 of 2"), window.GetFindStatus(), L"the first press finds the first");

            window.Click (window.GetFindControl (5));
            Assert::AreEqual (std::wstring (L"2 of 2"), window.GetFindStatus(), L"the next press the second");
        }


        TEST_METHOD (ClickingAToggleInsideTheBoxTurnsItOn)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            InputHost    host;
            InputWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.GetFindBox()->SetText (L"lda");

            window.Click (window.GetFindControl (1));
            window.Press (VK_RETURN);

            Assert::AreEqual (std::wstring (L"No results"), window.GetFindStatus(), L"match case is on, so LDA is not lda");
        }


        TEST_METHOD (EachButtonHasItsOwnTip)
        {
            CassoTheme             theme  = CassoTheme::MakeSkeuomorphic();
            InputHost              host;
            InputWindow            window (theme, host);
            RECT                   anchor = {};
            std::set<std::wstring> seen;



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);

            for (size_t i = 0; i < window.GetFindControls().size(); i++)
            {
                const wchar_t * tip = window.GetFindBarTip (InputWindow::GetCenter (window.GetFindControl (i)), anchor);

                Assert::IsNotNull (tip);
                Assert::IsTrue    (seen.insert (tip).second, (std::wstring (L"a tip of its own: ") + tip).c_str());
            }
        }


        TEST_METHOD (TheButtonsShowTheArrowAndTheBoxTheIBeam)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            InputHost    host;
            InputWindow  window (theme, host);
            RECT         box    = {};



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            box = window.GetFindBox()->GetBounds();

            for (size_t i = 1; i < window.GetFindControls().size(); i++)
            {
                Assert::IsTrue (window.GetCursorForPoint (InputWindow::GetCenter (window.GetFindControl (i))) == IDC_ARROW, L"the arrow over a button");
            }

            Assert::IsTrue (window.GetCursorForPoint (POINT { box.left + 6, (box.top + box.bottom) / 2 }) == IDC_IBEAM, L"the I-beam over the text");
        }


        TEST_METHOD (TheButtonsAreFlatAsAToolbarsAre)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            InputHost    host;
            InputWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);

            for (size_t i = 1; i < window.GetFindControls().size(); i++)
            {
                const DxuiButton * button = dynamic_cast<const DxuiButton *> (window.GetFindControl (i));

                Assert::IsNotNull (button);
                Assert::IsTrue    (button->GetVariant() == DxuiButton::Variant::Toolbar);
            }
        }


        TEST_METHOD (TheCountSitsBetweenTheBoxAndTheArrows)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            InputHost    host;
            InputWindow  window (theme, host);
            RECT         box    = {};
            RECT         prev   = {};
            RECT         widget = {};



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            box    = window.GetFindControl (0)->GetBounds();
            prev   = window.GetFindControl (4)->GetBounds();
            widget = window.GetFindWidgetBounds();

            Assert::IsTrue (box.right < prev.left,                       L"the box ends before the arrows");
            Assert::IsTrue (prev.left - box.right >= 40,                 L"with room for the count between them");
            Assert::IsTrue (box.left >= widget.left && prev.right <= widget.right);
        }


        TEST_METHOD (TypingSearchesAndHighlightsEveryMatch)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            InputHost    host;
            InputWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.Type (L"$");

            Assert::AreEqual (std::wstring (L"1 of 2"), window.GetFindStatus(),                 L"found with no Return");
            Assert::AreEqual ((size_t) 2, window.GetConsoleView()->GetFindHighlights().size(), L"and both highlighted");

            window.Press (VK_BACK);
            Assert::AreEqual ((size_t) 0, window.GetConsoleView()->GetFindHighlights().size(), L"an empty box highlights nothing");
        }


        TEST_METHOD (WithNoMatchOnScreenTheFirstIsScrolledIntoView)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            InputHost        host;
            InputWindow      window (theme, host);
            DxuiTextView              * view  = nullptr;
            std::vector<std::string>    lines;



            window.Build();
            view = window.GetConsoleView();
            view->SetCellSize (8, 16);

            for (int i = 0; i < 400; i++)
            {
                lines.push_back ((i == 300) ? "zzz" : "nop");
            }

            window.AppendConsole (lines);
            window.OpenFindIn (DebuggerLayout::kConsole);
            view->ScrollLines (-1000);
            Assert::AreEqual (0, view->GetTopLine());

            window.Type (L"zzz");

            Assert::AreEqual (std::wstring (L"1 of 1"), window.GetFindStatus());
            Assert::IsTrue   (view->GetTopLine() > 250, L"scrolled down to the match");
            Assert::AreEqual (std::wstring (L"zzz"), view->GetSelectionText(), L"and the match selected");
        }


        TEST_METHOD (TheHighlightStandsOutInEveryTheme)
        {
            CassoTheme      skeuo  = CassoTheme::MakeSkeuomorphic();
            CassoTheme      modern = CassoTheme::MakeDarkModern();
            CassoTheme      retro  = CassoTheme::MakeRetroTerminal();
            DxuiLightTheme  light;
            DxuiDarkTheme   dark;



            for (const IDxuiTheme * theme : std::initializer_list<const IDxuiTheme *> { &skeuo, &modern, &retro, &light, &dark })
            {
                uint32_t  fill = DxuiTextView::GetFindHighlightFill (*theme);
                uint32_t  ink  = DxuiTextView::GetFindHighlightText (*theme);

                Assert::IsTrue (DxuiColor::ComputeContrastRatio (fill, theme->ContentBackground() | 0xFF000000u) >= 3.0f, L"the fill against the background");
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (ink, fill) >= 4.5f,                                    L"the text against the fill");
            }
        }
    };
}
