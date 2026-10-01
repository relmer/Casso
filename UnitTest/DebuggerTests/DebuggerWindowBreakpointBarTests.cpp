#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Widgets/DxuiToolbar.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerBreakpointBarTests
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
    //  BarWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class BarWindow : public DebuggerWindow
    {
    public:
        BarWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::ApplyBreakpoints;
        using DebuggerWindow::GetBreakpointList;
        using DebuggerWindow::GetBreakpointBar;
        using DebuggerWindow::IsBreakpointBarEnabled;
        using DebuggerWindow::RunBreakpointBarEntry;
        using DebuggerWindow::SetSnapshotForTest;

        //  Three breakpoints at $0300 to $0302, the last disabled.
        void  Build (bool canUndo = false)
        {
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            OnCreate();

            for (int i = 0; i < 3; i++)
            {
                DebuggerViewSnapshot::BreakpointLine  bp;

                bp.id           = i + 1;
                bp.address      = (Word) (0x0300 + i);
                bp.enabled      = i != 2;
                bp.info.id      = bp.id;
                bp.info.address = bp.address;
                bp.info.last    = bp.address;
                bp.info.enabled = bp.enabled;
                snapshot->breakpoints.push_back (bp);
            }

            snapshot->canUndoBreakpoints = canUndo;
            SetSnapshotForTest (snapshot);
            ApplyBreakpoints();
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowBreakpointBarTests
    //
    //  The breakpoints pane's toolbar (FR-119): every button with a tip, each
    //  disabled while it cannot act, and each change sent as one step of the
    //  pane's undo list (FR-120).
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowBreakpointBarTests)
    {
    public:

        TEST_METHOD (TheBarHoldsEveryButtonEachWithAnIconAndATip)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            BarHost           host;
            BarWindow         window (theme, host);
            std::vector<int>       ids;
            BreakpointBarCommands  commands ({});



            window.OnCreate();

            for (int i = 0; i < window.GetBreakpointBar()->GetEntryCount(); i++)
            {
                if (window.GetBreakpointBar()->GetEntryCommandId (i) != DxuiToolbar::kSeeMoreId)
                {
                    ids.push_back (window.GetBreakpointBar()->GetEntryCommandId (i));
                }
            }

            Assert::IsTrue (ids == BreakpointBarCommands::GetIds(), L"New, Delete, Delete all, Enable all, Disable all, Undo, Redo, the two Go to, Show columns, Export, Import");

            for (int id : ids)
            {
                std::shared_ptr<DxuiCommand>  command = commands.Find (id);

                Assert::IsTrue (command->glyph != nullptr && *command->glyph != L'\0', std::format (L"entry {} has an icon", id).c_str());
                Assert::IsTrue (!command->tip.empty(), std::format (L"entry {} has a tip", id).c_str());
            }
        }


        TEST_METHOD (WithNothingSetOnlyNewShowColumnsAndImportCanAct)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.OnCreate();
            window.SetSnapshotForTest (std::make_shared<DebuggerViewSnapshot>());

            for (int id : BreakpointBarCommands::GetIds())
            {
                bool  expected = id == BreakpointBarCommands::kNew || id == BreakpointBarCommands::kColumns || id == BreakpointBarCommands::kImport;

                Assert::AreEqual (expected, window.IsBreakpointBarEnabled (id), std::format (L"entry {}", id).c_str());
            }
        }


        TEST_METHOD (EachButtonIsEnabledOnlyWhileItCanAct)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build (true);

            Assert::IsFalse (window.IsBreakpointBarEnabled (BreakpointBarCommands::kDelete),     L"nothing selected");
            Assert::IsFalse (window.IsBreakpointBarEnabled (BreakpointBarCommands::kGoToCode),   L"nothing selected");
            Assert::IsTrue  (window.IsBreakpointBarEnabled (BreakpointBarCommands::kDeleteAll));
            Assert::IsTrue  (window.IsBreakpointBarEnabled (BreakpointBarCommands::kEnableAll),  L"one is disabled");
            Assert::IsTrue  (window.IsBreakpointBarEnabled (BreakpointBarCommands::kDisableAll), L"two are enabled");
            Assert::IsTrue  (window.IsBreakpointBarEnabled (BreakpointBarCommands::kUndo));
            Assert::IsFalse (window.IsBreakpointBarEnabled (BreakpointBarCommands::kRedo));

            window.GetBreakpointList()->SetSelectedRow (0);

            Assert::IsTrue  (window.IsBreakpointBarEnabled (BreakpointBarCommands::kDelete));
            Assert::IsTrue  (window.IsBreakpointBarEnabled (BreakpointBarCommands::kGoToCode));
            Assert::IsFalse (window.IsBreakpointBarEnabled (BreakpointBarCommands::kGoToSource), L"no debug file, so no source line");
        }


        TEST_METHOD (DeleteIsOneStepForEverySelectedRow)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build();
            window.GetBreakpointList()->SetSelectedRows ({ 0, 2 }, 0);
            window.RunBreakpointBarEntry (BreakpointBarCommands::kDelete);

            Assert::AreEqual ((size_t) 1, host.actions.size());
            Assert::IsTrue   (host.actions[0].breakpointStep.has_value());
            Assert::IsTrue   (host.actions[0].breakpointStep->lines == std::vector<std::string> { "BPC 1", "BPC 3" });
        }


        TEST_METHOD (DeleteAllEnableAllAndDisableAllAreOneLineEach)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build();
            window.RunBreakpointBarEntry (BreakpointBarCommands::kDisableAll);
            window.RunBreakpointBarEntry (BreakpointBarCommands::kEnableAll);
            window.RunBreakpointBarEntry (BreakpointBarCommands::kDeleteAll);

            Assert::AreEqual ((size_t) 3, host.actions.size());
            Assert::IsTrue   (host.actions[0].breakpointStep->lines == std::vector<std::string> { "BPD *" });
            Assert::IsTrue   (host.actions[1].breakpointStep->lines == std::vector<std::string> { "BPE *" });
            Assert::IsTrue   (host.actions[2].breakpointStep->lines == std::vector<std::string> { "BPC *" });
        }


        TEST_METHOD (UndoAndRedoSendTheirStepsOnlyWhenTheyCanAct)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build (true);
            window.RunBreakpointBarEntry (BreakpointBarCommands::kRedo);
            window.RunBreakpointBarEntry (BreakpointBarCommands::kUndo);

            Assert::AreEqual ((size_t) 1, host.actions.size(), L"Redo has nothing to redo");
            Assert::IsTrue   (host.actions[0].breakpointStep->kind == BreakpointStep::Kind::Undo);
        }


        TEST_METHOD (ExportWritesBpsaveInAppleWinsWordsToTheFileChosen)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build();
            host.dialogs.path = L"C:\\Work\\my bps.txt";
            window.RunBreakpointBarEntry (BreakpointBarCommands::kExport);

            Assert::AreEqual ((size_t) 1, host.lines.size());
            Assert::AreEqual (std::string ("BPSAVE C:\\Work\\my bps.txt"), host.lines[0].first);
            Assert::IsTrue   (host.lines[0].second == CommandMode::AppleWin);
        }


        TEST_METHOD (ImportSendsTheFileChosenAsOneStep)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build();
            host.dialogs.path = L"C:\\Work\\bp.txt";
            window.RunBreakpointBarEntry (BreakpointBarCommands::kImport);

            host.dialogs.picks = false;
            window.RunBreakpointBarEntry (BreakpointBarCommands::kImport);

            Assert::AreEqual ((size_t) 1, host.actions.size(), L"backing out of the picker imports nothing");
            Assert::IsTrue   (host.actions[0].breakpointStep->kind == BreakpointStep::Kind::Import);
            Assert::AreEqual (std::string ("C:\\Work\\bp.txt"), host.actions[0].breakpointStep->path);
        }
    };
}
