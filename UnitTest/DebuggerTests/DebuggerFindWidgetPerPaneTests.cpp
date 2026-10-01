#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFindWidgetPerPaneTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PaneFindHost
    //
    //  A host that keeps the commands it was asked to run.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneFindHost : public IDebuggerWindowHost
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
    //  PaneFindWindow
    //
    //  The debugger window with its controls built and no HWND, with text in
    //  the console and in the first source document.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PaneFindWindow : public DebuggerWindow
    {
    public:
        PaneFindWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
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
        using DebuggerWindow::IsFindOpenIn;
        using DebuggerWindow::GetFindBoxOf;

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
    //  DebuggerFindWidgetPerPaneTests
    //
    //  Each pane's find widget is its own: opening find in the console leaves
    //  a source document's open, each with its own box and text.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerFindWidgetPerPaneTests)
    {
    public:

        TEST_METHOD (TwoPanesKeepTheirFindWidgetsOpenAtOnce)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            PaneFindHost    host;
            PaneFindWindow  window (theme, host);
            std::wstring    source = DebuggerLayout::GetSourcePaneId (0);



            window.Build();
            window.FocusControl (window.GetSourceView (0));
            window.Press ('F', true);
            window.Type  (L"rts");

            window.FocusControl (window.GetCommandBox());
            window.Press ('F', true);
            window.Type  (L"sta");

            Assert::IsTrue   (window.IsFindOpenIn (DebuggerLayout::kConsole), L"the console's widget is open");
            Assert::IsTrue   (window.IsFindOpenIn (source), L"and the source document's stays open");
            Assert::IsTrue   (window.GetFindBoxOf (source) != window.GetFindBoxOf (DebuggerLayout::kConsole), L"each has its own box");
            Assert::AreEqual (std::wstring (L"rts"), window.GetFindBoxOf (source)->GetText(), L"with its own text");
            Assert::AreEqual (std::wstring (L"sta"), window.GetFindBoxOf (DebuggerLayout::kConsole)->GetText());

            Assert::IsTrue   (window.Press (VK_ESCAPE), L"Escape in the console's widget");
            Assert::IsFalse  (window.IsFindOpenIn (DebuggerLayout::kConsole), L"closes the console's");
            Assert::IsTrue   (window.IsFindOpenIn (source), L"and leaves the source document's open");
        }


        TEST_METHOD (KeysInASourceWidgetSearchThatDocumentWhileTheConsolesIsOpen)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            PaneFindHost    host;
            PaneFindWindow  window (theme, host);
            std::wstring    source = DebuggerLayout::GetSourcePaneId (0);



            window.Build();
            window.FocusControl (window.GetSourceView (0));
            window.Press ('F', true);

            window.FocusControl (window.GetCommandBox());
            window.Press ('F', true);

            //  Back to the source document's box, as a click on it does.
            window.FocusControl (window.GetFindBoxOf (source));
            window.Type  (L"sta");

            Assert::IsTrue   (window.Press (VK_RETURN), L"Enter finds");
            Assert::AreEqual (std::wstring (L"sta"), window.GetFindBoxOf (source)->GetText(), L"the text went to the box with the keys");
            Assert::AreEqual (0, _wcsicmp (L"sta", window.GetSourceView (0)->GetSelectionText().c_str()), L"and the document is searched");
            Assert::IsFalse  (window.GetConsoleView()->HasSelection(), L"not the console");
        }
    };
}