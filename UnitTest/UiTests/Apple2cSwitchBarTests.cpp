#include "Pch.h"

#include "Ui/Chrome/Apple2cSwitchBar.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cSwitchBarTests
//
//  Pure-logic coverage for the //c case-switch strip: part layout order and
//  hit-testing, latching-state accessors, visibility (Hide) gating, per-part
//  tooltips, and the paint contract — the strip fills its band and a
//  pushed-in latching switch paints a visibly different key than an out one
//  (the "clicked down and staying" clue the feature requires).
//
////////////////////////////////////////////////////////////////////////////////

static RECT s_kBand = { 0, 500, 900, 540 };   // a full-width, 40px-tall band


static Apple2cSwitchBar MakeLaidOutBar (MockDxuiTextRenderer & text)
{
    Apple2cSwitchBar  bar;
    DxuiDpiScaler     scaler;

    scaler.SetDpi (96);
    bar.SetTextRenderer (&text);
    bar.Layout (s_kBand, scaler);
    return bar;
}


TEST_CLASS (Apple2cSwitchBarTests)
{
public:

    TEST_METHOD (Layout_FillsBandRect)
    {
        MockDxuiTextRenderer  text;
        Apple2cSwitchBar      bar = MakeLaidOutBar (text);

        RECT  b = bar.Bounds();

        Assert::AreEqual (s_kBand.left,   b.left,   L"bar spans the band left");
        Assert::AreEqual (s_kBand.right,  b.right,  L"bar spans the band right");
        Assert::AreEqual (s_kBand.top,    b.top,    L"bar spans the band top");
        Assert::AreEqual (s_kBand.bottom, b.bottom, L"bar spans the band bottom");
    }

    TEST_METHOD (PartAt_OrdersResetThenSwitches_WithAGap)
    {
        MockDxuiTextRenderer  text;
        Apple2cSwitchBar      bar = MakeLaidOutBar (text);

        int   midY        = (s_kBand.top + s_kBand.bottom) / 2;
        int   firstReset  = -1;
        int   firstEighty = -1;
        int   firstKbd    = -1;
        bool  sawGap      = false;

        for (int x = s_kBand.left; x < s_kBand.right; ++x)
        {
            switch (bar.PartAt (x, midY))
            {
                case Apple2cSwitchBar::Part::Reset:
                    if (firstReset < 0) { firstReset = x; }
                    break;
                case Apple2cSwitchBar::Part::EightyForty:
                    if (firstEighty < 0) { firstEighty = x; }
                    break;
                case Apple2cSwitchBar::Part::Keyboard:
                    if (firstKbd < 0) { firstKbd = x; }
                    break;
                default:
                    sawGap = true;
                    break;
            }
        }

        Assert::IsTrue (firstReset >= 0,  L"reset button is hit-testable");
        Assert::IsTrue (firstEighty >= 0, L"80/40 switch is hit-testable");
        Assert::IsTrue (firstKbd >= 0,    L"keyboard switch is hit-testable");
        Assert::IsTrue (firstReset < firstEighty && firstEighty < firstKbd,
            L"left group order: reset, then 80/40, then keyboard");
        Assert::IsTrue (sawGap,
            L"a neutral gap separates the switches from the right-edge indicators");
    }

    TEST_METHOD (Hide_ClearsBoundsAndHitRegions)
    {
        MockDxuiTextRenderer  text;
        Apple2cSwitchBar      bar = MakeLaidOutBar (text);

        bar.Hide();

        Assert::IsTrue (bar.Bounds().right <= bar.Bounds().left, L"hidden bar has empty bounds");

        int  midY = (s_kBand.top + s_kBand.bottom) / 2;
        for (int x = s_kBand.left; x < s_kBand.right; ++x)
        {
            Assert::IsTrue (bar.PartAt (x, midY) == Apple2cSwitchBar::Part::None,
                L"a hidden bar hit-tests to nothing");
        }
    }

    TEST_METHOD (LatchingAccessors_MirrorSetters)
    {
        Apple2cSwitchBar  bar;

        Assert::IsFalse (bar.IsEightyFortyIn(), L"80/40 defaults out");
        Assert::IsFalse (bar.IsKeyboardIn(),    L"keyboard defaults out");

        bar.SetEightyFortyIn (true);
        bar.SetKeyboardIn    (true);

        Assert::IsTrue (bar.IsEightyFortyIn(), L"80/40 in reflected");
        Assert::IsTrue (bar.IsKeyboardIn(),    L"keyboard in reflected");
    }

    TEST_METHOD (Tooltips_PerPart_NullInGaps)
    {
        MockDxuiTextRenderer  text;
        Apple2cSwitchBar      bar = MakeLaidOutBar (text);

        int   midY     = (s_kBand.top + s_kBand.bottom) / 2;
        bool  sawTip    = false;
        bool  sawNoTip  = false;

        for (int x = s_kBand.left; x < s_kBand.right; ++x)
        {
            const wchar_t * tip = bar.TooltipTextAt (x, midY);
            bool            hit = bar.PartAt (x, midY) != Apple2cSwitchBar::Part::None;

            if (hit) { Assert::IsNotNull (tip, L"a clickable part has a tooltip"); sawTip = true; }
            else     { Assert::IsNull (tip,   L"a gap has no tooltip");            sawNoTip = true; }
        }

        Assert::IsTrue (sawTip && sawNoTip, L"both tooltip and no-tooltip regions exist");
    }

    TEST_METHOD (Paint_FillsCaseBody)
    {
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        MockDxuiPainter       painter;
        Apple2cSwitchBar      bar = MakeLaidOutBar (text);

        bar.Paint (painter, text, theme);

        // The first primitive is the platinum case body: a gradient rect
        // spanning the full band width.
        Assert::IsTrue (!painter.Calls().empty(), L"Paint emits primitives");

        const RecordedPaintCall & first = painter.Calls().front();
        Assert::IsTrue (first.kind == RecordedPaintKind::FillGradientRect,
            L"case body is a gradient fill");
        Assert::AreEqual ((float) (s_kBand.right - s_kBand.left), first.width,
            L"case body spans the band width");
    }

    TEST_METHOD (PushedInKey_PaintsDifferentlyThanOut)
    {
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        Apple2cSwitchBar      barOut = MakeLaidOutBar (text);
        Apple2cSwitchBar      barIn  = MakeLaidOutBar (text);
        MockDxuiPainter       pOut;
        MockDxuiPainter       pIn;

        barOut.SetEightyFortyIn (false);
        barIn.SetEightyFortyIn  (true);

        barOut.Paint (pOut, text, theme);
        barIn.Paint  (pIn,  text, theme);

        // The pushed-in key draws a distinct primitive stream (flat sunk cap +
        // top shadow) vs the raised out key (highlit cap + exposed slot), so
        // the two paints must not be byte-identical — the visual "in vs out"
        // clue is real.
        bool  identical = pOut.Calls().size() == pIn.Calls().size();
        if (identical)
        {
            for (size_t i = 0; i < pOut.Calls().size(); ++i)
            {
                const RecordedPaintCall & a = pOut.Calls()[i];
                const RecordedPaintCall & b = pIn.Calls()[i];
                if (a.kind != b.kind || a.y != b.y || a.height != b.height ||
                    a.argb != b.argb)
                {
                    identical = false;
                    break;
                }
            }
        }

        Assert::IsFalse (identical,
            L"a pushed-in latching switch must paint differently than an out one");
    }
};
