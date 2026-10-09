#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/BreakpointBarCommands.h"
#include "Ui/Debugger/BreakpointColumns.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BreakpointVsLayoutTests
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

        //  Three breakpoints at $0300 to $0302, the last disabled, or all of them.
        void  Build (bool canUndo = false, bool someEnabled = true)
        {
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();



            OnCreate();

            for (int i = 0; i < 3; i++)
            {
                DebuggerViewSnapshot::BreakpointLine  bp;

                bp.id           = i + 1;
                bp.address      = (Word) (0x0300 + i);
                bp.enabled      = someEnabled && i != 2;
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
    //  BreakpointVsLayoutTests
    //
    //  The breakpoints pane's toolbar and columns, which follow Visual Studio's
    //  Breakpoints window: its buttons in its order, with Disable all in place
    //  of its two search-criteria buttons, and the columns it lists that apply
    //  to Casso, with Kind added and its Labels called Symbol.
    //
    ////////////////////////////////////////////////////////////////////////////////

    using Column = BreakpointColumns::Column;



    TEST_CLASS (BreakpointVsLayoutTests)
    {
    public:

        TEST_METHOD (TheButtonsAreVisualStudiosInItsOrder)
        {
            std::vector<int>  expected =
            {
                BreakpointBarCommands::kNew,        BreakpointBarCommands::kDelete,     BreakpointBarCommands::kDeleteAll,
                BreakpointBarCommands::kDisableAll, BreakpointBarCommands::kUndo,       BreakpointBarCommands::kRedo,
                BreakpointBarCommands::kGoToSource, BreakpointBarCommands::kGoToCode,   BreakpointBarCommands::kColumns,
                BreakpointBarCommands::kExport,     BreakpointBarCommands::kImport,
            };



            Assert::IsTrue (BreakpointBarCommands::GetIds() == expected);
        }


        TEST_METHOD (NewAndShowColumnsAreWordsAndTheRestAreIcons)
        {
            BreakpointBarCommands            commands ({});
            std::vector<DxuiToolbar::Entry>  entries = commands.BuildEntries();



            for (const DxuiToolbar::Entry & entry : entries)
            {
                bool  isMenu  = entry.command->id == BreakpointBarCommands::kNew || entry.command->id == BreakpointBarCommands::kColumns;
                bool  hasIcon = static_cast<bool> (entry.icon) || entry.command->glyph != nullptr;

                Assert::AreEqual (isMenu,  entry.kind == DxuiToolbar::Kind::DropDown, entry.command->label.c_str());
                Assert::AreEqual (!isMenu, entry.iconOnly,                            entry.command->label.c_str());
                Assert::AreEqual (!isMenu, hasIcon,                                   entry.command->label.c_str());
            }
        }


        TEST_METHOD (TheColumnsAreInThePanesOrder)
        {
            std::vector<std::wstring>  expected = { L"Name", L"Condition", L"Hit count", L"Kind", L"Symbol", L"When hit",
                                                    L"Function", L"File", L"Address", L"Data" };
            std::vector<std::wstring>  headings;



            for (size_t i = 0; i < BreakpointColumns::kCount; i++)
            {
                headings.push_back (BreakpointColumns::GetHeading ((Column) i));
            }

            Assert::IsTrue (headings == expected);
        }


        TEST_METHOD (FunctionAndDataSayWhatTheBreakpointStopsOn)
        {
            DebuggerViewSnapshot                  snapshot;
            DebuggerViewSnapshot::BreakpointLine  code;
            DebuggerViewSnapshot::BreakpointLine  data;
            BreakpointColumns::Cells              cells;



            code.info.kind    = BreakpointKind::Address;
            code.info.address = 0xFA62;
            code.label        = "RESET";
            data.info.kind    = BreakpointKind::Memory;
            data.info.access  = WatchAccess::ReadWrite;
            data.info.address = 0x0400;
            data.info.last    = 0x07FF;
            data.label        = "TEXT";

            cells = BreakpointColumns::GetCells (snapshot, code);
            Assert::AreEqual (std::string ("RESET"),   cells[(size_t) Column::Function]);
            Assert::AreEqual (std::string ("RESET"),   cells[(size_t) Column::Symbol]);
            Assert::AreEqual (std::string ("Address"), cells[(size_t) Column::Kind], L"the symbol may label a loop or data as well as a routine");
            Assert::AreEqual (std::string (""),        cells[(size_t) Column::Data]);

            cells = BreakpointColumns::GetCells (snapshot, data);
            Assert::AreEqual (std::string (""),                          cells[(size_t) Column::Function], L"a data breakpoint stops in no function");
            Assert::AreEqual (std::string ("TEXT"),                      cells[(size_t) Column::Symbol]);
            Assert::AreEqual (std::string ("Data read or write"),        cells[(size_t) Column::Kind]);
            Assert::AreEqual (std::string ("Read or write $0400-$07FF"), cells[(size_t) Column::Data]);
        }


        TEST_METHOD (OnlyTheDefaultColumnsShowOnOpening)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build();

            for (size_t i = 0; i < BreakpointColumns::kCount; i++)
            {
                Assert::AreEqual (BreakpointColumns::GetDefaultShown()[i], window.GetBreakpointList()->IsColumnVisible (i),
                                  BreakpointColumns::GetHeading ((Column) i).c_str());
            }
        }


        TEST_METHOD (DisableAllEnablesThemAllWhenNoneIsEnabled)
        {
            CassoTheme  theme  = CassoTheme::MakeSkeuomorphic();
            BarHost     host;
            BarWindow   window (theme, host);



            window.Build (false, false);

            Assert::IsTrue   (window.IsBreakpointBarEnabled (BreakpointBarCommands::kDisableAll), L"every one is disabled");

            window.RunBreakpointBarEntry (BreakpointBarCommands::kDisableAll);

            Assert::AreEqual ((size_t) 1, host.actions.size());
            Assert::AreEqual (std::string ("BPE *"), host.actions[0].breakpointStep->actions.at (0).echo);
        }
    };
}