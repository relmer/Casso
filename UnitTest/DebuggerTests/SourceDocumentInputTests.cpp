#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/CodeMargins.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace SourceDocumentInputTests
{
    //  main.a65: a comment on line 1, LDA #$41 at $0300 on line 2 and RTS at
    //  $0302 on line 3.
    static constexpr const char * s_kpszMainSource = "; main\n        lda #$41\n        rts\n";



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceHost
    //
    //  A host that finds main.a65, and the echo of every action the window
    //  runs, added to echoes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SourceHost : public NullDebuggerWindowHost
    {
    public:
        void  RunDebuggerAction (const DebuggerAction & action) override { echoes.push_back (action.echo); }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;



            lookup.match = SourceMatch::Exact;
            lookup.path  = L"C:\\Work\\main.a65";
            lookup.text  = s_kpszMainSource;
            return lookup;
        }

        std::vector<std::string>  echoes;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SourceWindow : public DebuggerWindow
    {
    public:
        SourceWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            DxuiDpiScaler  scaler;



            m_theme = &theme;
            m_host  = &host;

            scaler.SetDpi (96);
            OnCreate();
            Layout (RECT { 0, 0, 1400, 900 }, scaler);
        }

        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::Layout;
        using DebuggerWindow::GetSourceGroup;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetFocused;
        using DebuggerWindow::GetCursorForPoint;
        using DebuggerWindow::GetCodeList;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceDocumentInputTests
    //
    //  A source document takes the mouse and the keys as a disassembly view
    //  does: its glyph margin sets and clears breakpoints, its menu has the
    //  same breakpoint rows, and a click focuses a line the keys then move.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceDocumentInputTests)
    {
    public:
        static constexpr int  kBreakpointId = 7;
        static constexpr int  kCellPx       = 16;
        static constexpr int  kPadPx        = 6;
        static constexpr int  kTextX        = 300;
        static constexpr int  kMenuX        = 150;


        //  Stopped at line 2 of main.a65, with a breakpoint on line 3.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSnapshot (bool isDisabled = false)
        {
            auto             snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebugSourceFile  file;



            file.id   = 0;
            file.name = "main.a65";

            snapshot->isPaused                = true;
            snapshot->codeOpen[0]             = true;
            snapshot->source                  = DebuggerViewSnapshot::SourceState();
            snapshot->source->debugFilePath   = L"C:\\Work\\main.dbg";
            snapshot->source->programKey      = "key";
            snapshot->source->fileId          = 0;
            snapshot->source->line            = 2;
            snapshot->source->breakpointLines = { { 0, 3, kBreakpointId } };
            snapshot->source->lineAddresses   = std::make_shared<const std::map<std::pair<int, int>, Word>> (
                                                    std::map<std::pair<int, int>, Word> { { { 0, 2 }, 0x0300 }, { { 0, 3 }, 0x0302 } });
            snapshot->source->files.push_back (file);

            snapshot->breakpoints.push_back ({ kBreakpointId, 0x0302, "BP", !isDisabled });
            return snapshot;
        }


        static std::shared_ptr<DxuiIconImage>  MakeIcon (uint32_t argb)
        {
            auto  icon = std::make_shared<DxuiIconImage>();



            icon->width  = 1;
            icon->height = 1;
            icon->bgraPremul.assign (1, argb);
            return icon;
        }


        //  The middle of a row's line, whose rows are a line each.
        static int  GetRowY (int row)
        {
            return kPadPx + row * kCellPx + kCellPx / 2;
        }


        //  A point on the glyph margin, left of the text.
        static int  GetGutterX()
        {
            return CodeMargins::kGlyphDip / 2;
        }


        //  A source pane over a view and a banner, configured as its
        //  document configures it, with every action it runs echoed.
        struct PaneRig
        {
            DxuiTextView                         view;
            DxuiActionBanner                     banner;
            std::vector<std::string>             echoes;
            std::shared_ptr<DxuiIconImage>       enabled  = MakeIcon (0xFFFF0000u);
            std::shared_ptr<DxuiIconImage>       disabled = MakeIcon (0xFF800000u);
            std::shared_ptr<DxuiIconImage>       hover    = MakeIcon (0xFF808080u);
            SourcePane                           pane;

            PaneRig() :
                pane (&view, &banner,
                      [] (const DebugSourceFile &, const std::wstring &, const std::string &)
                      {
                          SourceLookup  lookup;

                          lookup.match = SourceMatch::Exact;
                          lookup.text  = s_kpszMainSource;
                          return lookup;
                      },
                      [this] (const DebuggerActionBuilder & build) { echoes.push_back (build (CommandMode::AppleWin).echo); })
            {
                DxuiDpiScaler       scaler;
                SourcePane::Style   style;



                scaler.SetDpi (96);
                style.enabledIcon  = enabled;
                style.disabledIcon = disabled;
                style.hoverIcon    = hover;

                pane.Configure   (nullptr);
                pane.SetStyle    (style);
                pane.SetFile     (0);
                view.SetCellSize (8, kCellPx);
                view.Layout      (RECT { 0, 0, 600, 400 }, scaler);
                pane.Apply       (*MakeSnapshot());
            }
        };


        //  The pointer over a line with code in the glyph margin draws the
        //  gray breakpoint there; a line with no code, or the text, draws
        //  none.
        TEST_METHOD (TheGlyphMarginDrawsTheGrayBreakpointOnALineWithCode)
        {
            PaneRig  rig;



            Assert::IsTrue  (rig.pane.SetGutterHover (POINT { GetGutterX(), GetRowY (1) }), L"over LDA's line");
            Assert::IsTrue  (rig.view.GetRows()[1].icon == rig.hover, L"the gray breakpoint");
            Assert::IsTrue  (rig.view.GetRows()[2].icon == rig.enabled, L"the RTS's breakpoint as it is");

            Assert::IsTrue  (rig.pane.SetGutterHover (POINT { GetGutterX(), GetRowY (0) }), L"over the comment");
            Assert::IsTrue  (rig.view.GetRows()[1].icon == nullptr, L"the comment has no code, so no gray breakpoint");
            Assert::IsTrue  (rig.view.GetRows()[0].icon == nullptr);

            Assert::IsFalse (rig.pane.SetGutterHover (POINT { kTextX, GetRowY (1) }), L"over the text, as over the comment: none");
        }


        //  A press on the glyph margin sets a breakpoint on a line with code,
        //  clears the one there, and does nothing on a line with no code. A
        //  press on the text isn't the margin's.
        TEST_METHOD (APressOnTheGlyphMarginSetsOrClearsTheBreakpoint)
        {
            PaneRig  rig;



            Assert::IsTrue   (rig.pane.ClickGutter (POINT { GetGutterX(), GetRowY (1) }));
            Assert::AreEqual ((size_t) 1, rig.echoes.size());
            Assert::AreEqual (DebuggerActions::GetSourceBreakpoint ("main.a65", 2, CommandMode::AppleWin).echo, rig.echoes.back(), L"set on LDA's line");

            Assert::IsTrue   (rig.pane.ClickGutter (POINT { GetGutterX(), GetRowY (2) }));
            Assert::AreEqual (DebuggerActions::GetClearBreakpoint (kBreakpointId, CommandMode::AppleWin).echo, rig.echoes.back(), L"cleared on RTS's line");

            Assert::IsTrue   (rig.pane.ClickGutter (POINT { GetGutterX(), GetRowY (0) }), L"the press is the margin's");
            Assert::AreEqual ((size_t) 2, rig.echoes.size(), L"but a comment can't take a breakpoint");

            Assert::IsFalse  (rig.pane.ClickGutter (POINT { kTextX, GetRowY (1) }), L"a press on the text");
            Assert::AreEqual ((size_t) 2, rig.echoes.size());
        }


        static std::vector<std::wstring>  GetLabels (const PaneMenuItems & items)
        {
            std::vector<std::wstring>  labels;



            for (const auto & [label, action] : items)
            {
                if (!label.empty())
                {
                    labels.push_back (label);
                }
            }

            return labels;
        }


        static std::wstring  JoinLabels (const PaneMenuItems & items)
        {
            std::wstring  joined;



            for (const auto & [label, action] : items)
            {
                joined += label + L"|";
            }

            return joined;
        }


        static void  Run (const PaneMenuItems & items, const std::wstring & label)
        {
            auto  found = std::ranges::find_if (items, [&label] (const auto & item) { return item.first == label; });



            Assert::IsTrue (found != items.end() && (bool) found->second, label.c_str());
            found->second();
        }


        //  The document's menu over a line has its breakpoint rows: Set
        //  breakpoint on a line with code and none, Disable breakpoint or
        //  Enable breakpoint then Remove breakpoint on a line with one, and
        //  none on a line with no code.
        TEST_METHOD (TheDocumentsMenuHasTheLinesBreakpointRows)
        {
            CassoTheme             theme    = CassoTheme::MakeSkeuomorphic();
            SourceHost             host;
            SourceWindow           window   (theme, host);
            DxuiDpiScaler          scaler;
            SourceDocumentPane   * document = nullptr;
            DxuiTextView         * view     = nullptr;
            RECT                   bounds   = {};
            PaneMenuItems          items;
            auto                   snapshot = MakeSnapshot();



            scaler.SetDpi (96);
            window.TakeSnapshot (snapshot);
            window.GetSourceGroup().Activate (0);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            document = &window.GetSourceGroup().GetDocument (0);
            view     = document->GetView();
            Assert::IsTrue (document->IsShown(), L"the PC's file opens");
            Assert::IsTrue (view->IsVisible(), L"in front of its group");

            view->SetCellSize (8, kCellPx);
            bounds = view->GetBounds();
            document->ApplyDocument (*snapshot, 0, 0, L"main.a65");

            document->AddContentMenuItems (POINT { bounds.left + kMenuX, bounds.top + GetRowY (1) }, items);
            Assert::IsTrue (GetLabels (items) == std::vector<std::wstring> { L"Go to disassembly", L"Set breakpoint" }, (L"LDA's line has no breakpoint: " + JoinLabels (items)).c_str());
            Run (items, L"Set breakpoint");
            Assert::AreEqual (DebuggerActions::GetSourceBreakpoint ("main.a65", 2, snapshot->mode).echo, host.echoes.back());

            items.clear();
            document->AddContentMenuItems (POINT { bounds.left + kMenuX, bounds.top + GetRowY (2) }, items);
            Assert::IsTrue (GetLabels (items) == std::vector<std::wstring> { L"Go to disassembly", L"Disable breakpoint", L"Remove breakpoint" }, L"RTS's line has one");
            Run (items, L"Disable breakpoint");
            Assert::AreEqual (DebuggerActions::GetEnableBreakpoint (kBreakpointId, false, snapshot->mode).echo, host.echoes.back());
            Run (items, L"Remove breakpoint");
            Assert::AreEqual (DebuggerActions::GetClearBreakpoint (kBreakpointId, snapshot->mode).echo, host.echoes.back());

            snapshot = MakeSnapshot (true);
            window.TakeSnapshot (snapshot);
            document->ApplyDocument (*snapshot, 0, 0, L"main.a65");

            items.clear();
            document->AddContentMenuItems (POINT { bounds.left + kMenuX, bounds.top + GetRowY (2) }, items);
            Assert::IsTrue (GetLabels (items) == std::vector<std::wstring> { L"Go to disassembly", L"Enable breakpoint", L"Remove breakpoint" }, L"a disabled one");

            items.clear();
            document->AddContentMenuItems (POINT { bounds.left + kMenuX, bounds.top + GetRowY (0) }, items);
            Assert::IsTrue (GetLabels (items).empty(), L"the comment has no code");
        }


        //  The window with main.a65's document in front, its rows a cell
        //  high, and the document's view.
        static DxuiTextView *  OpenDocument (SourceWindow & window, const std::shared_ptr<DebuggerViewSnapshot> & snapshot)
        {
            DxuiDpiScaler          scaler;
            SourceDocumentPane   * document = nullptr;



            scaler.SetDpi (96);
            window.TakeSnapshot (snapshot);
            window.GetSourceGroup().Activate (0);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            document = &window.GetSourceGroup().GetDocument (0);
            Assert::IsTrue (document->GetView()->IsVisible(), L"the document is in front of its group");

            document->GetView()->SetCellSize (8, kCellPx);
            return document->GetView();
        }


        static void  Press (SourceWindow & window, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = at;

            for (DxuiMouseEventKind kind : { DxuiMouseEventKind::Down, DxuiMouseEventKind::Up })
            {
                ev.kind = kind;
                (void) window.OnMouse (ev);
            }
        }


        static void  Key (SourceWindow & window, WPARAM vk, bool ctrl)
        {
            (void) window.OnKey (DxuiKeyEvent { DxuiKeyEventKind::Down, vk, false, false, ctrl, false });
        }


        //  A click on a document's line focuses it, as a click selects a
        //  disassembly row, and the keys move it, Ctrl with Home and End
        //  included.
        TEST_METHOD (AClickFocusesALineTheKeysThenMove)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            SourceHost       host;
            SourceWindow     window (theme, host);
            DxuiTextView   * view   = OpenDocument (window, MakeSnapshot());
            RECT             bounds = view->GetBounds();



            Press (window, POINT { bounds.left + kMenuX, bounds.top + GetRowY (1) });
            Assert::IsTrue   (window.GetFocused() == view, L"the document has the focus");
            Assert::AreEqual (1, view->GetFocusRow(), L"LDA's line");

            Key (window, VK_DOWN, false);
            Assert::AreEqual (2, view->GetFocusRow());

            Key (window, VK_HOME, true);
            Assert::AreEqual (0, view->GetFocusRow(), L"Ctrl+Home");

            Key (window, VK_END, true);
            Assert::AreEqual (2, view->GetFocusRow(), L"Ctrl+End");
        }


        //  The pointer is an arrow over a document's glyph margin and over the
        //  lanes left of its text where the branch arrow runs, and the I-beam
        //  over the text.
        TEST_METHOD (TheDocumentsPointerIsAnArrowOffItsText)
        {
            CassoTheme       theme  = CassoTheme::MakeSkeuomorphic();
            SourceHost       host;
            SourceWindow     window (theme, host);
            DxuiTextView   * view   = OpenDocument (window, MakeSnapshot());
            RECT             bounds = view->GetBounds();
            float            text   = 0.0f;
            int              y      = bounds.top + GetRowY (1);



            Assert::IsTrue (view->GetCellLeftPx (1, SourcePane::kTextCell, text));

            Assert::IsTrue (window.GetCursorForPoint (POINT { bounds.left + GetGutterX(), y }) == IDC_ARROW, L"the glyph margin");
            Assert::IsTrue (window.GetCursorForPoint (POINT { (LONG) text - 2, y }) == IDC_ARROW, L"the lanes left of the text");
            Assert::IsTrue (window.GetCursorForPoint (POINT { (LONG) text + 20, y }) == IDC_IBEAM, L"the text");
        }


        //  The same in a disassembly view: an arrow over the glyph margin and
        //  over the lanes left of the mnemonics, which overlap the label
        //  column's cells; and Ctrl+Page Down and Ctrl+Page Up select the
        //  bottom and top row in view, as in a document.
        TEST_METHOD (TheDisassemblysPointerAndPageKeysMatchADocuments)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            SourceHost                      host;
            SourceWindow                    window   (theme, host);
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  line;
            DxuiListView                  * list     = nullptr;
            MockDxuiPainter                 painter;
            MockDxuiTextRenderer            text;
            RECT                            bounds   = {};
            RECT                            cell     = {};
            int                             y        = 0;



            for (Word address = 0x0300; address < 0x0306; address += 2)
            {
                line.address     = address;
                line.bytes       = "A9 41";
                line.label       = "START";
                line.instruction = "LDA #$41";
                snapshot->codeViews[0].push_back (line);
            }

            snapshot->isPaused    = true;
            snapshot->codeOpen[0] = true;
            snapshot->code        = snapshot->codeViews[0];

            window.TakeSnapshot (snapshot);

            list   = window.GetCodeList (0);
            bounds = list->GetBounds();
            list->Paint (painter, text, theme);
            Assert::IsTrue (list->IsVisible() && list->GetCellTextRectPx (1, DisassemblyCells::kInstructionColumn, cell), L"the view is in front");

            y = bounds.top + (cell.top + cell.bottom) / 2;

            Assert::IsTrue (window.GetCursorForPoint (POINT { bounds.left + GetGutterX(), y }) == IDC_ARROW, L"the glyph margin");
            Assert::IsTrue (window.GetCursorForPoint (POINT { bounds.left + cell.left - 6, y }) == IDC_ARROW, L"the lanes, over the label's cell");
            Assert::IsTrue (window.GetCursorForPoint (POINT { bounds.left + cell.left + 2, y }) == IDC_IBEAM, L"the mnemonic");

            window.FocusControl (list);
            list->SetSelectedRow (1);

            Key (window, VK_NEXT, true);
            Assert::AreEqual (2, list->GetSelectedRow(), L"Ctrl+Page Down: the bottom row");

            Key (window, VK_PRIOR, true);
            Assert::AreEqual (0, list->GetSelectedRow(), L"Ctrl+Page Up: the top row");
        }
    };
}