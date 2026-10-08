#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockGuideTests
//
//  Visual Studio's drop guides, measured at 125%: the small cross over a
//  tool window group, the large one over a document group, and the box at
//  each window edge. Their sizes, buttons, chamfers and pictures in pixels
//  at 100% and 125%, and the colors a rendered guide takes from the theme.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiDockGuideTests
{
    static constexpr UINT  s_kDpi100 = 96;
    static constexpr UINT  s_kDpi125 = 120;



    static DxuiDpiScaler MakeScaler (UINT dpi)
    {
        DxuiDpiScaler  scaler;



        scaler.SetDpi (dpi);
        return scaler;
    }



    static DxuiDockGuideColors MakeColors (const IDxuiTheme & theme)
    {
        DxuiDockGuideColors  colors;



        colors.border       = theme.DockGuideBorder();
        colors.fill         = theme.DockGuideFill();
        colors.buttonBorder = theme.DockGuideButtonBorder();
        colors.buttonFill   = theme.DockGuideButtonFill();
        colors.glyph        = theme.DockGuideGlyph();
        colors.arrow        = theme.DockGuideArrow();
        colors.hover        = theme.FocusAccent();
        return colors;
    }



    static uint32_t GetPixel (const DxuiIconImage & image, int x, int y)
    {
        return image.bgraPremul[(size_t) y * (size_t) image.width + (size_t) x];
    }



    static void CheckRect (const RECT & expected, const RECT & actual, const wchar_t * what)
    {
        Assert::AreEqual (expected.left,   actual.left,   what);
        Assert::AreEqual (expected.top,    actual.top,    what);
        Assert::AreEqual (expected.right,  actual.right,  what);
        Assert::AreEqual (expected.bottom, actual.bottom, what);
    }



    static void CheckPoint (float x, float y, const DxuiPointF & actual, const wchar_t * what = L"")
    {
        Assert::AreEqual (x, actual.x, 0.001f, what);
        Assert::AreEqual (y, actual.y, 0.001f, what);
    }



    TEST_CLASS (DxuiDockGuideTests)
    {
    public:

        TEST_METHOD (SizesFollowTheScale)
        {
            DxuiDpiScaler  at100 = MakeScaler (s_kDpi100);
            DxuiDpiScaler  at125 = MakeScaler (s_kDpi125);



            Assert::AreEqual (112L, DxuiDockGuide::GetSizePx (DxuiDockGuideKind::SmallCross, at100).cx);
            Assert::AreEqual (184L, DxuiDockGuide::GetSizePx (DxuiDockGuideKind::LargeCross, at100).cx);
            Assert::AreEqual (40L,  DxuiDockGuide::GetSizePx (DxuiDockGuideKind::Edge,       at100).cx);
            Assert::AreEqual (140L, DxuiDockGuide::GetSizePx (DxuiDockGuideKind::SmallCross, at125).cx);
            Assert::AreEqual (230L, DxuiDockGuide::GetSizePx (DxuiDockGuideKind::LargeCross, at125).cy);
            Assert::AreEqual (50L,  DxuiDockGuide::GetSizePx (DxuiDockGuideKind::Edge,       at125).cy);
        }


        TEST_METHOD (AGuideIsCenteredOnItsPoint)
        {
            DxuiDpiScaler  scaler = MakeScaler (s_kDpi125);
            POINT          origin = DxuiDockGuide::GetOrigin (DxuiDockGuideKind::SmallCross, POINT { 500, 300 }, scaler);
            RECT           button = DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::Edge, DxuiDockGuideButton::DockLeft, POINT { 7, 9 }, scaler);



            Assert::AreEqual (430L, origin.x);
            Assert::AreEqual (230L, origin.y);

            origin = DxuiDockGuide::GetOriginOfButton (DxuiDockGuideKind::Edge, DxuiDockGuideButton::DockLeft, button, scaler);
            Assert::AreEqual (7L, origin.x, L"the origin of a button's guide is where that button was placed from");
            Assert::AreEqual (9L, origin.y);
        }


        //  Buttons 32 DIP square, 36 DIP apart: 40 and 45 pixels at 125%.
        TEST_METHOD (ButtonsSitOnTheirPitch)
        {
            DxuiDpiScaler  at100 = MakeScaler (s_kDpi100);
            DxuiDpiScaler  at125 = MakeScaler (s_kDpi125);
            POINT          zero  = {};



            CheckRect (RECT {  50,  50,  90,  90 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center,      zero, at125), L"small center");
            CheckRect (RECT {  50,   5,  90,  45 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockTop,     zero, at125), L"small top");
            CheckRect (RECT {   5,  50,  45,  90 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockLeft,    zero, at125), L"small left");
            CheckRect (RECT {  95,  50, 135,  90 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockRight,   zero, at125), L"small right");
            CheckRect (RECT {  50,  95,  90, 135 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockBottom,  zero, at125), L"small bottom");
            CheckRect (RECT {  95,   5, 135,  45 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::DockTop,     zero, at125), L"large top");
            CheckRect (RECT {  95,  50, 135,  90 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitTop,    zero, at125), L"large split top");
            CheckRect (RECT {  95,  95, 135, 135 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::Center,      zero, at125), L"large center");
            CheckRect (RECT {  95, 140, 135, 180 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitBottom, zero, at125), L"large split bottom");
            CheckRect (RECT { 185,  95, 225, 135 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::DockRight,   zero, at125), L"large right");
            CheckRect (RECT {   5,   5,  45,  45 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::Edge,       DxuiDockGuideButton::DockBottom,  zero, at125), L"edge");
            CheckRect (RECT {  40,  40,  72,  72 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center,      zero, at100), L"small center at 100%");
            CheckRect (RECT {  76,  40, 108,  72 }, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockRight,   zero, at100), L"pitch 36 at 100%");
            CheckRect (RECT {}, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::SplitTop, zero, at125), L"a small cross has no split buttons");
            CheckRect (RECT {}, DxuiDockGuide::GetButtonRect (DxuiDockGuideKind::Edge,       DxuiDockGuideButton::Center,   zero, at125), L"an edge guide has only a dock button");
        }


        TEST_METHOD (EachKindHasItsButtons)
        {
            std::vector<DxuiDockGuideButton>  edge = DxuiDockGuide::GetButtons (DxuiDockGuideKind::Edge, DxuiDockSide::Right);



            Assert::AreEqual ((size_t) 5, DxuiDockGuide::GetButtons (DxuiDockGuideKind::SmallCross, DxuiDockSide::Left).size());
            Assert::AreEqual ((size_t) 9, DxuiDockGuide::GetButtons (DxuiDockGuideKind::LargeCross, DxuiDockSide::Left).size());
            Assert::AreEqual ((size_t) 1, edge.size());
            Assert::IsTrue   (edge[0] == DxuiDockGuideButton::DockRight, L"the right edge shows the dock-right picture");
        }


        //  Arms 40 DIP wide, each inside corner cut at 45 degrees: legs of 10
        //  DIP on the small cross and 9.25 on the large one.
        TEST_METHOD (TheCrossesChamferTheirInsideCorners)
        {
            DxuiDpiScaler            at100 = MakeScaler (s_kDpi100);
            DxuiDpiScaler            at125 = MakeScaler (s_kDpi125);
            std::vector<DxuiPointF>  cross = DxuiDockGuide::GetOutline (DxuiDockGuideKind::SmallCross, at100);
            std::vector<DxuiPointF>  wide  = DxuiDockGuide::GetOutline (DxuiDockGuideKind::LargeCross, at125);
            std::vector<DxuiPointF>  edge  = DxuiDockGuide::GetOutline (DxuiDockGuideKind::Edge,       at125);



            Assert::AreEqual ((size_t) 16, cross.size());
            CheckPoint ( 36.0f,   0.0f, cross[0],  L"the top arm's left end");
            CheckPoint ( 76.0f,   0.0f, cross[1],  L"its right end");
            CheckPoint ( 76.0f,  25.3f, cross[2],  L"the chamfer starts 10.7 DIP above the right arm");
            CheckPoint ( 86.7f,  36.0f, cross[3],  L"and ends 10.7 DIP right of the top arm");
            CheckPoint (112.0f,  36.0f, cross[4],  L"the right arm's end");
            CheckPoint ( 25.3f,  76.0f, cross[11], L"the bottom left chamfer");

            Assert::AreEqual ((size_t) 16, wide.size());
            CheckPoint ( 90.0f,    0.0f,    wide[0], L"72 DIP at 125%");
            CheckPoint (140.0f,    0.0f,    wide[1]);
            CheckPoint (140.0f,   77.25f,   wide[2], L"61.8 DIP");
            CheckPoint (152.75f,  90.0f,    wide[3], L"122.2 DIP");
            CheckPoint (230.0f,   90.0f,    wide[4]);

            for (size_t i = 0; i < wide.size(); i++)
            {
                const DxuiPointF  & a  = wide[i];
                const DxuiPointF  & b  = wide[(i + 1) % wide.size()];
                float               dx = std::fabs (b.x - a.x);
                float               dy = std::fabs (b.y - a.y);

                Assert::IsTrue (dx == 0.0f || dy == 0.0f || std::fabs (dx - dy) < 0.001f, L"every edge is straight or at 45 degrees");
            }

            Assert::AreEqual ((size_t) 4, edge.size(), L"an edge guide is a plain box");
            CheckPoint (50.0f, 50.0f, edge[2]);
        }


        TEST_METHOD (APointIsInsideOnlyWithinTheOutline)
        {
            DxuiDpiScaler  scaler = MakeScaler (s_kDpi125);
            POINT          origin = { 100, 200 };



            Assert::IsTrue  (DxuiDockGuide::IsInside (DxuiDockGuideKind::SmallCross, origin, POINT { 170, 270 }, scaler), L"the middle");
            Assert::IsTrue  (DxuiDockGuide::IsInside (DxuiDockGuideKind::SmallCross, origin, POINT { 200, 238 }, scaler), L"just inside a chamfer");
            Assert::IsFalse (DxuiDockGuide::IsInside (DxuiDockGuideKind::SmallCross, origin, POINT { 204, 236 }, scaler), L"just outside it");
            Assert::IsFalse (DxuiDockGuide::IsInside (DxuiDockGuideKind::SmallCross, origin, POINT { 105, 205 }, scaler), L"between two arms");
            Assert::IsTrue  (DxuiDockGuide::IsInside (DxuiDockGuideKind::Edge,       origin, POINT { 101, 201 }, scaler), L"an edge box is solid");
            Assert::IsFalse (DxuiDockGuide::IsInside (DxuiDockGuideKind::Edge,       origin, POINT { 150, 201 }, scaler));
        }


        //  A picture 24 DIP square, or the half of it on the dock button's
        //  side, 4 DIP inside its button, with a 3-DIP title band.
        TEST_METHOD (GlyphsSitInsideTheirButtons)
        {
            DxuiDpiScaler  at100 = MakeScaler (s_kDpi100);
            DxuiDpiScaler  at125 = MakeScaler (s_kDpi125);



            CheckRect (RECT {  55,  55,  85,  85 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center,     at125), L"center");
            CheckRect (RECT {  55,  10,  85,  25 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockTop,    at125), L"top half");
            CheckRect (RECT {  55, 115,  85, 130 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockBottom, at125), L"bottom half");
            CheckRect (RECT {  10,  55,  25,  85 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockLeft,   at125), L"left half");
            CheckRect (RECT { 115,  55, 130,  85 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockRight,  at125), L"right half");
            CheckRect (RECT { 100,  55, 130,  85 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitTop,   at125), L"split top");
            CheckRect (RECT {  44,  44,  68,  68 }, DxuiDockGuide::GetGlyphRect (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::Center,     at100), L"center at 100%");

            Assert::AreEqual (3.0f,  DxuiDockGuide::GetBandPx (at100), 0.001f, L"the title band is 3 DIP deep");
            Assert::AreEqual (3.75f, DxuiDockGuide::GetBandPx (at125), 0.001f);
        }


        //  Base 8 DIP, height 4, the apex toward the picture; each corner on
        //  a whole pixel.
        TEST_METHOD (ArrowsPointAtTheirPictures)
        {
            DxuiDpiScaler            at100  = MakeScaler (s_kDpi100);
            DxuiDpiScaler            at125  = MakeScaler (s_kDpi125);
            std::vector<DxuiPointF>  top    = DxuiDockGuide::GetArrow (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockTop,    at125);
            std::vector<DxuiPointF>  bottom = DxuiDockGuide::GetArrow (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockBottom, at125);
            std::vector<DxuiPointF>  left   = DxuiDockGuide::GetArrow (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockLeft,   at125);
            std::vector<DxuiPointF>  right  = DxuiDockGuide::GetArrow (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockRight,  at125);
            std::vector<DxuiPointF>  plain  = DxuiDockGuide::GetArrow (DxuiDockGuideKind::SmallCross, DxuiDockGuideButton::DockTop,    at100);



            Assert::AreEqual ((size_t) 3, top.size());
            CheckPoint ( 70.0f,  33.0f, top[0],    L"top apex: 26 DIP rounds to 33 px");
            CheckPoint ( 65.0f,  38.0f, top[1],    L"top base");
            CheckPoint ( 75.0f,  38.0f, top[2]);
            CheckPoint ( 70.0f, 108.0f, bottom[0], L"bottom apex");
            CheckPoint ( 65.0f, 103.0f, bottom[1], L"bottom base");
            CheckPoint ( 33.0f,  70.0f, left[0],   L"left apex");
            CheckPoint ( 38.0f,  65.0f, left[1],   L"left base");
            CheckPoint (108.0f,  70.0f, right[0],  L"right apex");
            CheckPoint (103.0f,  75.0f, right[2],  L"right base");
            CheckPoint ( 56.0f,  26.0f, plain[0],  L"apex at 100%");
            CheckPoint ( 60.0f,  30.0f, plain[2],  L"base at 100%");

            Assert::IsTrue (DxuiDockGuide::GetArrow (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitTop, at125).empty(), L"a split button has no arrow");
        }


        //  One-DIP dots every 2 DIP across the picture, just above or left of
        //  its middle for a top or left split, just below or right for the
        //  others.
        TEST_METHOD (SplitDotsMarkTheSplit)
        {
            DxuiDpiScaler                  scaler = MakeScaler (s_kDpi125);
            std::vector<DxuiCoverageRect>  top    = DxuiDockGuide::GetDots (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitTop,    scaler);
            std::vector<DxuiCoverageRect>  bottom = DxuiDockGuide::GetDots (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitBottom, scaler);
            std::vector<DxuiCoverageRect>  left   = DxuiDockGuide::GetDots (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitLeft,   scaler);
            std::vector<DxuiCoverageRect>  right  = DxuiDockGuide::GetDots (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::SplitRight,  scaler);



            Assert::AreEqual ((size_t) 12, top.size(), L"a dot every 2 DIP across 24");
            Assert::AreEqual (100.0f,  top[0].left,    0.001f);
            Assert::AreEqual (101.25f, top[0].right,   0.001f, L"1 DIP wide");
            Assert::AreEqual (102.5f,  top[1].left,    0.001f, L"2 DIP apart");
            Assert::AreEqual (68.75f,  top[0].top,     0.001f, L"glyph row 11");
            Assert::AreEqual (70.0f,   top[0].bottom,  0.001f);
            Assert::AreEqual (160.0f,  bottom[0].top,  0.001f, L"glyph row 12 of the split bottom picture");
            Assert::AreEqual (68.75f,  left[0].left,   0.001f, L"glyph column 11");
            Assert::AreEqual (100.0f,  left[0].top,    0.001f);
            Assert::AreEqual (160.0f,  right[0].left,  0.001f, L"glyph column 12");
            Assert::AreEqual (102.5f,  right[1].top,   0.001f);

            Assert::IsTrue (DxuiDockGuide::GetDots (DxuiDockGuideKind::LargeCross, DxuiDockGuideButton::Center, scaler).empty());
        }


        TEST_METHOD (ARenderedCrossTakesTheThemesColors)
        {
            DxuiDarkTheme        theme;
            DxuiDpiScaler        scaler = MakeScaler (s_kDpi125);
            DxuiDockGuideColors  colors = MakeColors (theme);
            DxuiIconImage        image  = DxuiDockGuide::Render (DxuiDockGuideKind::SmallCross, DxuiDockSide::Left, -1, colors, scaler);



            Assert::AreEqual (140, image.width);
            Assert::AreEqual (140, image.height);
            Assert::AreEqual (theme.DockGuideButtonFill(),   GetPixel (image, 52, 70),       L"deep inside the center button");
            Assert::AreEqual (theme.DockGuideGlyph(),        GetPixel (image, 55, 70),       L"the center picture's frame");
            Assert::AreEqual (theme.DockGuideGlyph(),        GetPixel (image, 70, 57),       L"its title band");
            Assert::AreEqual (theme.DockGuideButtonFill(),   GetPixel (image, 70, 70),       L"the picture is empty inside");
            Assert::AreEqual (theme.DockGuideArrow(),        GetPixel (image, 70, 37),       L"the top button's arrow");
            Assert::AreEqual (0u,                            GetPixel (image,  5,  5),       L"outside the outline");
            Assert::AreEqual (theme.DockGuideBorder(),       GetPixel (image, 70,  0),       L"the border at the top arm's end");
            Assert::AreEqual (theme.DockGuideButtonBorder(), GetPixel (image, 70,  5),       L"the top button's border");
            Assert::AreEqual (0x99u,                         GetPixel (image, 47, 20) >> 24, L"the translucent fill between the border and a button");
        }


        //  A button's corners are rounded 3 DIP, 3.75 pixels at 125%: the
        //  corner pixel itself is clear of the button, and the next one along
        //  the top edge is partly covered.
        TEST_METHOD (AButtonsCornersAreRounded)
        {
            DxuiDarkTheme  theme;
            DxuiDpiScaler  scaler = MakeScaler (s_kDpi125);
            DxuiIconImage  image  = DxuiDockGuide::Render (DxuiDockGuideKind::SmallCross, DxuiDockSide::Left, -1, MakeColors (theme), scaler);
            uint32_t       fill   = GetPixel (image, 47, 50);



            Assert::AreEqual    (fill, GetPixel (image, 50, 50), L"the center button's top left pixel is cut away");
            Assert::AreNotEqual (fill, GetPixel (image, 51, 50), L"the next one is partly the button's");
            Assert::AreNotEqual (theme.DockGuideButtonBorder(), GetPixel (image, 51, 50), L"but only partly");
            Assert::AreEqual    (theme.DockGuideButtonBorder(), GetPixel (image, 54, 50), L"past the corner the border is solid");
        }


        TEST_METHOD (TheHoveredButtonsBorderTakesTheFocusAccent)
        {
            DxuiDarkTheme        theme;
            DxuiDpiScaler        scaler = MakeScaler (s_kDpi125);
            DxuiDockGuideColors  colors = MakeColors (theme);
            DxuiIconImage        lit    = DxuiDockGuide::Render (DxuiDockGuideKind::SmallCross, DxuiDockSide::Left, (int) DxuiDockGuideButton::DockTop, colors, scaler);



            Assert::AreEqual (theme.FocusAccent() | 0xFF000000u, GetPixel (lit, 70,  5), L"the hovered button's border");
            Assert::AreEqual (theme.DockGuideButtonBorder(),     GetPixel (lit, 70, 50), L"the others keep theirs");
        }


        //  Visual Studio's light buttons have no border to see.
        TEST_METHOD (ALightButtonShowsNoBorder)
        {
            DxuiLightTheme  theme;
            DxuiDpiScaler   scaler = MakeScaler (s_kDpi125);
            DxuiIconImage   image  = DxuiDockGuide::Render (DxuiDockGuideKind::Edge, DxuiDockSide::Top, -1, MakeColors (theme), scaler);



            Assert::AreEqual (50, image.width);
            Assert::AreEqual (theme.DockGuideButtonFill(), GetPixel (image, 25, 5), L"the button's edge is its fill");
            Assert::AreEqual (theme.DockGuideGlyph(),      GetPixel (image, 10, 10), L"the dock-top picture's corner");
            Assert::AreEqual (theme.DockGuideBorder(),     GetPixel (image, 25, 0), L"the box's border");
        }
    };
}
