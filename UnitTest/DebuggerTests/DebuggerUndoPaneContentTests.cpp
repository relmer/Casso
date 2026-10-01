#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerUndoPaneContentTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BarHost
    //
    //  A host that keeps the actions and AppleWin lines the window sends.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BarHost : public IDebuggerWindowHost
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

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode mode) override { lines.push_back ({ line, mode }); }
        void  RunDebuggerAction        (const DebuggerAction & action)            override { actions.push_back (action); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<DebuggerAction>                       actions;
        std::vector<std::pair<std::string, CommandMode>>  lines;
        FakeHostDialogs                                   dialogs;
    };






    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ContentWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ContentWindow : public DebuggerWindow
    {
    public:
        ContentWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::GetRegisterList;
        using DebuggerWindow::GetStackList;
        using DebuggerWindow::GetWatchList;
        using DebuggerWindow::GetPaneContentForTest;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerUndoPaneContentTests
    //
    //  The registers, stack and watch panes are each placed as their frame:
    //  the Undo and Redo bar holds its height at the top and the list fills
    //  the rest.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerUndoPaneContentTests)
    {
    public:
        TEST_METHOD (TheContentKeepsTheUndoBarAtTheTopAndTheListBelow)
        {
            CassoTheme       theme;
            BarHost          host;
            ContentWindow    window (theme, host);
            DxuiDpiScaler    scaler;



            window.OnCreate();

            const std::array<std::pair<const wchar_t *, DxuiListView *>, 3>  panes =
            { {
                { DebuggerLayout::kRegisters, window.GetRegisterList() },
                { DebuggerLayout::kStack,     window.GetStackList()    },
                { DebuggerLayout::kWatches,   window.GetWatchList()    },
            } };

            for (const auto & [pane, list] : panes)
            {
                IDxuiControl  * content = window.GetPaneContentForTest (pane);
                RECT            bounds  = {};

                Assert::IsNotNull (content, pane);

                content->Layout (RECT { 0, 0, 400, 300 }, scaler);
                bounds = list->GetBounds();

                Assert::AreEqual (300L, bounds.bottom, pane);
                Assert::IsTrue (bounds.top > 0 && bounds.top < 100, pane);
            }
        }
    };
}