#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapRangeLayoutTests
//
//  The heat map focused on a set of ranges: each range under its header in
//  the set's order, its rows from its own first address and stretched to
//  the pane, the whole set fitted to the pane until the user zooms, and
//  every address the mouse finds the one its cell shows.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapRangeLayoutTests)
    {
    public:

        static constexpr uint32_t  kBackground = 0xFF000000;
        static constexpr uint32_t  kCold       = 0xFF303030;
        static constexpr uint32_t  kExecute    = 0xFF0000FF;
        static constexpr int       kWidth      = 600;
        static constexpr int       kHeight     = 400;

        //  Three ranges out of address order, so the stack follows the set.
        static std::vector<HeatMapView::Band> MakeRanges()
        {
            return
            {
                { L"Zero page",   0x0000, 0x0100 },
                { L"Sprites",     0x6000, 0x3600 },
                { L"Text page 1", 0x0400, 0x0400 },
            };
        }

        static void Place (HeatMapView & view, int width = kWidth, int height = kHeight)
        {
            DxuiDpiScaler         scaler;
            HeatMapView::Palette  palette;



            palette.background = kBackground;
            palette.cold       = kCold;
            palette.execute    = kExecute;
            view.SetPalette (palette);
            view.Layout (RECT { 0, 0, width, height }, scaler);
        }

        static POINT GetPointOf (const HeatMapView & view, Word address)
        {
            RECT  cell = view.GetCellRect (address);



            return { cell.left + 1, cell.top + 1 };
        }

        static DxuiMouseEvent MakeEvent (DxuiMouseEventKind kind, POINT point)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Down || kind == DxuiMouseEventKind::Up) ? DxuiMouseButton::Left : DxuiMouseButton::None;
            ev.positionDip = point;
            return ev;
        }



        TEST_METHOD (TheRangesStackInTheSetsOrderEachUnderItsHeader)
        {
            HeatMapView  view;
            RECT         zero   = {};
            RECT         sprite = {};
            RECT         text   = {};



            Place (view);
            view.SetRanges (MakeRanges());

            zero   = view.GetHeaderRect (0);
            sprite = view.GetHeaderRect (1);
            text   = view.GetHeaderRect (2);

            Assert::IsTrue   (view.HasRanges());
            Assert::AreEqual (view.GetMapRect().top, zero.top, L"the first header opens the map");
            Assert::AreEqual ((long) HeatMapView::kHeaderDip, zero.bottom - zero.top);
            Assert::AreEqual (zero.bottom, view.GetCellRect (0x0000).top, L"the range's first row under its header");
            Assert::IsTrue   (view.GetCellRect (0x00FF).bottom <= sprite.top, L"Sprites below the zero page");
            Assert::AreEqual (sprite.bottom, view.GetCellRect (0x6000).top);
            Assert::IsTrue   (view.GetCellRect (0x95FF).bottom <= text.top, L"and Text page 1 last, as the set has it, though its addresses are lower");
            Assert::AreEqual (text.bottom, view.GetCellRect (0x0400).top);
        }



        TEST_METHOD (ARangesRowsRunFromItsOwnFirstAddress)
        {
            HeatMapView  view;
            RECT         first  = {};
            RECT         second = {};
            int          across = 0;



            Place (view);
            view.SetRanges (MakeRanges());

            across = HeatMapView::GetColumnsOf (view.GetColumns(), 0x3600);
            first  = view.GetCellRect (0x6000);
            second = view.GetCellRect ((Word) (0x6000 + across));

            Assert::AreEqual (view.GetMapRect().left, first.left, L"the range's first address starts its first row");
            Assert::AreEqual (first.left, second.left, L"and the next row starts a row's worth of addresses on");
            Assert::IsTrue   (second.top > first.top);
            Assert::IsTrue   (view.GetCellRect (0x5FFF).right == 0 && view.GetCellRect (0x5FFF).bottom == 0, L"an address in no range has no cell");
        }



        TEST_METHOD (EachRangesRowsStretchToThePaneAndASmallRangeIsOneRow)
        {
            HeatMapView  view;
            RECT         map  = {};
            int          zero = 0;



            Place (view);
            view.SetRanges (MakeRanges());
            map  = view.GetMapRect();
            zero = HeatMapView::GetColumnsOf (view.GetColumns(), 0x0100);

            Assert::IsTrue   (zero <= 0x0100, L"no more columns than the range holds");
            Assert::AreEqual (map.right, view.GetCellRect ((Word) (zero - 1)).right + HeatMapView::kStreetPx, L"the zero page's row ends the map");
            Assert::AreEqual (map.right, view.GetCellRect ((Word) (0x6000 + HeatMapView::GetColumnsOf (view.GetColumns(), 0x3600) - 1)).right + HeatMapView::kStreetPx,
                              L"and so does a row of Sprites");
            Assert::AreEqual (view.GetCellRect (0x0000).bottom - view.GetCellRect (0x0000).top, view.GetCellRect (0x6000).bottom - view.GetCellRect (0x6000).top,
                              L"one zoom: every range's rows are as tall");
            Assert::AreEqual (1,   HeatMapView::GetColumnsOf (256, 1),       L"a single byte is a row of one");
            Assert::AreEqual (32,  HeatMapView::GetColumnsOf (256, 32),      L"32 bytes are 32 across");
            Assert::AreEqual (256, HeatMapView::GetColumnsOf (256, 0x3600),  L"a large range takes the map's columns");
        }



        TEST_METHOD (ANewSetFitsThePaneUntilTheUserZooms)
        {
            HeatMapView  view;
            int          fitted = 0;



            Place (view);
            Assert::AreEqual (3, view.GetCellPx(), L"all memory starts at three pixels");

            view.SetRanges (MakeRanges());

            Assert::IsFalse (view.HasVerticalScroll(),   L"every range in view");
            Assert::IsFalse (view.HasHorizontalScroll());

            view.SetRanges ({ { L"Zero page", 0x0000, 0x0100 }, { L"Text page 1", 0x0400, 0x0400 } });
            fitted = view.GetCellPx();

            Assert::IsFalse (view.HasVerticalScroll(),   L"a new set fits again");
            Assert::IsTrue  (fitted > 3, L"and as large as that allows: these ranges are 1,280 bytes");
            view.ZoomIn();
            Assert::IsTrue  (view.HasVerticalScroll(), L"so one notch larger no longer fits");
            view.ZoomOut();
            view.ZoomOut();
            fitted = view.GetCellPx();

            view.ZoomIn();
            Assert::IsTrue  (view.GetCellPx() > fitted, L"the zoom applies to the set");

            Place (view, kWidth, kHeight + 200);
            Assert::IsTrue  (view.GetCellPx() > fitted, L"a resize after a zoom keeps the user's zoom");

            view.ResetZoom();
            Assert::IsFalse (view.HasVerticalScroll(), L"Reset zoom fits the set again");
        }



        TEST_METHOD (TheMouseFindsTheAddressOfTheCellUnderItInEveryRange)
        {
            HeatMapView           view;
            std::optional<Word>   picked;
            POINT                 at     = {};



            Place (view);
            view.SetRanges (MakeRanges());
            view.SetOnPickAddress ([&picked] (Word address) { picked = address; });

            for (Word address : { (Word) 0x0000, (Word) 0x00FF, (Word) 0x6000, (Word) 0x7234, (Word) 0x95FF, (Word) 0x0400, (Word) 0x07FF })
            {
                at = GetPointOf (view, address);

                Assert::AreEqual (address, view.GetAddressAt (at).value_or (0), std::format (L"hit at ${:04X}", address).c_str());

                picked.reset();
                (void) view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, at));
                (void) view.OnMouse (MakeEvent (DxuiMouseEventKind::Up,   at));

                Assert::AreEqual (address, picked.value_or (0), std::format (L"a click shows ${:04X} in memory", address).c_str());
                Assert::IsTrue   (view.GetTipText (address).starts_with (std::format (L"${:04X}", address)), L"and its tip gives it");
            }
        }



        TEST_METHOD (AHeaderAndTheEndOfARangesLastRowHoldNoAddress)
        {
            HeatMapView  view;
            RECT         header = {};
            RECT         last   = {};



            Place (view);
            view.SetRanges ({ { L"Odd", 0x0300, 0x0123 } });

            header = view.GetHeaderRect (0);
            last   = view.GetCellRect (0x0422);

            Assert::IsFalse (view.GetAddressAt ({ header.left + 60, header.top + 2 }).has_value(), L"a header is no cell");
            Assert::AreEqual ((Word) 0x0422, view.GetAddressAt ({ last.left + 1, last.top + 1 }).value_or (0));
            Assert::IsFalse (view.GetAddressAt ({ last.right + 4, last.top + 1 }).has_value(), L"nothing past the range's last address");
        }



        TEST_METHOD (TheSnapFindsAHotCellInTheRangeTheMouseIsOver)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  none;
            RECT               cell = {};



            Place (view, 300, 1000);
            view.SetRanges ({ { L"Low", 0x0000, 0x2000 }, { L"High", 0x8000, 0x2000 } });
            Assert::IsTrue (view.GetCellPx() < 8, L"cells small enough to snap");

            execute[0x8001] = 255;
            view.SetLevels (execute, none, none);

            cell = view.GetCellRect (0x8000);

            Assert::AreEqual ((Word) 0x8001, view.GetPickAt ({ cell.left + 1, cell.top + 1 }).value_or (0), L"the hot cell beside it, in High");
        }



        TEST_METHOD (TheFrameDrawsEachRangesAddresses)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  none;
            RECT               map   = {};
            POINT              at    = {};
            long               width = 0;



            Place (view);
            execute[0x6003] = 255;
            view.SetLevels (execute, none, none);
            view.SetRanges ({ { L"Zero page", 0x0000, 0x0100 }, { L"Sprites", 0x6000, 0x0400 } });

            map   = view.GetMapRect();
            width = map.right - map.left;
            at    = GetPointOf (view, 0x6003);

            Assert::AreEqual (kExecute,    view.GetPixels()[(size_t) ((at.y - map.top) * width + (at.x - map.left))], L"the hot address in its range");
            at    = GetPointOf (view, 0x6004);
            Assert::AreEqual (kCold,       view.GetPixels()[(size_t) ((at.y - map.top) * width + (at.x - map.left))]);
            at    = { map.left + 1, view.GetHeaderRect (1).top + 2 };
            Assert::AreEqual (kBackground, view.GetPixels()[(size_t) ((at.y - map.top) * width + (at.x - map.left))], L"a header is the page");
        }



        TEST_METHOD (AllMemoryComesBackAtItsStartingZoom)
        {
            HeatMapView  view;



            Place (view);
            view.SetRanges (MakeRanges());
            view.ClearRanges();

            Assert::IsFalse  (view.HasRanges());
            Assert::AreEqual (3, view.GetCellPx());
            Assert::AreEqual (HeatMapView::kAddressCount / view.GetColumns(), view.GetRows());
            Assert::AreEqual ((Word) 0x1234, view.GetAddressAt (GetPointOf (view, 0x1234)).value_or (0));
        }



        TEST_METHOD (ASetWithNothingInItShowsNoCells)
        {
            HeatMapView  view;



            Place (view);
            view.SetRanges ({});

            Assert::IsTrue   (view.HasRanges());
            Assert::AreEqual (0, view.GetRows());
            Assert::IsFalse  (view.GetAddressAt ({ view.GetMapRect().left + 2, view.GetMapRect().top + 2 }).has_value());
        }
    };
}





