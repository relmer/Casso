#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/Panes/WatchesPane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerWatchAddRowTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  QuietDebuggerHost
    //
    //  A host that keeps the echo of every action the window runs.
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
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}
        void  RunDebuggerAction       (const DebuggerAction & action)   override { commands.push_back (action.echo); actions.push_back (action); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>     goTos;
        std::vector<std::string>     commands;
        std::vector<DebuggerAction>  actions;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WatchWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class WatchWindow : public DebuggerWindow
    {
    public:
        WatchWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::RouteMappedKey;
        using DebuggerWindow::GetWatchesPane;
        using DebuggerWindow::TakeSnapshot;

        void  Press (WPARAM vk)
        {
            DxuiKeyEvent  down = { DxuiKeyEventKind::Down, vk, false, false, false, false };

            (void) (OnKey (down) || RouteMappedKey (down));
        }

        void  Type (const std::wstring & text)
        {
            for (wchar_t ch : text)
            {
                DxuiKeyEvent  down  = { DxuiKeyEventKind::Down, (WPARAM) (ch == L' ' ? VK_SPACE : towupper (ch)), false, false, false, false };
                DxuiKeyEvent  typed = { DxuiKeyEventKind::Char, (WPARAM) ch, false, false, false, false };

                (void) (OnKey (down) || RouteMappedKey (down));
                (void) OnKey (typed);
            }
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AddRowTests
    //
    //  As in Visual Studio, the watches pane ends with a row that adds a watch
    //  when an expression is typed into it in place.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AddRowTests)
    {
    public:

        //  A symbol is a symbol in the Type column whether the instruction
        //  touched it or it was typed: memory shown by a symbol's name, or a
        //  watch typed by one.
        TEST_METHOD (AnAutomaticAndATypedSymbolWatchHaveOneType)
        {
            DebuggerViewSnapshot::WatchLine  watch;



            watch.expression = "count.b";
            watch.address    = 0x60C1;

            Assert::AreEqual (std::wstring (L"Symbol"),   WatchesPane::GetAutoType ("M:60C1", "count $60C1"));
            Assert::AreEqual (std::wstring (L"Memory"),   WatchesPane::GetAutoType ("M:0300", "$0300"));
            Assert::AreEqual (std::wstring (L"Register"), WatchesPane::GetAutoType ("R:A", "A"));
            Assert::AreEqual (std::wstring (L"Symbol"),   WatchesPane::GetWatchType (watch));
        }

        TEST_METHOD (TheLastRowAddsAWatchTypedInPlace)
        {
            CassoTheme                          theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                   host;
            WatchWindow                         window   (theme, host);
            DxuiDpiScaler                       scaler;
            auto                                snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::WatchLine     watch;
            int                                 last     = 0;



            watch.id      = 0;
            watch.address = 0x0300;
            watch.value   = "0000";
            watch.enabled = true;
            snapshot->watches = { watch };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (snapshot);

            //  The window sizes the Watch column to its text as it draws; with
            //  nothing drawn here it is given a width so its cell has a place.
            static_cast<DxuiEditableGrid *> (window.GetWatchesPane().GetList())->SetGridColumns ({ { { L"ID",    40, false, DxuiTextHAlign::Left }, false, false },
                                                                                    { { L"Watch", 120, false, DxuiTextHAlign::Left }, true, true },
                                                                                    { { L"Value", 120, false, DxuiTextHAlign::Left }, true, false } });

            Assert::AreEqual (2, window.GetWatchesPane().GetList()->GetRowCount(), L"the watch, then the add row");
            last = window.GetWatchesPane().GetList()->GetRowCount() - 1;
            Assert::AreEqual (std::wstring (L"Add new watch"), window.GetWatchesPane().GetList()->GetCellsOfRow (last)[2].text);

            window.GetWatchesPane().BeginEdit (last, 0);
            window.Type (L"400");
            window.GetWatchesPane().EndEdit (true);

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands.front().find ("400") != std::string::npos, L"the typed expression is watched");
        }


        //  Enter on the add row with nothing typed ends the add and runs no
        //  command; on a watch whose expression was erased, it removes the
        //  watch.
        TEST_METHOD (EnterOnAnEmptyEditEndsTheAddOrRemovesTheWatch)
        {
            CassoTheme                       theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                host;
            WatchWindow                      window   (theme, host);
            DxuiDpiScaler                    scaler;
            DxuiEditableGrid               * grid     = BeginAdd (window, scaler);
            auto                             snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::WatchLine  watch;



            window.Press (VK_RETURN);
            Assert::IsFalse  (grid->IsEditing(), L"the add ends");
            Assert::IsTrue   (host.commands.empty(), L"with nothing added");

            watch.id          = 0;
            watch.address     = 0x0300;
            watch.expression  = "300";
            watch.value       = "0000";
            watch.enabled     = true;
            snapshot->watches = { watch };
            window.TakeSnapshot (snapshot);

            window.GetWatchesPane().BeginEdit (0, 0);
            window.Press (VK_BACK);
            window.Press (VK_BACK);
            window.Press (VK_BACK);
            window.Press (VK_RETURN);

            Assert::AreEqual ((size_t) 1, host.actions.size(), L"one action");
            Assert::AreEqual (DebuggerActions::GetClearWatch (0, CommandMode::AppleWin).echo, host.commands[0], L"the watch is removed");
        }

        //  A window with an empty watch grid whose columns have widths, as after a
        //  paint, and its add row's edit begun.
        static DxuiEditableGrid * BeginAdd (WatchWindow & window, DxuiDpiScaler & scaler)
        {
            DxuiEditableGrid  * grid = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());

            grid = static_cast<DxuiEditableGrid *> (window.GetWatchesPane().GetList());
            grid->SetGridColumns ({ { { L"ID",    40, false, DxuiTextHAlign::Left }, false, false },
                                    { { L"Watch", 120, false, DxuiTextHAlign::Left }, true, true },
                                    { { L"Value", 200, false, DxuiTextHAlign::Left }, true, false } });

            window.GetWatchesPane().BeginEdit (grid->GetRowCount() - 1, 0);
            return grid;
        }


        //  Text that does not parse stays in the edit, with the reason in the
        //  add row's Value cell, and nothing runs.
        TEST_METHOD (TextThatDoesNotParseStaysInTheEditWithItsReasonInTheValueCell)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            WatchWindow         window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiEditableGrid  * grid   = BeginAdd (window, scaler);



            window.Type  (L"1+");
            window.Press (VK_RETURN);

            Assert::IsTrue   (host.commands.empty(), L"a watch was added");
            Assert::IsTrue   (grid->IsEditing(), L"the edit closed");
            Assert::AreEqual (1, grid->GetEditColumn(), L"the edit moved off the name");
            Assert::IsFalse  (grid->GetCellsOfRow (grid->GetDataRowCount())[3].text.empty(), L"no reason in the Value cell");
        }


        //  A name no symbol has yet is watched all the same: Enter adds it and
        //  opens the add row again, empty, so a second name and Enter add a
        //  second watch with no click. Esc ends it.
        TEST_METHOD (EnterOnANameAddsTheWatchAndOpensTheAddRowAgain)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            WatchWindow         window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiEditableGrid  * grid   = BeginAdd (window, scaler);



            window.Type  (L"asdf");
            window.Press (VK_RETURN);

            Assert::AreEqual ((size_t) 1, host.commands.size(), L"the watch was not added");
            Assert::IsTrue   (host.commands.front().find ("asdf") != std::string::npos);
            Assert::IsTrue   (grid->IsEditing() && grid->IsAddRow (grid->GetEditRow()), L"the add row is open again");

            window.Type  (L"qwer");
            window.Press (VK_RETURN);

            Assert::AreEqual ((size_t) 2, host.commands.size(), L"the second watch was not added");
            Assert::IsTrue   (host.commands.back().find ("qwer") != std::string::npos);

            window.Press (VK_ESCAPE);
            Assert::IsFalse  (grid->IsEditing(), L"Esc ends it");
        }


        //  An empty name adds nothing: Tab moves on to the value, and Enter
        //  ends the add.
        TEST_METHOD (AnEmptyNameAddsNothing)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            WatchWindow         window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiEditableGrid  * grid   = BeginAdd (window, scaler);



            window.Press (VK_TAB);

            Assert::IsTrue   (grid->IsEditing(), L"the edit stays open");
            Assert::AreEqual (1, grid->GetEditColumn(), L"Tab moved to the value");

            window.Press (VK_RETURN);

            Assert::IsTrue   (host.commands.empty());
            Assert::IsFalse  (grid->IsEditing(), L"Enter ends the add");
        }


        //  Tab on a name adds the watch, and its value is edited once a
        //  snapshot lists it.
        TEST_METHOD (TabOnANameAddsTheWatchAndEditsItsValue)
        {
            CassoTheme                        theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                 host;
            WatchWindow                       window   (theme, host);
            DxuiDpiScaler                     scaler;
            DxuiEditableGrid                * grid     = BeginAdd (window, scaler);
            auto                              snapshot = std::make_shared<DebuggerViewSnapshot>();



            window.Type  (L"kbd");
            window.Press (VK_TAB);
            Assert::AreEqual ((size_t) 1, host.commands.size(), L"the watch was not added");

            snapshot->watches = { { 0, 0xC000, "0000", true, "", "KBD" } };
            window.TakeSnapshot (snapshot);

            Assert::IsTrue   (grid->IsEditing(), L"the value is not edited");
            Assert::AreEqual (0, grid->GetEditRow());
            Assert::AreEqual (2, grid->GetEditColumn(), L"not the value");
        }


        //  A Tab's add whose watch no snapshot lists within the wait, as when
        //  the add failed in the engine, edits no value: a watch listed later
        //  is not edited for it.
        TEST_METHOD (AWatchListedPastTheWaitIsNotEditedForATabsAdd)
        {
            static constexpr ULONGLONG  kPastTheWaitMs = 60'000;
            CassoTheme                  theme          = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost           host;
            WatchWindow                       window   (theme, host);
            DxuiDpiScaler                     scaler;
            DxuiEditableGrid                * grid     = BeginAdd (window, scaler);
            auto                              snapshot = std::make_shared<DebuggerViewSnapshot>();
            ULONGLONG                         now      = 1000;



            window.GetWatchesPane().SetClock ([&now] { return now; });

            window.Type  (L"kbd");
            window.Press (VK_TAB);
            Assert::AreEqual ((size_t) 1, host.commands.size(), L"the watch was not added");

            now += kPastTheWaitMs;

            snapshot->watches = { { 0, 0xC000, "0000", true, "", "KBD" } };
            window.TakeSnapshot (snapshot);

            Assert::IsFalse (grid->IsEditing() && grid->GetEditRow() == 0, L"the watch listed past the wait is edited");
        }


        //  As the heat map ranges pane's add row: one click starts the edit.
        TEST_METHOD (OneClickOnTheAddRowStartsTheEdit)
        {
            CassoTheme          theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            WatchWindow         window   (theme, host);
            DxuiDpiScaler       scaler;
            auto                snapshot = std::make_shared<DebuggerViewSnapshot>();
            DxuiEditableGrid  * grid     = nullptr;
            RECT                cell     = {};
            DxuiMouseEvent      click;
            int                 last     = 0;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (snapshot);

            grid = static_cast<DxuiEditableGrid *> (window.GetWatchesPane().GetList());
            grid->SetGridColumns ({ { { L"ID",    40, false, DxuiTextHAlign::Left }, false, false },
                                    { { L"Watch", 120, false, DxuiTextHAlign::Left }, true, true },
                                    { { L"Value", 120, false, DxuiTextHAlign::Left }, true, false } });

            last = grid->GetDataRowCount();
            Assert::IsTrue (grid->GetCellTextRectPx (last, 2, cell), L"the add row's cell is in view");

            click.button      = DxuiMouseButton::Left;
            click.positionDip = { (cell.left + cell.right) / 2, (cell.top + cell.bottom) / 2 };
            click.kind        = DxuiMouseEventKind::Down;
            (void) grid->OnMouse (click);
            click.kind        = DxuiMouseEventKind::Up;
            (void) grid->OnMouse (click);

            Assert::IsTrue   (grid->IsEditing(), L"one click does not start the edit");
            Assert::AreEqual (last, grid->GetEditRow());
        }


        //  A watch and a zero-page pointer have no check box; a pointer's row
        //  has where it points and the bytes there.
        TEST_METHOD (WatchesAndPointersHaveNoChecksAndPointersTheirTargets)
        {
            CassoTheme                          theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                   host;
            WatchWindow                         window   (theme, host);
            DxuiDpiScaler                       scaler;
            auto                                snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->watches          = { { 0, 0x0300, "0000", true } };
            snapshot->zeroPagePointers = { { 1, 0x0006, "$0300: A9 41 8D 00", true } };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (snapshot);

            Assert::AreEqual (4, window.GetWatchesPane().GetList()->GetRowCount(), L"the watch, the pointers' heading, the pointer, the add row");
            Assert::IsFalse  (window.GetWatchesPane().GetList()->GetCellsOfRow (0)[1].check != DxuiListView::Cell::Check::None, L"no check box on the watch");
            Assert::AreEqual (std::wstring (L"0"),   window.GetWatchesPane().GetList()->GetCellsOfRow (0)[1].text, L"the watch's number is its ID");
            Assert::AreEqual (std::wstring (L"ZP1"), window.GetWatchesPane().GetList()->GetCellsOfRow (2)[1].text);
            Assert::AreEqual (std::wstring (L"$06"), window.GetWatchesPane().GetList()->GetCellsOfRow (2)[2].text);
            Assert::AreEqual (std::wstring (L"$0300: A9 41 8D 00"), window.GetWatchesPane().GetList()->GetCellsOfRow (2)[3].text);
            Assert::IsFalse  (window.GetWatchesPane().GetList()->GetCellsOfRow (2)[1].check != DxuiListView::Cell::Check::None, L"nor on the pointer");
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WatchUndoTests
    //
    //  Adding and removing a watch are steps Ctrl+Z takes back: a removed
    //  watch comes back under the id it had, so in its place, and an added
    //  one goes again.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WatchUndoTests)
    {
    public:

        TEST_METHOD (ARemovedWatchComesBackUnderItsId)
        {
            CassoTheme                        theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                 host;
            WatchWindow                       window   (theme, host);
            DxuiDpiScaler                     scaler;
            auto                              snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::WatchLine   watch;



            watch.id         = 3;
            watch.expression = "X";
            watch.isRegister = true;
            watch.value      = "00";
            snapshot->watches = { watch };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (snapshot);

            Assert::AreEqual (std::wstring (L"Register"), static_cast<DxuiEditableGrid *> (window.GetWatchesPane().GetList())->GetCellsOfRow (FindRow (window, L"3"))[4].text, L"the Type column");

            RunMenuItem (window, FindRow (window, L"3"), L"Remove");

            Assert::IsTrue   (host.actions.back().command.verb == DebugVerb::ClearWatch, L"Remove clears the watch");
            Assert::AreEqual ((uint32_t) 3, host.actions.back().command.count);
            Assert::IsTrue   (window.GetWatchesPane().CanUndo (false), L"and can be undone");

            window.TakeSnapshot (std::make_shared<DebuggerViewSnapshot>());
            window.GetWatchesPane().Undo (false);

            Assert::IsTrue   (host.actions.back().command.verb == DebugVerb::AddWatch, L"the undo adds it back");
            Assert::AreEqual (std::string ("X"), host.actions.back().operand);
            Assert::AreEqual ((uint32_t) 4, host.actions.back().command.count, L"under id 3");

            window.GetWatchesPane().Undo (true);
            Assert::IsTrue   (host.actions.back().command.verb == DebugVerb::ClearWatch, L"the redo removes it again");
        }


        TEST_METHOD (AnAddedWatchIsUndoneOnceASnapshotListsIt)
        {
            CassoTheme                        theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                 host;
            WatchWindow                       window   (theme, host);
            DxuiDpiScaler                     scaler;
            auto                              after    = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::WatchLine   watch;



            (void) AddRowTests::BeginAdd (window, scaler);
            window.Type  (L"KBD");
            window.Press (VK_RETURN);
            window.Press (VK_ESCAPE);

            Assert::IsFalse (window.GetWatchesPane().CanUndo (false), L"nothing to undo before the watch is listed");

            watch.id         = 7;
            watch.expression = "KBD";
            after->watches   = { watch };
            window.TakeSnapshot (after);

            Assert::IsTrue (window.GetWatchesPane().CanUndo (false), L"the add can be undone");
            window.GetWatchesPane().Undo (false);

            Assert::IsTrue   (host.actions.back().command.verb == DebugVerb::ClearWatch, L"the undo removes it");
            Assert::AreEqual ((uint32_t) 7, host.actions.back().command.count);
        }

    private:

        static int FindRow (WatchWindow & window, const std::wstring & id)
        {
            DxuiEditableGrid  * grid = static_cast<DxuiEditableGrid *> (window.GetWatchesPane().GetList());



            for (int row = 0; row < grid->GetDataRowCount(); row++)
            {
                if (grid->GetCellsOfRow (row)[1].text == id)
                {
                    return row;
                }
            }

            Assert::Fail (L"no row with that id");
        }


        static void RunMenuItem (WatchWindow & window, int row, const std::wstring & label)
        {
            PaneMenuItems  items;



            (void) window.GetWatchesPane().AddMenuItems (window.GetWatchesPane().GetList(), row, items);

            for (const auto & [text, run] : items)
            {
                if (text == label)
                {
                    run();
                    return;
                }
            }

            Assert::Fail (L"no such menu item");
        }
    };
}
