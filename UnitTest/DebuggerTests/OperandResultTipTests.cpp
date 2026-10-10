#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerWindow.h"
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
        using DebuggerWindow::TryGetOperandTip;
        using DebuggerWindow::PlaceOperandTip;
        using DebuggerWindow::CheckOperandTipPointer;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::HasOperandTip;
        using DebuggerWindow::GetOperandTipLayout;
        using DebuggerWindow::GetOperandTipCell;
        using DebuggerWindow::SetOperandTipDeviceForTest;
        using DebuggerWindow::GetDockSite;
        using DebuggerWindow::EditPaneLayout;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetColorKey;
        using DebuggerWindow::GetColorKeyPopup;
        using DebuggerWindow::GetStatusBar;
        using DebuggerWindow::IsZoomPopupOpen;
        using DebuggerWindow::GetZoomPopupRect;
        using DebuggerWindow::kStatusZoom;
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
        static constexpr uint32_t  kBorder      = 0xFF808080;
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
            placement.border            = kBorder;
            placement.workArea          = kScreen;

            return placement;
        }


        //  The disassembly's own cell: the operand in its color, the result
        //  after it in the result color.
        static DxuiListView::Cell  MakeCell (const std::string & annotation, const std::string & effect)
        {
            DxuiListView::Cell  cell = DebuggerWindow::GetOperandAndResultCell (annotation, effect, kResult);



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
            Assert::AreEqual ((LONG) (kTextLeft - kPadPx), layout.rect.left,  L"the cell's padding is ahead of it");
            Assert::AreEqual ((LONG) (kRowTop - OperandResultTip::kBorderPx), layout.rect.top, L"the outline lies past the row above");

            //  Drawn with the tip's top left at the origin, as a popup is: the
            //  first character lands on the cell's own.
            OperandResultTip::Paint (layout, painter, text);

            Assert::IsFalse  (text.Calls().empty(), L"the tip draws its text");
            Assert::AreEqual ((float) kTextLeft,  (float) layout.rect.left + text.Calls()[0].x, L"the first line's left");
            Assert::AreEqual ((float) kRowTop,    (float) layout.rect.top  + text.Calls()[0].y, L"the first line's top");
            Assert::AreEqual ((float) kRowHeight, text.Calls()[0].height,                       L"a row high, as the cell is");
        }


        TEST_METHOD (TheResultStartsALineOfItsOwn)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 $1234=41", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            //  The operand fits a line with room to spare; the result still
            //  starts the next one.
            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (30)), text, layout));

            Assert::AreEqual ((size_t) 2,                     layout.lines.size());
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"Result: A=41"),       GetLineText (layout, 1));
            Assert::AreEqual ((LONG) (kRowTop + 2 * kRowHeight + OperandResultTip::kBorderPx), layout.rect.bottom, L"two rows high");
        }


        TEST_METHOD (ALongOperandWrapsAtItsSpacesBeforeTheResult)
        {
            std::string               operand = "A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0";
            DxuiListView::Cell        cell    = MakeCell (operand, "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;
            std::wstring              joined;



            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (OperandResultTip::kMinWrapChars)), text, layout));

            Assert::AreEqual ((size_t) 3, layout.lines.size(), L"two lines of operand, one of result");
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"C=1 Z=0 N=1 V=0"),         GetLineText (layout, 1));
            Assert::AreEqual (std::wstring (L"Result: A=41"),            GetLineText (layout, 2));

            for (size_t i = 0; i < layout.lines.size(); i++)
            {
                Assert::IsTrue (layout.lines[i].length <= OperandResultTip::kMinWrapChars, GetLineText (layout, i).c_str());
            }

            joined = GetLineText (layout, 0) + L" " + GetLineText (layout, 1);

            Assert::AreEqual (TextEncoding::NarrowToWide (operand), joined, L"nothing of the operand is lost at a break");
        }


        TEST_METHOD (AWordWiderThanALineBreaksWhereTheLineEnds)
        {
            DxuiListView::Cell        cell = MakeCell ("Turn_on_the_80_column_display_now", "");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (OperandResultTip::kMinWrapChars)), text, layout));

            Assert::AreEqual ((size_t) 2,                                 layout.lines.size());
            Assert::AreEqual (OperandResultTip::kMinWrapChars,            layout.lines[0].length);
            Assert::AreEqual (std::wstring (L"Turn_on_the_80_column_di"), GetLineText (layout, 0));
            Assert::AreEqual (std::wstring (L"splay_now"),                GetLineText (layout, 1));
        }


        TEST_METHOD (ANarrowRoomStillWrapsAtTheTipsLeastWidth)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            //  Three characters of room would wrap three characters to a line.
            Assert::IsTrue (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (3)), text, layout));

            Assert::AreEqual ((size_t) 3, layout.lines.size(), L"wrapped at the least width, not the room");
            Assert::IsTrue   (layout.rect.right > GetEdgeFor (3), L"the tip reaches past the pane");
        }


        TEST_METHOD (TheTipCoversTheCellToThePanesEdge)
        {
            DxuiListView::Cell        cell = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41");
            MockDxuiTextRenderer      text;
            OperandResultTip::Layout  layout;



            //  The operand breaks at its last space short of the pane's edge,
            //  so its first line ends three characters before the list's cut
            //  text does.
            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, MakePlacement (GetEdgeFor (30)), text, layout));
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41 C=1"), GetLineText (layout, 0));

            Assert::AreEqual ((LONG) GetEdgeFor (30), layout.rect.right, L"the tip stops short of the pane's edge, and the cut text shows beside it");
        }


        TEST_METHOD (AtTheScreensRightEdgeTheTipGrowsDown)
        {
            constexpr int                kRoomChars = 12;

            DxuiListView::Cell           cell      = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0", "A=41");
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (30));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  The pane runs past the screen's edge, which leaves the text less
            //  room than the tip's least width.
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


        TEST_METHOD (AtTheScreensBottomTheTipRewrapsWider)
        {
            DxuiListView::Cell           cell      = MakeCell ("A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0", "A=41");
            OperandResultTip::Placement  placement = MakePlacement (GetEdgeFor (OperandResultTip::kMinWrapChars));
            MockDxuiTextRenderer         text;
            OperandResultTip::Layout     layout;



            //  Wrapped in the pane's room the operand takes two lines and the
            //  result a third; the screen has room below the row for two.
            placement.workArea.bottom = kRowTop + 2 * kRowHeight + OperandResultTip::kBorderPx;

            Assert::IsTrue   (OperandResultTip::TryMakeLayout (cell, placement, text, layout));
            Assert::AreEqual ((size_t) 2, layout.lines.size(), L"the operand rewraps across the room the screen has");
            Assert::AreEqual (std::wstring (L"A=00 X=01 Y=02 $1234=41 C=1 Z=0 N=1 V=0"), GetLineText (layout, 0));
            Assert::IsTrue   (layout.rect.bottom <= placement.workArea.bottom, L"the tip runs off the bottom of the screen");
            Assert::AreEqual ((LONG) kRowTop, layout.textOrigin.y, L"on the cell's row still");

            //  With room for a single row, nothing fits.
            placement.workArea.bottom = kRowTop + kRowHeight + OperandResultTip::kBorderPx;

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
            Assert::AreEqual ((size_t) 2, text.Calls().size(), L"one run a line, each in one color");
            Assert::AreEqual (std::wstring (L"A=00 X=01 $1234=41"), text.Calls()[0].text);
            Assert::AreEqual (kOperand,                             text.Calls()[0].argb, L"the operand's color");
            Assert::AreEqual (std::wstring (L"Result: A=41"),       text.Calls()[1].text);
            Assert::AreEqual (kResult,                              text.Calls()[1].argb, L"the result's color");
            Assert::AreEqual ((float) kRowHeight, text.Calls()[1].y - text.Calls()[0].y, L"the result a row below");
            Assert::IsTrue   (text.Calls()[0].fontSizeDip == kFontPx, L"the cell's size");

            Assert::IsFalse  (painter.Calls().empty(), L"the tip is outlined");
            Assert::IsTrue   (painter.Calls()[0].kind == RecordedPaintKind::OutlineRect);
            Assert::AreEqual (kBorder, painter.Calls()[0].argb);
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
            window.SetOperandTipDeviceForTest (&text, MakeScreen());

            //  A paint fits the columns to their text, which places the cells.
            window.GetCodeList (0)->Paint (painter, text, theme);
        }


        //  A point on a row's operand, just past where its text starts.
        static POINT  GetOperandPoint (TipWindow & window, int row)
        {
            DxuiListView  * list   = window.GetCodeList (0);
            RECT            bounds = list->GetBounds();
            RECT            cell   = {};



            Assert::IsTrue (list->GetCellTextRectPx (row, 5, cell), L"the row's operand is in view");

            return POINT { bounds.left + cell.left + 1, bounds.top + (cell.top + cell.bottom) / 2 };
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



            Assert::IsTrue (window.GetCodeList (0)->GetCellTextRectPx (row, 5, cell), L"the row's operand is in view");

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


        static bool  TryGetTip (TipWindow & window, POINT at, MockDxuiTextRenderer & text, OperandResultTip::Layout & layout)
        {
            DxuiDpiScaler             scaler;
            TipWindow::OperandTipCell cell;



            scaler.SetDpi (96);

            return window.TryGetOperandTip (at, scaler, text, MakeScreen(), layout, cell);
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

            Assert::IsTrue (list->GetCellTextRectPx (0, 5, cell), L"the long row's operand is in view");
            Assert::IsTrue (window.TryGetOperandTip (GetOperandPoint (window, 0), scaler, text, MakeScreen(), layout, tipCell),
                            L"the pane cuts the long operand off");

            Assert::AreEqual (0, tipCell.view);
            Assert::AreEqual (0, tipCell.row);
            Assert::AreEqual (bounds.left + cell.left, layout.textOrigin.x, L"the tip's text starts at the cell's");
            Assert::AreEqual (bounds.top  + cell.top,  layout.textOrigin.y);
            Assert::IsTrue   (layout.lines.size() > 1, L"the operand wraps in the pane's width");
            Assert::AreEqual (bounds.right - (list->IsScrollbarVisible() ? list->GetScrollbarWidthPx() : 0), layout.rect.right,
                              L"the tip covers the cell to the pane's edge");

            Assert::IsFalse (window.TryGetOperandTip (GetOperandPoint (window, 1), scaler, text, MakeScreen(), layout, tipCell),
                             L"a short operand fits");
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
            Assert::IsTrue   (window.HasOperandTip(), L"a move onto the cut-off operand shows the tip");
            Assert::AreEqual (0, window.GetOperandTipCell().view);
            Assert::AreEqual (0, window.GetOperandTipCell().row);

            Send (window, DxuiMouseEventKind::Wheel, at);
            Assert::IsFalse (window.HasOperandTip(), L"the wheel leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.HasOperandTip());

            (void) window.OnKey (key);
            Assert::IsFalse (window.HasOperandTip(), L"a key leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.HasOperandTip());

            Send (window, DxuiMouseEventKind::Down, at);
            Assert::IsFalse (window.HasOperandTip(), L"a press leaves the tip up");

            Send (window, DxuiMouseEventKind::Up, at);
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue  (window.HasOperandTip());

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 1));
            Assert::IsFalse (window.HasOperandTip(), L"a move onto an operand that fits leaves the tip up");
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

            window.PlaceOperandTip (move, false, scaler, text, MakeScreen());
            Assert::IsTrue  (window.HasOperandTip());

            window.PlaceOperandTip (move, true, scaler, text, MakeScreen());
            Assert::IsFalse (window.HasOperandTip(), L"a drag over the cell leaves the tip up");

            //  As the window gets it: a move made with the left button down.
            (void) window.OnMouse (move);
            Assert::IsTrue  (window.HasOperandTip());

            move.button = DxuiMouseButton::Left;

            (void) window.OnMouse (move);
            Assert::IsFalse (window.HasOperandTip(), L"a move with the button down leaves the tip up");
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
            first = window.GetOperandTipLayout();

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 2));
            Assert::IsTrue   (window.HasOperandTip());
            Assert::AreEqual (2, window.GetOperandTipCell().row);
            Assert::IsFalse  (OperandResultTip::IsSame (first, window.GetOperandTipLayout()), L"the tip still shows the first row's");

            Assert::IsTrue   (list->GetCellTextRectPx (2, 5, cell));
            Assert::AreEqual (list->GetBounds().top + cell.top, window.GetOperandTipLayout().textOrigin.y, L"over the third row");
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
            window.CheckOperandTipPointer (POINT { at.x + 50, at.y });
            Assert::IsTrue (window.HasOperandTip(), L"a pointer still on the cell keeps the tip");

            //  Off the window's client area, as a pointer leaving across the
            //  frame or the caption is, with no move reaching the window.
            window.CheckOperandTipPointer (POINT { at.x, -1 });
            Assert::IsFalse (window.HasOperandTip(), L"a pointer off the window leaves the tip up");

            Send (window, DxuiMouseEventKind::Move, at);
            window.CheckOperandTipPointer (GetOperandPoint (window, 2));
            Assert::IsFalse (window.HasOperandTip(), L"a pointer on another row leaves the tip up");
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
            cellFill = list->GetCellsOfRow (2)[5].background;

            Assert::IsTrue   (window.HasOperandTip());
            Assert::AreEqual (DxuiColor::Composite (cellFill, DxuiColor::Composite (theme.ContentSelection(), theme.ContentBackground())),
                              window.GetOperandTipLayout().fill, L"the tip is not in the selected row's fill");
        }


        //  A disassembly's rows lie on a text view's background, which a theme
        //  can set apart from a list's, as the debugger's light theme does. The
        //  tip takes that background on a row at rest, and the hover seen
        //  through over it on a hovered row, never the list color.
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
            cellFill = list->GetCellsOfRow (2)[5].background;

            Assert::IsTrue   (TryGetTip (window, at, text, layout), L"the pane cuts the third row's operand off");
            Assert::AreEqual (DxuiColor::Composite (cellFill, kTextView), layout.fill, L"a row at rest shows the text view's fill");

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

            Assert::IsTrue (window.HasOperandTip(), L"the same rows again take the tip down");
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
            Assert::IsTrue (window.HasOperandTip());

            //  The tip's own row is as it was; another's instruction is wider,
            //  which widens its column and moves the operand along.
            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.HasOperandTip(), L"the tip stays where the cell was");
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
            Assert::IsTrue (window.HasOperandTip());

            //  The text is the same; only the PC's row fill leaves the row.
            snapshot->codeViews[0][0].isCurrent = false;
            snapshot->codeViews[0][1].isCurrent = true;
            snapshot->code                      = snapshot->codeViews[0];
            snapshot->pc                        = snapshot->codeViews[0][1].address;

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.HasOperandTip(), L"the tip keeps the PC's row fill");
        }


        TEST_METHOD (ANewLayoutThatMovesTheCellTakesTheTipDown)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;



            scaler.SetDpi (96);
            Build (window, theme, text);
            Send  (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsTrue (window.HasOperandTip());

            window.Layout (RECT { 0, 0, kWindowWidth - kNarrowerPx, kWindowHeight }, scaler);

            Assert::IsFalse (window.HasOperandTip(), L"the tip keeps the width of a pane that has narrowed");
        }


        TEST_METHOD (TheColorTipShowsBelowTheOperandTip)
        {
            CassoTheme            theme = CassoTheme::MakeSkeuomorphic();
            TipHost               host;
            TipWindow             window (theme, host);
            MockDxuiTextRenderer  text;
            RECT                  under = {};



            Build (window, theme, text);

            //  The move that puts the operand tip up also starts the operand's
            //  color tip, which waits out its dwell.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            under = window.GetTooltip().GetPendingAnchor();

            Assert::IsTrue (window.HasOperandTip());
            Assert::IsTrue (window.GetTooltip().WantsTick(), L"the operand's color tip is not on its way");
            Assert::IsTrue (EqualRect (&under, &window.GetOperandTipLayout().rect) != FALSE, L"the color tip is placed by its cell, under the operand tip");

            //  Over an operand that fits, the color tip is placed by its cell.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 1));
            under = window.GetTooltip().GetPendingAnchor();

            Assert::IsFalse (window.HasOperandTip());
            Assert::IsTrue  (window.GetTooltip().WantsTick());
            Assert::AreEqual (window.GetCodeList (0)->GetBounds().top + GetCellRect (window, 1).top, under.top, L"the color tip lies away from its cell");
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

            Assert::IsFalse (window.HasOperandTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after a key, the color tip still comes up below a tip no longer there");

            //  The pointer leaving the cell with no move reaching the window.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            window.CheckOperandTipPointer (POINT { at.x, -1 });

            Assert::IsFalse (window.HasOperandTip());
            Assert::IsFalse (window.GetTooltip().WantsTick(), L"after the pointer leaves, the color tip still comes up below a tip no longer there");

            //  New rows that move the cell.
            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.GetTooltip().WantsTick());

            snapshot->codeViews[0][1].instruction = "LDA A_LONG_SYMBOL_WIDENING_THE_COLUMN,X";
            snapshot->code                        = snapshot->codeViews[0];

            window.ApplyCodeSnapshot (snapshot, 0);

            Assert::IsFalse (window.HasOperandTip());
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
            Assert::IsTrue (list->IsCellTextSelected (0, 5), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.HasOperandTip(), L"the tip lies over the selection");

            //  A row whose tip reaches nowhere near the selection still has one.
            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 2));
            Assert::IsTrue (window.HasOperandTip(), L"a selection in one row takes every row's tip away");
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
            Assert::IsTrue (list->IsCellTextSelected (1, 5), L"the drag selects part of the operand");

            Send (window, DxuiMouseEventKind::Move, GetOperandPoint (window, 0));
            Assert::IsFalse (window.HasOperandTip(), L"the tip of the row above lies over the selection");
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
            window.SetOperandTipDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsFalse (window.HasOperandTip(), L"the tip shows over the floating window");

            //  The same window clear of the tip.
            OffsetRect (&screen.overWindows[0], 0, kOverSizePx);
            window.SetOperandTipDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);
            Assert::IsTrue (window.HasOperandTip(), L"a window clear of the tip takes it away");
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
            window.SetOperandTipDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue   (window.HasOperandTip(), L"a pointer on the second monitor has no tip");
            Assert::AreEqual (start,   window.GetOperandTipLayout().textOrigin.x, L"the text starts where the cell's does, on the first monitor");
            Assert::AreEqual (visible, window.GetOperandTipLayout().rect.right,   L"the tip stops at the first monitor's edge, and the cut text shows beside it");

            //  A gap between them, as a taskbar down the first one's right side
            //  leaves: the tip keeps to the first monitor, with the pointer on
            //  the second.
            screen.workAreas[0].right = split - kGapPx;
            window.SetOperandTipDeviceForTest (&text, screen);

            Send (window, DxuiMouseEventKind::Move, at);

            Assert::IsTrue (window.HasOperandTip(), L"a pointer on the second monitor has no tip");
            Assert::IsTrue (window.GetOperandTipLayout().rect.right <= split - kGapPx, L"the tip runs into the gap");
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

            button = window.GetColorKey (DebuggerLayout::GetCodePaneId (0));
            Assert::IsNotNull (button);
            Assert::IsTrue    (button->IsVisible(), L"the disassembly's info button shows");

            Send (window, DxuiMouseEventKind::Down, GetCenter (button->GetBounds()));
            Send (window, DxuiMouseEventKind::Up,   GetCenter (button->GetBounds()));
            key = window.GetColorKeyPopup().GetRect();

            Assert::IsTrue (window.GetColorKeyPopup().IsHeld(), L"the click holds the key open");
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
                if (list->GetCellTextRectPx (row, 5, cell))
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

            lda.address       = 0x0300;
            lda.bytes         = "A5 10";
            lda.instruction   = "LDA $10";
            lda.annotation    = longOperand;
            lda.effect        = "A=41";
            lda.isCurrent     = true;
            rts.address       = 0x0302;
            rts.bytes         = "60";
            rts.instruction   = "RTS";
            rts.annotation    = "S=FD";
            sta.address       = 0x0303;
            sta.bytes         = "85 11";
            sta.instruction   = "STA $11";
            sta.annotation    = longOperand;

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
}
