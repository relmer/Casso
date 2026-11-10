#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerPaneMenuTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuHost
    //
    //  A host that finds main.a65 and adds each address the window moves a
    //  code view to, none for the PC, to addresses.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuHost : public NullDebuggerWindowHost
    {
    public:
        void  SetDebuggerCodeAddress (std::optional<Word> address, int) override { addresses.push_back (address); }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;



            lookup.match = SourceMatch::Exact;
            lookup.path  = L"C:\\Work\\main.a65";
            lookup.text  = "; main\n\n\n\ndraw    lda #$41\n        rts\n";
            return lookup;
        }

        std::vector<std::optional<Word>>  addresses;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MenuWindow
    //
    //  The debugger window with its controls built and no HWND, which adds the
    //  pane of each Dock to menu it opens to dockMenus.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class MenuWindow : public DebuggerWindow
    {
    public:
        MenuWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;

            OnCreate();
            Relayout();
        }

        void  Relayout()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }

        void  ShowDockToMenu (const std::wstring & pane, POINT) override { dockMenus.push_back (pane); }

        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::GetSourceGroup;
        using DebuggerWindow::AddGoToDefinition;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::GetFocused;
        using DebuggerWindow::FindPaneForTest;

        std::vector<std::wstring>  dockMenus;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DockToMenuTests
    //
    //  A right-click over a pane's content opens the content's menu or none;
    //  the Dock to menu opens from the pane's tab or title bar, or from the
    //  tab of a pane hidden on an edge.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DockToMenuTests)
    {
    public:
        static void  RightClick (MenuWindow & window, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.button      = DxuiMouseButton::Right;
            ev.positionDip = at;

            for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
            {
                ev.kind = kind;
                (void) window.OnMouse (ev);
            }
        }


        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshot()
        {
            auto  snapshot = std::make_shared<DebuggerViewSnapshot>();



            snapshot->isPaused    = true;
            snapshot->codeOpen[0] = true;
            return snapshot;
        }


        //  Over the Registers pane's content: no Dock to menu. Over its title
        //  bar, above the content: the Dock to menu.
        TEST_METHOD (TheDockToMenuOpensFromTheTitleBarAndNotTheContent)
        {
            CassoTheme       theme   = CassoTheme::MakeSkeuomorphic();
            MenuHost         host;
            MenuWindow       window  (theme, host);
            IDxuiControl   * content = nullptr;
            RECT             group   = {};
            RECT             inner   = {};



            window.TakeSnapshot (MakeSnapshot());

            content = window.GetDockSite()->GetPaneContent (DebuggerLayout::kRegisters);
            Assert::IsNotNull (content, L"the Registers pane is docked");
            Assert::IsTrue    (window.GetDockSite()->TryGetPaneGroupRect (DebuggerLayout::kRegisters, group));

            inner = content->GetBounds();
            Assert::IsTrue (inner.top > group.top, L"the title bar is above the content");

            RightClick (window, POINT { (inner.left + inner.right) / 2, (inner.top + inner.bottom) / 2 });
            Assert::IsTrue (window.dockMenus.empty(), L"no Dock to menu over the content");

            RightClick (window, POINT { group.left + 20, (group.top + inner.top) / 2 });
            Assert::AreEqual ((size_t) 1, window.dockMenus.size(), L"the title bar's Dock to menu");
            Assert::AreEqual (std::wstring (DebuggerLayout::kRegisters), window.dockMenus.back());
        }


        //  Over a code view's tab: the Dock to menu; over its rows: none.
        TEST_METHOD (TheDockToMenuOpensFromATabAndNotTheRows)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            MenuHost         host;
            MenuWindow       window (theme, host);
            std::wstring     code   = DebuggerLayout::GetCodePaneId (0);
            std::wstring     tip;
            RECT             group  = {};
            RECT             tab    = {};
            RECT             rows   = {};
            bool             isTab  = false;



            window.TakeSnapshot (MakeSnapshot());

            Assert::IsTrue (window.GetDockSite()->TryGetPaneGroupRect (code, group));

            for (LONG y = group.top; y < group.bottom && !isTab; y++)
            {
                for (LONG x = group.left; x < group.right && !isTab; x += 4)
                {
                    isTab = window.GetDockSite()->GetTabAt (POINT { x, y }, tab, tip) == code;
                }
            }

            Assert::IsTrue (isTab, L"the code view has a tab");

            rows = window.GetCodeList (0)->GetBounds();
            RightClick (window, POINT { (rows.left + rows.right) / 2, (rows.top + rows.bottom) / 2 });
            Assert::IsTrue (window.dockMenus.empty(), L"no Dock to menu over the rows");

            RightClick (window, POINT { (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2 });
            Assert::AreEqual ((size_t) 1, window.dockMenus.size(), L"the tab's Dock to menu");
            Assert::AreEqual (code, window.dockMenus.back());
        }


        //  A pane hidden on an edge has the Dock to menu on its edge tab,
        //  also where the tab lies over the place its content had while it
        //  was docked: the hidden content isn't under the pointer. The first
        //  code view, hidden on the left, has its tab there.
        TEST_METHOD (TheDockToMenuOpensFromAnEdgeTab)
        {
            CassoTheme       theme   = CassoTheme::MakeSkeuomorphic();
            MenuHost         host;
            MenuWindow       window  (theme, host);
            IDxuiControl   * content = nullptr;
            RECT             tab     = {};
            POINT            middle  = {};



            window.TakeSnapshot (MakeSnapshot());

            content = window.GetDockSite()->GetPaneContent (DebuggerLayout::kCode);
            Assert::IsNotNull (content, L"the first code view is docked");
            Assert::IsTrue    (window.GetDockSite()->EditPaneLayout().AutoHide (DebuggerLayout::kCode, DxuiDockSide::Left), L"hidden on the left");
            window.Relayout();

            tab    = window.GetDockSite()->GetEdgeTabRect (DebuggerLayout::kCode);
            middle = POINT { (tab.left + tab.right) / 2, (tab.top + tab.bottom) / 2 };
            Assert::IsTrue (DxuiDockSite::Contains (content->GetBounds(), middle), L"the edge tab lies over the content's docked place");
            Assert::IsFalse (content->IsVisible(), L"the hidden content isn't drawn");

            RightClick (window, middle);
            Assert::AreEqual ((size_t) 1, window.dockMenus.size(), L"the edge tab's Dock to menu");
            Assert::AreEqual (std::wstring (DebuggerLayout::kCode), window.dockMenus.back());
        }

        //  A right-click on a row of a pane without the focus selects that
        //  row and moves the focus to the list, so the row the menu acts on is
        //  drawn selected.
        TEST_METHOD (ARightClickOnARowSelectsItAndFocusesItsList)
        {
            CassoTheme                              theme    = CassoTheme::MakeSkeuomorphic();
            MenuHost                                host;
            MenuWindow                              window   (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>   snapshot = MakeSnapshot();
            DxuiListView                          * list     = nullptr;
            RECT                                    rows     = {};
            int                                     rowH     = 0;



            snapshot->registers = { { "A", "00" }, { "X", "05" }, { "Y", "0A" } };
            window.TakeSnapshot (snapshot);

            list = window.FindPaneForTest (DebuggerLayout::kRegisters)->GetLists()[0];
            Assert::IsTrue (window.GetFocused() != list, L"the Registers list doesn't have the focus");

            rows = list->GetBounds();
            rowH = list->GetRowHeightDip();
            RightClick (window, POINT { rows.left + 20, rows.top + list->GetHeaderHeightPx() + rowH + rowH / 2 });

            Assert::IsTrue (list->IsRowSelected (1),   L"the row clicked is selected");
            Assert::IsTrue (window.GetFocused() == list, L"and its list has the focus");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  GoToDefinitionTests
    //
    //  Over a symbol in a code view, the content menu has Go to definition
    //  first, except for a constant. It opens the source line that defines
    //  the symbol when a debug file maps one, and otherwise brings the
    //  symbol's address into view in the disassembly.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (GoToDefinitionTests)
    {
    public:
        static constexpr Word  kDraw = 0x0300;

        using Items = std::vector<std::pair<std::wstring, std::function<void()>>>;


        //  A JSR DRAW on the first row and an LDA #BAR, a constant, on the
        //  second, with a debug file that maps line 5 of main.a65 to DRAW
        //  when `hasSource` is set.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshot (bool hasSource)
        {
            auto                                          snapshot = std::make_shared<DebuggerViewSnapshot>();
            auto                                          symbols  = std::make_shared<DebuggerViewSnapshot::SymbolsState>();
            DebuggerViewSnapshot::CodeLine                jsr;
            DebuggerViewSnapshot::CodeLine                lda;
            DebugSourceFile                               file;



            jsr.address       = 0x0310;
            jsr.bytes         = "20 00 03";
            jsr.instruction   = "JSR DRAW";
            jsr.shownOperand  = "DRAW";
            jsr.memoryOperand = "$0300";
            jsr.isCurrent     = true;

            lda.address       = 0x0313;
            lda.bytes         = "A9 28";
            lda.instruction   = "LDA #BAR";
            lda.shownOperand  = "#BAR";
            lda.memoryOperand = "#$28";

            symbols->symbols = { { "DRAW", kDraw }, { "BAR", 0x0028, SymbolTableId::User, true } };

            snapshot->isPaused     = true;
            snapshot->codeOpen[0]  = true;
            snapshot->pc           = 0x0310;
            snapshot->codeViews[0] = { jsr, lda };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->symbols      = symbols;

            if (hasSource)
            {
                file.id   = 0;
                file.name = "main.a65";

                snapshot->source                = DebuggerViewSnapshot::SourceState();
                snapshot->source->debugFilePath = L"C:\\Work\\main.dbg";
                snapshot->source->programKey    = "key";
                snapshot->source->lineAddresses = std::make_shared<const std::map<std::pair<int, int>, Word>> (
                                                      std::map<std::pair<int, int>, Word> { { { 0, 5 }, kDraw }, { { 0, 6 }, 0x0302 } });
                snapshot->source->files.push_back (file);
            }

            return snapshot;
        }


        //  The middle of a row's Instruction cell, the view painted so its
        //  cells are measured.
        static POINT  GetInstructionPoint (MenuWindow & window, const CassoTheme & theme, int row)
        {
            MockDxuiPainter        painter;
            MockDxuiTextRenderer   text;
            DxuiListView         * list   = window.GetCodeList (0);
            RECT                   bounds = list->GetBounds();
            RECT                   cell   = {};



            list->Paint (painter, text, theme);
            Assert::IsTrue (list->GetCellTextRectPx (row, DisassemblyCells::kInstructionColumn, cell), L"the row is in view");

            return POINT { bounds.left + cell.left + 2, bounds.top + (cell.top + cell.bottom) / 2 };
        }


        static Items  GetMenu (MenuWindow & window, POINT at)
        {
            Items  items = { { L"Copy", [] {} } };



            window.AddGoToDefinition (at, items);
            return items;
        }


        //  Over JSR DRAW, the menu's first row is Go to definition; over
        //  LDA #BAR, whose symbol is a constant, the menu doesn't have the row.
        TEST_METHOD (GoToDefinitionIsFirstOverASymbolAndNotOfferedForAConstant)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            MenuHost     host;
            MenuWindow   window (theme, host);
            Items        items;



            window.TakeSnapshot (MakeSnapshot (false));

            items = GetMenu (window, GetInstructionPoint (window, theme, 0));
            Assert::AreEqual ((size_t) 2, items.size());
            Assert::AreEqual (std::wstring (L"Go to definition"), items[0].first, L"first on the menu over DRAW");

            items = GetMenu (window, GetInstructionPoint (window, theme, 1));
            Assert::AreEqual ((size_t) 1, items.size(), L"BAR is a constant, which has no definition to go to");
            Assert::AreEqual (std::wstring (L"Copy"), items[0].first);
        }


        //  Without a debug file, Go to definition brings DRAW's address into
        //  view in the disassembly.
        TEST_METHOD (WithoutADebugFileGoToDefinitionShowsTheAddress)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            MenuHost     host;
            MenuWindow   window (theme, host);
            Items        items;



            window.TakeSnapshot (MakeSnapshot (false));

            items = GetMenu (window, GetInstructionPoint (window, theme, 0));
            Assert::AreEqual (std::wstring (L"Go to definition"), items[0].first);

            host.addresses.clear();
            items[0].second();

            Assert::AreEqual ((size_t) 1, host.addresses.size(), L"the disassembly moves");
            Assert::IsTrue   (host.addresses.back() == std::optional<Word> (kDraw), L"to DRAW");
            Assert::IsFalse  (window.GetSourceGroup().IsOpen (0), L"no document opens");
        }


        //  With a debug file that maps DRAW to line 5 of main.a65, Go to
        //  definition opens that line in its document, and the disassembly
        //  isn't moved.
        TEST_METHOD (WithADebugFileGoToDefinitionOpensTheDefiningLine)
        {
            CassoTheme   theme  = CassoTheme::MakeSkeuomorphic();
            MenuHost     host;
            MenuWindow   window (theme, host);
            Items        items;



            window.TakeSnapshot (MakeSnapshot (true));
            Assert::IsFalse (window.GetSourceGroup().IsOpen (0), L"no document is open before");

            items = GetMenu (window, GetInstructionPoint (window, theme, 0));
            Assert::AreEqual (std::wstring (L"Go to definition"), items[0].first);

            host.addresses.clear();
            items[0].second();

            //  The next frame, as the window takes one.
            window.TakeSnapshot (MakeSnapshot (true));
            window.Relayout();

            Assert::IsTrue   (window.GetSourceGroup().IsOpen (0), L"main.a65's document opens");
            Assert::IsTrue   (window.GetSourceGroup().GetDocument (0).IsShown());
            Assert::IsTrue   (window.GetSourceGroup().GetDocument (0).GetView()->IsVisible(), L"in front of its group");
            Assert::AreEqual (0, window.GetSourceGroup().GetDocument (0).GetModel().GetFile(), L"main.a65");
            Assert::IsTrue   (host.addresses.empty(), L"the disassembly isn't moved");
        }


        //  An instruction selected for the window, as a Ctrl+click on the heat
        //  map asks, has its source line selected in its file's document, which
        //  opens for it, even in the frame that brings the debug file.
        TEST_METHOD (ARequestedInstructionsSourceLineIsSelectedInItsDocument)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            MenuHost                               host;
            MenuWindow                             window (theme, host);
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = MakeSnapshot (true);



            snapshot->codeViews[0][1].sourceFileId = 0;
            snapshot->codeViews[0][1].sourceLine   = 6;
            snapshot->code                         = snapshot->codeViews[0];
            snapshot->codeSelect[0]                = 0x0313;
            snapshot->codeSelectSerial[0]          = 1;

            window.TakeSnapshot (snapshot);
            window.Relayout();
            window.TakeSnapshot (snapshot);
            window.Relayout();

            Assert::AreEqual (2, window.GetCodeList (0)->GetSelectedRow(), L"the LDA's row, under its source row");
            Assert::IsTrue   (window.GetSourceGroup().IsOpen (0), L"main.a65's document opens");
            Assert::AreEqual (std::wstring (L"        rts"), window.GetSourceGroup().GetDocument (0).GetView()->GetSelectionText(), L"with line 6 selected");
        }
    };
}
