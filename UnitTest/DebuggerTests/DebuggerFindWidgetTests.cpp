#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFindWidgetTests
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
    //  DebuggerFindWidgetTests
    //
    //  The find widget is Visual Studio Code's: a small box floating at the
    //  top right of its pane's text, not a strip across it, with find in
    //  selection; and each pane keeps its own text, options and history.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerFindWidgetTests)
    {
    public:

        TEST_METHOD (TheWidgetFloatsAtTheTopRightOfThePanesText)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);
            RECT              text   = {};
            RECT              widget = {};



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);

            text   = window.GetConsoleView()->GetBounds();
            widget = window.GetFindWidgetBounds();

            Assert::AreEqual (text.top, widget.top,                                   L"at the top of the text");
            Assert::IsTrue   (widget.right <= text.right && widget.right > text.right - 40, L"at its right, clear of the scrollbar");
            Assert::IsTrue   (widget.left > text.left + (text.right - text.left) / 4, L"not across the pane");
            Assert::IsTrue   (window.GetFindBox()->GetBounds().right <= widget.right, L"the box is inside the widget");
            Assert::AreEqual (text.top, window.GetConsoleView()->GetBounds().top,     L"and the text keeps its place under it");
        }


        TEST_METHOD (EachPaneKeepsItsOwnFindTextAndHistory)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::GetSourcePaneId (0));
            window.Type  (L"sta");
            window.Press (VK_RETURN);

            window.OpenFindIn (DebuggerLayout::kConsole);
            Assert::AreEqual (std::wstring(), window.GetFindBox()->GetText(), L"the console's widget starts empty");

            window.Type  (L"lda");
            window.Press (VK_RETURN);

            //  A selection would become the text to find, so there is none.
            window.GetSourceView (0)->ClearSelection();
            window.OpenFindIn (DebuggerLayout::GetSourcePaneId (0));
            Assert::AreEqual (std::wstring (L"sta"), window.GetFindBox()->GetText(), L"the source keeps its own text");

            Assert::AreEqual ((size_t) 1, window.GetFindHistory (DebuggerLayout::GetSourcePaneId (0)).size());
            Assert::AreEqual (std::wstring (L"lda"), window.GetFindHistory (DebuggerLayout::kConsole).front(), L"and each its own history");
        }


        TEST_METHOD (UpAndDownStepThroughThePanesHistory)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);



            window.Build();
            window.OpenFindIn (DebuggerLayout::kConsole);

            for (const wchar_t * text : { L"lda", L"sta" })
            {
                window.GetFindBox()->SetText (text);
                window.Press (VK_RETURN);
            }

            window.GetFindBox()->SetText (L"");

            Assert::IsTrue   (window.Press (VK_UP));
            Assert::AreEqual (std::wstring (L"sta"), window.GetFindBox()->GetText(), L"Up: the newest");
            window.Press (VK_UP);
            Assert::AreEqual (std::wstring (L"lda"), window.GetFindBox()->GetText(), L"Up again: the one before");
            window.Press (VK_DOWN);
            window.Press (VK_DOWN);
            Assert::AreEqual (std::wstring(), window.GetFindBox()->GetText(), L"Down past the newest empties the box");
        }


        TEST_METHOD (FindInSelectionKeepsToTheSelection)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindWidgetHost    host;
            FindWidgetWindow  window (theme, host);



            window.Build();
            window.GetConsoleView()->Select ({ 1, 0 }, { 1, 9 });
            window.OpenFindIn (DebuggerLayout::kConsole);
            window.GetFindBox()->SetText (L"$");

            window.Press (VK_RETURN);
            Assert::AreEqual (std::wstring (L"1 of 2"), window.GetFindStatus(), L"the whole text has two");

            window.GetConsoleView()->Select ({ 1, 0 }, { 1, 9 });
            window.SetFindInSelection (true);
            window.Press (VK_RETURN);
            Assert::AreEqual (std::wstring (L"1 of 1"), window.GetFindStatus(), L"the selected line has one");
        }
    };
}
