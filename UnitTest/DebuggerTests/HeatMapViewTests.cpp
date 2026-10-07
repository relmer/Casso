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
        static constexpr uint32_t  kExecute    = 0xFF0000FF;
        static constexpr uint32_t  kRead       = 0xFF00FF00;
        static constexpr uint32_t  kWrite      = 0xFFFF0000;

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
        }

        //  A point inside an address's cell, `inset` pixels in from its top left.
        static POINT GetPointOf (const HeatMapView & view, Word address, int inset = 1)
        {
            RECT  cell = view.GetCellRect (address);



            return { cell.left + inset, cell.top + inset };
        }

        //  The frame's pixel under a point.
        static uint32_t GetPixelAt (const HeatMapView & view, POINT point)
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
            Assert::IsTrue      ((faint & 0xFF) > 0x60, L"at least kFaintest of the way to the color");
            Assert::IsTrue      ((faint & 0xFF) < (hot & 0xFF));
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



        TEST_METHOD (CellsStartThreePixelsWideWithAOnePixelStreet)
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

                view.ScrollBy (-100000, -100000);
                view.ZoomAt ({ map.left, map.top }, 1.0f);
            }

            Assert::AreEqual (HeatMapView::kMaxCellPx, view.GetCellPx(), L"the zoom stops at its largest");
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
            Assert::AreEqual (16,   HeatMapView::GetColumnsFor (10,     4), L"never fewer than kMinColumns, which then scroll across");
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

                Assert::IsFalse  (view.HasHorizontalScroll(), at.c_str());
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



        TEST_METHOD (ZoomKeepsTheAddressUnderThePointerWhereTheRowsScrollAcross)
        {
            HeatMapView  view;
            RECT         map     = {};
            POINT        point   = {};
            Word         address = 0;



            Place (view);
            map = view.GetMapRect();
            view.ZoomAt ({ map.left, map.top }, 30.0f);
            view.ZoomAt ({ map.left, map.top }, -2.0f);
            Assert::IsTrue (view.HasHorizontalScroll(), L"sixteen cells overflow the pane");

            address = (Word) (view.GetColumns() * 2 + 3);
            point   = GetPointOf (view, address, 10);

            view.ZoomAt (point, 1.0f);
            Assert::AreEqual (address, view.GetAddressAt (point).value_or (0), L"zooming in");

            view.ZoomAt (point, -1.0f);
            Assert::AreEqual (address, view.GetAddressAt (point).value_or (0), L"and out again");
        }



        TEST_METHOD (ZoomThatChangesTheRowKeepsTheAddressAtThePointersHeight)
        {
            HeatMapView  view;
            POINT        point   = {};
            int          columns = 0;



            Place (view);
            columns = view.GetColumns();
            point   = GetPointOf (view, 0x1456);

            view.ZoomAt (point, 3.0f);
            Assert::IsTrue (view.GetCellPx() > 3);
            Assert::IsTrue (view.GetColumns() < columns, L"larger cells, fewer to a row");
            Assert::IsTrue (IsAtHeight (view, 0x1456, point.y), L"zooming in, its row stays at the pointer");

            view.ZoomAt (point, -3.0f);
            Assert::AreEqual (columns, view.GetColumns());
            Assert::IsTrue (IsAtHeight (view, 0x1456, point.y), L"and zooming out");
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
            Assert::AreEqual (4,   view.GetCellPx(), L"taller cells");
            Assert::AreEqual (128, view.GetColumns(), L"since 64 would stretch past one and a half times");
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

            view.ScrollBy (-50, -50);
            Assert::AreEqual (0L, view.GetScroll().x);
            Assert::AreEqual (0L, view.GetScroll().y);

            view.ScrollBy (100000, 100000);
            Assert::AreEqual (0L, view.GetScroll().x, L"the rows fit across, so nothing scrolls that way");
            Assert::AreEqual (content - (map.bottom - map.top), view.GetScroll().y);
            Assert::AreEqual ((Word) 0xFFFF, view.GetAddressAt ({ map.right - 2, map.bottom - 2 }).value_or (0), L"the last address in the corner");
        }



        TEST_METHOD (ResetZoomGoesBackToTheStart)
        {
            HeatMapView  view;
            POINT        point = {};



            Place (view);
            point = GetPointOf (view, 0x4040);
            view.ZoomAt (point, 4.0f);
            view.ResetZoom();

            Assert::AreEqual (3,  view.GetCellPx());
            Assert::AreEqual (0L, view.GetScroll().x);
            Assert::AreEqual (0L, view.GetScroll().y);
        }



        TEST_METHOD (ZoomInAndZoomOutGoAboutTheMiddleOfTheMap)
        {
            HeatMapView  view;
            RECT         map    = {};
            POINT        middle = {};
            Word         before = 0;



            Place (view);
            view.ScrollBy (0, 300);
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
            Assert::IsTrue   (view.GetScroll().y > 0, L"the wheel down scrolls down");
            Assert::AreEqual (0L, view.GetScroll().y % (view.GetCellPx() + HeatMapView::kStreetPx), L"by whole rows");
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



        TEST_METHOD (ShiftWheelScrollsAcrossOnlyWhenTheRowsOverflow)
        {
            HeatMapView  view;
            RECT         map = {};



            Place (view);
            map = view.GetMapRect();

            Assert::IsFalse  (view.OnMouse (MakeWheel (GetPointOf (view, 0x0000), -1.0f, false, true)), L"the rows fit, so there is nothing across to scroll");
            Assert::AreEqual (0L, view.GetScroll().x);

            view.ZoomAt ({ map.left, map.top }, 30.0f);
            Assert::IsTrue   (view.HasHorizontalScroll(), L"sixteen of the largest cells overflow the pane");

            Assert::IsTrue   (view.OnMouse (MakeWheel (GetPointOf (view, 0x0000), -1.0f, false, true)));
            Assert::IsTrue   (view.GetScroll().x > 0, L"Shift and the wheel down scrolls across");
            Assert::AreEqual (0L, view.GetScroll().y, L"and not down");
        }



        TEST_METHOD (TheWheelOffTheMapIsLeftAlone)
        {
            HeatMapView  view;



            Place (view);

            Assert::IsFalse (view.OnMouse (MakeWheel ({ 100, 2 }, 1.0f, true, false)), L"over the bar, the window's text zoom takes it");
            Assert::AreEqual (3, view.GetCellPx());
        }



        TEST_METHOD (ScrollbarsShowAlongEachSideTheMapOverflows)
        {
            HeatMapView  view;
            HeatMapView  large;
            RECT         map = {};



            Place (view);
            map = view.GetMapRect();

            Assert::IsTrue   (view.HasVerticalScroll(),    L"512 rows are taller than the pane");
            Assert::IsFalse  (view.HasHorizontalScroll(),  L"the rows fit across");
            Assert::AreEqual (map.right, view.GetVerticalBarRect().left, L"the scrollbar beside the map");
            Assert::IsTrue   (view.IsOverMap ({ view.GetVerticalBarRect().left + 1, map.top + 1 }), L"the scrollbar counts as the map's");

            view.ZoomAt ({ map.left, map.top }, 30.0f);
            Assert::IsTrue   (view.HasHorizontalScroll());
            Assert::AreEqual (view.GetMapRect().bottom, view.GetHorizontalBarRect().top, L"and below it");

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
            Assert::AreEqual (page, view.GetScroll().y, L"a press below the thumb goes down a page");

            view.ScrollBy (0, -100000);
            thumb = { bar.left + 2, bar.top + 20 };

            Assert::IsTrue   (view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, thumb)));
            Assert::IsTrue   (view.IsPressed(), L"the thumb is held");
            Assert::IsTrue   (view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, { thumb.x, thumb.y + 50 })), L"the drag is the thumb's");
            Assert::IsTrue   (view.GetScroll().y > 100, L"dragging the thumb a little goes a long way");
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
            view.ScrollBy (-100000, -100000);
            view.ScrollBy (200, 200);

            start = GetPointOf (view, (Word) (view.GetColumns() * 5 + 5));
            moved = { start.x - 30, start.y - 20 };

            Assert::IsTrue (view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, start)));
            Assert::IsTrue (view.IsPressed());
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, moved));
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   moved));

            Assert::AreEqual (230L, view.GetScroll().x, L"the map followed the mouse across");
            Assert::AreEqual (220L, view.GetScroll().y, L"and down");
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



        TEST_METHOD (WhileCellsAreSmallTheHottestCellNearbyIsPicked)
        {
            HeatMapView        view;
            std::vector<Byte>  read (0x10000, 0);
            std::vector<Byte>  none;
            Word               below = 0;



            Place (view);
            below        = (Word) (0x2001 + view.GetColumns());
            read[0x2001] = 100;
            read[below]  = 200;
            read[0x2010] = 255;
            view.SetLevels (none, read, none);

            //  Over cold $2000, three pixels from $2001 and four from the cell
            //  under it.
            Assert::AreEqual (below,         view.GetPickAt (GetPointOf (view, 0x2000, 2)).value_or (0), L"the hotter of the two near it");
            Assert::AreEqual ((Word) 0x2001, view.GetPickAt (GetPointOf (view, 0x2001)).value_or (0),    L"a touched cell under the mouse is its own");
            Assert::AreEqual ((Word) 0x2008, view.GetPickAt (GetPointOf (view, 0x2008)).value_or (0),    L"nothing touched near it, the cell under it");
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



        TEST_METHOD (AClickOnAModeShowsIt)
        {
            HeatMapView     view;
            DxuiMouseEvent  click;
            int             top     = HeatMapView::kBarDip / 2;
            int             changed = 0;



            Place (view);
            view.SetOnOptionsChanged ([&changed] { changed++; });
            click = MakeEvent (DxuiMouseEventKind::Down, { HeatMapView::kGutterDip + HeatMapView::kTabDip + 5, top });

            Assert::IsTrue   (view.OnMouse (click));
            Assert::AreEqual ((int) Mode::Code, (int) view.GetMode());
            Assert::AreEqual ((int) Mode::Code, (int) view.GetOptions().view, L"the view is one of the options kept");
            Assert::AreEqual (1, changed);

            click.positionDip = { HeatMapView::kGutterDip + HeatMapView::kTabDip * 2 + 5, top };
            view.OnMouse (click);
            Assert::AreEqual ((int) Mode::Data, (int) view.GetMode());

            click.positionDip = { HeatMapView::kGutterDip + HeatMapView::kTabDip * 3 + 5, top };
            Assert::IsFalse  (view.OnMouse (click), L"past the last mode is no mode");
            Assert::AreEqual ((int) Mode::Data, (int) view.GetMode());
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
            std::vector<Byte>  none;
            RECT               anchor   = {};
            RECT               expected = {};
            std::wstring       text;
            POINT              beside   = {};



            Place (view);
            view.SetTop (50000.0);
            read[0x2001] = 200;
            view.SetLevels (none, read, none);
            beside   = GetPointOf (view, 0x2000, 2);
            expected = view.GetCellRect (0x2001);
            InflateRect (&expected, HeatMapView::kStreetPx, HeatMapView::kStreetPx);

            Assert::IsTrue   (view.TryGetTipAt (beside, anchor, text));
            Assert::IsTrue   (text.starts_with (L"$2001  read "), text.c_str());
            Assert::IsTrue   (EqualRect (&anchor, &expected) != FALSE, L"anchored on the busier cell the pick snapped to");

            Assert::IsFalse  (view.TryGetTipAt ({ 2, 100 }, anchor, text), L"none over the row labels");

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, beside));
            Assert::IsFalse  (view.TryGetTipAt (beside, anchor, text), L"none while the button is down");
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
            text.Reset();
            view.Paint (painter, text, theme);
            notes = std::ranges::count_if (text.Calls(), [] (const RecordedTextCall & call) { return call.text == HeatMapView::kpszRebuildingNote; });
            Assert::AreEqual ((size_t) 1, notes, L"one note while the heat is rebuilt");
        }
    };
}
