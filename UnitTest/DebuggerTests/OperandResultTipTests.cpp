#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/BasicPanes.h"
#include "Ui/Debugger/OperandResultTip.h"
#include "Ui/Debugger/ColorKeyButton.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace OperandResultTipTests
{
    //  A host that does nothing; the tests read only the window's own state.
    using TipHost = NullDebuggerWindowHost;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TipWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TipWindow : public DebuggerWindow
    {
    public:
        TipWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OperandTipCell;
        using DebuggerWindow::OperandTipScreen;
        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnMouse;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetOperandTips;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetColorKeys;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::IsZoomPopupOpen;
        using DebuggerWindow::GetZoomPopupRect;
        using DebuggerWindow::kStatusZoom;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::GetTipList;
        using DebuggerWindow::kBasicTipView;
        using DebuggerWindow::FindPaneForTest;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  OperandResultTipTests
    //
    //  A disassembly row's operand and result shown whole over their cell
    //  where the pane cuts them off: only then, with the text exactly where
    //  the cell's is, the operand wrapped first and the result on a line of
    //  its own, in the cell's colors on its fill, covering the whole of the
    //  cell the pane shows and staying on the screen without leaving the
    //  cell, on both monitors where the cell runs across two. Never over
    //  selected text or a window lying over the pane, and a color tip below
    //  it goes with it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (OperandResultTipTests)
    {
    public:

        //  The mock renderer measures every character this wide.
        static constexpr int       kGlyphPx     = 7;
        static constexpr int       kTextLeft    = 100;
        static constexpr int       kRowTop      = 40;
        static constexpr int       kRowHeight   = 16;
        static constexpr int       kColumnRight = 2000;
        static constexpr int       kPadPx       = 4;
        static constexpr int       kScreenRight = 4000;
        static constexpr int       kScreenBelow = 3000;
        static constexpr float     kFontPx      = 12.0f;
        static constexpr uint32_t  kPage        = 0xFF1E1E1E;
        static constexpr uint32_t  kInk         = 0xFFDCDCDC;
        static constexpr uint32_t  kOperand     = 0xFF57A64A;
        static constexpr uint32_t  kResult      = 0xFF4EC9B0;
        static constexpr uint32_t  kPcRow       = 0xFF4B4B18;
        static constexpr uint32_t  kPcTint      = 0x604B4B18;
        static constexpr uint32_t  kSelection   = 0xFF264F78;
        static constexpr RECT      kScreen      = { 0, 0, kScreenRight, kScreenBelow };


        //  Room for `chars` characters of text after the cell's padding, to
        //  the pane's edge.
        static int  GetEdgeFor (int chars)
        {
            return kTextLeft + chars * kGlyphPx + kPadPx;
        }


        static OperandResultTip::Placement  MakePlacement (int visibleRight)
        {
            OperandResultTip::Placement  placement;



            placement.textRect          = RECT { kTextLeft, kRowTop, kColumnRight, kRowTop + kRowHeight };
            placement.visibleLeft       = 0;
            placement.visibleRight      = visibleRight;
            placement.padLeftPx         = kPadPx;
            placement.padRightPx        = kPadPx;
            placement.fontPx            = kFontPx;
            placement.face              = DxuiTheme::kMonoFace;
            placement.contentBackground = kPage;
            placement.ink               = kInk;
            placement.workArea          = kScreen;

            return placement;
        }


        //  The disassembly's own cell: the operand in its color, the result
        //  after it in the result color.
        static DxuiListView::Cell  MakeCell (const std::string & annotation, const std::string & effect)
        {
            DxuiListView::Cell  cell = DisassemblyCells::GetOperandAndResultCell (annotation, effect, kResult);



            cell.argb = kOperand;
            return cell;
        }


        static std::wstring  GetLineText (const OperandResultTip::Layout & layout, size_t index)
        {
            const OperandResultTip::Line &  line = layout.lines[index];



            return layout.text.substr ((size_t) line.start, (size_t) line.length);
        }


        TEST_METHOD (AppearsOnlyWhereThePaneCutsTheCellOff)
        {
            DxuiListView::Cell        cell = MakeCell ("$0010=41", "A=41");
            int                       size = (int) cell.text.size();
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            //  Text that ends exactly at the pane's edge fits.
            Assert::IsFalse (OperandResultTip::TryMakeLayout (cell, MakePlacement (kTextLeft + size * kGlyphPx), text, layout),
                             L"a cell that fits has no tip");
            Assert::IsFalse (OperandResultTip::TryMakeLayout (cell, MakePlacement (kColumnRight), text, layout),
                             L"a wide pane has no tip");

            Assert::IsTrue  (OperandResultTip::TryMakeLayout (cell, MakePlacement (kTextLeft + size * kGlyphPx - 1), text, layout),
                             L"a pixel short of the text is cut off");
        }


        TEST_METHOD (AColumnNarrowerThanItsTextCutsItOffToo)
        {
            DxuiListView::Cell           cell      = MakeCell ("$0010=41", "A=41");
            OperandResultTip::Placement  placement = MakePlacement (kColumnRight);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.textRect.right = GetEdgeFor (10);

            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, placement, text, layout), L"the column cuts the text off");
            Assert::IsTrue (layout.rect.right >= placement.textRect.right, L"the tip covers the whole of the column");
        }


        TEST_METHOD (NothingToShowOrMeasureHasNoTip)
        {
            DxuiListView::Cell           empty;
            DxuiListView::Cell           cell   = MakeCell ("A=00 X=01 $1234=41", "A=41");
            OperandResultTip::Placement  hidden = MakePlacement (GetEdgeFor (4));
            MockDxuiTextRenderer         text;
            DxuiNullTextRenderer         none;
            OperandResultTip::Layout     layout;



            hidden.visibleLeft = kTextLeft + 1;

            Assert::IsFalse (OperandResultTip::TryMakeLayout (empty, MakePlacement (GetEdgeFor (4)), text, layout), L"an empty cell");
            Assert::IsFalse (OperandResultTip::TryMakeLayout (cell,  MakePlacement (GetEdgeFor (4)), none, layout), L"text that cannot be measured");
            Assert::IsFalse (OperandResultTip::TryMakeLayout (cell,  hidden,                         text, layout), L"a start scrolled out of sight");
        }


        TEST_METHOD (TheTipsTextStartsExactlyAtTheCellsText)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 $1234=41", "A=41");
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            OperandResultTip::Layout  layout;



            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (30)), text, layout));

            Assert::AreEqual ((LONG) kTextLeft,          layout.textOrigin.x, L"the text starts where the cell's does");
            Assert::AreEqual ((LONG) kRowTop,            layout.textOrigin.y, L"on the cell's row");
            Assert::AreEqual ((LONG) (kTextLeft - kPadPx), (LONG) layout.firstLeft, L"the cell's padding is ahead of it");
            Assert::AreEqual ((LONG) kRowTop,            layout.rect.top,     L"the tip lies on the row, not over the row above");

            //  Drawn with the tip's top left at the origin, as a popup is: the
            //  first character lands on the cell's own.
            OperandResultTip::Paint (layout, painter, text);

            Assert::IsFalse  (text.Calls().empty(), L"the tip draws its text");
            Assert::AreEqual ((float) kTextLeft,  (float) layout.rect.left + text.Calls()[0].x, L"the first line's left");
            Assert::AreEqual ((float) kRowTop,    (float) layout.rect.top  + text.Calls()[0].y, L"the first line's top");
            Assert::AreEqual ((float) kRowHeight, text.Calls()[0].height,                       L"a row high, as the cell is");
        }


        //  The operand and the result on the one line the cell draws them on,
        //  running past the pane's edge to the end of the text, a row high.
        TEST_METHOD (TextTheScreenHasRoomForStaysOnOneLinePastThePane)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 $1234=41", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;
            int                       size = (int) cell.text.size();



            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (10)), text, layout));

            Assert::AreEqual ((size_t) 1,                                   layout.lines.size(), L"the tip does not wrap");
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41  Result: A=41"), GetLineText (layout, 0));
            Assert::AreEqual ((LONG) (kTextLeft + size * kGlyphPx + kPadPx),  layout.rect.right,  L"past the pane's edge to the end of the text");
            Assert::AreEqual ((LONG) (kRowTop + kRowHeight),                  layout.rect.bottom, L"one row high");
            Assert::IsFalse  (OperandResultTip::IsShaped (layout));
        }


        //  Only a screen too narrow for the line wraps it: the operand at its
        //  spaces, then the result starting a new line.
        TEST_METHOD (ALongOperandWrapsAtItsSpacesBeforeTheResult)
        {
            constexpr int                kRoomChars = 24;
            std::string                  operand    = "A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0";
            DxuiListView::Cell           cell       = MakeCell (operand, "A=41");
            OperandResultTip::Placement  placement  = MakePlacement (GetEdgeFor (kRoomChars));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;
            std::wstring                 joined;



            placement.workArea.right = GetEdgeFor (kRoomChars);

            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, placement, text, layout));

            Assert::AreEqual ((size_t) 3, layout.lines.size(), L"two lines of operand, one of result");
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"C=1 Z=0 N=1 V=0"),         GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: A=41"),            GetLineText (layout, 2));

            for (size_t i = 0; i < layout.lines.size(); i++)
            {
                Assert::IsTrue (layout.lines[i].length <= kRoomChars, GetLineText (layout, i).c_str());
            }

            joined = GetLineText (layout, 0) + L" " + GetLineText (layout, 1);

            Assert::AreEqual (TextEncoding::NarrowToWide (operand), joined, L"nothing of the operand is lost at a break");
        }


        TEST_METHOD (AWordWiderThanALineBreaksWhereTheLineEnds)
        {
            constexpr int                kRoomChars = 24;
            DxuiListView::Cell           cell       = MakeCell ("Turn_on_the_80_column_display_now", "");
            OperandResultTip::Placement  placement  = MakePlacement (GetEdgeFor (kRoomChars));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.workArea.right = GetEdgeFor (kRoomChars);

            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, placement, text, layout));

            Assert::AreEqual ((size_t) 2,                                 layout.lines.size());
            Assert::AreEqual (kRoomChars,                                 layout.lines[0].length);
            Assert::AreEqual (std::wstring (L"Turn_on_the_80_column_di"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"splay_now"),                GetLineText (layout, 1));
        }


        //  However little of the cell is visible in the pane, the tip is one line.
        TEST_METHOD (ANarrowPaneLeavesTheTipOnOneLine)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;
            int                       size = (int) cell.text.size();



            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (3)), text, layout));

            Assert::AreEqual ((size_t) 1, layout.lines.size(), L"wrapped in the pane's room");
            Assert::AreEqual ((LONG) (kTextLeft + size * kGlyphPx + kPadPx), layout.rect.right, L"the tip stops short of the text's end");
        }


        //  The lines a tip wraps onto at the screen's edge are covered from
        //  the pane's left edge, so only the tip's text is visible on them; its
        //  first line starts at the cell, so the address and mnemonic of the
        //  tip's row stay in view.
        TEST_METHOD (AWrappedTipCoversTheRowsBelowFromThePanesEdge)
        {
            constexpr int                   kPaneLeft = 20;
            DxuiListView::Cell              cell      = MakeCell ("A=00 X=01 $1234=41", "A=41");
            OperandResultTip::Placement     placement = MakePlacement (GetEdgeFor (30));
            MockDxuiTextRenderer            text;
            MockDxuiPainter                 painter;
            OperandResultTip::Layout        layout;
            std::vector<RecordedPaintCall>  fills;
            float                           width     = 0.0f;
            float                           below     = 0.0f;



            placement.visibleLeft    = kPaneLeft;
            placement.workArea.right = GetEdgeFor (20);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::IsTrue   (layout.lines.size() > 1, L"the result wraps onto a line of its own");
            Assert::AreEqual ((LONG) kPaneLeft, layout.rect.left, L"the lines below start at the pane's edge");
            Assert::AreEqual (kTextLeft - kPadPx, layout.firstLeft, L"the first starts at the cell");
            Assert::IsTrue   (OperandResultTip::IsShaped (layout));

            OperandResultTip::Paint (layout, painter, text);

            std::ranges::copy_if (painter.Calls(), std::back_inserter (fills), [&layout] (const RecordedPaintCall & call)
            {
                return call.kind == RecordedPaintKind::FillRect && call.argb == layout.fill;
            });

            width = (float) (layout.rect.right - layout.rect.left);
            below = (float) (kRowTop + kRowHeight - layout.rect.top);

            Assert::AreEqual ((size_t) 2, painter.Calls().size(), L"the fill and nothing else: the tip has no outline");
            Assert::AreEqual ((size_t) 2, fills.size(), L"the first line's fill and the lines' below");
            Assert::AreEqual ((float) (layout.firstLeft - kPaneLeft), fills[0].x, L"the first line's fill starts at the cell");
            Assert::AreEqual (0.0f,  fills[0].y);
            Assert::AreEqual (below, fills[0].height, L"to the bottom of the cell's row");
            Assert::AreEqual (0.0f,  fills[1].x,      L"the lines below are filled from the pane's edge");
            Assert::AreEqual (below, fills[1].y);
            Assert::AreEqual (width, fills[1].width,  L"across the whole tip");
        }


        //  A tip of one line is the cell's rectangle, as it was.
        TEST_METHOD (AOneLineTipStaysARectangle)
        {
            DxuiListView::Cell           cell      = MakeCell ("A=00 X=01 Y=02 $1234=41", "");
            OperandResultTip::Placement  placement = MakePlacement (kScreenRight);
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  Cut off at twenty characters by its column.
            placement.visibleLeft    = 20;
            placement.textRect.right = GetEdgeFor (20);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::AreEqual ((size_t) 1, layout.lines.size(), L"the operand on one line");
            Assert::IsFalse  (OperandResultTip::IsShaped (layout));
            Assert::AreEqual ((LONG) (kTextLeft - kPadPx), layout.rect.left, L"the cell's left, not the pane's");
        }


        TEST_METHOD (AtTheScreensRightEdgeTheTipGrowsDown)
        {
            constexpr int                kRoomChars = 12;

            DxuiListView::Cell           cell      = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41");
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (30));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  The pane runs past the screen's edge, which leaves the text less
            //  room than the text takes.
            placement.workArea.right = GetEdgeFor (kRoomChars);

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::AreEqual ((LONG) kTextLeft, layout.textOrigin.x, L"the text still starts where the cell's does");
            Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"the tip runs off the screen, where its popup would be moved off the cell");
            Assert::IsTrue   (layout.lines.size() > 3, L"the tip grows down instead");

            for (size_t i = 0; i < layout.lines.size(); i++)
            {
                Assert::IsTrue (layout.lines[i].length <= kRoomChars, GetLineText (layout, i).c_str());
            }
        }


        TEST_METHOD (ATipTooTallForTheScreenBelowItsRowHasNoTip)
        {
            constexpr int                kRoomChars = 24;
            DxuiListView::Cell           cell       = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0", "A=41");
            OperandResultTip::Placement  placement  = MakePlacement (GetEdgeFor (kRoomChars));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  Wrapped at the screen's edge the operand takes two lines and
            //  the result a third.
            placement.workArea.right  = GetEdgeFor (kRoomChars);
            placement.workArea.bottom = kRowTop + 3 * kRowHeight;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::AreEqual ((size_t) 3, layout.lines.size());
            Assert::IsTrue   (layout.rect.bottom <= placement.workArea.bottom, L"the tip runs off the bottom of the screen");

            placement.workArea.bottom = kRowTop + 2 * kRowHeight;

            Assert::IsFalse (OperandResultTip::TryMakeLayout (cell, placement, text, layout), L"a tip too tall for the screen");
        }


        TEST_METHOD (AtTheScreensLeftEdgeThePaddingGivesWay)
        {
            DxuiListView::Cell           cell      = MakeCell ("A=00 X=01 Y=02 $1234=41", "A=41");
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            placement.workArea.left = kTextLeft - 1;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::AreEqual ((LONG) (kTextLeft - 1), layout.rect.left,    L"the tip starts at the screen's edge");
            Assert::AreEqual ((LONG) kTextLeft,       layout.textOrigin.x, L"and its text where the cell's does");

            placement.workArea.left = kTextLeft + 1;

            Assert::IsFalse (OperandResultTip::TryMakeLayout (cell, placement, text, layout), L"a start off the screen");
        }


        TEST_METHOD (TheTipsSizeSurvivesItsPopupsTripThroughDips)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            for (UINT dpi : { 120u, 144u, 168u })
            {
                OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (30));
                int                          width     = 0;
                int                          height    = 0;

                placement.dpi = dpi;

                Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, placement, text, layout));

                width  = layout.rect.right  - layout.rect.left;
                height = layout.rect.bottom - layout.rect.top;

                Assert::AreEqual (width,  OperandResultTip::GetPopupPx (width,  dpi), L"the popup comes up wider than the outline");
                Assert::AreEqual (height, OperandResultTip::GetPopupPx (height, dpi), L"the popup comes up taller than the outline");
                Assert::IsTrue   (layout.rect.right >= GetEdgeFor (30), L"the tip still covers the cell");

                //  Against the screen's edge it cannot grow, so it shrinks.
                placement.workArea.right = GetEdgeFor (30);

                Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, placement, text, layout));

                width = layout.rect.right - layout.rect.left;

                Assert::AreEqual (width, OperandResultTip::GetPopupPx (width, dpi));
                Assert::IsTrue   (layout.rect.right <= placement.workArea.right, L"the tip runs off the screen");
            }
        }


        TEST_METHOD (TheTipDrawsInTheCellsColorsOnItsFill)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 $1234=41", "A=41");
            MockDxuiTextRenderer      text;
            MockDxuiPainter           painter;
            OperandResultTip::Layout  layout;



            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (30)), text, layout));
            OperandResultTip::Paint (layout, painter, text);

            Assert::AreEqual (kPage, layout.fill, L"the pane's own fill");
            Assert::AreEqual ((size_t) 2, text.Calls().size(), L"one run for each color");
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41  "), text.Calls()[0].text);
            Assert::AreEqual (kOperand,                               text.Calls()[0].argb, L"the operand's color");
            Assert::AreEqual (std::wstring (L"Result: A=41"),         text.Calls()[1].text);
            Assert::AreEqual (kResult,                                text.Calls()[1].argb, L"the result's color");
            Assert::AreEqual (text.Calls()[0].y, text.Calls()[1].y, L"the result on the operand's line");
            Assert::IsTrue   (text.Calls()[0].fontSizeDip == kFontPx, L"the cell's size");

            //  A rectangle: its popup is cleared to the fill, and the tip has
            //  no outline to draw over it.
            Assert::IsTrue (painter.Calls().empty(), L"the tip draws an outline");
        }


        TEST_METHOD (ARowsFillShowsUnderTheTip)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 $1234=41", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            cell.background = kPcRow;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (30)), text, layout));
            Assert::AreEqual (DxuiColor::Composite (kPcRow, kPage), layout.fill, L"the PC's row reads as the PC's row");
        }


        TEST_METHOD (ASelectedRowsFillShowsUnderTheTip)
        {
            DxuiListView::Cell           cell      = MakeCell ("A=00 X=01 $1234=41", "A=41");
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (10));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            cell.background   = kPcTint;
            placement.rowFill = kSelection;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::AreEqual (DxuiColor::Composite (kPcTint, DxuiColor::Composite (kSelection, kPage)), layout.fill,
                              L"the cell's fill over the selection's, over the pane's");
            Assert::AreEqual (DxuiColor::ComputeInkForContrast (kOperand, layout.fill, DebuggerTextColors::s_kMinTextContrast), layout.colors[0],
                              L"the text reads on that fill");
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


        //  A point on another row's operand that the tip up now doesn't
        //  cover, or just below the tip when it covers every row below: a
        //  tip that wraps below its row is the pointer's to move into.
        static POINT  GetPointOffTip (TipWindow & window)
        {
            RECT   tip   = window.GetOperandTips().GetLayout().rect;
            POINT  first = GetOperandPoint (window, 0);
            int    row   = 0;
            RECT   cell  = {};



            for (row = 1; window.GetCodeList (0)->GetCellTextRectPx (row, DisassemblyCells::kOperandColumn, cell); row++)
            {
                POINT  at = GetOperandPoint (window, row);



                if (PtInRect (&tip, at) == FALSE)
                {
                    return at;
                }
            }

            return POINT { first.x, tip.bottom + 1 };
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



            screen.workAreas.push_back (kScreen);
            return screen;
        }


        //  A screen whose right edge is the code pane's, so a cut-off cell has
        //  no room past the pane and its tip wraps.
        static TipWindow::OperandTipScreen  MakeScreenEndingAtPane (TipWindow & window)
        {
            TipWindow::OperandTipScreen  screen;



            screen.workAreas.push_back (RECT { 0, 0, window.GetCodeList (0)->GetBounds().right, kScreenBelow });
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
            Assert::AreEqual (bounds.left + cell.left, layout.textOrigin.x, L"the tip's text starts at the cell's");
            Assert::AreEqual (bounds.top  + cell.top,  layout.textOrigin.y);
            Assert::AreEqual ((size_t) 1, layout.lines.size(), L"the operand wraps in the pane's width");
            Assert::IsTrue   (layout.rect.right > bounds.right, L"the tip stops at the pane's edge");

            Assert::IsFalse (window.GetOperandTips().TryGet (GetOperandPoint (window, 1), scaler, text, MakeScreen(), layout, tipCell),
                             L"a short operand fits");
        }


        //  A row whose text at the right edge runs on from a cell ahead of
        //  the operand, as a data line's string does, has the tip over that
        //  cell, from its column to the row's end.
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
            OperandResultTip::Layout        layout;
            TipWindow::OperandTipCell       tipCell;



            data.address     = 0x0300;
            data.bytes       = "48 49";
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
            Assert::AreEqual (list->GetCellsOfRow (0)[DisassemblyCells::kInstructionColumn].text, layout.text);
            Assert::AreEqual (bounds.left + cell.left, layout.textOrigin.x, L"the tip's text starts at the cell's");
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
            Assert::AreEqual (bounds.left + cell.left, layout.textOrigin.x, L"the tip's text starts at the cell's");
        }


        //  The BASIC listing's menu copies, has the line's breakpoint, and
        //  loads and saves the program.
        TEST_METHOD (TheBasicListingsMenuCopiesAndLoadsAndSaves)
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

            Assert::IsTrue (labels == std::vector<std::wstring> { L"Copy", L"", L"Load BASIC program...", L"Save BASIC program..." });
        }


        TEST_METHOD (AMoveShowsTheTipAndAnythingElseTakesItDown)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            POINT                 at    = {};
            DxuiKeyEvent          key   = { DxuiKeyEventKind::Down, VK_SHIFT, false, false, false, false };



            Build (window, theme, text);
            at = GetOperandPoint (window, 0);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"a move onto the cut-off operand shows the tip");
            Assert::AreEqual (0, window.GetOperandTips().GetCell().view);
            Assert::AreEqual (0, window.GetOperandTips().GetCell().row);

            Send (window, DxuiMouseEventKind::Wheel, at);
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the wheel leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.GetOperandTips().HasTip());

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
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"a move onto an operand that fits leaves the tip up");
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
            Assert::AreEqual (list->GetBounds().top + cell.top, window.GetOperandTips().GetLayout().textOrigin.y, L"over the third row");
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
        //  can set apart from a list's, as the debugger's light theme does. A
        //  tip's row is the row under the pointer, so the tip takes the hover
        //  seen through over that background, never the list color, whether
        //  or not the list has marked the row as hovered yet.
        TEST_METHOD (ATipTakesTheTextViewFillItsRowLiesOn)
        {
            constexpr uint32_t        kTextView = 0xFF2A2A40;
            constexpr uint32_t        kHover    = 0x40FFFFFF;
            CassoTheme                theme     = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            DxuiListView            * list      = nullptr;
            POINT                     at        = {};
            uint32_t                  cellFill  = 0;
            OperandResultTip::Layout  layout;



            theme.textViewBg   = kTextView;
            theme.contentHover = kHover;
            Assert::AreNotEqual (theme.ContentBackground(), theme.TextViewBackground());

            Build (window, theme, text);
            list     = window.GetCodeList (0);
            at       = GetOperandPoint (window, 2);
            cellFill = list->GetCellsOfRow (2)[DisassemblyCells::kOperandColumn].background;

            Assert::IsTrue   (TryGetTip (window, at, text, layout), L"the pane cuts the third row's operand off");
            Assert::AreEqual (DxuiColor::Composite (cellFill, DxuiColor::Composite (kHover, kTextView)), layout.fill,
                              L"a row the list has not marked yet takes the hover over the text view's fill");

            list->SetHoveredRow (2);

            Assert::IsTrue   (TryGetTip (window, at, text, layout));
            Assert::AreEqual (DxuiColor::Composite (cellFill, DxuiColor::Composite (kHover, kTextView)), layout.fill,
                              L"a hovered row shows the hover over the text view's fill");
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


        //  The window passes the move that brings the tip up to the list after
        //  the tip, and the list then marks the row as hovered. Neither that,
        //  nor the move the system sends when the tip's popup appears under a
        //  pointer at rest, nor the next layout, changes the tip. Before the
        //  fix, each change hid the tip and the next move brought it back up.
        TEST_METHOD (TheTipStaysUpWhileThePointerRestsOnTheCell)
        {
            constexpr uint32_t        kHover = 0x40FFFFFF;
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            DxuiDpiScaler             scaler;
            POINT                     at     = {};
            OperandResultTip::Layout  first;



            //  The third row, whose fill lets the hover through.
            theme.contentHover = kHover;
            scaler.SetDpi (96);
            Build (window, theme, text);
            at = GetOperandPoint (window, 2);

            Assert::AreNotEqual (2, window.GetCodeList (0)->GetHoveredRow(), L"the list has the row marked before the move");

            Send (window, DxuiMouseEventKind::Move, at);
            first = window.GetOperandTips().GetLayout();
            Assert::IsTrue (window.GetOperandTips().HasTip());

            //  As the list does once it receives the move.
            window.GetCodeList (0)->SetHoveredRow (2);

            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the row's hover takes the tip down");

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a move to the same point takes the tip down");
            Assert::IsTrue (OperandResultTip::IsSame (first, window.GetOperandTips().GetLayout()), L"a move to the same point lays the tip out anew");
        }


        TEST_METHOD (NewRowsThatWidenAColumnAheadTakeTheTipDown)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            //  The tip's own row is as it was; another's instruction is wider,
            //  which widens its column and moves the operand along.
            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip stays where the cell was");
        }


        TEST_METHOD (ThePcLeavingTheRowTakesTheTipDown)
        {
            CassoTheme                                theme    = CassoTheme::MakeSkeuomorphic();
            TipHost                                   host;
            TipWindow                                 window (theme, host);
            MockDxuiTextRenderer                      text;
            std::shared_ptr<DebuggerViewSnapshot>     snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());



            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            //  The text is the same; only the PC's row fill leaves the row.
            snapshot->codeViews[0][0].isCurrent = false;
            snapshot->codeViews[0][1].isCurrent = true;
            snapshot->code                      = snapshot->codeViews[0];
            snapshot->pc                        = snapshot->codeViews[0][1].address;

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip keeps the PC's row fill");
        }


        //  The tip runs to the end of its text wherever the pane's edge is, so
        //  narrowing the pane under it changes only how much of the cell is in
        //  view.
        TEST_METHOD (ANarrowedPaneKeepsTheTip)
        {
            CassoTheme                theme = CassoTheme::MakeSkeuomorphic();
            TipHost                   host;
            TipWindow                 window (theme, host);
            MockDxuiTextRenderer      text;
            DxuiDpiScaler             scaler;
            OperandResultTip::Layout  first;
            RECT                      area  = {};



            scaler.SetDpi (96);
            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip());

            first = window.GetOperandTips().GetLayout();
            area  = window.GetOperandTips().GetCell().area;

            window.Layout (RECT { 0, 0, kWindowWidth - kNarrowerPx, kWindowHeight }, scaler);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the narrowed pane takes the tip down");
            Assert::IsTrue (OperandResultTip::IsSame (first, window.GetOperandTips().GetLayout()), L"the tip follows the pane's edge");
            Assert::IsTrue (window.GetOperandTips().GetCell().area.right < area.right, L"the cell in view is as wide as it was");
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
            tip.left = window.GetOperandTips().GetLayout().firstLeft;

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

            Assert::IsFalse (window.GetOperandTips().HasTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after new rows, the color tip still comes up below a tip no longer there");
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
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreenEndingAtPane (window));

            //  The short row under the first, whose tip wraps over it at the
            //  screen's edge.
            DragAlong (window, 1);
            Assert::IsTrue (list->IsCellTextSelected (1, DisassemblyCells::kOperandColumn), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.GetOperandTips().HasTip(), L"the tip of the row above lies over the selection");
        }


        //  Below its first line a tip wrapped at the screen's edge covers the
        //  whole row, so a selection in another column of a row it wraps over
        //  takes it away too.
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
            window.GetOperandTips().SetDeviceForTest (&text, MakeScreenEndingAtPane (window));

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.GetOperandTips().HasTip(), L"the first row has a tip before anything is selected");
            Assert::IsTrue (window.GetOperandTips().GetLayout().lines.size() > 1, L"the tip wraps at the screen's edge");

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
            screen.workAreas = { RECT { 0, 0, split, kScreenBelow }, RECT { split, 0, kScreenRight, kScreenBelow } };
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.GetOperandTips().HasTip(), L"a pointer on the second monitor has no tip");
            Assert::AreEqual (start,   window.GetOperandTips().GetLayout().textOrigin.x, L"the text starts where the cell's does, on the first monitor");
            Assert::IsTrue   (window.GetOperandTips().GetLayout().rect.right > visible, L"the tip stops at the pane's edge on the second monitor");

            //  A gap between them, as a taskbar down the first one's right side
            //  leaves: the tip keeps to the first monitor, with the pointer on
            //  the second.
            screen.workAreas[0].right = split - kGapPx;
            window.GetOperandTips().SetDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue (window.GetOperandTips().HasTip(), L"a pointer on the second monitor has no tip");
            Assert::IsTrue (window.GetOperandTips().GetLayout().rect.right <= split - kGapPx, L"the tip runs into the gap");
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
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchArrowTipTests
    //
    //  The tip over the PC's branch arrow has whether the branch will be
    //  taken; the tip over the dotted arrow has the branch the last step
    //  took to the PC.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BranchArrowTipTests)
    {
    public:

        static constexpr int  kWindowWidth  = 1400;
        static constexpr int  kWindowHeight = 900;

        //  $0300 DEY, $0302 BNE $0300 at the PC, which won't branch, $0304
        //  DEX, $0306 JMP $0302, the step that led to the PC.
        static std::shared_ptr<const DebuggerViewSnapshot>  MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  dey;
            DebuggerViewSnapshot::CodeLine  bne;
            DebuggerViewSnapshot::CodeLine  dex;
            DebuggerViewSnapshot::CodeLine  jmp;



            dey.address       = 0x0300;
            dey.bytes         = "88";
            dey.instruction   = "DEY";
            bne.address       = 0x0302;
            bne.bytes         = "D0 FC";
            bne.instruction   = "BNE $0300";
            bne.isCurrent     = true;
            bne.target        = 0x0300;
            bne.isTargetTaken = false;
            dex.address       = 0x0304;
            dex.bytes         = "CA";
            dex.instruction   = "DEX";
            jmp.address       = 0x0306;
            jmp.bytes         = "4C 02 03";
            jmp.instruction   = "JMP $0302";
            jmp.target        = 0x0302;

            snapshot->pc           = 0x0302;
            snapshot->isPaused     = true;
            snapshot->branchedFrom = 0x0306;
            snapshot->codeViews[0] = { dey, bne, dex, jmp };
            snapshot->codeOpen[0]  = true;
            snapshot->code         = snapshot->codeViews[0];
            return snapshot;
        }

        //  A point on the arrows' upright, left of the mnemonics, halfway
        //  down from one row to the next.
        static POINT  GetUprightPoint (TipWindow & window, int fromRow)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            bounds = list->GetBounds();
            RECT            from   = {};
            RECT            to     = {};



            Assert::IsTrue (list->GetCellTextRectPx (fromRow,     DisassemblyCells::kInstructionColumn, from));
            Assert::IsTrue (list->GetCellTextRectPx (fromRow + 1, DisassemblyCells::kInstructionColumn, to));

            return POINT { bounds.left + from.left - (LONG) (BranchArrow::Input().marginPx + BranchArrow::Input().stubPx),
                           bounds.top + (from.top + from.bottom + to.top + to.bottom) / 4 };
        }


        TEST_METHOD (EachArrowsTipHasWhatItsBranchDoes)
        {
            CassoTheme            theme;
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            MockDxuiPainter       painter;
            DxuiDpiScaler         scaler;
            DxuiTip               tip;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, kWindowWidth, kWindowHeight }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);
            window.GetCodeList (0)->Paint (painter, text, theme);

            Assert::IsTrue (static_cast<const DxuiPanel &> (window).TryGetTip (GetUprightPoint (window, 0), tip), L"the PC's arrow, from its row up to the target's");
            Assert::AreEqual (std::wstring (L"This branch won't be taken"), tip.text);

            Assert::IsTrue   (static_cast<const DxuiPanel &> (window).TryGetTip (GetUprightPoint (window, 2), tip), L"the dotted arrow, from the step's row up to the PC's");
            Assert::AreEqual (std::wstring (L"This jump was taken"), tip.text);
        }
    };
}
