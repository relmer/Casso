#include "Pch.h"

#include "Widgets/DxuiButton.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




static const RECT  s_kWideRow = { 100, 40, 500, 68 };    // 400 x 28 -- a path row





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLinkFocusRingTests
//
//  The focus ring on a link-styled button.
//
//  A link gets whatever box the layout hands it, which for a folder-path row
//  is the entire content width. A ring drawn on that box is a large square
//  frame around a short run of words: it reads as a text field rather than as
//  focus, and it reaches into the row below. So the ring is measured to the
//  DRAWN TEXT and rounded.
//
//  The mock renderer reports 7 DIP per character and a 16 DIP line, which is
//  what lets these assert exact geometry.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiLinkFocusRingTests)
{
public:

    static constexpr float  kGlyph  = 7.0f;
    static constexpr float  kLine   = 16.0f;
    static constexpr float  kPadX   = 6.0f;
    static constexpr float  kPadY   = 3.0f;

    // The one ring the paint produced, or a default-constructed call if it
    // drew none.
    RecordedPaintCall FindRing (const MockDxuiPainter & painter) const
    {
        RecordedPaintCall   found;
        size_t              i = 0;

        for (i = 0; i < painter.Calls().size(); i++)
        {
            if (painter.Calls()[i].kind == RecordedPaintKind::OutlineRoundedRect
                || painter.Calls()[i].kind == RecordedPaintKind::OutlineRect)
            {
                found = painter.Calls()[i];
            }
        }

        return found;
    }


    void PaintLink (MockDxuiPainter      & painter,
                    MockDxuiTextRenderer & text,
                    const wchar_t        * label,
                    bool                   focused) const
    {
        MockDxuiTheme   theme;
        DxuiButton      link;

        link.SetVariant (DxuiButton::Variant::Link);
        link.SetLabel   (label);
        link.Layout     (s_kWideRow);
        link.SetFocused (focused);
        link.Paint      (painter, text, theme);
    }


    TEST_METHOD (AnUnfocusedLinkDrawsNoRingAtAll)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        PaintLink (painter, text, L"C:\\Pictures", false);

        Assert::AreEqual ((size_t) 0, painter.Calls().size(),
                          L"a link paints text only -- no fill, no border, no ring");
    }


    TEST_METHOD (TheRingIsRoundedRatherThanSquare)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        PaintLink (painter, text, L"C:\\Pictures", true);

        Assert::IsTrue (FindRing (painter).kind == RecordedPaintKind::OutlineRoundedRect);
        Assert::IsTrue (FindRing (painter).radius > 0.0f, L"and actually asks for a radius");
    }


    // The bug this guards: the ring used to be the button's bounds, so a
    // twelve-character link in a 400 DIP row got a 400 DIP frame.
    TEST_METHOD (TheRingIsSizedToTheTextNotToTheBox)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        RecordedPaintCall      ring;

        PaintLink (painter, text, L"C:\\Pictures", true);
        ring = FindRing (painter);

        Assert::AreEqual (11.0f * kGlyph + kPadX * 2.0f, ring.width, 0.01f);
        Assert::AreEqual (kLine + kPadY * 2.0f,          ring.height, 0.01f);
        Assert::IsTrue   (ring.width < (float) (s_kWideRow.right - s_kWideRow.left),
                          L"nowhere near the width of the row it sits in");
    }


    // The row below is the Browse button; a ring taller than its own row
    // reaches into it.
    TEST_METHOD (TheRingStaysInsideItsRowAndCentersOnIt)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        RecordedPaintCall      ring;

        PaintLink (painter, text, L"C:\\Pictures", true);
        ring = FindRing (painter);

        Assert::IsTrue (ring.y            >= (float) s_kWideRow.top,    L"does not reach the row above");
        Assert::IsTrue (ring.y + ring.height <= (float) s_kWideRow.bottom, L"nor the row below");

        //  Equal slack top and bottom.
        Assert::AreEqual (ring.y - (float) s_kWideRow.top,
                          (float) s_kWideRow.bottom - (ring.y + ring.height), 0.01f);
    }


    // A longer label makes a longer ring: it tracks the string rather than
    // landing on some fixed width.
    TEST_METHOD (TheRingGrowsWithTheLabel)
    {
        MockDxuiPainter        shortPainter;
        MockDxuiPainter        longPainter;
        MockDxuiTextRenderer   text;

        PaintLink (shortPainter, text, L"Open",       true);
        PaintLink (longPainter,  text, L"Open folder", true);

        Assert::IsTrue (FindRing (longPainter).width > FindRing (shortPainter).width);
    }


    // A measure that comes back empty -- which DirectWrite does transiently
    // during a live resize -- draws NO ring rather than one on the bounds.
    // The bounds are the layout's column, so a ring there would mark the
    // wrong thing, and the next paint puts the real one back.
    TEST_METHOD (AFailedMeasureDrawsNoRingRatherThanTheWrongOne)
    {
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        text.SetMeasureReturnsZero (true);
        PaintLink (painter, text, L"C:\\Pictures", true);

        Assert::AreEqual (0.0f, FindRing (painter).width, L"no ring at all");
    }
};
