#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerColumnWidthTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColumnWidthHost
    //
    //  A host whose saved state is one breakpoints column, in dips.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ColumnWidthHost : public IDebuggerWindowHost
    {
    public:
        static constexpr int  kSavedDip = 120;

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

        std::string  GetDebuggerWindowState (const std::string & name) override
        {
            return (name == "columns") ? std::format ("breakpoints={};", kSavedDip) : std::string();
        }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColumnWidthWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ColumnWidthWindow : public DebuggerWindow
    {
    public:
        ColumnWidthWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetBreakpointsPane;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerColumnWidthTests
    //
    //  Saved column widths are dips, restored at the DPI the window has once
    //  it is laid out, not at the default it has while its controls are
    //  built.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerColumnWidthTests)
    {
    public:

        TEST_METHOD (ASavedWidthIsRestoredAtTheWindowsDpi)
        {
            static constexpr UINT  kDpi = 144;

            CassoTheme         theme  = CassoTheme::MakeSkeuomorphic();
            ColumnWidthHost    host;
            ColumnWidthWindow  window (theme, host);
            DxuiDpiScaler      scaler;



            scaler.SetDpi (kDpi);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::AreEqual (MulDiv (ColumnWidthHost::kSavedDip, (int) kDpi, DxuiDpiScaler::kBaseDpi),
                              window.GetBreakpointsPane().GetList()->GetColumnEffectiveWidthPx (0));
        }
    };
}
