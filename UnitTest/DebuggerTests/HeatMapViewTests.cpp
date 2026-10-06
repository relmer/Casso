#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapViewTests
//
//  What the heat map pane draws for each address in each mode, where its
//  addresses, modes and actions are, how it zooms and pans, which address
//  the mouse picks, and what its readout says. The blit and the text are
//  checked on screen.
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

        using Mode   = HeatMapView::Mode;
        using Action = HeatMapView::Action;

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

        static void Place (HeatMapView & view)
        {
            DxuiDpiScaler  scaler;



            view.SetPalette (MakePalette());
            view.Layout (RECT { 0, 0, kWidth, kHeight }, scaler);
        }

        //  A point inside an address's cell, `inset` pixels in from its top left.
        static POINT GetPointOf (const HeatMapView & view, Word address, int inset = 1)
        {
            RECT   map   = view.GetMapRect();
            int    pitch = view.GetCellPx() + HeatMapView::kStreetPx;
            POINT  at    = {};



            at.x = map.left + (address & 0xFF) * pitch - view.GetScroll().x + inset;
            at.y = map.top  + (address >> 8)   * pitch - view.GetScroll().y + inset;
            return at;
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

        static POINT GetActionPoint (const HeatMapView & view, Action action)
        {
            int  y = HeatMapView::kBarDip + HeatMapView::kBarDip / 2;



            for (int x = 0; x < kWidth; x++)
            {
                if (view.GetActionAt ({ x, y }) == action)
                {
                    return { x + 2, y };
                }
            }

            Assert::Fail (L"the action is not on the second row");
            return {};
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



        TEST_METHOD (ACellIsItsAddressByPageAndByte)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  none;



            Place (view);
            execute[0x0102] = 255;
            view.SetLevels (execute, none, none);

            Assert::AreEqual (kExecute, view.GetCellColor (0x0102));
            Assert::AreEqual (kCold,    view.GetCellColor (0x0103));
            Assert::AreEqual (kExecute, GetPixelAt (view, GetPointOf (view, 0x0102)));
            Assert::AreEqual (kCold,    GetPixelAt (view, GetPointOf (view, 0x0103)));
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
            RECT         map  = {};
            int          cell = 0;



            Place (view);
            map = view.GetMapRect();
            view.ZoomAt ({ map.left, map.top }, -10.0f);
            Assert::AreEqual (1, view.GetCellPx(), L"the smallest cell is a pixel");

            while (cell != view.GetCellPx())
            {
                cell = view.GetCellPx();

                Assert::AreEqual (kCold,       GetPixelAt (view, { map.left + cell - 1, map.top }), L"a cell's last pixel");
                Assert::AreEqual (kBackground, GetPixelAt (view, { map.left + cell,     map.top }), L"the street after it");
                Assert::AreEqual (kBackground, GetPixelAt (view, { map.left,            map.top + cell }));

                view.ZoomAt ({ map.left, map.top }, 1.0f);
            }

            Assert::AreEqual (HeatMapView::kMaxCellPx, view.GetCellPx(), L"the zoom stops at its largest");
        }



        TEST_METHOD (TheAddressUnderAPointIsFoundByRowAndColumn)
        {
            HeatMapView  view;



            Place (view);

            Assert::AreEqual ((Word) 0x0000, view.GetAddressAt (GetPointOf (view, 0x0000)).value_or (1));
            Assert::AreEqual ((Word) 0x4A63, view.GetAddressAt (GetPointOf (view, 0x4A63)).value_or (0));
            Assert::AreEqual ((Word) 0x4A63, view.GetAddressAt (GetPointOf (view, 0x4A63, 3)).value_or (0), L"its street is the cell's");
            Assert::IsFalse  (view.GetAddressAt ({ 2, 100 }).has_value(), L"the page numbers are no address");
            Assert::IsFalse  (view.GetAddressAt ({ 100, 2 }).has_value(), L"the bar is no address");
        }



        TEST_METHOD (ZoomKeepsTheAddressUnderThePointer)
        {
            HeatMapView  view;
            POINT        point = {};



            Place (view);
            point = GetPointOf (view, 0x3456);

            view.ZoomAt (point, 3.0f);
            Assert::IsTrue   (view.GetCellPx() > 3);
            Assert::AreEqual ((Word) 0x3456, view.GetAddressAt (point).value_or (0), L"zooming in");

            view.ZoomAt (point, -1.0f);
            Assert::AreEqual ((Word) 0x3456, view.GetAddressAt (point).value_or (0), L"and out again");
        }



        TEST_METHOD (TheMapScrollsNoFurtherThanItsEdges)
        {
            HeatMapView  view;
            RECT         map     = {};
            long         content = 0;



            Place (view);
            map     = view.GetMapRect();
            content = HeatMapView::kSide * (view.GetCellPx() + HeatMapView::kStreetPx);

            view.ScrollBy (-50, -50);
            Assert::AreEqual (0L, view.GetScroll().x);
            Assert::AreEqual (0L, view.GetScroll().y);

            view.ScrollBy (100000, 100000);
            Assert::AreEqual (content - (map.right  - map.left), view.GetScroll().x);
            Assert::AreEqual (content - (map.bottom - map.top),  view.GetScroll().y);
            Assert::AreEqual ((Word) 0xFFFF, view.GetAddressAt ({ map.right - 2, map.bottom - 2 }).value_or (0), L"the last address in the corner");
        }



        TEST_METHOD (ResetZoomGoesBackToTheStart)
        {
            HeatMapView  view;
            POINT        point = {};



            Place (view);
            point = GetPointOf (view, 0x4040);
            view.ZoomAt (point, 4.0f);
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, GetActionPoint (view, Action::ResetZoom)));

            Assert::AreEqual (3,  view.GetCellPx());
            Assert::AreEqual (0L, view.GetScroll().x);
            Assert::AreEqual (0L, view.GetScroll().y);
        }



        TEST_METHOD (TheWheelZoomsAboutThePointerAndShiftWheelScrolls)
        {
            HeatMapView     view;
            DxuiMouseEvent  wheel;
            POINT           point = {};



            Place (view);
            point             = GetPointOf (view, 0x2020);
            wheel             = MakeEvent (DxuiMouseEventKind::Wheel, point);
            wheel.wheelDelta  = 1.0f;

            Assert::IsTrue   (view.OnMouse (wheel));
            Assert::IsTrue   (view.GetCellPx() > 3);
            Assert::AreEqual ((Word) 0x2020, view.GetAddressAt (point).value_or (0));

            view.ScrollBy (-100000, -100000);
            wheel.positionDip = GetPointOf (view, 0x0000);
            wheel.wheelDelta  = -1.0f;
            wheel.shift       = true;
            Assert::IsTrue   (view.OnMouse (wheel));
            Assert::IsTrue   (view.GetScroll().y > 0, L"Shift and the wheel down scrolls down");
        }



        TEST_METHOD (ADragPansAndPicksNothing)
        {
            HeatMapView  view;
            POINT        start  = {};
            POINT        moved  = {};
            int          picked = 0;



            Place (view);
            view.SetOnPickAddress ([&picked] (Word) { picked++; });
            view.ZoomAt (GetPointOf (view, 0x8080), 2.0f);
            view.ScrollBy (-100000, -100000);
            view.ScrollBy (200, 200);

            start = GetPointOf (view, 0x3030);
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



            Place (view);
            read[0x2001] = 100;
            read[0x2101] = 200;
            read[0x2110] = 255;
            view.SetLevels (none, read, none);

            //  Over cold $2000, three pixels from $2001 and four from $2101.
            Assert::AreEqual ((Word) 0x2101, view.GetPickAt (GetPointOf (view, 0x2000, 2)).value_or (0), L"the hotter of the two near it");
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

            at = GetPointOf (view, 0x2000, view.GetCellPx() - 1);
            Assert::AreEqual ((Word) 0x2000, view.GetPickAt (at).value_or (0));
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



        TEST_METHOD (CumulativeAndFadingSwitchAndTheFadeSteps)
        {
            HeatMapView  view;
            POINT        fade    = {};
            int          changed = 0;



            Place (view);
            view.SetOnOptionsChanged ([&changed] { changed++; });

            Assert::AreEqual (std::wstring (L"Fade 10 s"), view.GetActionLabel (Action::Fade));

            fade = GetActionPoint (view, Action::Fade);
            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, fade));
            Assert::AreEqual (20, view.GetOptions().fadeSeconds);

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, GetActionPoint (view, Action::Cumulative)));
            Assert::IsTrue   (view.GetOptions().cumulative);
            Assert::IsTrue   (view.GetActionAt (fade) == Action::ResetCounts, L"cumulative, the fade gives way to Reset counts");

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, GetActionPoint (view, Action::Fading)));
            Assert::IsFalse  (view.GetOptions().cumulative);
            Assert::AreEqual (3, changed, L"each change is reported to be kept");
        }



        TEST_METHOD (ResetCountsIsOfferedWhileCumulative)
        {
            HeatMapView     view;
            HeatMapOptions  options;
            int             resets = 0;



            options.cumulative = true;
            Place (view);
            view.SetOptions (options);
            view.SetOnResetCounts ([&resets] { resets++; });

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Down, GetActionPoint (view, Action::ResetCounts)));
            Assert::AreEqual (1, resets);
            Assert::AreEqual (std::wstring (L"Reset counts"), view.GetActionLabel (Action::ResetCounts));
        }



        TEST_METHOD (TheReadoutSaysTheAddressPickedAndWhatTouchedIt)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  write   (0x10000, 0);
            std::vector<Byte>  none;



            Place (view);
            execute[0x2010] = 3;
            write[0x2010]   = 9;
            view.SetLevels (execute, none, write);

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, GetPointOf (view, 0x2010)));
            Assert::AreEqual (std::wstring (L"$2010  executed, written"), view.GetReadout());

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Move, GetPointOf (view, 0x0300)));
            Assert::AreEqual (std::wstring (L"$0300"), view.GetReadout());

            view.OnMouse (MakeEvent (DxuiMouseEventKind::Leave, GetPointOf (view, 0x0300)));
            Assert::AreEqual (std::wstring(), view.GetReadout());
        }
    };
}
