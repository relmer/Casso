#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiListView.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerValueEditTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ValueEditHost
    //
    //  A host that keeps the echo of every action the window runs.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ValueEditHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
        void  RunDebuggerAction       (const DebuggerAction & action)   override { echoes.push_back (action.echo); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  echoes;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ValueEditWindow
    //
    //  The debugger window with its controls built and no HWND, driven by keys
    //  as the window's message handling delivers them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ValueEditWindow : public DebuggerWindow
    {
    public:
        ValueEditWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::GetStackList;
        using DebuggerWindow::GetRegisterList;
        using DebuggerWindow::GetStackEditor;
        using DebuggerWindow::GetRegisterEditor;
        using DebuggerWindow::GetModeEntry;
        using DebuggerWindow::SetConsoleBarMenus;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::GetFocused;

        void  Press (WPARAM vk)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, false, false, false };

            (void) OnKey (ev);
        }

        void  Type (const std::wstring & text)
        {
            for (wchar_t ch : text)
            {
                DxuiKeyEvent  down  = { DxuiKeyEventKind::Down, (WPARAM) towupper (ch), false, false, false, false };
                DxuiKeyEvent  typed = { DxuiKeyEventKind::Char, (WPARAM) ch, false, false, false, false };

                (void) OnKey (down);
                (void) OnKey (typed);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ValueEditRig
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ValueEditRig
    {
    public:
        static constexpr int  kColumnPx = 80;


        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshot (bool paused)
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();

            snapshot->isPaused  = paused;
            snapshot->pc        = 0x0300;
            snapshot->registers = { { "A", "12" }, { "X", "34" }, { "Y", "56" }, { "P", "30" }, { "S", "FD" }, { "PC", "0300" } };
            snapshot->stack     = { { 0x01FE, 0x11 }, { 0x01FF, 0x22 } };

            return snapshot;
        }


        static void  Build (ValueEditWindow & window, bool paused)
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            window.OnCreate();
            window.ShowPane (DebuggerLayout::kStack);
            window.ShowPane (DebuggerLayout::kRegisters);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeSnapshot (paused));

            //  No paint measures the columns here, so give them widths.
            for (DxuiListView * list : { window.GetStackList(), window.GetRegisterList() })
            {
                for (size_t column = 0; column < list->GetColumnCount(); column++)
                {
                    list->SetColumnOverrideWidthPx (column, kColumnPx);
                }
            }

            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerStackValueEditTests
    //
    //  A stack byte's value edits in place, in a box over its cell, rather
    //  than in a prompt.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerStackValueEditTests)
    {
    public:

        TEST_METHOD (F2EditsTheStackByteInPlace)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window (theme, host);



            ValueEditRig::Build (window, true);
            window.FocusControl (window.GetStackList());
            window.GetStackList()->SetSelectedRow (0);
            window.Press (VK_F2);

            Assert::IsTrue   (window.GetStackEditor()->IsVisible(), L"the box opens over the value");
            Assert::AreEqual (std::wstring (L"22"), window.GetStackEditor()->GetText(), L"holding the newest byte");

            window.Type  (L"42");
            window.Press (VK_RETURN);

            Assert::IsFalse  (window.GetStackEditor()->IsVisible(), L"Enter closes the box");
            Assert::AreEqual ((size_t) 1, host.echoes.size(), L"one write runs");
            Assert::IsTrue   (host.echoes.front().find ("01FF") != std::string::npos, L"to the newest byte's address");
            Assert::IsTrue   (host.echoes.front().find ("42") != std::string::npos, L"with the typed value");
        }


        TEST_METHOD (EscapeLeavesTheStackByteAsItWas)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window (theme, host);



            ValueEditRig::Build (window, true);
            window.FocusControl (window.GetStackList());
            window.GetStackList()->SetSelectedRow (1);
            window.Press (VK_F2);
            Assert::IsTrue (window.GetStackEditor()->IsVisible(), L"F2 opens the box");

            window.Type  (L"99");
            window.Press (VK_ESCAPE);

            Assert::IsFalse  (window.GetStackEditor()->IsVisible());
            Assert::AreEqual ((size_t) 0, host.echoes.size(), L"nothing is written");
        }


        TEST_METHOD (TheStackDoesNotEditWhileRunning)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window (theme, host);



            ValueEditRig::Build (window, false);
            window.FocusControl (window.GetStackList());
            window.GetStackList()->SetSelectedRow (0);
            window.Press (VK_F2);

            Assert::IsFalse (window.GetStackEditor()->IsVisible());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerRegisterValueEditTests
    //
    //  A register's value edits in place; PC is not a byte and does not.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerRegisterValueEditTests)
    {
    public:

        TEST_METHOD (F2EditsTheRegisterInPlace)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window (theme, host);



            ValueEditRig::Build (window, true);
            window.FocusControl (window.GetRegisterList());
            window.GetRegisterList()->SetSelectedRow (1);
            window.Press (VK_F2);

            Assert::IsTrue   (window.GetRegisterEditor()->IsVisible(), L"the box opens over X's value");
            Assert::AreEqual (std::wstring (L"34"), window.GetRegisterEditor()->GetText());

            window.Type  (L"7F");
            window.Press (VK_RETURN);

            Assert::AreEqual ((size_t) 1, host.echoes.size(), L"one write runs");
            Assert::AreEqual (std::string ("R X 7F"), host.echoes.front());
        }


        TEST_METHOD (TheStackPointerEditsInPlaceToo)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window (theme, host);



            ValueEditRig::Build (window, true);
            window.FocusControl (window.GetRegisterList());
            window.GetRegisterList()->SetSelectedRow (4);
            window.Press (VK_F2);

            Assert::IsTrue   (window.GetRegisterEditor()->IsVisible(), L"S opens the box, not a prompt");
            Assert::AreEqual (std::wstring (L"FD"), window.GetRegisterEditor()->GetText());
        }


        TEST_METHOD (TextThatIsNotAByteWritesNothing)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window (theme, host);



            ValueEditRig::Build (window, true);
            window.FocusControl (window.GetRegisterList());
            window.GetRegisterList()->SetSelectedRow (0);
            window.Press (VK_F2);
            Assert::IsTrue (window.GetRegisterEditor()->IsVisible(), L"F2 opens the box");

            window.Type  (L"1FF");
            window.Press (VK_RETURN);

            Assert::IsFalse  (window.GetRegisterEditor()->IsVisible());
            Assert::AreEqual ((size_t) 0, host.echoes.size());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerConsoleModeTests
    //
    //  The console bar's mode is a "Mode:" label and a drop-down showing the
    //  mode in force.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerConsoleModeTests)
    {
    public:

        TEST_METHOD (TheModeBoxShowsTheModeInForce)
        {
            CassoTheme       theme    = CassoTheme::MakeSkeuomorphic();
            ValueEditHost    host;
            ValueEditWindow  window   (theme, host);
            auto             snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.OnCreate();

            Assert::IsNotNull (window.GetModeEntry(), L"the console bar holds the mode entry");
            Assert::AreEqual  (std::wstring (L"AppleWin"), window.GetModeEntry()->GetModeText(), L"AppleWin before any snapshot");

            snapshot->mode = CommandMode::Monitor;
            window.SetSnapshotForTest (snapshot);
            window.SetConsoleBarMenus();

            Assert::AreEqual (std::wstring (L"Monitor"), window.GetModeEntry()->GetModeText());
        }
    };
}




