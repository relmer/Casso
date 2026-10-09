#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipFrameTests
//
//  A tip's frame. By default it is a flyout's, as File Explorer's tips are.
//  Visual Studio's look, which the emulator and the debugger give their
//  tips, has 6.4-DIP corners, a border one whole line wide with the padding
//  inside it, and a small shadow. Measured at 125%: corners of 8 pixels, a
//  1-pixel border, the text 11 pixels in, and a one-line tip 32 pixels tall.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTooltipFrameTests)
{
public:

    static constexpr int  kDpis[] = { 96, 120, 144 };



    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }



    static DxuiDpiScaler GetScaler (int dpi)
    {
        DxuiDpiScaler  scaler;



        scaler.SetDpi ((UINT) dpi);
        return scaler;
    }



    //  The default frame scales the DIPs as they fall: a one-DIP border
    //  inside the 8- and 4-DIP padding, the overlay radius, and a menu's
    //  shadow.
    TEST_METHOD (TheDefaultFrameIsAFlyouts)
    {
        for (int dpi : kDpis)
        {
            DxuiTooltip         tip;
            DxuiDpiScaler       scaler = GetScaler (dpi);
            DxuiShadow::Style   shadow;
            std::wstring        at     = std::format (L"at {} DPI", dpi);



            tip.SetDpi ((UINT) dpi);
            shadow = tip.GetShadow();

            Assert::AreEqual (scaler.ToPxf (1.0f),                                tip.GetBorderPx(),       at.c_str());
            Assert::AreEqual (scaler.ToPxf (8.0f),                                tip.GetPadXPx(),         at.c_str());
            Assert::AreEqual (scaler.ToPxf (4.0f),                                tip.GetPadYPx(),         at.c_str());
            Assert::AreEqual (scaler.ToPxf (DxuiTheme::kOverlayCornerRadiusDip), tip.GetCornerRadiusPx(), at.c_str());
            Assert::AreEqual (DxuiShadow::kBlurDip,                               shadow.blurDip,          at.c_str());
            Assert::AreEqual (DxuiShadow::kOffsetYDip,                            shadow.offsetYDip,       at.c_str());
            Assert::AreEqual (0.0f,                                               shadow.insetDip,         at.c_str());
            Assert::AreEqual (DxuiShadow::kOpacity,                               shadow.opacity,          at.c_str());
        }
    }



    //  Visual Studio's frame at 100, 125 and 150%: the border is a pane's
    //  outline, 1, 1 and 2 pixels; the padding is measured inside it in
    //  whole pixels; and the corners are 6.4 DIP rounded to a pixel.
    TEST_METHOD (VisualStudiosFrameAtEachScale)
    {
        struct Expected
        {
            int    dpi;
            float  border;
            float  padX;
            float  padY;
            float  radius;
        };

        constexpr Expected  kExpected[] =
        {
            {  96, 1.0f,  9.0f, 5.0f,  6.0f },
            { 120, 1.0f, 11.0f, 6.0f,  8.0f },
            { 144, 2.0f, 14.0f, 8.0f, 10.0f },
        };



        for (const Expected & expected : kExpected)
        {
            DxuiTooltip   tip;
            std::wstring  at  = std::format (L"at {} DPI", expected.dpi);



            tip.ApplyVisualStudioLook();
            tip.SetDpi ((UINT) expected.dpi);

            Assert::AreEqual (expected.border, tip.GetBorderPx(),       (L"the border " + at).c_str());
            Assert::AreEqual (expected.padX,   tip.GetPadXPx(),         (L"the text's left " + at).c_str());
            Assert::AreEqual (expected.padY,   tip.GetPadYPx(),         (L"the text's top " + at).c_str());
            Assert::AreEqual (expected.radius, tip.GetCornerRadiusPx(), (L"the corners " + at).c_str());
        }
    }



    //  Visual Studio's shadow reaches 4 DIP past a card 1 DIP inside the
    //  tip, dropped 2 DIP, at 14% black on a dark tip and about half that on
    //  a light one, so the window holds it in 8 DIP rather than a menu's 22.
    TEST_METHOD (VisualStudiosShadowIsSmallAndLighterOnALightTip)
    {
        DxuiTooltip         tip;
        DxuiDarkTheme       dark;
        DxuiLightTheme      light;
        DxuiShadow::Style   shadow;



        tip.ApplyVisualStudioLook();
        tip.SetTheme (dark);
        shadow = tip.GetShadow();

        Assert::AreEqual (4.0f,  shadow.blurDip,    L"its reach");
        Assert::AreEqual (2.0f,  shadow.offsetYDip, L"its drop");
        Assert::AreEqual (1.0f,  shadow.insetDip,   L"how far inside the tip it starts");
        Assert::AreEqual (0.14f, shadow.opacity,    L"its darkness on a dark tip");
        Assert::AreEqual (8.0f,  DxuiShadow::GetMarginDip (shadow), L"the room it needs");
        Assert::AreEqual (DxuiShadow::kMarginDip, DxuiShadow::GetMarginDip (DxuiShadow::Style()), L"a menu's needs its usual room");

        tip.SetTheme (light);

        Assert::AreEqual (0.075f, tip.GetShadow().opacity, L"lighter on a light tip");
        Assert::AreEqual (4.0f,   tip.GetShadow().blurDip, L"with the same reach");
    }



    //  The shadow is a stack of twelve layers; at 150% Visual Studio's
    //  reaches 6 pixels past a card 1.5 pixels inside the tip, all of it 3
    //  pixels down, each layer's alpha accumulating to 14%.
    TEST_METHOD (TheShadowsLayersStartInsideTheCard)
    {
        constexpr float     kScale  = 1.5f;
        constexpr float     kLeft   = 100.0f;
        constexpr float     kTop    = 50.0f;
        constexpr float     kWidth  = 110.0f;
        constexpr float     kHeight = 41.0f;
        constexpr float     kRadius = 10.0f;

        MockDxuiPainter     painter;
        DxuiShadow::Style   style   = { 4.0f, 2.0f, 1.0f, 0.14f };
        float               reach   = 6.0f * (1.0f - sqrtf (1.0f / 12.0f)) - 1.5f;
        uint32_t            alpha   = (uint32_t) ((1.0f - powf (1.0f - 0.14f, 1.0f / 12.0f)) * 255.0f + 0.5f);



        DxuiShadow::Paint (painter, kLeft, kTop, kWidth, kHeight, kRadius, kScale, 0, style);

        Assert::AreEqual ((size_t) 12, painter.Calls().size(), L"twelve layers");
        Assert::AreEqual (kLeft - reach,       painter.Calls().front().x, 0.001f, L"the outermost reaches past the card");
        Assert::AreEqual (kTop  - reach + 3.0f, painter.Calls().front().y, 0.001f, L"dropped 3 pixels");
        Assert::AreEqual (kLeft + 1.5f,        painter.Calls().back().x,  0.001f, L"the innermost is 1.5 pixels inside the card");
        Assert::AreEqual (kWidth - 3.0f,       painter.Calls().back().width, 0.001f);
        Assert::AreEqual (kRadius - 1.5f,      painter.Calls().back().radius, 0.001f, L"its corners follow it in");
        Assert::AreEqual (alpha << 24,         painter.Calls().back().argb, L"black, each layer a share of 14%");
    }



    //  A Visual Studio tip is sized to the pixel: the text, rounded up, and
    //  the padding inside the border on each side. Here the text is the
    //  estimate a popup without a renderer gives for 8 characters: 60 by 17
    //  pixels of a 12-pixel face at 96 DPI, 75 by 21 of a 15-pixel face at
    //  120. At 120 DPI whole DIPs could not give the box's 97 by 33 pixels.
    TEST_METHOD (AVisualStudioTipIsSizedToThePixel)
    {
        struct Expected
        {
            UINT   dpi;
            LONG   width;
            LONG   height;
            float  radius;
        };

        constexpr Expected  kExpected[] =
        {
            {  96, 60 + 2 *  9, 17 + 2 * 5, 6.0f },
            { 120, 75 + 2 * 11, 21 + 2 * 6, 8.0f },
        };



        for (const Expected & expected : kExpected)
        {
            DxuiHwndSource     host;
            DxuiTooltip        tip;
            DxuiDarkTheme      theme;
            DxuiPopupHost    * popup  = nullptr;
            RECT               placed = {};
            std::wstring       at     = std::format (L"at {} DPI", expected.dpi);



            host.SetDpiForTest (expected.dpi);

            tip.SetPopupHost   (&host);
            tip.SetTheme       (theme);
            tip.ApplyVisualStudioLook();
            tip.RequestShowNow (RECT { 100, 200, 101, 201 }, L"Memory 3", 0);

            popup = tip.GetActivePopup();
            Assert::IsNotNull (popup, at.c_str());

            placed = popup->GetPlacedRectScreenPx();

            Assert::AreEqual (expected.width,  popup->GetParams().sizePx.cx, (L"the text and the padding each side " + at).c_str());
            Assert::AreEqual (expected.height, popup->GetParams().sizePx.cy, (L"the line and the padding above and below " + at).c_str());
            Assert::AreEqual (expected.width,  placed.right - placed.left,   (L"the card is that wide " + at).c_str());
            Assert::AreEqual (expected.height, placed.bottom - placed.top,   (L"and that tall " + at).c_str());
            Assert::AreEqual (expected.radius, popup->GetParams().cornerRadiusPx, (L"its card is rounded to the tip's corners " + at).c_str());
            Assert::AreEqual (4.0f,            popup->GetParams().shadowStyle.blurDip, (L"under the tip's own shadow " + at).c_str());
            Assert::AreEqual (theme.TooltipBackground(), popup->GetParams().backgroundArgb, (L"in the theme's fill " + at).c_str());

            tip.HideImmediate();
        }
    }



    //  The border is drawn at the card's edge, one pixel wide, with the
    //  tip's corners, and the text starts inside it: 9 pixels in and 5 down
    //  at 96 DPI, where a flyout's frame starts it 8 and 4.
    TEST_METHOD (TheBorderIsOnTheEdgeAndTheTextInsideIt)
    {
        DxuiHwndSource             host;
        DxuiTooltip                tip;
        DxuiDarkTheme              theme;
        MockDxuiPainter            painter;
        MockDxuiTextRenderer       text;
        const RecordedPaintCall  * outline = nullptr;



        tip.SetPopupHost   (&host);
        tip.SetTheme       (theme);
        tip.ApplyVisualStudioLook();
        tip.RequestShowNow (RECT { 100, 200, 101, 201 }, L"Memory 3", 0);

        Assert::IsNotNull (tip.GetActivePopup());

        tip.GetActivePopup()->GetParams().renderContent (painter, text);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            outline = (call.kind == RecordedPaintKind::OutlineRoundedRect) ? &call : outline;
        }

        Assert::IsNotNull (outline, L"a border is drawn");
        Assert::AreEqual  (0.0f,                  outline->x,         L"at the card's left");
        Assert::AreEqual  (0.0f,                  outline->y,         L"and top");
        Assert::AreEqual  (78.0f,                 outline->width,     L"around the whole card");
        Assert::AreEqual  (1.0f,                  outline->thickness, L"one pixel wide");
        Assert::AreEqual  (6.0f,                  outline->radius,    L"with the tip's corners");
        Assert::AreEqual  (theme.TooltipBorder(), outline->argb,      L"in the theme's border color");
        Assert::IsFalse   (text.Calls().empty(),  L"the text is drawn");
        Assert::AreEqual  (9.0f,                  text.Calls().front().x, L"inside the border and the 8-pixel padding");
        Assert::AreEqual  (5.0f,                  text.Calls().front().y, L"and the 4-pixel padding");

        tip.HideImmediate();
    }



    //  A popup takes a card size in pixels as it is given, where one in DIPs
    //  is scaled, and MoveToPx resizes the card to the pixel too; MoveTo in
    //  DIPs goes back to scaling.
    TEST_METHOD (APopupTakesACardSizeInPixelsAsGiven)
    {
        constexpr RECT              kAnchor = { 100, 100, 101, 101 };
        DxuiPopupHost               popup;
        DxuiPopupHost::ShowParams   params;
        HRESULT                     hr      = S_OK;
        RECT                        placed  = {};



        popup.InitializeForTest();

        params.anchorRectScreen = kAnchor;
        params.dismiss          = DxuiPopupDismiss::Manual;
        params.sizeDip          = SIZE { 30, 20 };
        params.sizePx           = SIZE { 33, 27 };

        hr = popup.Show (std::move (params));
        Assert::IsTrue (SUCCEEDED (hr));

        placed = popup.GetPlacedRectScreenPx();
        Assert::AreEqual (33L, placed.right - placed.left, L"the pixel width as given");
        Assert::AreEqual (27L, placed.bottom - placed.top, L"and the pixel height");

        hr = popup.MoveToPx (kAnchor, SIZE { 40, 21 });
        Assert::IsTrue (SUCCEEDED (hr));

        placed = popup.GetPlacedRectScreenPx();
        Assert::AreEqual (40L, placed.right - placed.left, L"moved to a new pixel size");
        Assert::AreEqual (21L, placed.bottom - placed.top);

        hr = popup.MoveTo (kAnchor, SIZE { 30, 20 });
        Assert::IsTrue (SUCCEEDED (hr));

        placed = popup.GetPlacedRectScreenPx();
        Assert::AreEqual (30L, placed.right - placed.left, L"a size in DIPs is scaled again, at 96 DPI one to one");
        Assert::AreEqual (20L, placed.bottom - placed.top);

        popup.Close();
    }
};
