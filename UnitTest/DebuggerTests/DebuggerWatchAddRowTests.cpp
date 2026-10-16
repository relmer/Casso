#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

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
        using DebuggerWindow::BeginWatchEdit;
        using DebuggerWindow::EndWatchEdit;
        using DebuggerWindow::GetWatchList;
        using DebuggerWindow::TakeSnapshot;

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
            window.GetWatchList()->SetColumns ({ { L"Watch", 120, false, DxuiTextHAlign::Left },
                                                 { L"Value", 120, false, DxuiTextHAlign::Left } });

            Assert::AreEqual (2, window.GetWatchList()->GetRowCount(), L"the watch, then the add row");
            last = window.GetWatchList()->GetRowCount() - 1;
            Assert::AreEqual (std::wstring (L"Add item to watch"), window.GetWatchList()->GetCellsOfRow (last)[0].text);

            window.BeginWatchEdit (last, 0);
            window.Type (L"400");
            window.EndWatchEdit (true);

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands.front().find ("400") != std::string::npos, L"the typed expression is watched");
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
            window.GetWatchList()->SetColumns ({ { L"Watch", 160, false, DxuiTextHAlign::Left },
                                                 { L"Value", 200, false, DxuiTextHAlign::Left } });

            Assert::AreEqual (4, window.GetWatchList()->GetRowCount(), L"the watch, the pointers' heading, the pointer, the add row");
            Assert::IsTrue   (window.GetWatchList()->GetCellsOfRow (0)[0].check.value_or (false), L"the watch is on");
            Assert::AreEqual (std::wstring (L"ZP1 $06"), window.GetWatchList()->GetCellsOfRow (2)[0].text);
            Assert::AreEqual (std::wstring (L"$0300: A9 41 8D 00"), window.GetWatchList()->GetCellsOfRow (2)[1].text);
            Assert::IsFalse  (window.GetWatchList()->GetCellsOfRow (2)[0].check.value_or (true), L"the pointer is off");

            Assert::IsTrue   (window.GetWatchList()->GetCheckRectPx (0, 0, check), L"the watch's check is in view");
            press.kind        = DxuiMouseEventKind::Down;
            press.button      = DxuiMouseButton::Left;
            press.positionDip = { (check.left + check.right) / 2, (check.top + check.bottom) / 2 };
            (void) window.GetWatchList()->OnMouse (press);

            Assert::AreEqual ((size_t) 1, host.commands.size());
            Assert::IsTrue   (host.commands.back().starts_with ("WD 0"), L"its check disables the watch");

            window.GetWatchList()->EnsureVisible (2);
            Assert::IsTrue   (window.GetWatchList()->GetCheckRectPx (2, 0, check), L"the pointer's check is in view");
            press.positionDip = { (check.left + check.right) / 2, (check.top + check.bottom) / 2 };
            (void) window.GetWatchList()->OnMouse (press);

            Assert::IsTrue   (host.commands.back().starts_with ("ZPE 1"), L"and the pointer's enables it");
        }
    };
}
