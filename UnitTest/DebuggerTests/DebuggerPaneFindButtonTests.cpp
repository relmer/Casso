#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerPaneFindButtonTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FindButtonHost
    //
    //  A host that does nothing; the window's own state is what the tests read.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FindButtonHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                      override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)         override {}
        void  RunDebuggerAction        (const DebuggerAction &)                   override {}
        void  RunEmulatorCommand       (int)                                      override {}
        void  PauseDebugger            ()                                         override {}
        void  SetDebuggerCodeLines     (int, int)                                 override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop       (Word, int)                                override {}
        void  SetDebuggerFollowView    (int)                                      override {}
        void  CloseDebuggerCodeView    (int)                                      override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory       (int, const std::string &)                 override {}
        void  ScrollDebuggerCode       (int, int)                                 override {}
        void  OnDebuggerWindowClosed   ()                                         override {}
        void  SetDebuggerKeyScheme     (const std::string &)                      override {}
        void  SetDebuggerLayout        (const std::string &)                      override {}
        void  SetDebuggerOpenViews     (const std::string &)                      override {}
        void  SetDebuggerPlacement     (const RECT &)                             override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                 override {}

        bool  TakeDebuggerUpdate      (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        FakeHostDialogs  dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  FindButtonWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class FindButtonWindow : public DebuggerWindow
    {
    public:
        FindButtonWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetConsoleBar;
        using DebuggerWindow::GetSourceBar;
        using DebuggerWindow::GetPaneOfControl;
        using DebuggerWindow::IsFindOpen;
        using DebuggerWindow::GetFindPane;
        using DebuggerWindow::kFindEntry;


        //  Presses a bar's search button as a click does.
        static void  PressFind (DxuiToolbar * bar)
        {
            int  index = -1;



            for (int each = 0; each < bar->GetEntryCount(); each++)
            {
                if (bar->GetEntryCommandId (each) == kFindEntry)
                {
                    index = each;
                }
            }

            Assert::IsTrue (index >= 0, L"the bar has a search button");

            bar->SetFocusIndex (index);
            bar->ActivateFocused();
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerPaneFindButtonTests
    //
    //  Find belongs to each pane that has it: a search button on the pane's
    //  own toolbar opens that pane's find bar as Ctrl+F does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerPaneFindButtonTests)
    {
    public:

        TEST_METHOD (ConsoleSearchButtonOpensTheConsolesFindBar)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindButtonHost    host;
            FindButtonWindow  window (theme, host);



            window.OnCreate();

            Assert::IsFalse (window.IsFindOpen());

            FindButtonWindow::PressFind (window.GetConsoleBar());

            Assert::IsTrue   (window.IsFindOpen());
            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), window.GetFindPane());
        }


        TEST_METHOD (SourceSearchButtonOpensThatDocumentsFindBar)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindButtonHost    host;
            FindButtonWindow  window (theme, host);



            window.OnCreate();

            FindButtonWindow::PressFind (window.GetSourceBar (1));

            Assert::IsTrue   (window.IsFindOpen());
            Assert::AreEqual (DebuggerLayout::GetSourcePaneId (1), window.GetFindPane());
        }


        TEST_METHOD (SourceBarGoesWithItsDocument)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            FindButtonHost    host;
            FindButtonWindow  window (theme, host);



            window.OnCreate();

            Assert::AreEqual (DebuggerLayout::GetSourcePaneId (0), window.GetPaneOfControl (window.GetSourceBar (0)));
        }
    };
}
