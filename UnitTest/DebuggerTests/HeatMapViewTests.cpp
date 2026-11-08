#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapViewTests
//
//  What the heat map pane draws for each address in each mode, how many
//  addresses a row holds, where its addresses and modes are, how it zooms,
//  scrolls and pans, which address the mouse picks, and what the tip says.
//  The blit and the text are checked on screen.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapViewTests)
    {
    public:

        static constexpr uint32_t  kBackground = 0xFF000000;
        static constexpr uint32_t  kCold       = 0xFF303030;
        static constexpr uint32_t  kExecute    = 0xFFE0201C;   // the heat column's hottest run color, HeatStrip::GetScaleStops
        static constexpr uint32_t  kRead       = 0xFF9AEEFF;
        static constexpr uint32_t  kWrite      = 0xFFFFA6E8;

        //  The pane's size, at 96 dpi.
        static constexpr int       kWidth      = 600;
        static constexpr int       kHeight     = 400;

        using Mode = HeatMapView::Mode;

        static HeatMapView::Palette MakePalette()
        {
            HeatMapView::Palette  palette;



            palette.background = kBackground;
            palette.cold       = kCold;
            palette.execute    = kExecute;
            palette.read       = kRead;
            palette.write      = kWrite;
            return palette;
        }

        static void Place (HeatMapView & view, int width = kWidth, int height = kHeight)
        {
            DxuiDpiScaler  scaler;



            view.SetPalette (MakePalette());
            view.Layout (RECT { 0, 0, width, height }, scaler);

            //  The map starts fitted to the pane; these tests are of a map at
            //  three-pixel cells, as the user can zoom it.
            view.ZoomTo (3);
        }

        //  A point inside an address's cell, `inset` pixels in from its top left.
        static POINT GetPointOf (const HeatMapView & view, Word address, int inset = 1)
        {
            RECT  cell = view.GetCellRect (address);



            return { cell.left + inset, cell.top + inset };
        }

        //  The frame's pixel under a point.
        static uint32_t GetPixelAt (HeatMapView & view, POINT point)
        {
            RECT  map   = view.GetMapRect();
            long  width = map.right - map.left;



            return view.GetPixels()[(size_t) ((point.y - map.top) * width + (point.x - map.left))];
        }

        static DxuiMouseEvent MakeEvent (DxuiMouseEventKind kind, POINT point)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Down || kind == DxuiMouseEventKind::Up) ? DxuiMouseButton::Left : DxuiMouseButton::None;
            ev.positionDip = point;
            return ev;
        }

        static DxuiMouseEvent MakeWheel (POINT point, float delta, bool ctrl, bool shift)
        {
            DxuiMouseEvent  ev = MakeEvent (DxuiMouseEventKind::Wheel, point);



            ev.wheelDelta = delta;
            ev.ctrl       = ctrl;
            ev.shift      = shift;
            return ev;
        }

        //  Whether an address's row, street included, is at a height.
        static bool IsAtHeight (const HeatMapView & view, Word address, long y)
        {
            RECT  cell = view.GetCellRect (address);



            return y >= cell.top && y <= cell.bottom;
        }



        TEST_METHOD (AColdAddressIsTheColdGrayNotThePage)
        {
            Assert::AreEqual (kCold, HeatMapView::GetColor (Mode::All, 0, 0, 0, MakePalette()));
        }



        TEST_METHOD (AllShowsCodeAndDataInTheirOwnColors)
        {
            HeatMapView::Palette  palette = MakePalette();



            Assert::AreEqual (kExecute, HeatMapView::GetColor (Mode::All, 255, 0,   0,   palette));
            Assert::AreEqual (kRead,    HeatMapView::GetColor (Mode::All, 0,   255, 0,   palette));
            Assert::AreEqual (kWrite,   HeatMapView::GetColor (Mode::All, 0,   0,   255, palette));
            Assert::AreEqual (kExecute, HeatMapView::GetColor (Mode::All, 255, 255, 0,   palette), L"code shows where code and data are as hot");
            Assert::AreEqual (kRead,    HeatMapView::GetColor (Mode::All, 10,  255, 0,   palette), L"the hotter kind shows");
        }



        TEST_METHOD (CodeShowsExecutesAloneAndDataShowsReadsAndWritesAlone)
        {
            HeatMapView::Palette  palette = MakePalette();



            Assert::AreEqual (kCold,    HeatMapView::GetColor (Mode::Code, 0,   255, 255, palette));
            Assert::AreEqual (kExecute, HeatMapView::GetColor (Mode::Code, 255, 255, 255, palette));
            Assert::AreEqual (kCold,    HeatMapView::GetColor (Mode::Data, 255, 0,   0,   palette));
            Assert::AreEqual (kWrite,   HeatMapView::GetColor (Mode::Data, 255, 10,  255, palette));
        }



        TEST_METHOD (AFaintAddressIsStillSetApartFromTheColdGray)
        {
            uint32_t  faint = HeatMapView::GetColor (Mode::All, 1, 0, 0, MakePalette());
            uint32_t  hot   = HeatMapView::GetColor (Mode::All, 255, 0, 0, MakePalette());



            Assert::AreNotEqual (kCold, faint);
            Assert::AreEqual    (0xFFD8C060u, faint, L"the first stop of the heat column's run scale");
            Assert::AreNotEqual (hot, faint);
        }



        TEST_METHOD (ACellIsItsAddressByRowAndColumn)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  none;
            RECT               map   = {};
            int                pitch = 0;



            Place (view);
            execute[0x0102] = 255;
            view.SetLevels (execute, none, none);
            map   = view.GetMapRect();
            pitch = view.GetCellPx() + HeatMapView::kStreetPx;

            Assert::AreEqual (kExecute, view.GetCellColor (0x0102));
            Assert::AreEqual (kCold,    view.GetCellColor (0x0103));
            Assert::AreEqual (kExecute, GetPixelAt (view, GetPointOf (view, 0x0102)));
            Assert::AreEqual (kCold,    GetPixelAt (view, GetPointOf (view, 0x0103)));

            Assert::AreEqual (map.left + (0x0142 % view.GetColumns()) * (map.right - map.left) / view.GetColumns(), view.GetCellRect (0x0142).left,
                              L"its column is the address within its row, the row stretched across the map");
            Assert::AreEqual (map.top  + (0x0102 / view.GetColumns()) * pitch, view.GetCellRect (0x0102).top,  L"and its row the address over the columns");
        }



        //  All of memory starts at the largest cell that fits the pane with
        //  no scrolling, which is the zoom's 100%; a notch larger scrolls, and
        //  a larger pane fits it again.
        TEST_METHOD (AllMemoryStartsFittedToThePane)
        {
            HeatMapView    view;
            DxuiDpiScaler  scaler;
            int            fitted = 0;



            view.SetPalette (MakePalette());
            view.Layout (RECT { 0, 0, 1200, 900 }, scaler);
            fitted = view.GetCellPx();

            Assert::IsFalse  (view.HasVerticalScroll(),   L"all of memory in view");
            Assert::AreEqual (fitted, view.GetStartCellPx(), L"the zoom's 100%");

            view.ZoomIn();
            Assert::IsTrue   (view.HasVerticalScroll(), L"a notch larger no longer fits");

            view.ResetZoom();
            Assert::AreEqual (fitted, view.GetCellPx(), L"Reset zoom fits it again");

            view.Layout (RECT { 0, 0, 2400, 1800 }, scaler);
            Assert::IsTrue   (view.GetCellPx() > fitted, L"a larger pane fits a larger cell");
        }



        //  A map the user zoomed keeps the zoom when the pane is resized, but
        //  its 100%, and what Reset zoom fits, is the new pane's.
        TEST_METHOD (AResizeKeepsTheUsersZoomAndRefitsTheHundredPercent)
        {
            HeatMapView    view;
            DxuiDpiScaler  scaler;
            int            fitted = 0;
            int            zoomed = 0;



            view.SetPalette (MakePalette());
            view.Layout (RECT { 0, 0, 1200, 900 }, scaler);
            fitted = view.GetStartCellPx();

            view.ZoomIn();
            zoomed = view.GetCellPx();

            view.Layout (RECT { 0, 0, 2400, 1800 }, scaler);

            Assert::AreEqual (zoomed, view.GetCellPx(), L"the user's zoom stays");
            Assert::IsTrue   (view.GetStartCellPx() > fitted, L"100% is the larger pane's fit");

            view.ResetZoom();
            Assert::AreEqual (view.GetStartCellPx(), view.GetCellPx(), L"Reset zoom fits the new pane");
            Assert::IsFalse  (view.HasVerticalScroll());
        }



        TEST_METHOD (AThreePixelCellHasAOnePixelStreet)
        {
            HeatMapView  view;
            RECT         map = {};



            Place (view);
            map = view.GetMapRect();

            Assert::AreEqual (3, view.GetCellPx());
            Assert::AreEqual (kCold,       GetPixelAt (view, { map.left + 2, map.top + 2 }), L"the first cell's last pixel");
            Assert::AreEqual (kBackground, GetPixelAt (view, { map.left + 3, map.top + 1 }), L"the street after it, across");
            Assert::AreEqual (kBackground, GetPixelAt (view, { map.left + 1, map.top + 3 }), L"and below");
            Assert::AreEqual (kCold,       GetPixelAt (view, { map.left + 4, map.top + 4 }), L"the next cell");
        }



        TEST_METHOD (EveryZoomKeepsTheStreets)
        {
            HeatMapView  view;
            RECT         map   = {};
            RECT         first = {};
            int          cell  = 0;



            Place (view);
            map = view.GetMapRect();
            view.ZoomAt ({ map.left, map.top }, -10.0f);
            Assert::AreEqual (1, view.GetCellPx(), L"the smallest cell is a pixel");

            while (cell != view.GetCellPx())
            {
                cell  = view.GetCellPx();
                map   = view.GetMapRect();
                first = view.GetCellRect (0);

                Assert::IsTrue   (first.right - first.left >= 1, L"a cell is at least a pixel wide");
                Assert::AreEqual (kCold,       GetPixelAt (view, { first.right - 1, map.top }), L"a cell's last pixel");
                Assert::AreEqual (kBackground, GetPixelAt (view, { first.right,     map.top }), L"the street after it");
                Assert::AreEqual (kBackground, GetPixelAt (view, { map.left,        map.top + cell }));

                view.ScrollBy (-100000);
                view.ZoomAt ({ map.left, map.top }, 1.0f);
            }

            Assert::AreEqual (HeatMapView::kMaxCellPx, view.GetCellPx(), L"the zoom stops at its largest");
        }



        //  The panel grows out of the button: its bottom right is the
        //  button's, the slider's row is above the button, and Reset is on
        //  the button's row at the panel's left. The wheel over the button
        //  or the panel zooms.
        TEST_METHOD (TheZoomPanelGrowsOutOfTheButton)
        {
            HeatMapView     view;
            DxuiMouseEvent  ev;
            RECT            button = {};
            RECT            panel  = {};
            RECT            track  = {};
            RECT            reset  = {};
            int             before = 0;



            Place (view);
            button = view.GetZoomWidget().GetButtonRect();

            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = POINT { (button.left + button.right) / 2, (button.top + button.bottom) / 2 };
            view.OnMouse (ev);
            Assert::IsTrue (view.GetZoomWidget().IsOpen(), L"open on hover");

            panel = view.GetZoomWidget().GetPanelRect();
            track = view.GetZoomWidget().GetTrackRect();
            reset = view.GetZoomWidget().GetResetRect();

            Assert::AreEqual (button.right,  panel.right);
            Assert::AreEqual (button.bottom, panel.bottom);
            Assert::IsTrue   (track.bottom <= button.top,                     L"the slider's row above the button");
            Assert::IsTrue   (reset.top >= button.top && reset.bottom <= button.bottom && reset.right <= button.left, L"Reset left of the percentage on its row");

            before         = view.GetCellPx();
            ev.kind        = DxuiMouseEventKind::Wheel;
            ev.wheelDelta  = 1.0f;
            ev.positionDip = POINT { (track.left + track.right) / 2, panel.top + 1 };
            view.OnMouse (ev);
            Assert::IsTrue (view.GetCellPx() > before, L"the wheel over the panel zooms in");
        }



        //  The slider stays open with the pointer over it or near it, and
        //  closes once the pointer moves further away or off the map. During a
        //  drag of its thumb it is open wherever the pointer goes.
        TEST_METHOD (TheZoomSliderClosesWhenThePointerMovesAway)
        {
            HeatMapView     view;
            DxuiMouseEvent  ev;
            RECT            button = {};
            RECT            panel  = {};
            RECT            track  = {};
            DxuiDpiScaler   scaler;
            int             nearPx = scaler.ToPx (HeatMapZoomWidget::kNearDip);



            Place (view);
            button = view.GetZoomWidget().GetButtonRect();

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (button.left + button.right) / 2, (button.top + button.bottom) / 2 };
            view.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            view.OnMouse (ev);
            Assert::IsTrue (view.GetZoomWidget().IsOpen());

            panel  = view.GetZoomWidget().GetPanelRect();
            track  = view.GetZoomWidget().GetTrackRect();

            ev.kind        = DxuiMouseEventKind::Move;
            ev.button      = DxuiMouseButton::None;
            ev.positionDip = POINT { panel.left - nearPx / 2, (panel.top + panel.bottom) / 2 };
            view.OnMouse (ev);
            Assert::IsTrue (view.GetZoomWidget().IsOpen(), L"nearPx the panel");

            ev.positionDip = POINT { panel.left - nearPx - 4, (panel.top + panel.bottom) / 2 };
            view.OnMouse (ev);
            Assert::IsFalse (view.GetZoomWidget().IsOpen(), L"away from it");

            //  Open again, then drag the thumb far away.
            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (button.left + button.right) / 2, (button.top + button.bottom) / 2 };
            view.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            view.OnMouse (ev);
            ev.kind        = DxuiMouseEventKind::Down;
            ev.positionDip = POINT { (track.left + track.right) / 2, (track.top + track.bottom) / 2 };
            view.OnMouse (ev);
            ev.kind        = DxuiMouseEventKind::Move;
            ev.positionDip = POINT { panel.left - 10 * nearPx, panel.top - 10 * nearPx };
            view.OnMouse (ev);
            Assert::IsTrue (view.GetZoomWidget().IsOpen(), L"open while its thumb is dragged");

            ev.kind = DxuiMouseEventKind::Up;
            view.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Leave;
            view.OnMouse (ev);
            Assert::IsFalse (view.GetZoomWidget().IsOpen(), L"the pointer moved off the map");
        }

        //  A click in another pane is not routed to the map, so the slider
        //  closes when the focus moves from the map's pane.
        TEST_METHOD (TheZoomSliderClosesWithTheFocus)
        {
            HeatMapView     view;
            DxuiMouseEvent  ev;
            RECT            button = {};



            Place (view);
            button = view.GetZoomWidget().GetButtonRect();

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (button.left + button.right) / 2, (button.top + button.bottom) / 2 };
            view.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            view.OnMouse (ev);
            Assert::IsTrue (view.GetZoomWidget().IsOpen(), L"a click on the button opens the slider");

            view.OnFocusChanged (true);
            Assert::IsTrue (view.GetZoomWidget().IsOpen(), L"it is open after the map gains the focus");

            view.OnFocusChanged (false);
            Assert::IsFalse (view.GetZoomWidget().IsOpen());
        }



        TEST_METHOD (ARowHoldsThePowerOfTwoOfAddressesNearestTheZoom)
        {
            Assert::AreEqual (256,  HeatMapView::GetColumnsFor (1024,   4), L"256 cells of four pixels fill 1,024");
            Assert::AreEqual (128,  HeatMapView::GetColumnsFor (700,    4), L"128 stretched 1.37 times fill 700");
            Assert::AreEqual (256,  HeatMapView::GetColumnsFor (1023,   4), L"128 would stretch twice over, so 256 a little narrower");
            Assert::AreEqual (256,  HeatMapView::GetColumnsFor (800,    4), L"and past one and a half times, too");
            Assert::AreEqual (128,  HeatMapView::GetColumnsFor (500,    2), L"never narrower than a pixel and its street");
            Assert::AreEqual (512,  HeatMapView::GetColumnsFor (2048,   4), L"more than a page when wide");
            Assert::AreEqual (1024, HeatMapView::GetColumnsFor (100000, 2), L"no more than kMaxColumns");
            Assert::AreEqual (2,    HeatMapView::GetColumnsFor (10,     4), L"two cells of four pixels in ten");
            Assert::AreEqual (8,    HeatMapView::GetColumnsFor (538,   65), L"eight of the largest cells in a pane 600 wide");
            Assert::AreEqual (2,    HeatMapView::GetColumnsFor (138,   65), L"two of them in a narrow pane");
            Assert::AreEqual (2,    HeatMapView::GetColumnsFor (100,   65), L"one would stretch past one and a half times, so two narrower");
            Assert::AreEqual (1,    HeatMapView::GetColumnsFor (40,    65), L"one cell narrowed to a pane narrower than it");
            Assert::AreEqual (1,    HeatMapView::GetColumnsFor (0,      4), L"one cell in no width at all");
        }



        //  However large the zoom and however narrow the pane, a row is as
        //  wide as the map's area and no wider, so the map never scrolls
        //  across: at a larger zoom a row has fewer addresses instead.
        TEST_METHOD (NoRowRunsPastThePaneAtAnyZoomOrWidth)
        {
            constexpr int  kFirst = 70;
            constexpr int  kLast  = 1600;
            constexpr int  kStep  = 61;
            int            placed = 0;



            for (int width = kFirst; width <= kLast; width += kStep)
            {
                HeatMapView  view;



                Place (view, width, kHeight);

                for (int cell = 1; cell <= HeatMapView::kMaxCellPx; cell++)
                {
                    RECT          map     = {};
                    RECT          first   = {};
                    RECT          last    = {};
                    long          room    = 0;
                    int           columns = 0;
                    std::wstring  at      = std::format (L"at a width of {} and a cell of {}", width, cell);



                    view.ZoomTo (cell);
                    map     = view.GetMapRect();
                    columns = view.GetColumns();
                    room    = width - HeatMapView::kInsetDip - (view.HasVerticalScroll() ? HeatMapView::kScrollbarDip : 0);
                    first   = view.GetCellRect (0);
                    last    = view.GetCellRect ((Word) (columns - 1));

                    Assert::AreEqual (cell,      view.GetCellPx(), at.c_str());
                    Assert::IsTrue   (columns >= 1 && (columns & (columns - 1)) == 0, (L"a power of two to a row " + at).c_str());
                    Assert::AreEqual (room,      map.right, (L"the map spans the room " + at).c_str());
                    Assert::AreEqual (map.left,  first.left, (L"the row starts at the map's left " + at).c_str());
                    Assert::AreEqual (map.right, last.right + HeatMapView::kStreetPx, (L"and its last cell ends the map " + at).c_str());
                    Assert::AreEqual (first.top, last.top, (L"on one line " + at).c_str());
                    placed++;
                }
            }

            Assert::IsTrue (placed > 1000, L"more than 1,000 layouts checked");
        }



        //  At the largest zoom a narrow pane has a row of two cells, or one
        //  narrowed to the pane when even one is wider, and the rows run on
        //  down instead.
        TEST_METHOD (TheLargestZoomFitsANarrowPaneWithFewCellsToARow)
        {
            HeatMapView  view;
            RECT         map  = {};
            RECT         cell = {};



            Place (view, 200, kHeight);
            view.ZoomTo (HeatMapView::kMaxCellPx);
            map  = view.GetMapRect();
            cell = view.GetCellRect (0);

            Assert::AreEqual (HeatMapView::kMaxCellPx, view.GetCellPx(), L"the zoom is at its largest");
            Assert::AreEqual (2, view.GetColumns(), L"two to a row");
            Assert::AreEqual ((long) HeatMapView::kMaxCellPx, cell.bottom - cell.top, L"each as tall as the zoom");
            Assert::IsTrue   (view.HasVerticalScroll(), L"and the rows run on down");
            Assert::AreEqual ((long) HeatMapView::kAddressCount / 2, (long) view.GetRows());

            Place (view, 120, kHeight);
            view.ZoomTo (HeatMapView::kMaxCellPx);
            map  = view.GetMapRect();
            cell = view.GetCellRect (0x0001);

            Assert::AreEqual (1, view.GetColumns(), L"one to a row where even one is wider than the pane");
            Assert::AreEqual (map.left, cell.left, L"$0001 starts the second row");
            Assert::AreEqual (map.right, cell.right + HeatMapView::kStreetPx, L"and spans the map");
        }



        //  At every small row width a point anywhere in a cell or its street
        //  finds that cell, the tip is for it, and a click picks it.
        TEST_METHOD (APointFindsItsCellInRowsOfFewAddresses)
        {
            constexpr int  kCells[] = { 64, 32, 16, 8 };
            HeatMapView    view;
            Word           picked   = 0;
            int            checked  = 0;



            Place (view, 120, kHeight);
            view.SetOnPickAddress ([&picked] (Word address) { picked = address; });

            for (int cell : kCells)
            {
                view.ZoomTo (cell);
                view.ScrollBy (-100000);
                Assert::IsTrue (view.GetColumns() <= 8, L"a row of eight or fewer");

                for (int offset = 0; offset < view.GetColumns() * 3; offset++)
                {
                    Word                   address  = (Word) (view.GetColumns() + offset);
                    RECT                   rect     = view.GetCellRect (address);
                    RECT                   anchor   = {};
                    std::wstring           tip;
                    std::vector<uint32_t>  swatches;
                    std::wstring  label   = std::format (L"${:04X}", address);
                    POINT         nearEnd = { rect.left,      rect.top + 1 };
                    POINT         farEnd  = { rect.right - 1, rect.bottom - 1 };
                    POINT         street  = { rect.right,     rect.top + 1 };
                    bool          isLast  = (offset + 1) % view.GetColumns() == 0;



                    Assert::AreEqual (address, view.GetAddressAt (nearEnd).value_or (0), label.c_str());
                    Assert::AreEqual (address, view.GetAddressAt (farEnd).value_or (0),  label.c_str());

                    if (!isLast)
                    {
                        Assert::AreEqual (address, view.GetAddressAt (street).value_or (0), (L"its street " + label).c_str());
                    }

                    Assert::IsTrue (view.TryGetTipAt (farEnd, anchor, tip, swatches), label.c_str());
                    Assert::IsTrue (tip.starts_with (label), tip.c_str());
                    Assert::AreEqual ((size_t) 1, swatches.size(), L"a swatch for the first line");

                    view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, farEnd));
                    view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   farEnd));
                    Assert::AreEqual (address, picked, (L"a click picks " + label).c_str());
                    checked++;
                }
            }

            Assert::IsTrue (checked >= 12, L"every row width checked");
        }



        TEST_METHOD (TheRowsStretchToFillThePaneAtEveryWidth)
        {
            constexpr int  kFirst = 200;
            constexpr int  kLast  = 1600;
            constexpr int  kStep  = 23;
            int            placed = 0;



            for (int width = kFirst; width <= kLast; width += kStep)
            {
                HeatMapView   view;
                RECT          map    = {};
                RECT          last   = {};
                long          right  = 0;
                int           pitch  = 0;
                int           narrow = INT_MAX;
                int           wide   = 0;
                std::wstring  at     = std::format (L"at a width of {}", width);



                Place (view, width, kHeight);
                map   = view.GetMapRect();
                pitch = view.GetCellPx() + HeatMapView::kStreetPx;
                right = width - HeatMapView::kInsetDip - (view.HasVerticalScroll() ? HeatMapView::kScrollbarDip : 0);
                last  = view.GetCellRect ((Word) (view.GetColumns() - 1));

                Assert::AreEqual (right, map.right, (L"no empty strip right of the map " + at).c_str());
                Assert::AreEqual (map.right, last.right + HeatMapView::kStreetPx, (L"the last cell and its street end the map " + at).c_str());

                for (int column = 0; column < view.GetColumns(); column++)
                {
                    RECT  cell = view.GetCellRect ((Word) column);
                    RECT  next = view.GetCellRect ((Word) (column + 1));
                    int   step = (column + 1 < view.GetColumns()) ? (int) (next.left - cell.left) : (int) (map.right - cell.left);



                    narrow = std::min (narrow, step);
                    wide   = std::max (wide,   step);

                    Assert::AreEqual (kBackground, GetPixelAt (view, { cell.right, map.top }), (L"a street after every cell " + at).c_str());
                    Assert::AreEqual (kCold,       GetPixelAt (view, { cell.right - 1, map.top }), (L"and the cell up to it " + at).c_str());
                }

                Assert::IsTrue (wide - narrow <= 1, (L"every column within a pixel of the others " + at).c_str());
                Assert::IsTrue (wide * 2 <= pitch * 3 + 1, (L"no column past one and a half times the zoom's " + at).c_str());
                Assert::IsTrue (narrow * 4 >= pitch * 3 - 3, (L"nor narrower than three quarters of it " + at).c_str());
                Assert::AreEqual ((long) view.GetCellPx(), view.GetCellRect (0).bottom - view.GetCellRect (0).top, L"a row is as tall as the zoom's cell");
                placed++;
            }

            Assert::IsTrue (placed > 50, L"the widths were tried");
        }



        TEST_METHOD (AStretchedCellIsHitAndPickedAcrossItsWholeWidth)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  none;
            Word               picked = 0;
            Word               last   = 0;
            POINT              edge   = {};
            int                wider  = 0;



            Place (view, 700, kHeight);
            view.SetOnPickAddress ([&picked] (Word address) { picked = address; });

            for (int column = 0; column < view.GetColumns(); column += 2)
            {
                execute[(size_t) (0x1200 + column)] = 255;
            }

            view.SetLevels (execute, none, none);

            for (int column = 1; column < view.GetColumns(); column++)
            {
                Word      address = (Word) (0x1200 + column);
                RECT      cell    = view.GetCellRect (address);
                uint32_t  color   = (column % 2 == 0) ? kExecute : kCold;
                POINT     farEdge = { cell.right - 1, cell.top + 1 };



                wider += (cell.right - cell.left > view.GetCellPx()) ? 1 : 0;

                Assert::AreEqual (color,   GetPixelAt (view, farEdge), L"the cell is drawn to its far edge");
                Assert::AreEqual (address, view.GetAddressAt ({ cell.left, cell.top + 1 }).value_or (0), L"its near edge");
                Assert::AreEqual (address, view.GetAddressAt (farEdge).value_or (0),                      L"its far edge");
                Assert::AreEqual (address, view.GetAddressAt ({ cell.right, cell.top + 1 }).value_or (0), L"and its street");
                Assert::AreEqual ((Word) (address - 1), view.GetAddressAt ({ cell.left - 1, cell.top + 1 }).value_or (0), L"the street before it is the cell before");
            }

            Assert::IsTrue (wider > view.GetColumns() / 2, L"the cells were stretched");

            view.SetLevels (none, none, none);
            last = (Word) (0x1200 + view.GetColumns() - 1);
            edge = { view.GetCellRect (last).right - 1, view.GetCellRect (last).top + 1 };

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, edge));
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   edge));
            Assert::AreEqual (last, picked, L"a click on the far edge of the row's last cell picks it");
        }



        TEST_METHOD (TheAddressUnderAPointIsFoundByRowAndColumn)
        {
            HeatMapView  view;



            Place (view);

            Assert::AreEqual ((Word) 0x0000, view.GetAddressAt (GetPointOf (view, 0x0000)).value_or (1));
            Assert::AreEqual ((Word) 0x2A63, view.GetAddressAt (GetPointOf (view, 0x2A63)).value_or (0));
            Assert::AreEqual ((Word) 0x2A63, view.GetAddressAt (GetPointOf (view, 0x2A63, 3)).value_or (0), L"its street is the cell's");
            Assert::IsFalse  (view.GetAddressAt ({ 2, 100 }).has_value(), L"the row labels are no address");
            Assert::IsFalse  (view.GetAddressAt ({ 100, 2 }).has_value(), L"the bar is no address");
        }



        //  Zoomed in, a row has fewer addresses rather than running past the
        //  pane, and the row of the address under the pointer stays at its
        //  height at every notch up to the largest zoom and back.
        TEST_METHOD (ZoomingInGivesARowFewerAddressesAndKeepsThePointersRow)
        {
            HeatMapView  view;
            RECT         map     = {};
            POINT        point   = {};
            Word         under   = 0;
            int          columns = 0;
            int          notches = 0;



            Place (view);
            map     = view.GetMapRect();
            columns = view.GetColumns();
            point   = GetPointOf (view, 0x1456);

            while (view.GetCellPx() < HeatMapView::kMaxCellPx)
            {
                under = view.GetAddressAt (point).value_or (0);

                view.ZoomAt (point, 1.0f);
                notches++;

                Assert::IsTrue   (view.GetColumns() <= columns, L"no more addresses to a row than before");
                Assert::AreEqual (map.right, view.GetCellRect ((Word) (view.GetColumns() - 1)).right + HeatMapView::kStreetPx, L"and the row ends at the map's edge");
                Assert::IsTrue   (IsAtHeight (view, under, point.y), L"the pointer's row stays under it");

                columns = view.GetColumns();
            }

            Assert::IsTrue   (notches > 1, L"the zoom went up a notch at a time");
            Assert::AreEqual (8, view.GetColumns(), L"eight of the largest cells to a row");

            under = view.GetAddressAt (point).value_or (0);

            view.ZoomAt (point, -3.0f);
            Assert::IsTrue   (view.GetColumns() > 8, L"zoomed out, more to a row again");
            Assert::IsTrue   (IsAtHeight (view, under, point.y), L"and the row is under the pointer");
        }



        //  A touchpad turns the wheel in fractions of a notch. Over a row far
        //  down the map, the row stays under the pointer after each one, all
        //  the way in and back out.
        TEST_METHOD (FractionalNotchesKeepARowFarDownUnderThePointer)
        {
            HeatMapView  view;
            POINT        point = {};
            RECT         cell  = {};
            Word         under = 0;
            int          steps = 0;



            Place (view, 420, 700);
            view.ZoomAt (GetPointOf (view, 0x1000), 2.0f);
            view.ScrollBy ((int) (view.GetCellRect (0xC283).top - (view.GetMapRect().top + view.GetMapRect().bottom) / 2));

            cell  = view.GetCellRect (0xC283);
            point = { (cell.left + cell.right) / 2, (cell.top + cell.bottom) / 2 };

            for (float notch : { 0.25f, -0.25f })
            {
                for (steps = 0; steps < 60; steps++)
                {
                    under = view.GetAddressAt (point).value_or (0);

                    view.ZoomAt (point, notch);

                    //  Zoomed out, the end of memory comes into view, and the
                    //  map can't scroll past it.
                    if (view.GetScrollTop() >= view.GetRows() * (view.GetCellPx() + HeatMapView::kStreetPx) - (view.GetMapRect().bottom - view.GetMapRect().top))
                    {
                        break;
                    }

                    Assert::IsTrue (IsAtHeight (view, under, point.y),
                                    std::format (L"${:04X} under the pointer after step {} of {}", under, steps, notch).c_str());
                }
            }
        }



        //  The arrows move the framed cell a cell or a row, Ctrl+End to the last
        //  address, scrolling it into view, and Enter opens it in a memory
        //  window as a click does.
        TEST_METHOD (TheKeysMoveTheFramedCell)
        {
            HeatMapView   view;
            DxuiKeyEvent  key;
            Word          opened = 0;
            RECT          cell   = {};
            auto          press  = [&view, &key] (WPARAM vk, bool ctrl = false)
            {
                key.vk   = vk;
                key.ctrl = ctrl;
                return view.OnKey (key);
            };



            Place (view);
            view.SetOnPickAddress ([&opened] (Word address) { opened = address; });

            Assert::IsTrue   (press (VK_RIGHT), L"the first key frames a cell");
            Assert::AreEqual ((Word) 0x0000, view.GetHover().value_or (0xFFFF));

            Assert::IsTrue   (press (VK_RIGHT));
            Assert::AreEqual ((Word) 0x0001, *view.GetHover());

            Assert::IsTrue   (press (VK_DOWN));
            Assert::AreEqual ((Word) (0x0001 + view.GetColumns()), *view.GetHover(), L"a row down");

            Assert::IsTrue   (press (VK_END, true));
            Assert::AreEqual ((Word) 0xFFFF, *view.GetHover(), L"Ctrl+End, the last address");
            cell = view.GetCellRect (0xFFFF);
            Assert::IsTrue   (cell.bottom <= view.GetMapRect().bottom, L"scrolled into view");

            Assert::IsTrue   (press (VK_RETURN));
            Assert::AreEqual ((Word) 0xFFFF, opened, L"Enter opens it");

            Assert::IsFalse  (press ('A'), L"other keys are left to the window");
        }



        TEST_METHOD (ZoomThatKeepsTheRowKeepsTheStretchedCellUnderThePointer)
        {
            HeatMapView  view;
            POINT        point = {};
            RECT         cell  = {};



            Place (view, 700, kHeight);
            Assert::AreEqual (128, view.GetColumns());

            cell  = view.GetCellRect (0x1277);
            point = { cell.right - 1, cell.top + 1 };

            view.ZoomAt (point, 1.0f);
            Assert::AreEqual (5,   view.GetCellPx(), L"taller cells");
            Assert::AreEqual (128, view.GetColumns(), L"the same addresses to a row");
            Assert::AreEqual ((Word) 0x1277, view.GetAddressAt (point).value_or (0), L"the cell under the pointer stays under it");
        }



        TEST_METHOD (TheMapScrollsNoFurtherThanItsEdges)
        {
            HeatMapView  view;
            RECT         map     = {};
            long         content = 0;



            Place (view);
            map     = view.GetMapRect();
            content = view.GetRows() * (view.GetCellPx() + HeatMapView::kStreetPx);

            view.ScrollBy (-50);
            Assert::AreEqual (0L, view.GetScrollTop());

            view.ScrollBy (100000);
            Assert::AreEqual (content - (map.bottom - map.top), view.GetScrollTop());
            Assert::AreEqual ((Word) 0xFFFF, view.GetAddressAt ({ map.right - 2, map.bottom - 2 }).value_or (0), L"the last address in the corner");
        }



        TEST_METHOD (ResetZoomGoesBackToTheFittedSize)
        {
            HeatMapView  view;
            POINT        point = {};



            Place (view);
            point = GetPointOf (view, 0x4040);
            view.ZoomAt (point, 4.0f);
            view.ResetZoom();

            Assert::AreEqual (view.GetStartCellPx(), view.GetCellPx());
            Assert::AreEqual (0L, view.GetScrollTop());
        }



        //  The zoom widget's percentage is of the starting cell, and its
        //  slider runs on the logarithm from one pixel to the largest cell, so
        //  its middle is the cell halfway in scale, and each end its size.
        TEST_METHOD (TheZoomWidgetShowsAPercentageAndSlidesInScale)
        {
            constexpr int  kMax = HeatMapView::kMaxCellPx;



            Assert::AreEqual (100,                HeatMapZoomWidget::GetPercent (3, 3));
            Assert::AreEqual (200,                HeatMapZoomWidget::GetPercent (6, 3));
            Assert::AreEqual (33,                 HeatMapZoomWidget::GetPercent (1, 3));
            Assert::AreEqual (std::wstring (L"Zoom 300%"), HeatMapZoomWidget::GetPercentLabel (9, 3));

            Assert::AreEqual (0.0f,               HeatMapZoomWidget::GetPosition (1, kMax));
            Assert::AreEqual (1.0f,               HeatMapZoomWidget::GetPosition (kMax, kMax));
            Assert::AreEqual (1,                  HeatMapZoomWidget::GetCellPxAtPosition (0.0f, kMax));
            Assert::AreEqual (kMax,               HeatMapZoomWidget::GetCellPxAtPosition (1.0f, kMax));
            Assert::AreEqual (8,                  HeatMapZoomWidget::GetCellPxAtPosition (0.5f, kMax), L"halfway in scale from 1 to 64");

            for (int cell = 1; cell <= kMax; cell++)
            {
                Assert::AreEqual (cell, HeatMapZoomWidget::GetCellPxAtPosition (HeatMapZoomWidget::GetPosition (cell, kMax), kMax), L"every cell size has its place");
            }
        }



        //  The zoom's percentage is the user's zoom, not a size in pixels, so
        //  it is the same after a floating map is dragged to a monitor of
        //  another scale. The cells grow with the monitor's scale as the text
        //  does.
        TEST_METHOD (AMoveToAnotherScaleKeepsTheZoomPercentage)
        {
            constexpr int   kZoom     = 4;
            HeatMapView     view;
            DxuiDpiScaler   landscape;
            DxuiDpiScaler   portrait;
            int             startPx   = 0;
            int             zoomedPx  = 0;



            landscape.SetDpi (96);
            portrait.SetDpi  (192);

            view.SetPalette (MakePalette());
            view.Layout     (RECT { 0, 0, kWidth * 2, kHeight * 3 }, landscape);
            view.ZoomTo     (view.GetStartCellPx() * kZoom);

            startPx  = view.GetStartCellPx();
            zoomedPx = view.GetCellPx();
            Assert::AreEqual (400, HeatMapZoomWidget::GetPercent (view.GetCellPx(), view.GetStartCellPx()), L"zoomed to 400%");

            view.Layout (RECT { 0, 0, kWidth * 4, kHeight * 6 }, portrait);
            Assert::AreEqual (400, HeatMapZoomWidget::GetPercent (view.GetCellPx(), view.GetStartCellPx()), L"400% at twice the scale");
            Assert::IsTrue   (view.GetStartCellPx() > startPx, std::format (L"the size that fits the larger pane is larger: {} then {}", startPx, view.GetStartCellPx()).c_str());
            Assert::IsTrue   (view.GetCellPx() > zoomedPx, L"the cells grow with the scale");

            view.Layout (RECT { 0, 0, kWidth * 2, kHeight * 3 }, landscape);
            Assert::AreEqual (400,      HeatMapZoomWidget::GetPercent (view.GetCellPx(), view.GetStartCellPx()), L"400% back at the first scale");
            Assert::AreEqual (zoomedPx, view.GetCellPx(), L"at the cell it had there");
        }



        TEST_METHOD (ZoomInAndZoomOutGoAboutTheMiddleOfTheMap)
        {
            HeatMapView  view;
            RECT         map    = {};
            POINT        middle = {};
            Word         before = 0;



            Place (view);
            view.ScrollBy (300);
            map    = view.GetMapRect();
            middle = { (map.left + map.right) / 2, (map.top + map.bottom) / 2 };
            before = view.GetAddressAt (middle).value_or (0);

            view.ZoomIn();
            Assert::IsTrue   (view.GetCellPx() > 3);
            Assert::IsTrue   (IsAtHeight (view, before, middle.y), L"the middle row stays");

            view.ZoomOut();
            Assert::AreEqual (3, view.GetCellPx());
        }



        TEST_METHOD (ThePlainWheelScrollsDownAndLeavesTheZoom)
        {
            HeatMapView  view;



            Place (view);

            Assert::IsTrue   (view.OnMouse (MakeWheel (GetPointOf (view, 0x0000), -1.0f, false, false)));
            Assert::AreEqual (3, view.GetCellPx(), L"the wheel alone does not zoom");
            Assert::IsTrue   (view.GetScrollTop() > 0, L"the wheel down scrolls down");
            Assert::AreEqual (0L, view.GetScrollTop() % (view.GetCellPx() + HeatMapView::kStreetPx), L"by whole rows");
        }



        TEST_METHOD (CtrlWheelZoomsAboutThePointer)
        {
            HeatMapView  view;
            POINT        point = {};



            Place (view);
            point = GetPointOf (view, 0x0820);

            Assert::IsTrue (view.OnMouse (MakeWheel (point, 1.0f, true, false)));
            Assert::IsTrue (view.GetCellPx() > 3);
            Assert::IsTrue (IsAtHeight (view, 0x0820, point.y), L"the pointer's row stays under it");
        }



        //  The rows always fit across, so Shift with the wheel is left for the
        //  window, even at the largest zoom.
        TEST_METHOD (ShiftWheelIsLeftAloneAtEveryZoom)
        {
            HeatMapView  view;
            RECT         map = {};



            Place (view);
            map = view.GetMapRect();

            Assert::IsFalse  (view.OnMouse (MakeWheel (GetPointOf (view, 0x0000), -1.0f, false, true)), L"the rows fit, so there is nothing across to scroll");
            Assert::AreEqual (0L, view.GetScrollTop());

            view.ZoomAt ({ map.left, map.top }, 30.0f);
            Assert::AreEqual (HeatMapView::kMaxCellPx, view.GetCellPx());

            Assert::IsFalse  (view.OnMouse (MakeWheel (GetPointOf (view, 0x0000), -1.0f, false, true)), L"nor at the largest zoom");
            Assert::AreEqual (0L, view.GetScrollTop(), L"and the map stays where it was");
        }



        TEST_METHOD (TheWheelOffTheMapIsLeftAlone)
        {
            HeatMapView  view;



            Place (view);

            Assert::IsFalse (view.OnMouse (MakeWheel ({ 100, 2 }, 1.0f, true, false)), L"over the bar, the window's text zoom takes it");
            Assert::AreEqual (3, view.GetCellPx());
        }



        TEST_METHOD (AScrollbarShowsBesideTheMapWhenItOverflows)
        {
            HeatMapView  view;
            HeatMapView  large;
            RECT         map = {};



            Place (view);
            map = view.GetMapRect();

            Assert::IsTrue   (view.HasVerticalScroll(), L"512 rows are taller than the pane");
            Assert::AreEqual (map.right, view.GetVerticalBarRect().left, L"the scrollbar beside the map");
            Assert::IsTrue   (view.IsOverMap ({ view.GetVerticalBarRect().left + 1, map.top + 1 }), L"the scrollbar counts as the map's");

            view.ZoomAt ({ map.left, map.top }, 30.0f);
            Assert::AreEqual (map.right, view.GetVerticalBarRect().left, L"at the largest zoom, the same map beside the same scrollbar");
            Assert::AreEqual (map.bottom, view.GetMapRect().bottom, L"and no scrollbar below it");

            Place (large, 2400, 400);
            large.ZoomAt ({ large.GetMapRect().left, large.GetMapRect().top }, -10.0f);
            Assert::AreEqual (1024, large.GetColumns());
            Assert::IsFalse  (large.HasVerticalScroll(), L"64 rows of two pixels fit");
            Assert::AreEqual (0L, large.GetVerticalBarRect().right - large.GetVerticalBarRect().left);
        }



        TEST_METHOD (APressOnTheScrollbarPagesAndADragOnItsThumbScrolls)
        {
            HeatMapView  view;
            RECT         bar   = {};
            RECT         map   = {};
            long         page  = 0;
            POINT        thumb = {};



            Place (view);
            bar  = view.GetVerticalBarRect();
            map  = view.GetMapRect();
            page = map.bottom - map.top;

            Assert::IsTrue   (view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, { bar.left + 2, bar.bottom - 20 })), L"the scrollbar takes the press");
            Assert::IsFalse  (view.IsPressed(), L"a press on the track is not held");
            Assert::AreEqual (page, view.GetScrollTop(), L"a press below the thumb goes down a page");

            view.ScrollBy (-100000);
            thumb = { bar.left + 2, bar.top + 20 };

            Assert::IsTrue   (view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, thumb)));
            Assert::IsTrue   (view.IsPressed(), L"the thumb is held");
            Assert::IsTrue   (view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, { thumb.x, thumb.y + 50 })), L"the drag is the thumb's");
            Assert::IsTrue   (view.GetScrollTop() > 100, L"dragging the thumb a little goes a long way");
            Assert::IsTrue   (view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   { thumb.x, thumb.y + 50 })), L"and so is the release");
            Assert::IsFalse  (view.IsPressed());
        }



        TEST_METHOD (ADragPansAndPicksNothing)
        {
            HeatMapView  view;
            POINT        start  = {};
            POINT        moved  = {};
            int          picked = 0;



            Place (view);
            view.SetOnPickAddress ([&picked] (Word) { picked++; });
            view.ZoomAt (GetPointOf (view, 0x8080), 30.0f);
            view.ScrollBy (-100000);
            view.ScrollBy (200);

            start = GetPointOf (view, (Word) (view.GetColumns() * 5 + 5));
            moved = { start.x - 30, start.y - 20 };

            Assert::IsTrue (view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, start)));
            Assert::IsTrue (view.IsPressed());
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, moved));
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   moved));

            Assert::AreEqual (220L, view.GetScrollTop(), L"the map followed the mouse up and down");
            Assert::AreEqual (view.GetMapRect().left, view.GetCellRect (0).left, L"and not across");
            Assert::AreEqual (0, picked, L"a drag is not a click");
            Assert::IsFalse  (view.IsPressed());
        }



        TEST_METHOD (AClickPicksTheCellUnderIt)
        {
            HeatMapView  view;
            POINT        at     = {};
            Word         picked = 0;



            Place (view);
            view.SetOnPickAddress ([&picked] (Word address) { picked = address; });
            at = GetPointOf (view, 0x1234);

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, at));
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, { at.x + 1, at.y }));
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   { at.x + 1, at.y }));

            Assert::AreEqual ((Word) 0x1234, picked, L"a move short of a drag is still a click");
        }



        TEST_METHOD (WhileCellsAreSmallTheNearestHotCellIsPicked)
        {
            HeatMapView        view;
            std::vector<Byte>  read (0x10000, 0);
            std::vector<Byte>  none;
            Word               below = 0;
            RECT               hot   = {};



            Place (view);
            below        = (Word) (0x2001 + view.GetColumns());
            read[0x2001] = 100;
            read[below]  = 200;
            read[0x2010] = 255;
            view.SetLevels (none, read, none);
            hot = view.GetCellRect (0x2001);

            Assert::AreEqual ((Word) 0x2001, view.GetPickAt ({ hot.left - 1, hot.top }).value_or (0),     L"a pixel from $2001's edge, $2001 over the hotter cell further off");
            Assert::AreEqual ((Word) 0x2001, view.GetPickAt (GetPointOf (view, 0x2001)).value_or (0),    L"a touched cell under the mouse is its own");
            Assert::AreEqual ((Word) 0x2008, view.GetPickAt (GetPointOf (view, 0x2008)).value_or (0),    L"nothing touched near it, the cell under it");
        }



        //  In a cumulative map a row nothing touched can lie between two hot
        //  rows; its cells are picked from their middles, not skipped.
        TEST_METHOD (AColdRowBetweenHotRowsCanBePicked)
        {
            HeatMapView        view;
            std::vector<Byte>  read (0x10000, 0);
            std::vector<Byte>  none;
            Word               columns = 0;
            RECT               cell    = {};



            Place (view);
            columns = (Word) view.GetColumns();

            for (Word column = 0; column < columns; column++)
            {
                read[(Word) (0x0000 + column)]               = 255;
                read[(Word) (0x0000 + 2 * columns + column)] = 255;
            }

            view.SetLevels (none, read, none);
            cell = view.GetCellRect ((Word) (0x0000 + columns + 5));

            Assert::AreEqual ((Word) (0x0000 + columns + 5), view.GetPickAt ({ (cell.left + cell.right) / 2, (cell.top + cell.bottom) / 2 }).value_or (0));
        }



        TEST_METHOD (OnceCellsAreLargeThePickIsTheCellUnderTheMouse)
        {
            HeatMapView        view;
            std::vector<Byte>  read (0x10000, 0);
            std::vector<Byte>  none;
            POINT              at = {};



            Place (view);
            read[0x2001] = 255;
            view.SetLevels (none, read, none);
            view.ZoomAt (GetPointOf (view, 0x2000), 5.0f);
            Assert::IsTrue (view.GetCellPx() >= HeatMapView::kComfortCellDip);

            at = { view.GetCellRect (0x2000).right - 1, view.GetCellRect (0x2000).bottom - 1 };
            Assert::AreEqual ((Word) 0x2000, view.GetPickAt (at).value_or (0), L"the cell's far corner, next to the hot one");
        }



        TEST_METHOD (TheTipGivesTheAddressWhatTouchedItAndHowFast)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  write   (0x10000, 0);
            std::vector<Byte>  none;



            Place (view);
            view.SetTop (50000.0);
            execute[0x2010] = 113;
            write[0x2010]   = 35;
            execute[0x2011] = 255;
            write[0x2012]   = 1;
            view.SetLevels (execute, none, write);

            Assert::AreEqual (std::wstring (L"$2011  executed 50,000+/s"),            view.GetTipText (0x2011), L"the top level is the top or more");
            Assert::AreEqual (std::wstring (L"$2012  written under 0.1/s"),           view.GetTipText (0x2012));
            Assert::AreEqual (std::wstring (L"$0300  untouched"),                     view.GetTipText (0x0300));

            //  Run as code and written too, so called out as self-modifying.
            Assert::AreEqual (std::wstring (L"$2010  executed 120/s, written 3.4/s\nSelf-modifying: run as code and written"), view.GetTipText (0x2010));
        }



        TEST_METHOD (CumulativeTheTipGivesTheCount)
        {
            HeatMapView        view;
            HeatMapOptions     options;
            std::vector<Byte>  read (0x10000, 0);
            std::vector<Byte>  none;



            options.cumulative = true;
            Place (view);
            view.SetOptions (options);
            view.SetTop (1200.0);
            read[0x0400] = 255;
            read[0x0401] = 25;
            view.SetLevels (none, read, none);

            Assert::AreEqual (std::wstring (L"$0400  read 1,200 times"), view.GetTipText (0x0400));
            Assert::AreEqual (std::wstring (L"$0401  read once"),        view.GetTipText (0x0401));
        }



        TEST_METHOD (TheTipIsOnTheCellPickedAndNotWhileAButtonIsDown)
        {
            HeatMapView        view;
            std::vector<Byte>  read (0x10000, 0);
            std::vector<Byte>      none;
            RECT                   anchor   = {};
            RECT                   expected = {};
            std::wstring           text;
            std::vector<uint32_t>  swatches;
            POINT                  beside   = {};



            Place (view);
            view.SetTop (50000.0);
            read[0x2001] = 200;
            view.SetLevels (none, read, none);
            beside   = GetPointOf (view, 0x2000, 2);
            expected = view.GetCellRect (0x2001);
            InflateRect (&expected, HeatMapView::kStreetPx, HeatMapView::kStreetPx);

            Assert::IsTrue   (view.TryGetTipAt (beside, anchor, text, swatches));
            Assert::IsTrue   (text.starts_with (L"Read\n$2001  read "), text.c_str());
            Assert::AreEqual ((size_t) 1, swatches.size(), L"the color of the cell, beside the first line");
            Assert::AreEqual (view.GetCellColor (0x2001), swatches.front());
            Assert::IsTrue   (EqualRect (&anchor, &expected) != FALSE, L"anchored on the busier cell the pick snapped to");

            Assert::IsFalse  (view.TryGetTipAt ({ 2, 100 }, anchor, text, swatches), L"none over the row labels");

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, beside));
            Assert::IsFalse  (view.TryGetTipAt (beside, anchor, text, swatches), L"none while the button is down");
        }



        //  With a machine looking up what touched the cell, the tip is whole
        //  only once the answer for that cell is in, so it is not shown with
        //  one line and then grown by two.
        TEST_METHOD (TheTipIsWholeOnlyOnceTheCellsWriterAndReaderAreIn)
        {
            HeatMapView      view;
            HeatAccessHover  hover;
            POINT            point = {};



            Place (view);
            view.SetTop (50000.0);
            point = GetPointOf (view, 0x2000, 0);

            Assert::IsTrue (view.IsTipComplete (point), L"whole when nothing looks them up");

            view.SetOnHoverChanged ([] (std::optional<Word>) {});
            Assert::IsFalse (view.IsTipComplete (point), L"not before the answer");

            hover.address = 0x2001;
            view.SetHoverAccess (std::make_shared<const HeatAccessHover> (hover));
            Assert::IsFalse (view.IsTipComplete (point), L"nor with another cell's");

            hover.address = 0x2000;
            view.SetHoverAccess (std::make_shared<const HeatAccessHover> (hover));
            Assert::IsTrue (view.IsTipComplete (point), L"whole with its own");
        }



        TEST_METHOD (WhileTheHeatIsRebuiltTheBarSaysSo)
        {
            HeatMapView           view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            size_t                notes = 0;



            Place (view);

            view.Paint (painter, text, theme);
            notes = std::ranges::count_if (text.Calls(), [] (const RecordedTextCall & call) { return call.text == HeatMapView::kpszRebuildingNote; });
            Assert::AreEqual ((size_t) 0, notes, L"no note while nothing is rebuilt");

            view.SetRebuilding (true);
            view.SetNoteRect   (RECT { 0, 0, 400, HeatMapView::kBarDip });
            text.Reset();
            view.Paint (painter, text, theme);
            notes = std::ranges::count_if (text.Calls(), [] (const RecordedTextCall & call) { return call.text == HeatMapView::kpszRebuildingNote; });
            Assert::AreEqual ((size_t) 1, notes, L"one note while the heat is rebuilt");
        }



        //  The DPIs the text inset is checked at, and the pane's left. 106 DPI
        //  is where the inset in pixels is not the sum of its parts converted
        //  one at a time.
        static constexpr int   kInsetDpis[]  = { 96, 106, 120, 144, 168 };
        static constexpr LONG  kPaneLeft     = 20;

        //  What the mock renderer measures a character as, whatever its size.
        static constexpr float  kMockCharPx  = 7.0f;



        //  The row labels are set right, against the map, so the widest of them
        //  -- every one, in the fixed-width face -- starts at the pane's text
        //  inset, where the pane's title does.
        TEST_METHOD (TheWidestRowLabelStartsOnTheInset)
        {
            for (int dpi : kInsetDpis)
            {
                HeatMapView           view;
                MockDxuiPainter       painter;
                MockDxuiTextRenderer  text;
                MockDxuiTheme         theme;
                DxuiDpiScaler         scaler;
                size_t                labels = 0;
                std::wstring          at     = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);
                view.SetPalette (MakePalette());
                view.Layout     (RECT { kPaneLeft, 0, kPaneLeft + kWidth, kHeight }, scaler);
                view.Paint      (painter, text, theme);

                for (const RecordedTextCall & call : text.Calls())
                {
                    if (call.kind != RecordedTextKind::DrawString || call.hAlign != DxuiTextHAlign::Right || !call.text.starts_with (L"$"))
                    {
                        continue;
                    }

                    labels++;
                    Assert::AreEqual ((float) (kPaneLeft + DxuiPaneMetrics::GetContentTextInsetPx (scaler)),
                                      call.x + call.width - (float) call.text.size() * kMockCharPx,
                                      (call.text + L" starts at the pane's text inset " + at).c_str());
                }

                Assert::IsTrue (labels > 0, (L"rows are labeled " + at).c_str());
            }
        }



        //  A row scrolled partly off the top has no label: its label would be
        //  drawn over the bar above the map.
        TEST_METHOD (ARowPartlyScrolledOffHasNoLabel)
        {
            HeatMapView           view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            DxuiDpiScaler         scaler;
            RECT                  map    = {};
            size_t                labels = 0;



            scaler.SetDpi (96);
            view.SetPalette (MakePalette());
            view.Layout     (RECT { kPaneLeft, 0, kPaneLeft + kWidth, kHeight }, scaler);

            map = view.GetMapRect();
            view.ZoomAt   ({ map.left, map.top }, 10.0f);
            view.ScrollBy (3);
            view.Paint    (painter, text, theme);

            map = view.GetMapRect();

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind != RecordedTextKind::DrawString || call.hAlign != DxuiTextHAlign::Right || !call.text.starts_with (L"$"))
                {
                    continue;
                }

                labels++;
                Assert::IsTrue (call.y >= (float) map.top, (call.text + L" is drawn below the map's top").c_str());
            }

            Assert::AreEqual (3L,   view.GetScrollTop(), L"the view is scrolled part of a row");
            Assert::IsTrue   (labels > 0, L"the rows in full view are labeled");
        }



        //  A range's header starts at the pane's text inset too.
        TEST_METHOD (ARangeHeaderStartsOnTheInset)
        {
            constexpr int  kRangeBytes = 0x100;



            for (int dpi : kInsetDpis)
            {
                HeatMapView               view;
                MockDxuiPainter           painter;
                MockDxuiTextRenderer      text;
                MockDxuiTheme             theme;
                DxuiDpiScaler             scaler;
                const RecordedTextCall  * header = nullptr;
                std::wstring              at     = std::format (L"at {} DPI", dpi);



                scaler.SetDpi (dpi);
                view.SetPalette (MakePalette());
                view.Layout     (RECT { kPaneLeft, 0, kPaneLeft + kWidth, kHeight }, scaler);
                view.SetRanges  ({ HeatMapView::Band { L"Zero page", 0x0000, kRangeBytes } });
                view.Paint      (painter, text, theme);

                for (const RecordedTextCall & call : text.Calls())
                {
                    if (header == nullptr && call.kind == RecordedTextKind::DrawString && call.text.starts_with (L"Zero page"))
                    {
                        header = &call;
                    }
                }

                Assert::IsNotNull (header, (L"the header is drawn " + at).c_str());
                Assert::AreEqual  ((float) (kPaneLeft + DxuiPaneMetrics::GetContentTextInsetPx (scaler)), header->x,
                                   (L"and starts at the pane's text inset " + at).c_str());
            }
        }
    };
}
