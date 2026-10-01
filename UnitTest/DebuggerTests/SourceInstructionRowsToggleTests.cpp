#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SourceInstructionRowsToggleTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ToggleHost
    //
    //  A host that does nothing; the window's own state is what the tests read.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ToggleHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                      override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)         override {}
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
    //  ToggleWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ToggleWindow : public DebuggerWindow
    {
    public:
        ToggleWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::SetWindowMenus;
        using DebuggerWindow::GetMenuCommands;
        using DebuggerWindow::GetSourceBar;
        using DebuggerWindow::IsShowingSourceCode;
        using DebuggerWindow::kCodeEntry;


        std::shared_ptr<DxuiCommand>  FindRow (const std::wstring & label) const
        {
            std::shared_ptr<DxuiCommand>  found;



            for (const std::shared_ptr<DxuiCommand> & command : GetMenuCommands())
            {
                found = (command->label == label) ? command : found;
            }

            return found;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceInstructionRowsToggleTests
    //
    //  The instructions each source line assembled to are listed under it, and
    //  a switch on the Debug menu and on each source document's toolbar turns
    //  them off and on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceInstructionRowsToggleTests)
    {
    public:

        //  A debug file of one source line that assembled to two instructions,
        //  with the PC on that line.
        static DebuggerViewSnapshot  MakeSnapshot()
        {
            DebuggerViewSnapshot                 snapshot;
            DebuggerViewSnapshot::SourceState    source;
            DebugSourceFile                      file;
            auto                                 code     = std::make_shared<DebuggerViewSnapshot::LineCode>();



            file.id              = 0;
            file.name            = "main.a65";
            (*code)[{ 0, 1 }]    = { "0300  LDA #$41", "0302  STA $0400" };
            source.files         = { file };
            source.fileId        = 0;
            source.line          = 1;
            source.lineCode      = code;
            source.debugFilePath = L"C:\\Work\\main.dbg";
            snapshot.source      = source;
            snapshot.isPaused    = true;

            return snapshot;
        }


        static size_t  CountCodeRows (const DxuiTextView & view)
        {
            std::vector<int>  lines = SourcePane::GetRowLines (view.GetRows());



            return (size_t) std::count (lines.begin(), lines.end(), 0);
        }


        TEST_METHOD (TheSwitchTakesTheInstructionRowsAwayAndBack)
        {
            DxuiTextView          view;
            DxuiActionBanner      banner;
            SourcePane            pane (&view, &banner,
                                        [] (const DebugSourceFile &, const std::wstring &, const std::string &)
                                        {
                                            SourceLookup  lookup;

                                            lookup.match = SourceMatch::Exact;
                                            lookup.text  = "start   lda #$41\n        sta $0400\n";
                                            return lookup;
                                        },
                                        [] (const DebuggerActionBuilder &) {},
                                        [] (Word) {});
            DebuggerViewSnapshot  snapshot = MakeSnapshot();



            pane.SetFile (0);
            pane.Apply   (snapshot);
            Assert::AreEqual ((size_t) 2, CountCodeRows (view), L"two instructions under the line");

            pane.SetShowCode (false);
            pane.Apply       (snapshot);
            Assert::AreEqual ((size_t) 0, CountCodeRows (view), L"switched off");

            pane.SetShowCode (true);
            pane.Apply       (snapshot);
            Assert::AreEqual ((size_t) 2, CountCodeRows (view), L"switched back on");
        }


        TEST_METHOD (TheDebugMenuRowSwitchesThem)
        {
            CassoTheme                    theme  = CassoTheme::MakeSkeuomorphic();
            ToggleHost                    host;
            ToggleWindow                  window (theme, host);
            std::shared_ptr<DxuiCommand>  row;



            window.OnCreate();
            window.SetWindowMenus();
            row = window.FindRow (L"Show instructions under source lines");

            Assert::IsNotNull (row.get(), L"the Debug menu has the row");
            Assert::IsTrue    (row->IsChecked());

            row->dispatch();

            Assert::IsFalse (window.IsShowingSourceCode());
            Assert::IsFalse (row->IsChecked());
        }


        TEST_METHOD (EachSourceToolbarHasTheSwitch)
        {
            CassoTheme    theme  = CassoTheme::MakeSkeuomorphic();
            ToggleHost    host;
            ToggleWindow  window (theme, host);
            DxuiToolbar * bar    = nullptr;
            int           index  = -1;



            window.OnCreate();
            bar = window.GetSourceBar (2);

            for (int each = 0; each < bar->GetEntryCount(); each++)
            {
                index = (bar->GetEntryCommandId (each) == ToggleWindow::kCodeEntry) ? each : index;
            }

            Assert::IsTrue (index >= 0, L"the bar has the switch");

            bar->SetFocusIndex (index);
            bar->ActivateFocused();

            Assert::IsFalse (window.IsShowingSourceCode());
        }
    };
}
