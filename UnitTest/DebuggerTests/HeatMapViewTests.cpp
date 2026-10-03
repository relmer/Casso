#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapViewTests
//
//  What the heat map pane draws for each address in each mode, where its
//  addresses and modes are, and what its readout says. The drawing itself,
//  the bitmap blit and the text, is checked on screen.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (HeatMapViewTests)
    {
    public:

        static constexpr uint32_t  kBackground = 0xFF000000;
        static constexpr uint32_t  kExecute    = 0xFF0000FF;
        static constexpr uint32_t  kRead       = 0xFF00FF00;
        static constexpr uint32_t  kWrite      = 0xFFFF0000;

        using Mode = HeatMapView::Mode;

        static HeatMapView::Palette MakePalette()
        {
            HeatMapView::Palette  palette;



            palette.background = kBackground;
            palette.execute    = kExecute;
            palette.read       = kRead;
            palette.write      = kWrite;
            return palette;
        }

        //  A 256 by 256 map with its bar and page numbers around it, at 96
        //  dpi: the map's top left is (kGutterDip, kBarDip).
        static void Place (HeatMapView & view)
        {
            DxuiDpiScaler  scaler;
            RECT           bounds = { 0, 0, HeatMapView::kGutterDip + HeatMapView::kSide + HeatMapView::kInsetDip,
                                            HeatMapView::kBarDip + HeatMapView::kSide + HeatMapView::kInsetDip };



            view.Layout (bounds, scaler);
        }

        static POINT GetPointOf (Word address)
        {
            return { HeatMapView::kGutterDip + (address & 0xFF), HeatMapView::kBarDip + (address >> 8) };
        }



        TEST_METHOD (AColdAddressIsTheBackground)
        {
            Assert::AreEqual (kBackground, HeatMapView::GetColor (Mode::All, 0, 0, 0, MakePalette()));
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



            Assert::AreEqual (kBackground, HeatMapView::GetColor (Mode::Code, 0,   255, 255, palette));
            Assert::AreEqual (kExecute,    HeatMapView::GetColor (Mode::Code, 255, 255, 255, palette));
            Assert::AreEqual (kBackground, HeatMapView::GetColor (Mode::Data, 255, 0,   0,   palette));
            Assert::AreEqual (kWrite,      HeatMapView::GetColor (Mode::Data, 255, 10,  255, palette));
        }



        TEST_METHOD (AFaintAddressIsStillSetApartFromTheBackground)
        {
            uint32_t  faint = HeatMapView::GetColor (Mode::All, 1, 0, 0, MakePalette());
            uint32_t  hot   = HeatMapView::GetColor (Mode::All, 255, 0, 0, MakePalette());



            Assert::AreNotEqual (kBackground, faint);
            Assert::IsTrue      ((faint & 0xFF) > 0x40, L"at least kFaintest of the way to the color");
            Assert::IsTrue      ((faint & 0xFF) < (hot & 0xFF));
        }



        TEST_METHOD (APixelIsItsAddressByPageAndByte)
        {
            HeatMapView        view;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  none;



            view.SetPalette (MakePalette());
            execute[0xC65E] = 255;
            view.SetLevels (execute, none, none);

            Assert::AreEqual ((size_t) 0x10000, view.GetPixels().size());
            Assert::AreEqual (kExecute,    view.GetPixels()[0xC65E]);
            Assert::AreEqual (kBackground, view.GetPixels()[0xC65F]);
        }



        TEST_METHOD (TheAddressUnderAPointIsFoundByRowAndColumn)
        {
            HeatMapView  view;



            Place (view);

            Assert::AreEqual ((Word) 0xC65E, view.GetAddressAt (GetPointOf (0xC65E)).value_or (0));
            Assert::AreEqual ((Word) 0x0000, view.GetAddressAt (GetPointOf (0x0000)).value_or (1));
            Assert::AreEqual ((Word) 0xFFFF, view.GetAddressAt (GetPointOf (0xFFFF)).value_or (0));
            Assert::IsFalse  (view.GetAddressAt ({ 2, 100 }).has_value(), L"the page numbers are no address");
            Assert::IsFalse  (view.GetAddressAt ({ 100, 2 }).has_value(), L"the bar is no address");
        }



        TEST_METHOD (AClickOnAModeShowsIt)
        {
            HeatMapView     view;
            DxuiMouseEvent  click;
            int             top = HeatMapView::kBarDip / 2;



            Place (view);
            click.kind        = DxuiMouseEventKind::Down;
            click.button      = DxuiMouseButton::Left;
            click.positionDip = { HeatMapView::kGutterDip + HeatMapView::kTabDip + 5, top };

            Assert::IsTrue   (view.OnMouse (click));
            Assert::AreEqual ((int) Mode::Code, (int) view.GetMode());

            click.positionDip = { HeatMapView::kGutterDip + HeatMapView::kTabDip * 2 + 5, top };
            view.OnMouse (click);
            Assert::AreEqual ((int) Mode::Data, (int) view.GetMode());

            click.positionDip = { HeatMapView::kGutterDip + HeatMapView::kTabDip * 3 + 5, top };
            Assert::IsFalse  (view.OnMouse (click), L"past the last mode is no mode");
            Assert::AreEqual ((int) Mode::Data, (int) view.GetMode());
        }



        TEST_METHOD (TheReadoutSaysTheAddressUnderTheMouseAndWhatTouchedIt)
        {
            HeatMapView        view;
            DxuiMouseEvent     move;
            std::vector<Byte>  execute (0x10000, 0);
            std::vector<Byte>  write   (0x10000, 0);
            std::vector<Byte>  none;



            Place (view);
            execute[0xC65E] = 3;
            write[0xC65E]   = 9;
            view.SetLevels (execute, none, write);

            move.kind        = DxuiMouseEventKind::Move;
            move.positionDip = GetPointOf (0xC65E);
            view.OnMouse (move);
            Assert::AreEqual (std::wstring (L"$C65E  executed, written"), view.GetReadout());

            move.positionDip = GetPointOf (0x0300);
            view.OnMouse (move);
            Assert::AreEqual (std::wstring (L"$0300"), view.GetReadout());

            move.kind = DxuiMouseEventKind::Leave;
            view.OnMouse (move);
            Assert::AreEqual (std::wstring(), view.GetReadout());
        }
    };
}
