#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoTipTests
//
//  The info tip tracks the pointer without capturing it: a move over the
//  glyph shows its tooltip, a move off it or a leave hides it, and no event
//  is consumed, so the control beside it still receives every one.
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


    TEST_METHOD (IsATabStopThatEnterSpaceAndAClickOpen)
    {
        DxuiInfoTip   tip;
        DxuiKeyEvent  enter;
        DxuiKeyEvent  space;



        tip.SetText (L"Some tools read only 8-bit WAV files.");
        tip.SetRect (RECT { 100, 10, 120, 30 });
        tip.SetVisible (true);
        enter.vk = VK_RETURN;
        space.vk = VK_SPACE;

        Assert::IsTrue (tip.IsFocusable());

        Assert::IsTrue  (tip.OnKey (enter));
        Assert::IsTrue  (tip.GetTooltip().IsVisible(), L"Enter opens it at once");
        Assert::IsTrue  (tip.OnKey (space));
        Assert::IsFalse (tip.GetTooltip().IsVisible(), L"a second press closes it");

        Assert::IsTrue (tip.OnMouse (Move (110, 20, DxuiMouseEventKind::Down)), L"a click is taken");
        Assert::IsTrue (tip.GetTooltip().IsVisible());

        tip.OnFocusChanged (false);
        Assert::IsTrue (tip.GetTooltip().WantsTick(), L"losing focus starts it closing");
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
