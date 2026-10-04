#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBarMeterClickTests
//
//  A field's meter, drawn at its right end and filled to its fraction in its
//  color, and a field's click action, run by a left press on it alone.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiStatusBarMeterClickTests)
{
public:

    static void  LayOut (DxuiStatusBar & bar)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        bar.Layout (RECT { 0, 0, 600, 24 }, scaler);
    }


    static DxuiMouseEvent  MakePress (int x, int y, DxuiMouseButton button = DxuiMouseButton::Left)
    {
        DxuiMouseEvent  ev;

        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = button;
        ev.positionDip = POINT { x, y };
        return ev;
    }


    TEST_METHOD (Meter_IsFilledToItsFractionInItsColor)
    {
        constexpr uint32_t  kFill = 0xFF112233u;

        DxuiStatusBar         bar;
        DxuiStatusBar::Field  meter;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        bool                  found = false;



        meter.widthDip      = 200;
        meter.meterWidthDip = 100;
        bar.SetFields ({ { L"left", 0, true }, meter });
        bar.SetMeter  (1, 0.25f, kFill);
        LayOut (bar);

        bar.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.argb == kFill)
            {
                found = true;
                Assert::AreEqual (25.0f,  call.width);
                Assert::AreEqual (492.0f, call.x);         // 600 less the 8 DIP pad less the 100 DIP meter
            }
        }

        Assert::IsTrue (found);
    }


    TEST_METHOD (Fill_GrowsFromTheLeftAsAGradientWithShadowedTextOverIt)
    {
        constexpr uint32_t  kFrom = 0xFF112233u;
        constexpr uint32_t  kTo   = 0xFF445566u;

        DxuiStatusBar         bar;
        DxuiStatusBar::Field  field;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        size_t                fills = 0;
        size_t                draws = 0;



        field.text     = L"History";
        field.widthDip = 200;
        bar.SetFields ({ { L"left", 0, true }, field });
        bar.SetFill   (1, 0.25f, kFrom, kTo);
        LayOut (bar);

        bar.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillHorizontalGradientRect)
            {
                fills++;
                Assert::AreEqual (400.0f, call.x);
                Assert::AreEqual (50.0f,  call.width);
                Assert::AreEqual (kFrom,  call.argb);
                Assert::AreEqual (kTo,    call.argbSecond);
            }
        }

        for (const RecordedTextCall & call : text.Calls())
        {
            draws += (call.text == L"History") ? 1 : 0;
        }

        Assert::AreEqual ((size_t) 1, fills);
        Assert::IsTrue (draws > 1, L"the text is drawn over its shadow");
    }


    TEST_METHOD (Fill_IsClampedAndRemovedWhenNegative)
    {
        DxuiStatusBar         bar;
        DxuiStatusBar::Field  field;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;



        field.widthDip = 200;
        bar.SetFields ({ field });

        bar.SetFill (0, 1.5f, 0, 0);
        Assert::AreEqual (1.0f, bar.GetField (0).fill);

        bar.SetFill (0, -0.5f, 0, 0);
        Assert::AreEqual (-1.0f, bar.GetField (0).fill);

        LayOut (bar);
        bar.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            Assert::IsTrue (call.kind != RecordedPaintKind::FillHorizontalGradientRect);
        }
    }

    TEST_METHOD (Meter_IsClampedAndHiddenWhenNegative)
    {
        DxuiStatusBar         bar;
        DxuiStatusBar::Field  meter;



        meter.widthDip      = 200;
        meter.meterWidthDip = 100;
        bar.SetFields ({ meter });

        bar.SetMeter (0, 1.5f, 0);
        Assert::AreEqual (1.0f, bar.GetField (0).meter);

        bar.SetMeter (0, -0.5f, 0);
        Assert::AreEqual (-1.0f, bar.GetField (0).meter);
    }


    TEST_METHOD (NoMeter_DrawsNoTrack)
    {
        DxuiStatusBar         bar;
        DxuiStatusBar::Field  field;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        size_t                outlines = 0;



        field.widthDip      = 200;
        field.meterWidthDip = 100;
        bar.SetFields ({ field });
        LayOut (bar);

        bar.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            outlines += (call.kind == RecordedPaintKind::OutlineRect) ? 1 : 0;
        }

        Assert::AreEqual ((size_t) 0, outlines);
    }


    TEST_METHOD (Press_RunsTheClickActionWithTheFieldRect)
    {
        DxuiStatusBar         bar;
        DxuiStatusBar::Field  zoom;
        RECT                  clicked = {};
        int                   clicks  = 0;
        bool                  handled = false;



        zoom.text     = L"100%";
        zoom.widthDip = 60;
        zoom.onClick  = [&] (const RECT & r) { clicked = r; clicks++; };
        bar.SetFields ({ { L"left", 0, true }, zoom });
        LayOut (bar);

        handled = bar.OnMouse (MakePress (570, 10));

        Assert::IsTrue (handled);
        Assert::AreEqual (1, clicks);
        Assert::AreEqual (540L, clicked.left);
        Assert::AreEqual (600L, clicked.right);
    }


    TEST_METHOD (Press_ElsewhereOrWithAnotherButton_RunsNothing)
    {
        DxuiStatusBar         bar;
        DxuiStatusBar::Field  zoom;
        int                   clicks  = 0;
        bool                  handled = false;



        zoom.widthDip = 60;
        zoom.onClick  = [&] (const RECT &) { clicks++; };
        bar.SetFields ({ { L"left", 0, true }, zoom });
        LayOut (bar);

        handled = bar.OnMouse (MakePress (100, 10));
        Assert::IsFalse (handled);

        bar.OnMouse (MakePress (570, 10, DxuiMouseButton::Right));
        Assert::AreEqual (0, clicks);
        Assert::AreEqual (-1, bar.FindFieldAt (POINT { 570, 40 }));
    }
};
