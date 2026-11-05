#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/BreakpointsPane.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

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

        void  SetDebuggerWindowState (const std::string & name, const std::string & text) override
        {
            if (name == "columns")
            {
                savedColumns = text;
            }
        }

        std::string  savedColumns;

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
        using DebuggerWindow::SetWindowMenus;
        using DebuggerWindow::GetMenuBarItems;
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


        //  Sizing one column saves that column alone; the others go on
        //  fitting their text. Reset window layout returns every column to
        //  fitting.
        TEST_METHOD (OnlyTheColumnsSizedAreSaved)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            ColumnWidthHost       host;
            ColumnWidthWindow     window (theme, host);
            DxuiDpiScaler         scaler;
            DxuiListView        * list   = nullptr;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         listTheme;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            list = window.GetBreakpointsPane().GetList();
            list->FitColumnToContent (1);
            list->Paint (painter, text, listTheme);

            Assert::IsTrue (host.savedColumns.starts_with (std::format ("breakpoints={},", ColumnWidthHost::kSavedDip)), L"the width sized before");
            Assert::IsTrue (host.savedColumns.ends_with (",0;"), L"a column never sized is saved as fitting");
            Assert::AreEqual (-1, list->GetColumnOverrideWidthPx (2), L"and is not sized");

            window.SetWindowMenus();

            for (const DxuiMenuBarItem & menu : window.GetMenuBarItems())
            {
                for (const DxuiPopupMenuItem & row : menu.submenu)
                {
                    if (row.command != nullptr && row.command->label == L"Reset window layout")
                    {
                        row.command->dispatch();
                    }
                }
            }

            Assert::AreEqual (std::string(), host.savedColumns, L"no widths saved after the reset");
            Assert::AreEqual (-1, list->GetColumnOverrideWidthPx (0));
        }
    };
}
