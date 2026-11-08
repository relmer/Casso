#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "TestHelpers.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/Panes/CodeEditCommands.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerCodeMenuTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuietDebuggerHost
    //
    //  A host that adds the echo of every action the window runs to commands.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class QuietDebuggerHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { commands.push_back (line); }
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string & text)            override { goTos.push_back (text); }
        void  ScrollDebuggerCode      (int lines, int)                           override { scrolls.push_back (lines); }
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
        void  RunDebuggerAction       (const DebuggerAction & action)   override { commands.push_back (action.echo); }

        bool  IsDebuggerSkipNoticeShown()          override { return isNoticeOn; }
        void  SetDebuggerSkipNoticeShown (bool on) override { isNoticeOn = on; }

        std::vector<int>  scrolls;
        bool              isNoticeOn = false;

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  goTos;
        std::vector<std::string>  commands;

    private:
        FakeHostDialogs  m_dialogs;
    };






    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuWindow : public DebuggerWindow
    {
    public:
        MenuWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetCodeEdits;
        using DebuggerWindow::GetCodeGroup;
        using DebuggerWindow::ScrollCodeAtEdge;
        using DebuggerWindow::OnMouse;

        std::optional<std::wstring> RunTextDialog (const std::wstring &, const std::wstring &, const std::wstring & placeholder, TextDialog::CheckFn check) override
        {
            std::wstring  error;



            shownPlaceholder = placeholder;
            return check (typed, error) ? std::optional<std::wstring> (typed) : std::nullopt;
        }

        int ShowMessageBox (HWND, const std::wstring & text, const wchar_t *, UINT) override
        {
            messages.push_back (text);
            return answer;
        }

        int ShowCheckMessageBox (HWND, const std::wstring & text, const wchar_t *, UINT, const std::wstring &, bool & ioChecked) override
        {
            notices.push_back (text);
            ioChecked = isNoticeTurnedOff;
            return IDOK;
        }

        std::wstring               typed;
        std::wstring               shownPlaceholder;
        std::vector<std::wstring>  messages;
        std::vector<std::wstring>  notices;
        bool                       isNoticeTurnedOff = false;
        int                        answer            = IDYES;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CodeMenuTests
    //
    //  The code pane's menu marks data, skips routines, toggles bookmarks,
    //  times with the stopwatch and calls a routine, each as a typed action
    //  (FR-182, FR-184, FR-189, FR-197).
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CodeMenuTests)
    {
    public:

        using Items = std::vector<std::pair<std::wstring, std::function<void()>>>;

        static std::shared_ptr<DebuggerViewSnapshot> MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  jsr;
            DebuggerViewSnapshot::CodeLine  rts;



            lda.address      = 0x0300;
            lda.bytes        = "A9 41";
            lda.instruction  = "LDA #$41";
            lda.isCurrent    = true;

            jsr.address      = 0x0302;
            jsr.bytes        = "20 ED FD";
            jsr.instruction  = "JSR COUT";
            jsr.shownOperand = "COUT";
            jsr.target       = 0xFDED;

            rts.address      = 0x0305;
            rts.bytes        = "60";
            rts.instruction  = "RTS";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, jsr, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->bookmarks    = { { 0, 0x0305, std::string(), true } };
            return snapshot;
        }

        static void Run (const Items & items, const std::wstring & label)
        {
            auto  found = std::ranges::find_if (items, [&label] (const auto & item) { return item.first == label; });



            Assert::IsTrue (found != items.end(), label.c_str());
            found->second();
        }

        static std::wstring JoinLabels (const Items & items)
        {
            std::wstring  joined;



            for (const auto & [label, action] : items)
            {
                joined += label + L"|";
            }

            return joined;
        }

        static bool Has (const Items & items, const std::wstring & label)
        {
            return std::ranges::any_of (items, [&label] (const auto & item) { return item.first == label; });
        }



        //  The view's rows are only the lines in view, so an arrow at the first or
        //  last of them scrolls the view through memory, as do Page Up and
        //  Page Down there; elsewhere the list moves its selection.
        TEST_METHOD (AnArrowAtTheEdgeScrollsTheView)
        {
            CassoTheme         theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window  (theme, host);
            DxuiDpiScaler      scaler;
            DxuiListView     * code    = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeSnapshot());
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            code = window.GetCodeList (0);
            Assert::IsTrue (code->GetRowCount() > 1, L"the view has its lines");
            code->SetSelectedRow (0);

            Assert::IsTrue  (window.ScrollCodeAtEdge (VK_UP, code),    L"Up on the first line");
            Assert::IsFalse (window.ScrollCodeAtEdge (VK_DOWN, code),  L"Down on the first line moves the selection");
            Assert::IsTrue  (window.ScrollCodeAtEdge (VK_PRIOR, code), L"Page Up on the first line");
            Assert::IsFalse (window.ScrollCodeAtEdge (VK_UP, nullptr), L"only for the focused view");

            code->SetSelectedRow (code->GetRowCount() - 1);
            Assert::IsTrue  (window.ScrollCodeAtEdge (VK_DOWN, code),  L"Down on the last line");

            Assert::AreEqual ((size_t) 3, host.scrolls.size());
            Assert::AreEqual (-1, host.scrolls[0]);
            Assert::IsTrue   (host.scrolls[1] < -1, L"a page up");
            Assert::AreEqual (1, host.scrolls[2]);
        }


        //  The vertical wheel over a view scrolls the disassembly through
        //  memory; the horizontal wheel and Shift with the wheel do not, since
        //  they scroll the list's columns sideways.
        TEST_METHOD (OnlyTheVerticalWheelScrollsTheDisassemblyThroughMemory)
        {
            CassoTheme         theme   = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window  (theme, host);
            DxuiDpiScaler      scaler;
            DxuiListView     * code    = nullptr;
            DxuiMouseEvent     wheel;
            RECT               bounds  = {};



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeSnapshot());
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            code   = window.GetCodeList (0);
            bounds = code->GetBounds();

            wheel.kind            = DxuiMouseEventKind::Wheel;
            wheel.positionDip     = POINT { (bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2 };
            wheel.wheelDelta      = 1.0f;
            wheel.wheelHorizontal = true;
            (void) window.OnMouse (wheel);
            Assert::IsTrue (host.scrolls.empty(), L"the horizontal wheel scrolled through memory");

            wheel.wheelHorizontal = false;
            wheel.shift           = true;
            (void) window.OnMouse (wheel);
            Assert::IsTrue (host.scrolls.empty(), L"Shift with the wheel scrolled through memory");

            wheel.shift = false;
            (void) window.OnMouse (wheel);
            Assert::AreEqual ((size_t) 1, host.scrolls.size(), L"the vertical wheel did not scroll");
            Assert::IsTrue   (host.scrolls[0] < 0, L"the wheel up went down through memory");
        }



        //  A routine the step filter has is a checked row whose click takes
        //  it out; adding one opens a notice of how to take it out again, until the
        //  notice's check box turns the notice off.
        TEST_METHOD (NeverStepIntoIsCheckedWhileTheFilterHasTheRoutine)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               filtered = MakeSnapshot();
            auto               plain    = MakeSnapshot();
            Items              items;



            filtered->stepFilters = { { "COUT", 0xFDED, 0xFDED } };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (filtered);

            window.GetCodeEdits().AddItems (0, filtered->code[1], 1, items);
            Run (items, PaneMenu::MakeChecked (L"Never step into this routine"));
            Assert::AreEqual (std::string ("SKIP -COUT"), host.commands.back(), L"a click on the checked row takes it out");

            window.TakeSnapshot (plain);
            items.clear();
            host.isNoticeOn          = true;
            window.isNoticeTurnedOff = true;

            window.GetCodeEdits().AddItems (0, plain->code[1], 1, items);
            Run (items, L"Never step into this routine");

            Assert::AreEqual (std::string ("SKIP COUT"), host.commands.back(), L"a click on the plain row adds it");
            Assert::AreEqual ((size_t) 1, window.notices.size(), L"with the notice");
            Assert::IsTrue   (window.notices.front().find (L"SKIP - COUT") != std::wstring::npos, L"with the command that takes it out");
            Assert::IsFalse  (host.isNoticeOn, L"its check box turned it off");

            Run (items, L"Never step into this routine");
            Assert::AreEqual ((size_t) 1, window.notices.size(), L"no notice once it is off");
        }



        TEST_METHOD (TheMenuRunsEachEntryAsATypedAction)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               snapshot = MakeSnapshot();
            Items              items;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            window.GetCodeEdits().AddItems (0, snapshot->code[1], 1, items);

            Run (items, PaneMenu::MakeInSubmenu (L"Mark as data", L"Words"));
            Run (items, L"Clear data marking");
            Run (items, L"Never step into this routine");
            Run (items, L"Add bookmark");
            Run (items, L"Start stopwatch here");
            Run (items, L"Call subroutine here");

            Assert::IsTrue (host.commands == std::vector<std::string> { "DW 0302:0304", "X 0302:0304", "SKIP COUT", "BMA 0302", "JSR 0302" },
                            L"the JSR's three bytes, the routine it calls by its symbol");

            items.clear();
            window.GetCodeEdits().AddItems (0, snapshot->code[2], 2, items);

            Assert::IsTrue (Has (items, L"Remove bookmark"), L"the RTS has a bookmark");
            Run (items, L"Stop stopwatch here");
            Assert::AreEqual (std::string ("STOPWATCH 0302 0305"), host.commands.back());
        }



        //  A shorter instruction is padded with NOP to the line's end; a
        //  longer one is written after a question and padded to the end of the last line it
        //  covers; each writes only over the bytes that were there, and Undo
        //  puts them back (FR-185).
        TEST_METHOD (AnEditedInstructionIsPaddedAndUndone)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            TestCpu            cpu;
            auto               snapshot = MakeSnapshot();
            Items              items;



            cpu.InitForTest();
            snapshot->instructionSet = cpu.GetInstructionSet();

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            window.typed = L"INX";
            window.GetCodeEdits().AddItems (0, snapshot->code[0], 0, items);
            Run (items, L"Edit instruction...");

            Assert::AreEqual (std::wstring (L"LDA #$41"), window.shownPlaceholder, L"the line's instruction is shown");
            Assert::AreEqual (std::string ("PATCH 0300 E8 EA"), host.commands.back());
            Assert::IsTrue   (window.messages.empty(), L"nothing covered, nothing asked");

            window.typed = L"JMP $1234";
            items.clear();
            window.GetCodeEdits().AddItems (0, snapshot->code[0], 0, items);
            Run (items, L"Edit instruction...");

            Assert::AreEqual ((size_t) 1, window.messages.size(), L"the JSR it covers is named first");
            Assert::IsTrue   (window.messages[0].find (L"JSR COUT") != std::wstring::npos, window.messages[0].c_str());
            Assert::AreEqual (std::string ("PATCH 0300 4C 34 12 EA EA"), host.commands.back(), L"padded to the JSR's end");

            items.clear();
            window.GetCodeEdits().AddItems (0, snapshot->code[0], 0, items);
            Run (items, L"Undo edit at $0300");

            Assert::AreEqual (std::string ("PATCH 0300 A9 41 20 ED FD"), host.commands.back(), L"the bytes that were there");
        }



        //  The breakpoint entry is labeled from the line right-clicked in that
        //  view, not from the first view's line at the same row.
        TEST_METHOD (TheBreakpointEntryIsLabeledFromTheViewsOwnLine)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler          scaler;
            auto                   snapshot = MakeSnapshot();
            Items                  items;
            DisassemblyViewPane  * second   = nullptr;



            snapshot->codeViews[1]                  = snapshot->codeViews[0];
            snapshot->codeViews[1][2].hasBreakpoint = true;
            snapshot->breakpoints.push_back ({ 1, snapshot->codeViews[1][2].address, "BP", true });
            snapshot->codeViews[0].resize (1);
            snapshot->code                          = snapshot->codeViews[0];
            snapshot->codeOpen[1]                   = true;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            second = &window.GetCodeGroup().GetView (1);
            Assert::IsTrue (second->AddMenuItems (second->GetList(), 2, items));

            Assert::IsTrue  (Has (items, L"Remove breakpoint"), L"the RTS in the second view has a breakpoint");
            Assert::IsFalse (Has (items, L"Set breakpoint"));
            Assert::IsTrue  (Has (items, L"Disable breakpoint"), L"and can turn it off");
        }



        //  The breakpoint rows a source document has too: Set breakpoint on a
        //  line with none, and on a line with one, Enable breakpoint or
        //  Disable breakpoint for its state, then Remove breakpoint.
        TEST_METHOD (TheBreakpointRowsAreSetOrEnableThenRemove)
        {
            CassoTheme                 theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost          host;
            MenuWindow                 window   (theme, host);
            DxuiDpiScaler              scaler;
            auto                       snapshot = MakeSnapshot();
            Items                      items;
            DisassemblyViewPane      * view     = nullptr;
            std::vector<std::wstring>  rows;



            snapshot->codeViews[0][2].hasBreakpoint = true;
            snapshot->codeViews[0][2].isEnabled     = false;
            snapshot->breakpoints.push_back ({ 1, snapshot->codeViews[0][2].address, "BP", false });
            snapshot->code                          = snapshot->codeViews[0];
            snapshot->codeOpen[0]                   = true;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            view = &window.GetCodeGroup().GetView (0);

            for (int row : { 1, 2 })
            {
                items.clear();
                rows.clear();
                Assert::IsTrue (view->AddMenuItems (view->GetList(), row, items));

                for (const auto & [label, action] : items)
                {
                    if (label.ends_with (L" breakpoint"))
                    {
                        rows.push_back (label);
                    }
                }

                Assert::IsTrue (rows == ((row == 1) ? std::vector<std::wstring> { L"Set breakpoint" }
                                                    : std::vector<std::wstring> { L"Enable breakpoint", L"Remove breakpoint" }),
                                JoinLabels (items).c_str());
            }

            Run (items, L"Enable breakpoint");
            Assert::AreEqual (DebuggerActions::GetEnableBreakpoint (1, true, snapshot->mode).echo, host.commands.back());
        }



        TEST_METHOD (ROMIsNotEditedAndNopsReplaceASelection)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               snapshot = MakeSnapshot();
            Items              items;



            snapshot->code[2].isRom         = true;
            snapshot->codeViews[0][2].isRom = true;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (snapshot);

            window.GetCodeEdits().AddItems (0, snapshot->code[2], 2, items);
            Run (items, L"Edit instruction...");

            Assert::IsTrue (host.commands.empty(), L"ROM is not written");
            Assert::AreEqual ((size_t) 1, window.messages.size());

            items.clear();
            window.GetCodeEdits().AddItems (0, snapshot->code[1], 1, items);
            Run (items, L"Replace with NOPs");

            Assert::AreEqual (std::string ("PATCH 0302 EA EA EA"), host.commands.back());
        }



        TEST_METHOD (ASelectionMarksFromItsFirstLineToItsLastByte)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeSnapshot());

            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            Assert::IsTrue (window.GetCodeEdits().GetSelection (0, 0) == std::pair<Word, Word> (0x0300, 0x0301), L"the clicked row alone");
        }


        //  Show last writer's snapshot asks the view to select the writer's
        //  instruction, and its row is selected and the view focused; a
        //  later snapshot with the same request selects nothing.
        TEST_METHOD (ASnapshotsAskedInstructionIsSelected)
        {
            CassoTheme         theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost  host;
            MenuWindow         window   (theme, host);
            DxuiDpiScaler      scaler;
            auto               asked    = MakeSnapshot();
            auto               again    = MakeSnapshot();



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            asked->codeSelect[0]       = 0x0302;
            asked->codeSelectSerial[0] = 1;
            window.TakeSnapshot (asked);
            window.ApplyCodeSnapshot (asked, 0);
            Assert::AreEqual (1, window.GetCodeList (0)->GetSelectedRow(), L"the JSR's row");
            Assert::IsTrue   (window.GetCodeList (0)->IsListFocused(), L"the view has the focus");

            again                      = MakeSnapshot();
            again->codeSelect[0]       = 0x0300;
            again->codeSelectSerial[0] = 1;
            window.TakeSnapshot (again);
            window.ApplyCodeSnapshot (again, 0);
            Assert::AreEqual (1, window.GetCodeList (0)->GetSelectedRow(), L"a request already applied selects nothing");

            again->codeSelectSerial[0] = 2;
            window.TakeSnapshot (again);
            window.ApplyCodeSnapshot (again, 0);
            Assert::AreEqual (0, window.GetCodeList (0)->GetSelectedRow(), L"a new request selects its row");
        }


        //  With Show heat on, a line's heat cell has its color, on the PC's row
        //  as on the others; the row's other cells have the row's fill.
        TEST_METHOD (TheHeatCellKeepsItsColorOnEveryRow)
        {
            CassoTheme          theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            MenuWindow          window   (theme, host);
            DxuiDpiScaler       scaler;
            auto                snapshot = MakeSnapshot();
            auto                levels   = std::make_shared<DebuggerViewSnapshot::HeatColumnLevels>();
            DisassemblyOptions  options;
            DxuiListView      * list     = nullptr;



            levels->execute.assign (0x10000, 200);
            levels->read.assign    (0x10000, 0);
            levels->write.assign   (0x10000, 0);
            snapshot->heatColumns = levels;

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            options.Set (DisassemblyOptions::Option::Heat, true);
            window.GetCodeGroup().SetOptions (options);
            window.ApplyCodeSnapshot (snapshot, 0);

            list = window.GetCodeList (0);

            for (int row = 0; row < 3; row++)
            {
                Assert::AreNotEqual (0u, list->GetCellsOfRow (row)[DisassemblyCells::kHeatColumn].background, L"the heat color");
            }
        }
    };
}
