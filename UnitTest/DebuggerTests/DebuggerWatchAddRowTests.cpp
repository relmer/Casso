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
        void  RunDebuggerAction       (const DebuggerAction & action)   override { commands.push_back (action.echo); }

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
        //  ends the edit.
        TEST_METHOD (EnterOnANameAddsTheWatchAndEndsTheEdit)
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
            Assert::IsFalse  (grid->IsEditing(), L"the edit is still open");
        }


        //  An empty name takes neither Enter nor Tab.
        TEST_METHOD (AnEmptyNameTakesNeitherEnterNorTab)
        {
            CassoTheme          theme  = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost   host;
            WatchWindow         window (theme, host);
            DxuiDpiScaler       scaler;
            DxuiEditableGrid  * grid   = BeginAdd (window, scaler);



            window.Press (VK_TAB);
            window.Press (VK_RETURN);

            Assert::IsTrue   (host.commands.empty());
            Assert::IsTrue   (grid->IsEditing(), L"the edit closed");
            Assert::AreEqual (1, grid->GetEditColumn(), L"Tab moved to the value");
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


        //  Each watch and zero-page pointer has a check that enables or
        //  disables it; a pointer's row has where it points and the bytes there.
        TEST_METHOD (WatchesAndPointersHaveChecksAndPointersTheirTargets)
        {
            CassoTheme                          theme    = CassoTheme::MakeSkeuomorphic();
            QuietDebuggerHost                   host;
            WatchWindow                         window   (theme, host);
            DxuiDpiScaler                       scaler;
            auto                                snapshot = std::make_shared<DebuggerViewSnapshot>();
            RECT                                check    = {};
            DxuiMouseEvent                      press;



            snapshot->watches          = { { 0, 0x0300, "0000", true } };
            snapshot->zeroPagePointers = { { 1, 0x0006, "$0300: A9 41 8D 00", false } };

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);
            window.TakeSnapshot (snapshot);
            static_cast<DxuiEditableGrid *> (window.GetWatchesPane().GetList())->SetGridColumns ({ { { L"ID",    40, false, DxuiTextHAlign::Left }, false, false },
                                                                                    { { L"Watch", 160, false, DxuiTextHAlign::Left }, true, true },
                                                                                    { { L"Value", 200, false, DxuiTextHAlign::Left }, true, false } });

            Assert::AreEqual (4, window.GetWatchesPane().GetList()->GetRowCount(), L"the watch, the pointers' heading, the pointer, the add row");
            Assert::IsTrue   (window.GetWatchesPane().GetList()->GetCellsOfRow (0)[1].check.value_or (false), L"the watch is on");
            Assert::AreEqual (std::wstring (L"0"),   window.GetWatchesPane().GetList()->GetCellsOfRow (0)[1].text, L"the watch's number is its ID");
            Assert::AreEqual (std::wstring (L"ZP1"), window.GetWatchesPane().GetList()->GetCellsOfRow (2)[1].text);
            Assert::AreEqual (std::wstring (L"$06"), window.GetWatchesPane().GetList()->GetCellsOfRow (2)[2].text);
            Assert::AreEqual (std::wstring (L"$0300: A9 41 8D 00"), window.GetWatchesPane().GetList()->GetCellsOfRow (2)[3].text);
            Assert::IsFalse  (window.GetWatchesPane().GetList()->GetCellsOfRow (2)[1].check.value_or (true), L"the pointer is off");

            Assert::IsTrue   (window.GetWatchesPane().GetList()->GetCheckRectPx (0, 1, check), L"the watch's check is in view");
            press.kind        = DxuiMouseEventKind::Down;
            press.button      = DxuiMouseButton::Left;
            press.positionDip = { (check.left + check.right) / 2, (check.top + check.bottom) / 2 };
            (void) window.GetWatchesPane().GetList()->OnMouse (press);

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands.back().starts_with ("WD 0"), L"its check disables the watch");

            window.GetWatchesPane().GetList()->EnsureVisible (2);
            Assert::IsTrue   (window.GetWatchesPane().GetList()->GetCheckRectPx (2, 1, check), L"the pointer's check is in view");
            press.positionDip = { (check.left + check.right) / 2, (check.top + check.bottom) / 2 };
            (void) window.GetWatchesPane().GetList()->OnMouse (press);

            Assert::IsTrue   (host.commands.back().starts_with ("ZPE 1"), L"and the pointer's enables it");
        }
    };
}
