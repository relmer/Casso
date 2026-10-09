#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewGutterTests
//
//  A gutter ahead of the text, for each row's icon: the text starts past it,
//  so a point maps to the character drawn under it and a line wraps short of
//  the right edge by the gutter's width. Cells are 8 by 16 pixels at 96 DPI,
//  inside a 6-pixel pad.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextViewGutterTests)
{
public:

    static void  LayOut (DxuiTextView & view, LONG width, LONG height)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSize (8, 16);
        view.Layout (RECT { 0, 0, width, height }, scaler);
    }


    TEST_METHOD (TheTextStartsPastTheGutter)
    {
        DxuiTextView       view;
        DxuiTextView::Row  row;



        row.cells = { L"abcdef" };
        view.SetGutter (24, 16);
        LayOut (view, 400, 400);
        view.SetRows ({ row });

        Assert::AreEqual (0, view.HitTest (POINT { 6 + 24, 6 + 4 }).offset);
        Assert::AreEqual (2, view.HitTest (POINT { 6 + 24 + 2 * 8, 6 + 4 }).offset);
    }


    TEST_METHOD (ALineWrapsShortOfTheGutter)
    {
        DxuiTextView       view;
        DxuiTextView::Row  row;



        //  Sixteen columns of width, less three for the gutter, leave thirteen.
        row.cells = { L"aaaaaa bbbbbb cc" };
        view.SetGutter (24, 16);
        LayOut (view, 12 + 16 * 8, 400);
        view.SetRows ({ row });

        Assert::AreEqual (2, view.GetLineCount());
    }


    //  Paints one row with an icon in a 17-DIP gutter, with or without a
    //  glyph margin, into `text`.
    static void  PaintIconRow (MockDxuiTextRenderer & text, int dpi, float glyphCenterDip)
    {
        constexpr LONG     kLeft = 20;
        DxuiDpiScaler      scaler;
        DxuiTextView       view;
        DxuiTextView::Row  row;
        MockDxuiPainter    painter;
        MockDxuiTheme      theme;
        auto               image = std::make_shared<DxuiIconImage>();



        scaler.SetDpi (dpi);

        image->width  = 4;
        image->height = 4;
        image->bgraPremul.assign (16, 0xFF0000FFu);

        row.cells = { L"LDA #$41" };
        row.icon  = image;

        view.SetGutter         (17, 16);
        view.SetGlyphCenterDip (glyphCenterDip);
        view.SetCellSize       (8, 16);
        view.Layout            (RECT { kLeft, 0, kLeft + 400, 400 }, scaler);
        view.SetRows           ({ row });
        view.Paint             (painter, text, theme);
    }


    //  A glyph margin centers each row's icon on a set line from the view's
    //  left, as Visual Studio centers a breakpoint 8.4 DIP in: 8.4, 10.5 and
    //  12.6 pixels at 100, 125 and 150%. Without one the icon sits in the
    //  middle of the gutter, after the view's pad.
    TEST_METHOD (AGlyphMarginCentersTheIconOnItsLine)
    {
        constexpr LONG   kLeft        = 20;
        constexpr float  kCenterDip   = 8.4f;
        constexpr int    kPadDip      = 6;
        constexpr int    kGlyphDpis[] = { 96, 120, 144 };



        for (int dpi : kGlyphDpis)
        {
            DxuiDpiScaler         scaler;
            MockDxuiTextRenderer  margin;
            MockDxuiTextRenderer  plain;
            std::wstring          at = std::format (L"at {} DPI", dpi);



            scaler.SetDpi (dpi);

            PaintIconRow (margin, dpi, kCenterDip);
            PaintIconRow (plain,  dpi, 0.0f);

            Assert::AreEqual ((size_t) 1, margin.IconCalls().size(), at.c_str());
            Assert::AreEqual ((size_t) 1, plain.IconCalls().size(),  at.c_str());

            Assert::AreEqual ((float) kLeft + scaler.ToPxf (kCenterDip),
                              margin.IconCalls()[0].x + margin.IconCalls()[0].width * 0.5f, 0.001f,
                              (L"centered on the line " + at).c_str());
            Assert::AreEqual ((float) (kLeft + scaler.ToPx (kPadDip)) + (float) scaler.ToPx (17) * 0.5f,
                              plain.IconCalls()[0].x + plain.IconCalls()[0].width * 0.5f, 0.001f,
                              (L"in the middle of the gutter without a margin " + at).c_str());
        }
    }


    //  The view fills itself with the text view color, which a light theme
    //  can make whiter than its lists.
    TEST_METHOD (TheViewFillsInTheTextViewColor)
    {
        DxuiTheme             theme = DxuiTheme::Light();
        DxuiDpiScaler         scaler;
        DxuiTextView          view;
        DxuiTextView::Row     row;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;



        theme.contentBg  = 0xFFF9F9F9;
        theme.textViewBg = 0xFFFFFFFF;
        row.cells        = { L"abc" };

        view.SetCellSize (8, 16);
        view.Layout      (RECT { 0, 0, 200, 100 }, scaler);
        view.SetRows     ({ row });
        view.Paint       (painter, text, theme);

        Assert::IsFalse  (painter.Calls().empty());
        Assert::AreEqual (0xFFFFFFFFu, painter.Calls().front().argb, L"the background is the text view's, not the content color");
        Assert::AreEqual (200.0f,      painter.Calls().front().width, L"across the view");
    }
};
