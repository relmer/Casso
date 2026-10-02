#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerWindowDetachTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DetachHost
    //
    //  A host that counts what the window sent it to run the machine.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DetachHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override { runs++; }
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

        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override { runs++; }
        void  RunDebuggerAction        (const DebuggerAction &)                  override { runs++; }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        void  DetachDebugger           ()                                          override { detaches++; }

        int              runs     = 0;
        int              detaches = 0;
        FakeHostDialogs  dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DetachWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DetachWindow : public DebuggerWindow
    {
    public:
        DetachWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::GetMenuBarItems;
        using DebuggerWindow::SetSnapshotForTest;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowDetachTests
    //
    //  Debug > Detach closes the debugger and leaves the machine running.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowDetachTests)
    {
    public:

        static const DxuiPopupMenuItem * GetDebugRow (const DetachWindow & window, const std::wstring & label)
        {
            for (const DxuiMenuBarItem & item : window.GetMenuBarItems())
            {
                if (item.label != L"&Debug")
                {
                    continue;
                }

                for (const DxuiPopupMenuItem & row : item.submenu)
                {
                    if (row.command != nullptr && row.command->label == label)
                    {
                        return &row;
                    }
                }
            }

            return nullptr;
        }


        TEST_METHOD (DetachReplacesStopDebugging)
        {
            CassoTheme    theme = CassoTheme::MakeSkeuomorphic();
            DetachHost    host;
            DetachWindow  window (theme, host);



            window.OnCreate();

            Assert::IsNotNull (GetDebugRow (window, L"Detach"));
            Assert::IsNull    (GetDebugRow (window, L"Stop debugging"));
            Assert::IsNotNull (GetDebugRow (window, L"Restart under debugger"), L"restart under debugger stays");
        }


        TEST_METHOD (DetachAsksTheHostToDetachAndRunsNoCommand)
        {
            CassoTheme                 theme    = CassoTheme::MakeSkeuomorphic();
            DetachHost                 host;
            DetachWindow               window   (theme, host);
            auto                       snapshot = std::make_shared<DebuggerViewSnapshot>();
            const DxuiPopupMenuItem  * row      = nullptr;



            snapshot->isPaused = true;

            window.OnCreate();
            window.SetSnapshotForTest (snapshot);

            row = GetDebugRow (window, L"Detach");
            Assert::IsNotNull (row);

            row->command->dispatch();
            Assert::AreEqual (1, host.detaches, L"the host takes the hook off and resumes the machine");
            Assert::AreEqual (0, host.runs,     L"no debugger run is started");
        }


        TEST_METHOD (DetachLeavesARunningMachineAlone)
        {
            CassoTheme                 theme    = CassoTheme::MakeSkeuomorphic();
            DetachHost                 host;
            DetachWindow               window   (theme, host);
            auto                       snapshot = std::make_shared<DebuggerViewSnapshot>();
            const DxuiPopupMenuItem  * row      = nullptr;



            snapshot->isPaused = false;

            window.OnCreate();
            window.SetSnapshotForTest (snapshot);

            row = GetDebugRow (window, L"Detach");
            Assert::IsNotNull (row);

            row->command->dispatch();
            Assert::AreEqual (0, host.runs);
        }
    };
}
