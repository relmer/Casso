#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFindReviewTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FindWidgetHost
    //
    //  A host that keeps the commands it was asked to run.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FindWidgetHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { commands.push_back (line); }
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

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode) override { commands.push_back (line); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  commands;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FindWidgetWindow
    //
    //  The debugger window with its controls built and no HWND, with text in
    //  the console and in the first source document.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FindWidgetWindow : public DebuggerWindow
    {
    public:
        FindWidgetWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::GetConsoleView;
        using DebuggerWindow::GetFindBox;
        using DebuggerWindow::GetFindPane;
        using DebuggerWindow::GetFocused;
        using DebuggerWindow::GetSourceView;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::IsFindOpen;
        using DebuggerWindow::GetFindHistory;
        using DebuggerWindow::GetFindStatus;
        using DebuggerWindow::GetFindWidgetBounds;
        using DebuggerWindow::SetFindInSelection;
        using DebuggerWindow::OpenFindIn;
        using DebuggerWindow::SetFindOptions;
        using DebuggerWindow::AppendConsole;

        void  Build()
        {
            DxuiDpiScaler                   scaler;
            std::vector<DxuiTextView::Row>  rows;



            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1100, 840 }, scaler);
            ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);
            AppendConsole ({ "LDA #$00", "STA $C030" });

            for (const wchar_t * line : { L"START   LDA #$01", L"        sta $0300", L"        RTS" })
            {
                DxuiTextView::Row  row;

                row.cells = { line };
                rows.push_back (std::move (row));
            }

            GetSourceView (0)->SetRows (std::move (rows));
        }

        //  A key as the window's message handling delivers it: to the window
        //  first, then to the key scheme if nothing took it.
        bool  Press (WPARAM vk, bool ctrl = false)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, false, ctrl, false };

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
    //  DebuggerFindReviewTests
    //
    //  The find widget's marks take the PC row's colors, with the PC arrow's
    //  yellow on the match the search is at; a changed option searches again
    //  at once; and find in selection keeps to the selection the user made,
    //  not to the match the search selected while the text was typed.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerFindReviewTests)
    {
    public:

        TEST_METHOD (MatchesTakeThePcRowColorAndTheCurrentOneThePcArrowYellow)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);
            DxuiTextView    * view   = nullptr;
            bool              dark   = false;
            uint32_t          row    = 0;
            uint32_t          arrow  = 0;



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.Type (L"$");

            view  = window.GetConsoleView();
            dark  = DebuggerTextColors::IsDark (theme.ContentBackground());
            row   = DebuggerTextColors::Make (theme.ContentBackground(), theme.Foreground(), theme.ForegroundMuted(), theme.resultText, theme.Accent()).pcRow;
            arrow = dark ? 0xFFFFE34Du : 0xFFA88300u;

            Assert::AreEqual ((size_t) 2, view->GetFindHighlights().size());
            Assert::AreEqual (arrow, view->GetFindFillOf (view->GetFindHighlights()[0], theme), L"the match the search is at: the PC arrow's yellow");
            Assert::AreEqual (row,   view->GetFindFillOf (view->GetFindHighlights()[1], theme), L"every other match: the PC row's color");

            window.Press (VK_RETURN);

            Assert::AreEqual (row,   view->GetFindFillOf (view->GetFindHighlights()[0], theme), L"the yellow leaves the first");
            Assert::AreEqual (arrow, view->GetFindFillOf (view->GetFindHighlights()[1], theme), L"and moves to the next");
        }


        TEST_METHOD (ChangingAnOptionSearchesAgain)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::GetSourcePaneId (0));
            window.Type (L"sta");
            Assert::AreEqual (std::wstring (L"1 of 2"), window.GetFindStatus(), L"START and sta");

            window.SetFindOptions (false, true, false);
            Assert::AreEqual (std::wstring (L"1 of 1"), window.GetFindStatus(), L"whole word: sta alone, found at once");
            Assert::AreEqual ((size_t) 1, window.GetSourceView (0)->GetFindHighlights().size());

            window.SetFindOptions (true, false, false);
            Assert::AreEqual (std::wstring (L"1 of 1"), window.GetFindStatus(), L"match case: sta alone");
        }


        TEST_METHOD (FindInSelectionKeepsToTheUsersSelection)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);



            window.Build();
            window.AppendConsole ({ "LDA $C000", "LDA $01" });

            //  The rows after the first: STA, LDA, LDA.
            window.GetConsoleView()->Select ({ 1, 0 }, { 3, 7 });
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.Type (L"lda");
            Assert::AreEqual (std::wstring (L"1 of 3"), window.GetFindStatus(), L"the whole text has three");

            //  Typing selected a match, but the selection taken is the user's.
            window.SetFindInSelection (true);
            Assert::AreEqual (std::wstring (L"1 of 2"), window.GetFindStatus(), L"the selected rows have two, searched at once");
            Assert::AreEqual ((size_t) 2, window.GetConsoleView()->GetFindHighlights().size());
        }
    };
}