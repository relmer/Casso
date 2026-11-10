#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipSizeTests
//
//  A balloon is never smaller than its text. A popup without a renderer, as
//  in a test, sizes its balloon from an estimate; the mock renderer that draws
//  it measures the text at its canned metrics, so a test can
//  make the drawn text wider or taller than the balloon.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTooltipSizeTests)
{
public:

    //  The padding each side of a Visual Studio tip's text at 96 DPI.
    static constexpr LONG  s_kPadX = 9;
    static constexpr LONG  s_kPadY = 5;



    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }



    static SIZE GetPlacedSize (const DxuiTooltip & tip)
    {
        RECT  placed = tip.GetActivePopup()->GetPlacedRectScreenPx();



        return SIZE { placed.right - placed.left, placed.bottom - placed.top };
    }



    //  The text drawn measures wider and taller than the estimate the
    //  balloon was sized from, so the balloon is grown on the next tick.
    TEST_METHOD (ABalloonSmallerThanItsDrawnTextIsGrown)
    {
        DxuiHwndSource        host;
        DxuiTooltip           tip;
        DxuiDarkTheme         theme;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        SIZE                  before = {};
        SIZE                  after  = {};



        host.SetDpiForTest (96);
        tip.SetPopupHost   (&host);
        tip.SetTheme       (theme);
        tip.ApplyVisualStudioLook();
        tip.RequestShowNow (RECT { 100, 200, 101, 201 }, L"Memory 3", 0);
        Assert::IsNotNull  (tip.GetActivePopup());

        before = GetPlacedSize (tip);
        text.SetCannedMetrics (L"Memory 3", SIZE { 200, 40 });

        tip.GetActivePopup()->GetParams().renderContent (painter, text);
        Assert::IsTrue (tip.WantsTick(), L"WantsTick is true for a balloon too small for its text");

        tip.Tick (1);
        after = GetPlacedSize (tip);

        Assert::IsTrue   (before.cx < 200,               L"the estimate was narrower than the text");
        Assert::AreEqual (200 + 2 * s_kPadX, after.cx,   L"grown to the text and its padding");
        Assert::AreEqual (40  + 2 * s_kPadY, after.cy,   L"and as tall");
        Assert::IsFalse  (tip.WantsTick(),               L"and noted once");

        tip.HideImmediate();
    }



    //  A balloon as large as its text, or larger, is left as it was placed.
    TEST_METHOD (ABalloonLargeEnoughForItsTextIsNotResized)
    {
        DxuiHwndSource        host;
        DxuiTooltip           tip;
        DxuiDarkTheme         theme;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        SIZE                  before = {};



        host.SetDpiForTest (96);
        tip.SetPopupHost   (&host);
        tip.SetTheme       (theme);
        tip.ApplyVisualStudioLook();
        tip.RequestShowNow (RECT { 100, 200, 101, 201 }, L"Memory 3", 0);
        Assert::IsNotNull  (tip.GetActivePopup());

        before = GetPlacedSize (tip);
        text.SetCannedMetrics (L"Memory 3", SIZE { 10, 10 });

        tip.GetActivePopup()->GetParams().renderContent (painter, text);
        tip.Tick (1);

        Assert::AreEqual (before.cx, GetPlacedSize (tip).cx, L"as wide as it was placed");
        Assert::AreEqual (before.cy, GetPlacedSize (tip).cy, L"and as tall");

        tip.HideImmediate();
    }



    //  A look set for the next tip, as the tip service sets one before the
    //  tip's request, doesn't change how the tip that is up is drawn: its
    //  face and its lines' swatches are the ones it was raised with.
    TEST_METHOD (TheTipUpIsDrawnInTheLookItWasRaisedWith)
    {
        DxuiHwndSource        host;
        DxuiTooltip           tip;
        DxuiDarkTheme         theme;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;



        host.SetDpiForTest (96);
        tip.SetPopupHost   (&host);
        tip.SetTheme       (theme);
        tip.ApplyVisualStudioLook();
        tip.RequestShowNow (RECT { 100, 200, 101, 201 }, L"Memory 3", 0);
        Assert::IsNotNull  (tip.GetActivePopup());

        tip.SetMonospace    (true);
        tip.SetLineSwatches ({ 0xFFFF0000u });

        tip.GetActivePopup()->GetParams().renderContent (painter, text);

        Assert::IsFalse  (text.Calls().empty(),                                      L"the text is drawn");
        Assert::AreEqual (std::wstring (DxuiTheme::GetUiFace()), text.Calls().front().fontFamily, L"in the face it was raised with");
        Assert::AreEqual ((float) s_kPadX, text.Calls().front().x,                    L"with no swatch before it");

        tip.HideImmediate();
    }
};
