#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerColorKeyTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColorKeyHost
    //
    //  A host that does nothing; the tests read only the window's own state.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ColorKeyHost : public IDebuggerWindowHost
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

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
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
    //  ColorKeyWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ColorKeyWindow : public DebuggerWindow
    {
    public:
        ColorKeyWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::GetHeatMapBar;
        using DebuggerWindow::GetColorKey;
        using DebuggerWindow::GetColorKeyPopup;
        using DebuggerWindow::GetPaneOfControl;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerColorKeyTests
    //
    //  Every pane whose colors mean something has an info button at the end of
    //  its toolbar. Resting the pointer on it shows that pane's key alone and
    //  leaving takes it away; a click, or Space or Enter on the focused button,
    //  holds it open until the same again, Escape, or the focus moving on.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerColorKeyTests)
    {
    public:

        static constexpr int  kWidth  = 1400;
        static constexpr int  kHeight = 900;


        static void  Build (ColorKeyWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);

            for (const wchar_t * pane : { DebuggerLayout::kHeatMap, DebuggerLayout::kCallStack, DebuggerLayout::kBreakpoints, DebuggerLayout::kWatches })
            {
                window.ShowPane (pane);
            }

            window.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }


        static void  Send (ColorKeyWindow & window, DxuiMouseEventKind kind, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;

            (void) window.OnMouse (ev);
        }


        static void  Press (ColorKeyWindow & window, WPARAM vk)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, false, false, false };



            (void) window.OnKey (ev);
        }


        static POINT  GetCenter (const RECT & rect)
        {
            return POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        }


        TEST_METHOD (EveryColoredPaneHasItsOwnKey)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            ColorKeyHost    host;
            ColorKeyWindow  window (theme, host);



            Build (window);

            for (const auto & [pane, legend] : { std::pair { DebuggerLayout::GetCodePaneId (0),         ColorLegend::Pane::Disassembly },
                                                 std::pair { DebuggerLayout::GetSourcePaneId (0),       ColorLegend::Pane::Source      },
                                                 std::pair { std::wstring (DebuggerLayout::kRegisters),   ColorLegend::Pane::Registers   },
                                                 std::pair { std::wstring (DebuggerLayout::kWatches),     ColorLegend::Pane::Watch       },
                                                 std::pair { std::wstring (DebuggerLayout::kStack),       ColorLegend::Pane::Stack       },
                                                 std::pair { DebuggerLayout::GetMemoryPaneId (1),       ColorLegend::Pane::Memory      },
                                                 std::pair { std::wstring (DebuggerLayout::kHeatMap),     ColorLegend::Pane::HeatMap     },
                                                 std::pair { std::wstring (DebuggerLayout::kBreakpoints), ColorLegend::Pane::Breakpoints } })
            {
                ColorKeyButton  * key = window.GetColorKey (pane);

                Assert::IsNotNull (key, pane.c_str());
                Assert::IsTrue    (key->GetLegend() == legend, pane.c_str());
                Assert::IsTrue    (key->IsFocusable(), L"Tab could not reach the button");
                Assert::AreEqual  (pane, window.GetPaneOfControl (key), L"the button would not go with its pane into a floating window");
            }

            Assert::IsNull (window.GetColorKey (DebuggerLayout::kConsole), L"the console has no colors to explain");
            Assert::IsNull (window.GetColorKey (DebuggerLayout::kCallStack), L"the call stack's rows say what their colors mean");
        }


        TEST_METHOD (TheButtonEndsTheToolbarWithoutCoveringIt)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            ColorKeyHost      host;
            ColorKeyWindow    window (theme, host);
            ColorKeyButton  * key    = nullptr;
            RECT              button = {};
            RECT              bar    = {};



            Build (window);

            key    = window.GetColorKey (DebuggerLayout::kHeatMap);
            button = key->GetBounds();
            bar    = window.GetHeatMapBar()->GetBounds();

            Assert::IsTrue   (key->IsVisible(), L"the heat map's button is not shown");
            Assert::AreEqual (bar.right, button.left, L"the toolbar runs up to the button");
            Assert::AreEqual (bar.top,   button.top,  L"in the toolbar's band");
            Assert::AreEqual (bar.bottom - bar.top, button.bottom - button.top);
        }


        TEST_METHOD (RestingOnTheButtonShowsThatPanesKeyAlone)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            ColorKeyHost      host;
            ColorKeyWindow    window (theme, host);
            ColorKeyButton  * key    = nullptr;



            Build (window);
            key = window.GetColorKey (DebuggerLayout::kHeatMap);

            Send (window, DxuiMouseEventKind::Move, GetCenter (key->GetBounds()));

            Assert::IsTrue   (window.GetColorKeyPopup().IsShown(), L"no key over the button");
            Assert::IsFalse  (window.GetColorKeyPopup().IsHeld(), L"a resting pointer holds nothing");
            Assert::IsTrue   (window.GetColorKeyPopup().GetPane() == ColorLegend::Pane::HeatMap);
            Assert::AreEqual (ColorLegend::GetEntriesFor (ColorLegend::Pane::HeatMap).size(), window.GetColorKeyPopup().GetRows().size(), L"another pane's lines in the key");

            Send (window, DxuiMouseEventKind::Move, POINT { 5, kHeight / 2 });
            Assert::IsFalse  (window.GetColorKeyPopup().IsShown(), L"the key stayed after the pointer left");

            key = window.GetColorKey (DebuggerLayout::kBreakpoints);
            Assert::IsTrue   (key->IsVisible(), L"the breakpoints' button is not shown");

            Send (window, DxuiMouseEventKind::Move, GetCenter (key->GetBounds()));
            Assert::IsTrue   (window.GetColorKeyPopup().GetPane() == ColorLegend::Pane::Breakpoints);
        }


        TEST_METHOD (AClickHoldsTheKeyAndASecondClosesIt)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            ColorKeyHost      host;
            ColorKeyWindow    window (theme, host);
            ColorKeyButton  * key    = nullptr;
            POINT             at     = {};



            Build (window);
            key = window.GetColorKey (DebuggerLayout::kBreakpoints);
            at  = GetCenter (key->GetBounds());

            Send (window, DxuiMouseEventKind::Move, at);
            Send (window, DxuiMouseEventKind::Down, at);
            Send (window, DxuiMouseEventKind::Up,   at);

            Assert::IsTrue  (window.GetColorKeyPopup().IsHeld(), L"a click does not hold the key");

            Send (window, DxuiMouseEventKind::Move, POINT { 5, kHeight / 2 });
            Assert::IsTrue  (window.GetColorKeyPopup().IsShown(), L"a held key went with the pointer");

            Send (window, DxuiMouseEventKind::Move, at);
            Send (window, DxuiMouseEventKind::Down, at);
            Send (window, DxuiMouseEventKind::Up,   at);
            Assert::IsFalse (window.GetColorKeyPopup().IsHeld(), L"a second click left it held");
        }


        TEST_METHOD (SpaceAndEnterOnTheFocusedButtonOpenAndCloseTheKey)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            ColorKeyHost      host;
            ColorKeyWindow    window (theme, host);
            ColorKeyButton  * key    = nullptr;



            Build (window);
            key = window.GetColorKey (DebuggerLayout::kWatches);

            window.FocusControl (key);
            Assert::IsTrue  (key->IsFocused(), L"the button cannot take the focus");

            Press (window, VK_SPACE);
            Assert::IsTrue  (window.GetColorKeyPopup().IsShown() && window.GetColorKeyPopup().IsHeld(), L"Space does not open the key");
            Assert::IsTrue  (window.GetColorKeyPopup().GetPane() == ColorLegend::Pane::Watch);

            Press (window, VK_SPACE);
            Assert::IsFalse (window.GetColorKeyPopup().IsShown(), L"Space again does not close it");

            Press (window, VK_RETURN);
            Assert::IsTrue  (window.GetColorKeyPopup().IsShown(), L"Enter does not open the key");

            Press (window, VK_ESCAPE);
            Assert::IsFalse (window.GetColorKeyPopup().IsShown(), L"Escape does not close it");

            Press (window, VK_RETURN);
            window.FocusControl (window.GetCommandBox());
            Assert::IsFalse (window.GetColorKeyPopup().IsShown(), L"the key stayed after the focus moved on");
        }


        //  A device panel's key follows what it shows: a memory map's sources,
        //  a disk head's states, and nothing for meters or a list alone, when
        //  its band and button go.
        TEST_METHOD (ADevicePanelsKeyFollowsItsGraphic)
        {
            DxuiListView         list;
            MemoryMapBar         map;
            DiskHeadView         head;
            MeterBar             meters;
            DiagnosticsPane      pane ("mmu", L"MMU", &list, &map, &head, &meters);
            DiagnosticsSnapshot  snapshot;



            Assert::IsFalse (pane.GetColorKey().has_value(), L"a list alone has a key");

            snapshot.visual = DiagnosticsMemoryMap {};
            (void) pane.Apply (snapshot);
            Assert::IsTrue  (pane.GetColorKey() == ColorLegend::Pane::MemoryMap);

            snapshot.visual = DiagnosticsDiskHead {};
            (void) pane.Apply (snapshot);
            Assert::IsTrue  (pane.GetColorKey() == ColorLegend::Pane::DiskHead);

            snapshot.visual = DiagnosticsMeters {};
            (void) pane.Apply (snapshot);
            Assert::IsFalse (pane.GetColorKey().has_value(), L"meters have a key");

            Assert::IsTrue  (pane.GetControls().front() == pane.GetKeySlot(), L"the band is not the pane's top part");
        }
    };
}
