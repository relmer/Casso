#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Debugger/HeatMapSymbols.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace HeatMapAccessWindowTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AccessHost
    //
    //  A host that keeps the heat map requests and options it is sent.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class AccessHost : public IDebuggerWindowHost
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

        std::string  GetDebuggerHeatMapOptions()                            override { return heatMapOptions; }
        void         SetDebuggerHeatMapOptions  (const std::string & text)  override { heatMapOptions = text; }
        void         SendDebuggerHeatMapRequest (const std::string & words) override { requests.push_back (words); }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<std::string>  requests;
        std::string               heatMapOptions;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AccessWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class AccessWindow : public DebuggerWindow
    {
    public:
        AccessWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::SyncHeatMapRecording;
        using DebuggerWindow::GetHeatMapView;
        using DebuggerWindow::GetHeatMapBankCommands;
        using DebuggerWindow::GetHeatMapBarLabel;
        using DebuggerWindow::IsHeatMapBarEnabled;
        using DebuggerWindow::SetHeatMapBarMenus;
        using DebuggerWindow::AddHeatMapAccessItems;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::OnMouse;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HeatMapAccessWindowTests
    //
    //  The window's side of the heat map's banks and last accesses: the cell
    //  under the mouse and a Ctrl+click's request go to the machine as words,
    //  the bar's bank drop-down lists the banks the machine has, and a menu
    //  on a cell or a byte has rows for its last write and read.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HeatMapAccessWindowTests)
    {
    public:

        using Bank = HeatMapOptions::Bank;

        static void BuildShown (AccessWindow & window)
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ShowPane (DebuggerLayout::kHeatMap);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.SyncHeatMapRecording();
        }

        static DxuiMouseEvent MakeEvent (DxuiMouseEventKind kind, POINT point, bool ctrl = false, bool shift = false, bool alt = false)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Move) ? DxuiMouseButton::None : DxuiMouseButton::Left;
            ev.positionDip = point;
            ev.ctrl        = ctrl;
            ev.shift       = shift;
            ev.alt         = alt;
            return ev;
        }

        static void Click (AccessWindow & window, POINT at, bool ctrl, bool shift, bool alt)
        {
            (void) window.OnMouse (MakeEvent (DxuiMouseEventKind::Move, at));
            (void) window.OnMouse (MakeEvent (DxuiMouseEventKind::Down, at, ctrl, shift, alt));
            (void) window.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   at, ctrl, shift, alt));
        }

        static std::shared_ptr<DebuggerViewSnapshot> MakeSnapshot (const std::vector<Bank> & banks)
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->machine       = "Apple2e";
            snapshot->heatMap.banks = banks;
            return snapshot;
        }



        TEST_METHOD (ACtrlClickOnACellAsksTheMachineForItsLastAccess)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            AccessHost      host;
            AccessWindow    window (theme, host);
            HeatMapView   * view   = nullptr;
            RECT            cell   = {};
            POINT           at     = {};
            Word            target = 0;



            BuildShown (window);
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 2 + 0x13);
            cell   = view->GetCellRect (target);
            at     = { cell.left + 1, cell.top + 1 };

            Click (window, at, true, false, false);
            Click (window, at, true, true,  true);

            Assert::IsTrue (std::ranges::find (host.requests, std::format ("code write cpu {:04X}",  target)) != host.requests.end(), L"Ctrl+click: show the last writer");
            Assert::IsTrue (std::ranges::find (host.requests, std::format ("rewind read cpu {:04X}", target)) != host.requests.end(), L"Ctrl+Alt+Shift+click: go back to the last read");
        }



        TEST_METHOD (TheCellUnderTheMouseIsSentAsItChanges)
        {
            CassoTheme      theme  = CassoTheme::MakeSkeuomorphic();
            AccessHost      host;
            AccessWindow    window (theme, host);
            HeatMapView   * view   = nullptr;
            RECT            cell   = {};
            Word            target = 0;



            BuildShown (window);
            view   = window.GetHeatMapView();
            target = (Word) (view->GetColumns() * 3 + 5);
            cell   = view->GetCellRect (target);

            (void) window.OnMouse (MakeEvent (DxuiMouseEventKind::Move, { cell.left + 1, cell.top + 1 }));
            Assert::IsFalse  (host.requests.empty());
            Assert::AreEqual (std::format ("hover {:04X}", target), host.requests.back());

            (void) window.OnMouse (MakeEvent (DxuiMouseEventKind::Move, { view->GetBounds().left + 2, view->GetBounds().top + 2 }));
            Assert::AreEqual (std::string ("hover none"), host.requests.back(), L"off the cells, no cell");
        }



        TEST_METHOD (TheBankDropDownListsTheBanksTheMachineHasAndAChoiceIsKept)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            AccessHost                 host;
            AccessWindow               window (theme, host);
            std::vector<std::wstring>  labels;



            BuildShown (window);

            window.TakeSnapshot (MakeSnapshot ({ Bank::Cpu, Bank::Main, Bank::Rom }));
            window.SetHeatMapBarMenus();
            Assert::IsFalse  (window.IsHeatMapBarEnabled (HeatMapBarCommands::kBank), L"nothing banked, nothing to choose");

            window.TakeSnapshot (MakeSnapshot ({ Bank::Cpu, Bank::Main, Bank::Aux, Bank::LanguageCard, Bank::AuxLanguageCard, Bank::Rom }));
            window.SetHeatMapBarMenus();
            Assert::IsTrue   (window.IsHeatMapBarEnabled (HeatMapBarCommands::kBank));
            Assert::AreEqual (std::wstring (L"CPU"), window.GetHeatMapBarLabel (HeatMapBarCommands::kBank));

            for (const std::shared_ptr<DxuiCommand> & row : window.GetHeatMapBankCommands())
            {
                labels.push_back (row->label);
            }

            Assert::IsTrue ((std::vector<std::wstring> { L"CPU", L"Main RAM", L"Aux RAM", L"Language card", L"Aux language card", L"ROM" }) == labels);

            window.GetHeatMapBankCommands()[2]->dispatch();

            Assert::AreEqual ((int) Bank::Aux, (int) window.GetHeatMapView()->GetOptions().bank);
            Assert::AreEqual (std::string ("fade=10 view=all bank=aux"), host.heatMapOptions, L"kept");
            Assert::AreEqual (std::wstring (L"Aux RAM"), window.GetHeatMapBarLabel (HeatMapBarCommands::kBank));

            window.TakeSnapshot (MakeSnapshot ({ Bank::Cpu, Bank::Main, Bank::Rom }));
            Assert::AreEqual (std::wstring (L"CPU"), window.GetHeatMapBarLabel (HeatMapBarCommands::kBank), L"a machine without aux shows the CPU's");
        }



        TEST_METHOD (AMenuOnACellHasRowsForItsLastWriteAndRead)
        {
            CassoTheme                                                    theme  = CassoTheme::MakeSkeuomorphic();
            AccessHost                                                    host;
            AccessWindow                                                  window (theme, host);
            std::vector<std::pair<std::wstring, std::function<void()>>>  items;
            std::vector<std::wstring>                                     labels;



            BuildShown (window);
            host.requests.clear();
            window.AddHeatMapAccessItems (0xC123, Bank::Main, items);

            for (const auto & [label, action] : items)
            {
                labels.push_back (label);
            }

            Assert::IsTrue ((std::vector<std::wstring> { L"Show last writer", L"Show last reader", L"Go to last write", L"Go to last read" }) == labels);

            items[2].second();
            items[1].second();

            Assert::AreEqual ((size_t) 2, host.requests.size());
            Assert::AreEqual (std::string ("rewind write main C123"), host.requests[0]);
            Assert::AreEqual (std::string ("code read main C123"),    host.requests[1]);
        }



        TEST_METHOD (TheSnapshotsPcStackBreakpointsAndSymbolsReachTheMap)
        {
            CassoTheme                              theme    = CassoTheme::MakeSkeuomorphic();
            AccessHost                              host;
            AccessWindow                            window (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>   snapshot = MakeSnapshot ({ Bank::Cpu, Bank::Main, Bank::Rom });
            std::shared_ptr<HeatMapSymbols>         symbols  = std::make_shared<HeatMapSymbols>();
            DebuggerViewSnapshot::BreakpointLine    line;
            HeatMapView                           * view     = nullptr;



            BuildShown (window);

            symbols->Add ("COUT", 0xFDED);

            line.id           = 1;
            line.address      = 0x0300;
            line.info.id      = 1;
            line.info.kind    = BreakpointKind::Address;
            line.info.address = 0x0300;
            snapshot->breakpoints.push_back (line);

            line.id           = 2;
            line.info.id      = 2;
            line.info.address = 0x0500;
            line.info.enabled = false;
            snapshot->breakpoints.push_back (line);

            snapshot->heatMap.pc     = Word (0x0300);
            snapshot->heatMap.stack  = Word (0x01FF);
            snapshot->heatMapSymbols = symbols;

            window.TakeSnapshot (snapshot);
            view = window.GetHeatMapView();

            Assert::AreEqual ((size_t) 2, view->GetOutlineColors (0x0300).size(), L"the PC and its breakpoint");
            Assert::AreEqual ((size_t) 1, view->GetOutlineColors (0x01FF).size(), L"the stack pointer");
            Assert::IsTrue   (view->GetOutlineColors (0x0500).empty(), L"a disabled breakpoint is not outlined");
            Assert::IsTrue   (view->GetTipText (0x0300).find (L"\nBreakpoint #1") != std::wstring::npos);
            Assert::IsTrue   (view->GetTipText (0xFDED).find (L"\nSymbol COUT")   != std::wstring::npos);
        }
    };
}
