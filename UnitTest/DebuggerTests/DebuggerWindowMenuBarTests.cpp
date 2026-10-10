#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "resource.h"

#include "CppUnitTest.h"
#include "Ui/Debugger/DebuggerCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerMenuBarTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuBarHost
    //
    //  A host that keeps the lines, actions and emulator commands the window
    //  sent it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuBarHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
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

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode mode) override
        {
            inModeLines.push_back (line);
            inModeModes.push_back (mode);
        }

        void  RunDebuggerAction (const DebuggerAction &) override { actions++; }

        void  RunEmulatorCommand (int commandId) override { emulatorCommands.push_back (commandId); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  inModeLines;
        std::vector<CommandMode>  inModeModes;
        std::vector<int>          emulatorCommands;
        int                       actions = 0;
        FakeHostDialogs           dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuBarWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuBarWindow : public DebuggerWindow
    {
    public:
        MenuBarWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyKeyScheme;
        using DebuggerWindow::SetWindowMenus;
        using DebuggerWindow::GetMenuBarItems;
        using DebuggerWindow::GetCommandBar;
        using DebuggerWindow::GetConsoleBar;
        using DebuggerWindow::GetPaneOfControl;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::kDialectEntry;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuRows
    //
    //  Finds a menu by its title and a row by its label.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuRows
    {
    public:
        static const std::vector<DxuiPopupMenuItem> *  GetMenu (const std::vector<DxuiMenuBarItem> & items, const std::wstring & title)
        {
            for (const DxuiMenuBarItem & item : items)
            {
                if (item.label == title)
                {
                    return &item.submenu;
                }
            }

            return nullptr;
        }


        static const DxuiPopupMenuItem *  GetRow (const std::vector<DxuiPopupMenuItem> & menu, const std::wstring & label)
        {
            for (const DxuiPopupMenuItem & row : menu)
            {
                if (row.command != nullptr && row.command->label == label)
                {
                    return &row;
                }
            }

            return nullptr;
        }


        static const DxuiPopupMenuItem *  GetRow (const std::vector<DxuiMenuBarItem> & items, const std::wstring & title, const std::wstring & label)
        {
            const std::vector<DxuiPopupMenuItem>  * menu = GetMenu (items, title);



            return (menu != nullptr) ? GetRow (*menu, label) : nullptr;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowMenuBarTests
    //
    //  The window's menu bar: File, Edit, View, Debug, Window, Tools and Help,
    //  with what the command bar's drop-downs used to hold.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowMenuBarTests)
    {
    public:

        //  Help held only Colors, which each pane's info button replaced, so
        //  there is no Help menu.
        TEST_METHOD (MenuBarHoldsFileEditMachineViewDebugAndTools)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            std::vector<std::wstring>  titles;



            window.OnCreate();

            for (const DxuiMenuBarItem & item : window.GetMenuBarItems())
            {
                titles.push_back (item.label);
            }

            Assert::AreEqual (6, (int) titles.size());
            Assert::AreEqual (std::wstring (L"&File"),    titles[0]);
            Assert::AreEqual (std::wstring (L"&Edit"),    titles[1]);
            Assert::AreEqual (std::wstring (L"&Machine"), titles[2]);
            Assert::AreEqual (std::wstring (L"&View"),    titles[3]);
            Assert::AreEqual (std::wstring (L"&Debug"),   titles[4]);
            Assert::AreEqual (std::wstring (L"&Tools"),   titles[5]);
        }


        TEST_METHOD (ViewMenuListsEveryDebugWindowAndShowsOne)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            DxuiDpiScaler                           scaler;
            const std::vector<DxuiPopupMenuItem>  * view   = nullptr;
            const DxuiPopupMenuItem               * row    = nullptr;
            size_t                                  rows   = 0;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            view = MenuRows::GetMenu (window.GetMenuBarItems(), L"&View");
            Assert::IsNotNull (view);

            for (const DxuiPopupMenuItem & item : *view)
            {
                bool  isPane = item.kind == DxuiPopupMenuItem::Kind::Command && item.command->label != L"Reset window layout";

                rows += (item.kind == DxuiPopupMenuItem::Kind::Submenu) ? item.children.size() : (isPane ? 1 : 0);
            }

            Assert::AreEqual (window.GetViewMenuPanes().size(), rows, L"a row for every debug window");

            row = MenuRows::GetRow (*view, L"Registers");
            Assert::IsNotNull (row);
            Assert::IsTrue    (row->command->IsChecked(), L"a shown window is checked");
        }


        TEST_METHOD (DebugRowsShowTheKeysOfTheSchemeInForce)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            const DxuiPopupMenuItem  * run = nullptr;



            window.OnCreate();

            run = MenuRows::GetRow (window.GetMenuBarItems(), L"&Debug", L"Run");
            Assert::IsNotNull (run);
            Assert::AreEqual  (DebuggerKeySchemes::GetMap (DebuggerKeyScheme::VisualStudio).GetChordText (DebuggerCommands::kRun), run->command->accelerator);

            window.ApplyKeyScheme (DebuggerKeyScheme::AppleWin);

            run = MenuRows::GetRow (window.GetMenuBarItems(), L"&Debug", L"Run");
            Assert::IsNotNull (run);
            Assert::AreEqual  (DebuggerKeySchemes::GetMap (DebuggerKeyScheme::AppleWin).GetChordText (DebuggerCommands::kRun), run->command->accelerator,
                               L"the rows follow a change of scheme");
        }


        TEST_METHOD (MachineRestartRowsReachTheEmulator)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);



            window.OnCreate();

            for (const wchar_t * label : { L"Reset", L"Power cycle" })
            {
                const DxuiPopupMenuItem  * row = MenuRows::GetRow (window.GetMenuBarItems(), L"&Machine", label);



                Assert::IsNotNull (row, label);
                row->command->dispatch();
            }

            Assert::AreEqual (2, (int) host.emulatorCommands.size());
            Assert::AreEqual ((int) IDM_MACHINE_RESET,      host.emulatorCommands[0]);
            Assert::AreEqual ((int) IDM_MACHINE_POWERCYCLE, host.emulatorCommands[1]);
        }


        TEST_METHOD (OpenSymbolFileLoadsThePickedFile)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            const DxuiPopupMenuItem  * row = nullptr;



            host.dialogs.path = L"C:\\games\\game.dbg";
            window.OnCreate();

            row = MenuRows::GetRow (window.GetMenuBarItems(), L"&File", L"Open symbol or debug file...");
            Assert::IsNotNull (row);
            row->command->dispatch();

            Assert::AreEqual (1, host.dialogs.opens);
            Assert::AreEqual (1, (int) host.inModeLines.size());
            Assert::AreEqual (std::string ("SYM LOAD \"C:\\games\\game.dbg\""), host.inModeLines[0]);
            Assert::IsTrue   (host.inModeModes[0] == CommandMode::AppleWin);
        }


        TEST_METHOD (ResetWindowLayoutPutsAFloatingPaneBack)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            DxuiDpiScaler              scaler;
            const DxuiPopupMenuItem  * reset  = nullptr;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (window.EditPaneLayout().Float (DebuggerLayout::kWatches, L"monitor", RECT { 100, 100, 500, 400 }));

            reset = MenuRows::GetRow (window.GetMenuBarItems(), L"&View", L"Reset window layout");
            Assert::IsNotNull (reset);
            reset->command->dispatch();

            Assert::IsFalse (window.GetPaneLayout().IsFloating (DebuggerLayout::kWatches));
            Assert::IsTrue  (window.GetPaneLayout().IsDocked   (DebuggerLayout::kWatches));
        }


        TEST_METHOD (ToolsKeyboardSchemeChoosesTheScheme)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            std::wstring               appleWin = DebuggerKeySchemes::GetMap (DebuggerKeyScheme::AppleWin).GetName();
            const DxuiPopupMenuItem  * keys     = nullptr;
            const DxuiPopupMenuItem  * run      = nullptr;



            window.OnCreate();

            keys = MenuRows::GetRow (window.GetMenuBarItems(), L"&Tools", L"Keyboard scheme");
            Assert::IsNotNull (keys);
            Assert::IsTrue    (keys->kind == DxuiPopupMenuItem::Kind::Submenu);
            Assert::IsNotNull (MenuRows::GetRow (keys->children, appleWin));

            MenuRows::GetRow (keys->children, appleWin)->command->dispatch();

            run = MenuRows::GetRow (window.GetMenuBarItems(), L"&Debug", L"Run");
            Assert::AreEqual (DebuggerKeySchemes::GetMap (DebuggerKeyScheme::AppleWin).GetChordText (DebuggerCommands::kRun), run->command->accelerator);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerCommandBarTests
    //
    //  The command bar holds the run and step buttons, as icons with tips, and
    //  the trace; find and the window's choices moved to the menu bar.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerCommandBarTests)
    {
    public:

        TEST_METHOD (CommandBarHoldsOnlyTheRunStepAndTraceEntries)
        {
            CassoTheme        theme    = CassoTheme::MakeSkeuomorphic();
            MenuBarHost       host;
            MenuBarWindow     window   (theme, host);
            std::vector<int>  expected = { DebuggerCommands::kRun, DebuggerCommands::kPause, DebuggerCommands::kStepInto, DebuggerCommands::kStepOver,
                                           DebuggerCommands::kStepOut, DebuggerCommands::kRunToCursor,
                                           DebuggerCommands::kStepBackInto, DebuggerCommands::kStepBackOver, DebuggerCommands::kStepBackOut,
                                           DebuggerCommands::kReverseContinue, DebuggerCommands::kShowNext, DebuggerCommands::kTrace };
            std::vector<int>  ids;



            window.OnCreate();

            //  The strip ends with its own See more entry, which is no command
            //  of ours.
            for (int i = 0; i + 1 < window.GetCommandBar()->GetEntryCount(); i++)
            {
                ids.push_back (window.GetCommandBar()->GetEntryCommandId (i));
            }

            Assert::IsTrue (ids == expected, L"no Find, View, Panels, Dialect or Keys on the command bar");
        }


        TEST_METHOD (RunAndStepButtonsAreIconsAlone)
        {
            DebuggerCommands  commands ({});



            for (const DxuiToolbar::Entry & entry : commands.BuildEntries())
            {
                bool  isTrace = entry.command->id == DebuggerCommands::kTrace;



                Assert::AreEqual (!isTrace, entry.iconOnly, entry.command->label.c_str());
            }
        }


        TEST_METHOD (EachTipGivesTheCommandAndItsKey)
        {
            DebuggerCommands  commands ({});
            std::wstring      key      = DebuggerKeySchemes::GetMap (DebuggerKeyScheme::VisualStudio).GetChordText (DebuggerCommands::kStepOver);



            commands.ApplyKeyScheme (DebuggerKeyScheme::VisualStudio);

            Assert::IsFalse (key.empty());
            Assert::IsTrue  (commands.Find (DebuggerCommands::kStepOver)->tip.starts_with (L"Step over (" + key + L")"));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerConsoleBarTests
    //
    //  The dialect governs the console, so it lives in the console pane's own
    //  toolbar and goes wherever the pane goes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerConsoleBarTests)
    {
    public:

        TEST_METHOD (ConsoleBarHoldsTheDialect)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);



            window.OnCreate();

            Assert::IsNotNull (window.GetConsoleBar());
            Assert::AreEqual  (MenuBarWindow::kDialectEntry, window.GetConsoleBar()->GetEntryCommandId (1), L"after the Mode: label");
        }


        TEST_METHOD (ConsoleBarGoesWithTheConsolePane)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            MenuBarHost    host;
            MenuBarWindow  window (theme, host);
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), window.GetPaneOfControl (window.GetConsoleBar()));
            Assert::IsTrue   (window.GetConsoleBar()->IsVisible(), L"the bar shows with the console");
        }
    };
}





