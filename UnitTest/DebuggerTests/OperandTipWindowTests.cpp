#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "DebuggerTests/OperandTipWindow.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/Panes/SourcePane.h"
#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/SourceSyntax.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace OperandResultTipTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OperandTipWindowTests
    //
    //  The operand tip in the debugger window: laid over a code, source or
    //  BASIC cell the pane cuts off, up while the pointer rests on the cell
    //  and down after a press, a key or the pointer leaving it, in the row's
    //  colors, on the monitors the row is on, and never over selected text,
    //  a window, a pane or a popup lying over the cell.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OperandTipWindowTests)
    {
    public:

        //  Where a disassembly row's line starts, as the list highlights it:
        //  the address column's left edge, past the gutter and heat columns.
        static LONG  GetRowStart (DxuiListView & list, int row)
        {
            RECT           address = {};
            DxuiDpiScaler  scaler;



            Assert::IsTrue (list.GetCellTextRectPx (row, DisassemblyCells::kAddressColumn, address));
            return list.GetBounds().left + address.left - scaler.ToPx (list.GetCellPadLeftDip());
        }


        //  The debugger window, with a code pane whose first row's operand
        //  the pane cuts off and whose second row's fits.
        static void  Build (TipWindow & window, const CassoTheme & theme, MockDxuiTextRenderer & text)
        {
            DxuiDpiScaler    scaler;
            MockDxuiPainter  painter;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            //  A paint fits the columns to their text, which places the cells.
            window.GetCodeList (0)->Paint (painter, text, theme);
        }


        //  A point on a row's operand, just past where its text starts.
        static POINT  GetOperandPoint (TipWindow & window, int row)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            bounds = list->GetBounds();
            RECT            cell   = {};



            Assert::IsTrue (list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell), L"the row's operand is in view");

            return POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };
        }


        //  A point on another row's operand that fits and that the tip up now
        //  doesn't cover, or else just below the tip and the rows: a tip that
        //  lies below its row is the pointer's to move into.
        static POINT  GetPointOffTip (TipWindow & window)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            tip    = window.GetOperandTips().GetLayout().rect;
            POINT           first  = GetOperandPoint (window, 0);
            LONG            below  = tip.bottom;
            int             row    = 0;
            RECT            cell   = {};



            for (row = 1; list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell); row++)
            {
                POINT  at     = GetOperandPoint (window, row);
                bool   isFull = (int) list->GetCellsOfRow (row)[DisassemblyCells::kOperandColumn].text.size() * TipMetrics::kGlyphPx > cell.right - cell.left;



                below = (std::max) (below, list->GetBounds().top + cell.bottom);

                if (PtInRect (&tip, at) == FALSE && !isFull)
                {
                    return at;
                }
            }

            return POINT { first.x, below + 1 };
        }


        static void  Send (TipWindow & window, DxuiMouseEventKind kind, POINT at)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Move) ? DxuiMouseButton::None : DxuiMouseButton::Left;
            ev.positionDip = at;
            ev.wheelDelta  = (kind == DxuiMouseEventKind::Wheel) ? -1.0f : 0.0f;

            (void) window.OnMouse (ev);
        }


        //  A row's operand, where the list draws its text, in the list's
        //  pixels.
        static RECT  GetCellRect (TipWindow & window, int row)
        {
            RECT  cell = {};



            Assert::IsTrue (window.GetCodeList (0)->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell), L"the row's operand is in view");

            return cell;
        }


        //  A drag along a row's operand from just past where its text starts,
        //  which selects the characters it passes over.
        static void  DragAlong (TipWindow & window, int row)
        {
            DxuiMouseEvent  ev;



            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = GetOperandPoint (window, row);
            (void) window.OnMouse (ev);

            ev.kind           = DxuiMouseEventKind::Move;
            ev.positionDip.x += kDragPx;
            (void) window.OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            (void) window.OnMouse (ev);
        }


        //  The screen the window's tips are laid out on: one monitor, and
        //  nothing lying over the window.
        static TipWindow::OperandTipScreen  MakeScreen()
        {
            TipWindow::OperandTipScreen  screen;



            screen.workAreas.push_back (TipMetrics::kScreen);
            return screen;
        }


        static bool  TryGetTip (TipWindow & window, POINT at, MockDxuiTextRenderer & text, OperandResultTip::Layout & layout)
        {
            DxuiDpiScaler             scaler;
            TipWindow::OperandTipCell cell;



            scaler.SetDpi (96);

            return window.GetOperandTips().TryGet (at, scaler, text, MakeScreen(), layout, cell);
        }


        TEST_METHOD (TheWindowLaysTheTipOverACutOffCodeCell)
        {
            CassoTheme                 theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                    host;
            TipWindow                  window (theme, host);
            DxuiDpiScaler              scaler;
            MockDxuiTextRenderer       text;
            DxuiListView             * list    = nullptr;
            RECT                       bounds  = {};
            RECT                       cell    = {};
            OperandResultTip::Layout   layout;
            TipWindow::OperandTipCell  tipCell;



            scaler.SetDpi (96);
            Build (window, theme, text);

            list   = window.GetCodeList (0);
            bounds = list->GetBounds();

            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kOperandColumn, cell), L"the long row's operand is in view");
            Assert::IsTrue (window.GetOperandTips().TryGet (GetOperandPoint (window, 0), scaler, text, MakeScreen(), layout, tipCell),
                            L"the pane cuts the long operand off");

            Assert::AreEqual (0, tipCell.view);
            Assert::AreEqual (0, tipCell.row);
            Assert::AreEqual (2, layout.lineCount, L"the operand on the row's line and the result on the next");
            Assert::AreEqual ((float) (bounds.left + cell.left), TipMetrics::GetLine (layout, 0)[0].leftPx, L"the tip's text starts at the cell's");
            Assert::AreEqual ((float) (bounds.left + cell.left), TipMetrics::GetLine (layout, 1)[0].leftPx, L"the result from the operand's left");
            Assert::AreEqual (bounds.top + cell.top, (LONG) layout.rowTop);
            Assert::IsTrue   (layout.rect.right > bounds.right, L"the tip runs on past the pane to the end of the operand");
            Assert::AreEqual (GetRowStart (*list, 0), layout.rect.left, L"from the row's address, as the list highlights it");
            Assert::AreEqual ((int) (bounds.left + cell.left - scaler.ToPx (list->GetCellPadLeftDip())), layout.firstLeft,
                              L"the row's line from the cell's padding");

            Assert::IsFalse (window.GetOperandTips().TryGet (GetOperandPoint (window, 1), scaler, text, MakeScreen(), layout, tipCell),
                             L"a short operand fits");
        }


        //  The tip is up exactly when the list's drawing of the operand is
        //  clipped: to the box its paint draws the text in, or the list's
        //  clip short of its scrollbar, whichever comes first.
        TEST_METHOD (TheWindowsTipIsUpExactlyWhenTheListClipsTheOperand)
        {
            CassoTheme                theme     = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            DxuiListView            * list      = nullptr;
            RECT                      bounds    = {};
            RECT                      cell      = {};
            float                     textLeft  = 0.0f;
            float                     boxRight  = 0.0f;
            float                     clipRight = 0.0f;
            bool                      hasClip   = false;
            bool                      hasRun    = false;
            int                       roomPx    = 0;
            std::wstring              operand;
            OperandResultTip::Layout  layout;



            Build (window, theme, text);

            list    = window.GetCodeList (0);
            bounds  = list->GetBounds();
            operand = list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text;

            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kOperandColumn, cell), L"the long row's operand is in view");

            textLeft = (float) (bounds.left + cell.left);

            text.Reset();
            list->Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::PushClipRect && !hasClip)
                {
                    clipRight = call.x + call.width;
                    hasClip   = true;
                }

                if (call.kind == RecordedTextKind::DrawString && !hasRun && call.x == textLeft && !call.text.empty() && operand.starts_with (call.text))
                {
                    boxRight = call.x + call.width;
                    hasRun   = true;
                }
            }

            Assert::IsTrue (hasClip, L"the list clips its paint");
            Assert::IsTrue (hasRun,  L"the list draws the operand where its text starts");

            roomPx = (int) ((std::min) (boxRight, clipRight) - textLeft);

            text.SetCannedMetrics (operand, SIZE { roomPx, TipMetrics::kRowHeight });
            Assert::IsFalse (TryGetTip (window, GetOperandPoint (window, 0), text, layout), L"an operand that fills its box exactly has no tip");

            text.SetCannedMetrics (operand, SIZE { roomPx + 1, TipMetrics::kRowHeight });
            Assert::IsTrue  (TryGetTip (window, GetOperandPoint (window, 0), text, layout), L"an operand a pixel wider than its box has a tip");
        }


        //  With the columns ahead of it widened past the pane's right edge,
        //  the operand and result are wholly out of view. With the pointer
        //  anywhere on the row, the tip is up with the operand on the row's
        //  line, two spaces after the code rather than at its column past the
        //  pane, and the list draws the row in the tip's color from its
        //  address.
        TEST_METHOD (AnOperandPastThePanesEdgeRunsOnAfterTheCode)
        {
            CassoTheme                 theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                    host;
            TipWindow                  window (theme, host);
            MockDxuiTextRenderer       text;
            MockDxuiPainter            painter;
            DxuiListView             * list    = nullptr;
            RECT                       bounds  = {};
            RECT                       cell    = {};
            RECT                       address = {};
            RECT                       code    = {};
            int                        right   = 0;
            POINT                      at      = {};



            Build (window, theme, text);

            list = window.GetCodeList (0);
            list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, kWindowWidth * 2);
            list->Paint (painter, text, theme);

            bounds = list->GetBounds();
            right  = bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0);

            Assert::IsTrue (list->GetCellTextRectPx (kLastRow, DisassemblyCells::kOperandColumn, cell));
            Assert::IsTrue (bounds.left + cell.left >= right, L"the operand starts past the pane's right edge");
            Assert::IsTrue (list->GetCellTextRectPx (kLastRow, DisassemblyCells::kAddressColumn, address));

            at = POINT { bounds.left + (address.left + address.right) / 2, bounds.top + (address.top + address.bottom) / 2 };
            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"no tip after a move onto the row's address");
            Assert::AreEqual (kLastRow, window.GetOperandTips().GetCell().row);
            Assert::AreEqual (GetRowStart (*list, kLastRow), window.GetOperandTips().GetCell().area.left, L"the row in view from its address is the tip's");
            Assert::AreEqual ((LONG) right, window.GetOperandTips().GetCell().area.right);
            Assert::AreEqual (bounds.top + cell.top, window.GetOperandTips().GetLayout().rect.top, L"from the row's line");
            Assert::AreEqual (1, window.GetOperandTips().GetLayout().lineCount, L"the operand, which has no result, on the row's line");
            Assert::AreEqual (kLastRow, list->GetHighlightRow(), L"the list draws the row in the tip's color");

            Assert::IsTrue   (list->GetCellTextRectPx (kLastRow, DisassemblyCells::kInstructionColumn, code));
            Assert::IsTrue   (TipMetrics::GetLine (window.GetOperandTips().GetLayout(), 0)[0].leftPx > (float) (bounds.left + code.left), L"after the code");
            Assert::IsTrue   (TipMetrics::GetLine (window.GetOperandTips().GetLayout(), 0)[0].leftPx < (float) right, L"not at its column past the pane");


            Send (window, DxuiMouseEventKind::Move, POINT { bounds.left + 1, bounds.bottom + kGapPx });
            Assert::IsFalse  (window.GetOperandTips().HasTip(), L"the tip is up with the pointer off the pane");
        }


        //  A row whose text at the right edge runs on from a cell ahead of
        //  the operand, as a data line's string does, has the tip over that
        //  cell, on one line where the list draws it, run on past the pane,
        //  in the italics the list draws a data line in. The label is the
        //  list's. The pointer on the label puts it up too.
        TEST_METHOD (TheWindowLaysTheTipOverACutOffCellThatSpansTheRow)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                         host;
            TipWindow                       window   (theme, host);
            DxuiDpiScaler                   scaler;
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  data;
            DxuiListView                  * list     = nullptr;
            RECT                            bounds   = {};
            RECT                            cell     = {};
            RECT                            label    = {};
            OperandResultTip::Layout        layout;
            TipWindow::OperandTipCell       tipCell;



            data.address     = 0x0300;
            data.bytes       = "48 49";
            data.label       = "msg";
            data.instruction = "ASC \"" + std::string (300, 'X') + "\"";
            data.dataOrigin  = DataOrigin::Marked;

            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { data };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            list = window.GetCodeList (0);
            list->Paint (painter, text, theme);
            bounds = list->GetBounds();

            Assert::IsTrue   (list->GetCellTextRectPx (0, DisassemblyCells::kInstructionColumn, cell), L"the data line's text is in view");
            Assert::IsTrue   (window.GetOperandTips().TryGet (POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 },
                                                              scaler, text, MakeScreen(), layout, tipCell),
                              L"the pane cuts the string off");
            Assert::AreEqual (1, layout.lineCount, L"the whole line on one line");
            Assert::AreEqual (list->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text, TipMetrics::GetLine (layout, 0)[0].text, L"the string");
            Assert::AreEqual ((float) (bounds.left + cell.left), TipMetrics::GetLine (layout, 0)[0].leftPx, L"the tip's text starts at the cell's");
            Assert::IsTrue   (TipMetrics::GetLine (layout, 0)[0].isItalic, L"a data line's tip is upright");
            Assert::IsTrue   (layout.rect.right > bounds.right, L"run on past the pane");

            Assert::IsTrue   (list->IsColumnVisible (DisassemblyCells::kLabelColumn));
            Assert::IsTrue   (list->GetCellTextRectPx (0, DisassemblyCells::kLabelColumn, label), L"the label is in view");

            Assert::IsTrue   (window.GetOperandTips().TryGet (POINT { bounds.left + label.left + 1, bounds.top + (label.top + label.bottom) / 2 },
                                                              scaler, text, MakeScreen(), layout, tipCell),
                              L"the pointer on the label puts the line's tip up");
        }


        //  A branch written with a symbol: with its operand and result out of
        //  view, the tip has the symbol's name and value among the operand's
        //  values, and the symbol tip doesn't come up on the row while the
        //  in-place tip is up. With the row whole in view, the symbol tip
        //  comes up.
        TEST_METHOD (AnOperandsSymbolJoinsTheTipInPlaceOfTheSymbolTip)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                         host;
            TipWindow                       window   (theme, host);
            DxuiDpiScaler                   scaler;
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  bne;
            DxuiListView                  * list     = nullptr;
            RECT                            bounds   = {};
            RECT                            code     = {};
            POINT                           at       = {};
            POINT                           local    = {};
            DxuiTip                         tip;



            bne.address               = 0x6100;
            bne.bytes                 = "D0 F8";
            bne.instruction           = "BNE w_inner";
            bne.shownOperand          = "w_inner";
            bne.memoryOperand         = "$60BA";
            bne.annotationParts.flags = "Z=0";
            bne.effect                = "PC=$60BA; 3 cycles";
            bne.isCurrent             = true;

            snapshot->pc           = 0x6100;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { bne };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            list = window.GetCodeList (0);
            Assert::IsTrue (list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text.starts_with (L"w_inner=$60BA  Z=0"), L"the operand column has the symbol");

            list->Paint (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kInstructionColumn, code));
            at    = POINT { bounds.left + code.left + 1, bounds.top + (code.top + code.bottom) / 2 };
            local = at;

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the row fits");
            Assert::IsTrue  (list->TryGetTip (local, tip), L"the symbol has no tip");
            Assert::IsTrue  (tip.text.starts_with (L"w_inner = $60BA"), tip.text.c_str());

            list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, kWindowWidth * 2);
            list->Paint (painter, text, theme);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"the operand out of view has no tip");
            Assert::AreEqual (std::wstring (L"w_inner=$60BA  Z=0"), TipMetrics::GetLine (window.GetOperandTips().GetLayout(), 0)[0].text,
                              L"the symbol first among the operand's values, as in its column");
            Assert::AreEqual (std::wstring (L"Result: PC=$60BA; 3 cycles"), TipMetrics::GetLine (window.GetOperandTips().GetLayout(), 1)[0].text);

            tip = DxuiTip();
            Assert::IsFalse (list->TryGetTip (local, tip) && tip.text.find (L"w_inner = ") != std::wstring::npos, L"the symbol tip over the in-place tip");
        }


        //  While the in-place tip has the operand's symbol, the label's symbol
        //  tip on the same row comes up as usual.
        TEST_METHOD (ALabelsSymbolTipComesUpWhileTheOperandsIsInTheTip)
        {
            CassoTheme                      theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                         host;
            TipWindow                       window   (theme, host);
            DxuiDpiScaler                   scaler;
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  bne;
            DxuiListView                  * list     = nullptr;
            RECT                            bounds   = {};
            RECT                            code     = {};
            RECT                            label    = {};
            POINT                           at       = {};
            DxuiTip                         tip;



            bne.address               = 0x6100;
            bne.bytes                 = "D0 F8";
            bne.label                 = "w_outer";
            bne.instruction           = "BNE w_inner";
            bne.shownOperand          = "w_inner";
            bne.memoryOperand         = "$60BA";
            bne.annotationParts.flags = "Z=0";
            bne.effect                = "PC=$60BA; 3 cycles";
            bne.isCurrent             = true;

            snapshot->pc           = 0x6100;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { bne };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];

            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreen());

            list = window.GetCodeList (0);
            list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, kWindowWidth * 2);
            list->Paint (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kInstructionColumn, code));
            Assert::IsTrue (list->GetCellTextRectPx (0, DisassemblyCells::kLabelColumn, label), L"the label is in view");

            at = POINT { bounds.left + code.left + 1, bounds.top + (code.top + code.bottom) / 2 };
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the operand out of view has no tip");
            Assert::IsTrue (window.GetOperandTips().GetLayout().hasSymbol);

            Assert::IsTrue (list->TryGetTip (POINT { bounds.left + label.left + 1, bounds.top + (label.top + label.bottom) / 2 }, tip) && tip.text.starts_with (L"w_outer = $6100"),
                            L"the label's symbol tip doesn't come up");

            tip = DxuiTip();
            Assert::IsFalse (list->TryGetTip (at, tip) && tip.text.find (L"w_inner = ") != std::wstring::npos,
                             L"the operand's symbol tip over the in-place tip");
        }


        //  A disassembly with LDA $10 at the PC, the second line of main.a65
        //  above it as a source row, LDA's operand and result on that row.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSourceSnapshot (const std::string & registers, const std::string & effect)
        {
            auto                               snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine     lda;
            DebuggerViewSnapshot::SourceState  source;



            lda.address                   = 0x0300;
            lda.bytes                     = "A5 10";
            lda.instruction               = "LDA $10";
            lda.annotationParts.registers = registers;
            lda.effect                    = effect;
            lda.isCurrent                 = true;
            lda.sourceFileId              = 0;
            lda.sourceLine                = 2;

            source.debugFilePath = L"C:\\Work\\main.dbg";
            source.programKey    = "key";
            source.files         = { { 0, "main.a65", 45, 0, "", 0 } };

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];
            snapshot->source       = source;

            return snapshot;
        }


        //  The window over a source snapshot, with the host's options and
        //  text, its tips laid out on `screen` and its list painted.
        static void  BuildSource (TipWindow & window, const CassoTheme & theme, MockDxuiTextRenderer & text,
                                  const std::shared_ptr<DebuggerViewSnapshot> & snapshot, const TipWindow::OperandTipScreen & screen)
        {
            DxuiDpiScaler    scaler;
            MockDxuiPainter  painter;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);
            window.GetOperandTips().SetDeviceForTest (&text, screen);
            window.GetCodeList (0)->Paint (painter, text, theme);
        }


        //  A source row drawn as one cell, with the label column hidden or
        //  the instruction column dragged too narrow for its code: the row's
        //  line is the source line, its comment and all, run on past the
        //  pane, and the operand and then the result go below it, under where
        //  the code starts.
        TEST_METHOD (ASourceRowInOneCellHasItsOperandBelowUnderItsCode)
        {
            const std::wstring  line = L"loop    lda $10 ; " + std::wstring (300, L'c');

            for (bool isLabelShown : { false, true })
            {
                CassoTheme                 theme    = CassoTheme::MakeSkeuomorphic();
                SourceTipHost              host;
                TipWindow                  window   (theme, host);
                MockDxuiTextRenderer       text;
                MockDxuiPainter            painter;
                auto                       snapshot = MakeSourceSnapshot ("A=00 X=01", "A=41");
                size_t                     column   = isLabelShown ? DisassemblyCells::kInstructionColumn : DisassemblyCells::kAddressColumn;
                DxuiListView             * list     = nullptr;
                RECT                       bounds   = {};
                RECT                       cell     = {};
                float                      codeLeft = 0.0f;
                OperandResultTip::Layout   layout;

                host.options    = isLabelShown ? "address bytes source symbols" : "address bytes source";
                host.sourceText = "; main\n" + TextEncoding::WideToNarrow (line) + "\n";

                BuildSource (window, theme, text, snapshot, MakeScreen());
                list = window.GetCodeList (0);

                if (isLabelShown)
                {
                    list->SetColumnOverrideWidthPx (DisassemblyCells::kInstructionColumn, list->GetFittedWidthPx (4));
                    window.ApplyCodeSnapshot (std::make_shared<DebuggerViewSnapshot> (*snapshot), 0);
                    list->Paint (painter, text, theme);
                }

                bounds = list->GetBounds();

                Assert::IsTrue (list->GetCellsOfRow (0)[column].spansRow, L"the line in one cell");
                Assert::IsTrue (list->GetCellTextRectPx (0, column, cell));

                codeLeft = (float) (bounds.left + cell.left + (isLabelShown ? 0 : 8 * TipMetrics::kGlyphPx));

                Assert::IsTrue   (TryGetTip (window, POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 }, text, layout),
                                  L"the pane cuts the line off");
                Assert::AreEqual (3, layout.lineCount);
                Assert::AreEqual (isLabelShown ? line.substr (8) : line, TipMetrics::GetLineText (layout, 0), L"the source line, its comment and all");
                Assert::AreEqual (std::wstring (L"A=00 X=01"),    TipMetrics::GetLineText (layout, 1));
                Assert::AreEqual (std::wstring (L"Result: A=41"), TipMetrics::GetLineText (layout, 2));
                Assert::AreEqual (codeLeft, TipMetrics::GetLine (layout, 1)[0].leftPx, L"the operand under the code");
                Assert::AreEqual (codeLeft, TipMetrics::GetLine (layout, 2)[0].leftPx, L"the result under the code");
            }
        }


        //  A source row's comment after an operand cell with no values, so
        //  with no space before its ;, is the line's comment: it wraps at the
        //  screen's edge under the comment's text.
        TEST_METHOD (ASourceRowsCommentWithNoSpaceBeforeItIsFound)
        {
            constexpr int                  kRoomChars = 40;

            CassoTheme                     theme      = CassoTheme::MakeSkeuomorphic();
            SourceTipHost                  host;
            TipWindow                      window     (theme, host);
            MockDxuiTextRenderer           text;
            auto                           snapshot   = MakeSourceSnapshot ("", "");
            std::wstring                   comment    = L";";
            DxuiListView                 * list       = nullptr;
            RECT                           bounds     = {};
            RECT                           cell       = {};
            DxuiDpiScaler                  scaler;
            OperandResultTip::Layout       layout;
            TipWindow::OperandTipCell      tipCell;
            TipWindow::OperandTipScreen    screen;



            for (int i = 0; i < 60; i++)
            {
                comment += L" word";
            }

            host.options    = "address bytes source symbols";
            host.sourceText = "; main\n        clc " + TextEncoding::WideToNarrow (comment) + "\n";

            BuildSource (window, theme, text, snapshot, MakeScreen());
            list   = window.GetCodeList (0);
            bounds = list->GetBounds();

            Assert::AreEqual (comment, list->GetCellsOfRow (0)[DisassemblyCells::kOperandColumn].text, L"the comment alone in the operand's cell");
            Assert::IsTrue   (list->GetCellTextRectPx (0, DisassemblyCells::kOperandColumn, cell));

            screen.workAreas.push_back (RECT { 0, 0, bounds.left + cell.left + kRoomChars * TipMetrics::kGlyphPx + TipMetrics::kPadPx, TipMetrics::kScreenBelow });
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            scaler.SetDpi (96);

            Assert::IsTrue   (window.GetOperandTips().TryGet (POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 },
                                                              scaler, text, screen, layout, tipCell),
                              L"the pane cuts the comment off");
            Assert::IsTrue   (layout.lineCount > 1, L"wider than the screen's room");
            Assert::AreEqual ((float) (bounds.left + cell.left), TipMetrics::GetLine (layout, 0)[0].leftPx);
            Assert::AreEqual ((float) (bounds.left + cell.left + 2 * TipMetrics::kGlyphPx), TipMetrics::GetLine (layout, 1)[0].leftPx, L"under the comment's text");
        }


        //  A BASIC line's values that the pane cuts off are laid over their
        //  cell the same way.
        TEST_METHOD (TheWindowLaysTheTipOverACutOffBasicValuesCell)
        {
            CassoTheme                                    theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                                       host;
            TipWindow                                     window (theme, host);
            DxuiDpiScaler                                 scaler;
            MockDxuiTextRenderer                          text;
            MockDxuiPainter                               painter;
            DxuiListView                                * list    = nullptr;
            RECT                                          bounds  = {};
            RECT                                          cell    = {};
            OperandResultTip::Layout                      layout;
            TipWindow::OperandTipCell                     tipCell;
            std::vector<std::vector<DxuiListView::Cell>>  rows;



            scaler.SetDpi (96);
            Build (window, theme, text);
            window.ShowPane (DebuggerLayout::kBasicProgram);
            window.Layout   (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);

            list = window.GetTipList (TipWindow::kBasicTipView);
            Assert::IsNotNull (list, L"the BASIC program pane's list");

            rows.push_back ({ DxuiListView::Cell {}, DxuiListView::Cell {}, DxuiListView::Cell { L"10 PRINT A$" },
                              DxuiListView::Cell { L"A$=\"" + std::wstring (300, L'X') + L"\"" } });
            list->SetRows (std::move (rows));
            list->Paint   (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, BasicProgramPane::kValuesColumn, cell), L"the values are in view");
            Assert::IsTrue (window.GetOperandTips().TryGet (POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 },
                                                            scaler, text, MakeScreen(), layout, tipCell),
                            L"the pane cuts the long values off");
            Assert::AreEqual (TipWindow::kBasicTipView, tipCell.view);
            Assert::AreEqual ((float) (bounds.left + cell.left), TipMetrics::GetLine (layout, 0)[0].leftPx, L"the tip's text starts at the cell's");
        }


        //  The BASIC listing's menu copies, has the line's breakpoint, and
        //  loads and saves the program.
        TEST_METHOD (TheBasicListingsMenuCopiesLoadsSavesAndClears)
        {
            CassoTheme                                    theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                                       host;
            TipWindow                                     window (theme, host);
            MockDxuiTextRenderer                          text;
            PaneMenuItems                                 items;
            std::vector<std::wstring>                     labels;
            IDebuggerPane                               * pane   = nullptr;
            DxuiListView                                * list   = nullptr;



            Build (window, theme, text);
            pane = window.FindPaneForTest (DebuggerLayout::kBasicProgram);
            list = window.GetTipList (TipWindow::kBasicTipView);

            Assert::IsTrue (pane->AddMenuItems (list, -1, items), L"the listing has a menu of its own, not its pane's Dock menu");

            for (const auto & [label, action] : items)
            {
                labels.push_back (label);
            }

            Assert::IsTrue (labels == std::vector<std::wstring> { L"Copy", L"Paste", L"", L"Load BASIC program...", L"Save BASIC program...", L"Clear BASIC program..." });
        }


        TEST_METHOD (TheTipIsUpAfterAMoveOrTheWheelAndDownAfterAPressOrAKey)
        {
            CassoTheme                theme = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            POINT                     at    = {};
            DxuiKeyEvent              key   = { DxuiKeyEventKind::Down, VK_SHIFT, false, false, false, false };
            OperandResultTip::Layout  first;



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"a move onto the cut-off operand shows the tip");
            Assert::AreEqual (0, window.GetOperandTips().GetCell().view);
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);

            first = window.GetOperandTips().GetLayout();

            //  The wheel over rows that stay where they are.
            Send (window, DxuiMouseEventKind::Wheel, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"the wheel takes the tip down");
            Assert::IsTrue  (OperandResultTip::IsSame (first, window.GetOperandTips().GetLayout()), L"the wheel moves the tip off its cell");

            (void) window.OnKey (key);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a key leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            Send (window, DxuiMouseEventKind::Down, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a press leaves the tip up");

            Send (window, DxuiMouseEventKind::Up, at);
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            Send (window, DxuiMouseEventKind::Move, GetPointOffTip (window));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip is up after a move off it, onto nothing cut off");
        }


        TEST_METHOD (AButtonHeldDownTakesTheTipDown)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;
            DxuiMouseEvent        move;



            scaler.SetDpi (96);
            Build (window, theme, text);

            move.kind        = DxuiMouseEventKind::Move;
            move.positionDip = GetOperandPoint (window, 0);

            window.GetOperandTips().Place (move, false, scaler, text, MakeScreen());
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            window.GetOperandTips().Place (move, true, scaler, text, MakeScreen());
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a drag over the cell leaves the tip up");

            //  As the window gets it: a move made with the left button down.
            (void) window.OnMouse (move);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

            move.button = DxuiMouseButton::Left;

            (void) window.OnMouse (move);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a move with the button down leaves the tip up");
        }


        TEST_METHOD (MovingToAnotherCutOffRowReplacesTheTip)
        {
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  first;
            DxuiListView            * list   = nullptr;
            RECT                      cell   = {};



            Build (window, theme, text);
            list = window.GetCodeList (0);

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            first = window.GetOperandTips().GetLayout();

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 2));
            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::AreEqual (2, window.GetOperandTips().GetCell().row);
            Assert::IsFalse  (OperandResultTip::IsSame (first, window.GetOperandTips().GetLayout()), L"the tip still shows the first row's");

            Assert::IsTrue   (list->GetCellTextRectPx (2, DisassemblyCells::kOperandColumn, cell));
            Assert::AreEqual ((int) (list->GetBounds().top + cell.top), window.GetOperandTips().GetLayout().rowTop, L"over the third row");
        }


        TEST_METHOD (ThePointerLeavingTheCellByAnyRouteTakesTheTipDown)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            POINT                 at    = {};



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            Send (window, DxuiMouseEventKind::Move, at);
            window.GetOperandTips().CheckPointer (POINT { at.x + 50, at.y });
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a pointer still on the cell keeps the tip");

            //  Off the window's client area, as a pointer leaving across the
            //  frame or the caption is, with no move reaching the window.
            window.GetOperandTips().CheckPointer (POINT { at.x, -1 });
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a pointer off the window leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            window.GetOperandTips().CheckPointer (GetPointOffTip (window));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a pointer on another row leaves the tip up");

            //  The tip stays up for a pointer moved from the row down into a tip
            //  that wraps below it.
            Send (window, DxuiMouseEventKind::Move, at);
            window.GetOperandTips().CheckPointer (POINT { at.x, window.GetOperandTips().GetLayout().rect.bottom - 1 });
            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"a pointer on the tip takes it down");
        }


        TEST_METHOD (ASelectedRowsTipTakesTheSelectionFill)
        {
            CassoTheme            theme    = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list     = nullptr;
            POINT                 at       = {};
            uint32_t              cellFill = 0;



            Build (window, theme, text);
            list = window.GetCodeList (0);

            //  The third row: the PC's row has an opaque fill of its own, which
            //  hides the selection's.
            at = GetOperandPoint (window, 2);

            Send (window, DxuiMouseEventKind::Down, at);
            Send (window, DxuiMouseEventKind::Up,   at);

            Assert::IsTrue      (list->IsListFocused(), L"the click gives the list the focus");
            Assert::AreEqual    (2, list->GetSelectedRow(), L"and selects the row");
            Assert::IsFalse     (list->HasTextSelection());
            Assert::AreNotEqual (theme.ContentSelection(), theme.ContentBackground());

            Send (window, DxuiMouseEventKind::Move, at);
            cellFill = list->GetCellsOfRow (2)[DisassemblyCells::kOperandColumn].background;

            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::AreEqual (DxuiColor::Composite (cellFill, DxuiColor::Composite (theme.ContentSelection(), theme.ContentBackground())),
                              window.GetOperandTips().GetLayout().fill, L"the tip is not in the selected row's fill");
        }


        //  A disassembly's rows lie on a text view's background, which a theme
        //  can set apart from a list's, as the debugger's light theme does. The
        //  tip is in the selection color over that background, whatever the
        //  cell's fill or the hover, and the list draws the tip's row in
        //  the same color from its left edge.
        TEST_METHOD (ATipIsInTheSelectionColorAndItsRowIsToo)
        {
            constexpr uint32_t        kTextView = 0xFF2A2A40;
            constexpr uint32_t        kHover    = 0x40FFFFFF;
            CassoTheme                theme     = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            DxuiListView            * list      = nullptr;
            POINT                     at        = {};
            OperandResultTip::Layout  layout;



            theme.textViewBg   = kTextView;
            theme.contentHover = kHover;
            Assert::AreNotEqual (theme.ContentBackground(), theme.TextViewBackground());

            Build (window, theme, text);
            list = window.GetCodeList (0);
            at   = GetOperandPoint (window, 2);

            Assert::IsTrue   (TryGetTip (window, at, text, layout), L"the pane cuts the third row's operand off");
            Assert::AreEqual (DxuiColor::Composite (theme.ContentSelection(), kTextView), layout.fill, L"the selection over the text view's fill");

            list->SetHoveredRow (2);

            Assert::IsTrue   (TryGetTip (window, at, text, layout));
            Assert::AreEqual (DxuiColor::Composite (theme.ContentSelection(), kTextView), layout.fill, L"the hover doesn't change it");

            Send             (window, DxuiMouseEventKind::Move, at);
            Assert::AreEqual (2, list->GetHighlightRow(), L"the list draws the tip's row in the tip's color");
        }


        TEST_METHOD (NewRowsThatChangeNothingKeepTheTip)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));

            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the same rows again take the tip down");
        }


        //  After a step under a pointer that has not moved, the tip is up, laid
        //  out for the row now under the pointer: up with the new text where
        //  the pane cuts it off, down where it fits, and up again where the
        //  next step cuts it off, with no move in between.
        TEST_METHOD (AfterAStepTheTipIsLaidOutForTheRowUnderTheRestingPointer)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            std::string                               other;
            std::wstring                              first;



            for (int i = 0; i < kLongOperandWords; i++)
            {
                other += "Y=02 Z=1 ";
            }

            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            first = window.GetOperandTips().GetLayout().runs[0].text;

            snapshot->codeViews[0][0].annotationParts.registers = other;
            snapshot->code                                      = snapshot->codeViews[0];
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"the step takes the tip down");
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);
            Assert::AreEqual (TextEncoding::NarrowToWide (other.substr (0, other.size() - 1)), window.GetOperandTips().GetLayout().runs[0].text,
                              L"the tip is the row's before the step");
            Assert::AreNotEqual (first, window.GetOperandTips().GetLayout().runs[0].text);

            snapshot->codeViews[0][0].annotationParts.registers = "S=FD";
            snapshot->code                                      = snapshot->codeViews[0];
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a tip is up over an operand that fits");

            snapshot->codeViews[0][0].annotationParts.registers = other;
            snapshot->code                                      = snapshot->codeViews[0];
            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"no tip for the cut-off operand under the resting pointer");
        }


        //  The wheel scrolls the BASIC listing under a pointer that stays
        //  put: the tip is laid out for the line now under the pointer, with
        //  no move in between, and goes where that line's values fit.
        TEST_METHOD (AfterTheWheelTheTipIsLaidOutForTheLineScrolledUnderThePointer)
        {
            CassoTheme                                    theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                                       host;
            TipWindow                                     window (theme, host);
            DxuiDpiScaler                                 scaler;
            MockDxuiTextRenderer                          text;
            MockDxuiPainter                               painter;
            DxuiListView                                * list    = nullptr;
            RECT                                          bounds  = {};
            RECT                                          cell    = {};
            POINT                                         at      = {};
            int                                           top     = 0;
            std::vector<std::vector<DxuiListView::Cell>>  rows;

            constexpr int                                 kLines  = 200;



            scaler.SetDpi (96);
            Build (window, theme, text);
            window.ShowPane (DebuggerLayout::kBasicProgram);
            window.Layout   (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);

            list = window.GetTipList (TipWindow::kBasicTipView);
            Assert::IsNotNull (list, L"the BASIC program pane's list");

            //  The first half of the lines' values are cut off, each in a
            //  different letter; the rest fit.
            for (int i = 0; i < kLines; i++)
            {
                std::wstring  values = (i < kLines / 2) ? L"A$=\"" + std::wstring (300, (wchar_t) (L'A' + i)) + L"\"" : std::wstring (L"A=1");

                rows.push_back ({ DxuiListView::Cell {}, DxuiListView::Cell {}, DxuiListView::Cell { L"10 PRINT A$" }, DxuiListView::Cell { values } });
            }

            list->SetRows (std::move (rows));
            list->Paint   (painter, text, theme);

            bounds = list->GetBounds();
            Assert::IsTrue (list->GetCellTextRectPx (0, BasicProgramPane::kValuesColumn, cell), L"the values are in view");

            at = POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);

            Send (window, DxuiMouseEventKind::Wheel, at);
            top = list->GetTopRow();

            Assert::IsTrue   (top > 0, L"the wheel didn't scroll the listing");
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"the wheel takes the tip down");
            Assert::AreEqual (top, window.GetOperandTips().GetCell().row, L"the tip is the line's that scrolled away");
            Assert::AreEqual (list->GetCellsOfRow (top)[BasicProgramPane::kValuesColumn].text, window.GetOperandTips().GetLayout().runs[0].text);

            for (int each = 0; each < kLines && list->GetTopRow() < kLines / 2; each++)
            {
                Send (window, DxuiMouseEventKind::Wheel, at);
            }

            Assert::IsTrue  (list->GetTopRow() >= kLines / 2, L"the wheel didn't scroll to the lines that fit");
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a tip is up over values that fit");
        }


        //  The tip is the row's, so new rows that move its operand along lay
        //  it out again where the operand is now.
        TEST_METHOD (NewRowsThatWidenAColumnAheadLayTheTipOutAgain)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            float                                     before   = 0.0f;



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            before = window.GetOperandTips().GetLayout().runs[0].leftPx;

            //  The tip's own row is as it was; another's instruction is wider,
            //  which widens its column and moves the operand along.
            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the new rows take the tip down");
            Assert::IsTrue (window.GetOperandTips().GetLayout().runs[0].leftPx > before, L"the tip stays where the operand was");
        }


        TEST_METHOD (AfterThePcMovesOffTheRowTheTipKeepsItsColor)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            uint32_t                                  fill     = 0;



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            fill = window.GetOperandTips().GetLayout().fill;

            //  The text is the same; only the PC's row fill leaves the row.
            snapshot->codeViews[0][0].isCurrent = false;
            snapshot->codeViews[0][1].isCurrent = true;
            snapshot->code                      = snapshot->codeViews[0];
            snapshot->pc                        = snapshot->codeViews[0][1].address;

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue      (window.GetOperandTips().HasTip(), L"the PC's move off the row takes the tip down");
            Assert::AreEqual    (fill, window.GetOperandTips().GetLayout().fill, L"the tip stays in the selection color, not the PC's row color");
        }


        TEST_METHOD (AfterANewLayoutTheTipIsLaidOutForThePaneAsItIsNow)
        {
            CassoTheme                 theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                    host;
            TipWindow                  window (theme, host);
            MockDxuiTextRenderer       text;
            DxuiDpiScaler              scaler;
            OperandResultTip::Layout   layout;



            scaler.SetDpi (96);
            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            window.Layout (RECT { 0, 0, kWindowWidth - kNarrowerPx, kWindowHeight }, scaler);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a new layout takes the tip down");
            Assert::IsTrue (TryGetTip (window, GetOperandPoint (window, 0), text, layout), L"the narrower pane cuts the operand off");
            Assert::IsTrue (OperandResultTip::IsSame (layout, window.GetOperandTips().GetLayout()), L"the tip is laid out for the pane as it was");
        }


        TEST_METHOD (TheColorTipShowsBelowTheOperandTip)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            RECT                  under = {};
            RECT                  tip   = {};



            Build (window, theme, text);

            //  The move that puts the operand tip up also starts the operand's
            //  color tip, which waits out its dwell.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            under = window.GetTooltip().GetPendingAnchor();
            tip   = window.GetOperandTips().GetLayout().rect;

            Assert::IsTrue (window.GetOperandTips().HasTip());
            Assert::IsTrue (window.GetTooltip().WantsTick(), L"the operand's color tip is not on its way");
            Assert::IsTrue (EqualRect (&under, &tip) != FALSE, L"the color tip is placed by its cell, under the operand tip");

            //  A plain row's operand has neither tip: the operand's colors are
            //  on the column's heading.
            Send (window, DxuiMouseEventKind::Move, GetPointOffTip (window));

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"a color tip over a plain row's operand");
        }


        TEST_METHOD (TheColorTipGoesWithTheOperandTip)
        {
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                host;
            TipWindow                              window (theme, host);
            MockDxuiTextRenderer                   text;
            DxuiKeyEvent                           key      = { DxuiKeyEventKind::Down, VK_SHIFT, false, false, false, false };
            POINT                                  at       = {};
            std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            //  A key.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            (void) window.OnKey (key);

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after a key, the color tip still comes up below a tip no longer there");

            //  The pointer leaving the cell with no move reaching the window.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            window.GetOperandTips().CheckPointer (POINT { at.x, -1 });

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after the pointer leaves, the color tip still comes up below a tip no longer there");

            //  New rows that move the cell.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsTrue  (window.GetOperandTips().HasTip(), L"the tip is laid out again for the row under the pointer");
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after new rows, the color tip comes up below the tip laid out before them");
        }


        TEST_METHOD (NoTipCoversTextSelectedInTheCell)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list   = nullptr;



            Build (window, theme, text);
            list = window.GetCodeList (0);

            DragAlong (window, 0);
            Assert::IsTrue (list->IsCellTextSelected (0, DisassemblyCells::kOperandColumn), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip lies over the selection");

            //  A row whose tip reaches nowhere near the selection still has one.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 2));
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a selection in one row takes every row's tip away");
        }


        TEST_METHOD (NoTipCoversTextSelectedInARowItReachesOver)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list   = nullptr;



            Build (window, theme, text);
            list = window.GetCodeList (0);

            //  The short row under the first, whose tip wraps over it.
            DragAlong (window, 1);
            Assert::IsTrue (list->IsCellTextSelected (1, DisassemblyCells::kOperandColumn), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip of the row above lies over the selection");
        }


        //  Below its row the tip is taken to cover every column, so a
        //  selection in another column of a row it lies over takes it away
        //  too.
        TEST_METHOD (NoTipCoversTextSelectedInAnyColumnOfARowItWrapsOver)
        {
            CassoTheme            theme  = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiListView        * list   = nullptr;
            RECT                  bounds = {};
            RECT                  cell   = {};
            DxuiMouseEvent        ev;



            Build (window, theme, text);
            list   = window.GetCodeList (0);
            bounds = list->GetBounds();

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the first row has a tip before anything is selected");

            Assert::IsTrue (list->GetCellTextRectPx (1, DisassemblyCells::kInstructionColumn, cell), L"the row below's mnemonic is in view");

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };
            (void) window.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Move;
            ev.positionDip.x = bounds.left + cell.right - 1;
            (void) window.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            (void) window.OnMouse (ev);

            Assert::IsTrue (list->IsCellTextSelected (1, DisassemblyCells::kInstructionColumn), L"the drag selects the mnemonic");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip of the row above lies over the selected mnemonic");
        }


        TEST_METHOD (NoTipShowsUnderAWindowLyingOverThePane)
        {
            CassoTheme                   theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                      host;
            TipWindow                    window (theme, host);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;
            TipWindow::OperandTipScreen  screen = MakeScreen();
            POINT                        at     = {};



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);
            Assert::IsTrue (TryGetTip (window, at, text, layout));

            //  A floating command bar over the rows the tip reaches down over,
            //  clear of the pointer and of the cell.
            screen.overWindows.push_back (RECT { layout.rect.left + kOverGapPx, layout.rect.bottom - kOverGapPx,
                                                 layout.rect.left + kOverSizePx, layout.rect.bottom + kOverSizePx });
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip shows over the floating window");

            //  The same window clear of the tip.
            OffsetRect (&screen.overWindows[0], 0, kOverSizePx);
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a window clear of the tip takes it away");
        }


        TEST_METHOD (ACellAcrossTwoMonitorsHasItsTipOnBoth)
        {
            CassoTheme                   theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                      host;
            TipWindow                    window (theme, host);
            MockDxuiTextRenderer         text;
            TipWindow::OperandTipScreen  screen;
            DxuiListView               * list    = nullptr;
            RECT                         bounds  = {};
            RECT                         cell    = {};
            LONG                         start   = 0;
            LONG                         split   = 0;
            LONG                         visible = 0;
            POINT                        at      = {};



            Build (window, theme, text);

            list    = window.GetCodeList (0);
            bounds  = list->GetBounds();
            cell    = GetCellRect (window, 0);
            start   = bounds.left + cell.left;
            visible = bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0);

            //  The monitors meet a little way into the cell's text, and the
            //  pointer is on the part of the cell on the second.
            split            = start + kSplitIntoTextPx;
            at               = POINT { (split + visible) / 2, GetOperandPoint (window, 0).y };
            screen.workAreas = { RECT { 0, 0, split, TipMetrics::kScreenBelow }, RECT { split, 0, TipMetrics::kScreenRight, TipMetrics::kScreenBelow } };
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"a pointer on the second monitor has no tip");
            Assert::AreEqual ((float) start, window.GetOperandTips().GetLayout().runs[0].leftPx, L"the text starts where the cell's does, on the first monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right > visible, L"the tip runs on past the pane, on the second monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right <= TipMetrics::kScreenRight, L"the tip runs off the second monitor");

            //  A gap between them, as a taskbar down the first one's right side
            //  leaves: the tip keeps to the first monitor, with the pointer on
            //  the second.
            screen.workAreas[0].right = split - kGapPx;
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a pointer on the second monitor has no tip");
            Assert::IsTrue (window.GetOperandTips().GetLayout().rect.right <= split - kGapPx, L"the tip runs into the gap");
        }


        //  The tip is on the monitor the row starts on and the one directly
        //  to its right, and doesn't extend onto a third monitor past those
        //  two, so the text wraps at the second one's edge.
        TEST_METHOD (TheTipLiesOnTheRowsMonitorAndTheOneBesideIt)
        {
            constexpr LONG               kSecondPx = 600;

            CassoTheme                   theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                      host;
            TipWindow                    window (theme, host);
            MockDxuiTextRenderer         text;
            TipWindow::OperandTipScreen  screen;
            LONG                         split   = 0;
            POINT                        at      = {};



            Build (window, theme, text);

            at    = GetOperandPoint (window, 0);
            split = window.GetCodeList (0)->GetBounds().left + GetCellRect (window, 0).left + kSplitIntoTextPx;

            screen.workAreas = { RECT { 0, 0, split, TipMetrics::kScreenBelow }, RECT { split, 0, split + kSecondPx, TipMetrics::kScreenBelow },
                                 RECT { split + kSecondPx, 0, TipMetrics::kScreenRight, TipMetrics::kScreenBelow } };
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip());
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right <= split + kSecondPx, L"the tip runs onto the third monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().lineCount > 2, L"the operand wraps at the second monitor's edge");
            Assert::AreEqual (GetRowStart (*window.GetCodeList (0), 0), window.GetOperandTips().GetLayout().rect.left, L"from the row's address, unmoved");
        }


        TEST_METHOD (NoTipShowsOverAHeldColorKey)
        {
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;
            ColorKeyButton          * button = nullptr;
            RECT                      key    = {};
            RECT                      both   = {};
            POINT                     at     = {};



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);
            Assert::IsTrue (TryGetTip (window, at, text, layout));

            button = window.GetColorKeys().Find (DebuggerLayout::GetCodePaneId (0));
            Assert::IsNotNull (button);
            Assert::IsTrue    (button->IsVisible(), L"the disassembly's info button shows");

            Send (window, DxuiMouseEventKind::Down, GetCenter (button->GetBounds()));
            Send (window, DxuiMouseEventKind::Up,   GetCenter (button->GetBounds()));
            key = window.GetColorKeys().GetPopup().GetRect();

            Assert::IsTrue (window.GetColorKeys().GetPopup().IsHeld(), L"the click holds the key open");
            Assert::IsTrue (IntersectRect (&both, &key, &layout.rect) != FALSE, L"the key hangs over the tip's cell");

            Assert::IsFalse (TryGetTip (window, at, text, layout), L"the tip shows over the key");
        }


        TEST_METHOD (NoTipShowsOverAPaneSlidOutOverTheDisassembly)
        {
            CassoTheme                theme = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            DxuiDpiScaler             scaler;
            OperandResultTip::Layout  layout;
            RECT                      slid  = {};
            RECT                      both  = {};
            POINT                     at    = {};



            scaler.SetDpi (96);
            Build (window, theme, text);

            Assert::IsTrue (window.EditPaneLayout().AutoHide (DebuggerLayout::kConsole, DxuiDockSide::Top));
            window.GetDockSite()->Relayout();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.GetCodeList (0)->Paint (painter, text, theme);

            at = GetOperandPoint (window, 0);
            Assert::IsTrue (TryGetTip (window, at, text, layout), L"with the console tucked away, the tip shows");

            window.GetDockSite()->SlideOut (DebuggerLayout::kConsole);
            slid = window.GetDockSite()->GetSlidRect();

            Assert::AreEqual (std::wstring (DebuggerLayout::kConsole), window.GetDockSite()->GetSlidPane());
            Assert::IsTrue   (IntersectRect (&both, &slid, &layout.rect) != FALSE, L"the console slides out over the tip's cell");

            Assert::IsFalse (TryGetTip (window, at, text, layout), L"the tip shows over the slid-out console");
        }


        TEST_METHOD (NoTipShowsOverTheOpenZoomPopup)
        {
            CassoTheme                theme   = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            DxuiDpiScaler             scaler;
            DxuiListView            * list    = nullptr;
            OperandResultTip::Layout  layout;
            RECT                      zoom    = {};
            RECT                      both    = {};
            RECT                      cell    = {};
            POINT                     field   = {};
            POINT                     at      = {};
            bool                      isFound = false;



            scaler.SetDpi (96);
            Build (window, theme, text);

            //  The disassembly alone, filling the window down to the status
            //  bar, under the zoom field at its right end.
            for (const std::wstring & pane : DebuggerLayout::GetPaneIds())
            {
                if (!pane.starts_with (DebuggerLayout::kCode) && window.IsPaneShown (pane))
                {
                    Assert::IsTrue (window.EditPaneLayout().AutoHide (pane, DxuiDockSide::Left), pane.c_str());
                }
            }

            window.GetDockSite()->Relayout();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot (kManyRows), 0);

            list = window.GetCodeList (0);
            list->Paint (painter, text, theme);

            field = GetCenter (window.GetStatusBar()->GetFieldRect (window.kStatusZoom));

            Send (window, DxuiMouseEventKind::Down, field);
            Assert::IsTrue (window.IsZoomPopupOpen());
            zoom = window.GetZoomPopupRect();
            Send (window, DxuiMouseEventKind::Down, field);
            Assert::IsFalse (window.IsZoomPopupOpen());

            //  A row whose tip lies under where the popup opens.
            for (int row = 0; row < kManyRows && !isFound; row++)
            {
                if (list->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell))
                {
                    at      = GetOperandPoint (window, row);
                    isFound = TryGetTip (window, at, text, layout) && IntersectRect (&both, &zoom, &layout.rect) != FALSE;
                }
            }

            Assert::IsTrue (isFound, L"no row's tip lies under the zoom popup");

            Send (window, DxuiMouseEventKind::Down, field);
            Assert::IsTrue  (window.IsZoomPopupOpen());
            Assert::IsFalse (TryGetTip (window, at, text, layout), L"the tip shows over the zoom popup");
        }


        static POINT  GetCenter (const RECT & rect)
        {
            return POINT { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        }


        //  A row whose operand the pane cuts off, broken by spaces, then one
        //  that fits, then another cut off; or `rows` rows, every one cut off.
        static std::shared_ptr<const DebuggerViewSnapshot>  MakeSnapshot (int rows = 0)
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            std::string                     longOperand;
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  rts;
            DebuggerViewSnapshot::CodeLine  sta;



            for (int i = 0; i < kLongOperandWords; i++)
            {
                longOperand += "A=00 X=01 ";
            }

            lda.address                   = 0x0300;
            lda.bytes                     = "A5 10";
            lda.instruction               = "LDA $10";
            lda.annotationParts.registers = longOperand;
            lda.effect                    = "A=41";
            lda.isCurrent                 = true;
            rts.address                   = 0x0302;
            rts.bytes                     = "60";
            rts.instruction               = "RTS";
            rts.annotationParts.registers = "S=FD";
            sta.address                   = 0x0303;
            sta.bytes                     = "85 11";
            sta.instruction               = "STA $11";
            sta.annotationParts.registers = longOperand;

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, rts, sta };
            snapshot->codeOpen[0]  = true;

            if (rows > 0)
            {
                snapshot->codeViews[0].clear();

                for (int i = 0; i < rows; i++)
                {
                    sta.address = (Word) (0x0300 + i * 2);
                    snapshot->codeViews[0].push_back (sta);
                }
            }

            snapshot->code = snapshot->codeViews[0];

            return snapshot;
        }


        static constexpr int  kWindowWidth      = 1400;
        static constexpr int  kWindowHeight     = 900;
        static constexpr int  kNarrowerPx       = 300;
        static constexpr int  kLongOperandWords = 20;
        static constexpr int  kManyRows         = 80;
        static constexpr int  kDragPx           = 20;
        static constexpr int  kOverGapPx        = 10;
        static constexpr int  kOverSizePx       = 200;
        static constexpr int  kSplitIntoTextPx  = 60;
        static constexpr int  kGapPx            = 20;
        static constexpr int  kLastRow          = 2;
    };
}
