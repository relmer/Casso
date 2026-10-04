#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTipTests
//
//  The info tip watches the pointer without ever taking it: a move over the
//  glyph asks for its tooltip, a move off it or a leave lets it go, and no
//  event is consumed, so the control beside it still gets every one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiInfoTipTests)
{
public:

    static DxuiMouseEvent Move (int x, int y, DxuiMouseEventKind kind = DxuiMouseEventKind::Move)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.positionDip = { x, y };

        return ev;
    }


    TEST_METHOD (HoverAsksForTheTipAndNeverConsumes)
    {
        DxuiInfoTip  tip;



        tip.SetText (L"Some tools read only 8-bit WAV files.");
        tip.SetRect (RECT { 100, 10, 120, 30 });
        tip.SetVisible (true);

        Assert::IsFalse (tip.OnMouse (Move (50, 20)));
        Assert::IsFalse (tip.GetTooltip().WantsTick(), L"nothing asked for away from the glyph");

        Assert::IsFalse (tip.OnMouse (Move (110, 20)), L"a hover is never consumed");
        Assert::IsTrue  (tip.GetTooltip().WantsTick(), L"over the glyph the tip is on its way");

        Assert::IsFalse (tip.OnMouse (Move (0, 0, DxuiMouseEventKind::Leave)));
    }


    TEST_METHOD (NoTextNoTip)
    {
        DxuiInfoTip  tip;



        tip.SetRect (RECT { 100, 10, 120, 30 });
        tip.SetVisible (true);

        Assert::IsFalse (tip.OnMouse (Move (110, 20)));
        Assert::IsFalse (tip.GetTooltip().WantsTick());
    }
};
