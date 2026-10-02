#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerShiftEscTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ShiftEscHost
    //
    //  A host that keeps nothing and answers every question with nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ShiftEscHost : public IDebuggerWindowHost
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
    //  ShiftEscWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ShiftEscWindow : public DebuggerWindow
    {
    public:
        ShiftEscWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::IsPaneShown;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowShiftEscTests
    //
    //  Shift+Esc closes the pane holding the focus, as the Close item of a
    //  pane's menu shows.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowShiftEscTests)
    {
    public:

        TEST_METHOD (ShiftEscClosesTheFocusedPane)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            ShiftEscHost    host;
            ShiftEscWindow  window (theme, host);
            DxuiDpiScaler   scaler;
            DxuiKeyEvent    ev     = {};



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.FocusControl (window.GetCommandBox());
            Assert::IsTrue (window.IsPaneShown (DebuggerLayout::kConsole), L"the console starts shown");

            ev.kind  = DxuiKeyEventKind::Down;
            ev.vk    = VK_ESCAPE;
            ev.shift = true;

            Assert::IsTrue  (window.OnKey (ev));
            Assert::IsFalse (window.IsPaneShown (DebuggerLayout::kConsole), L"Shift+Esc closed it");
        }
    };
}
