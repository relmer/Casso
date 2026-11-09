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
        Assert::IsFalse (tip.GetTooltip().IsVisible(), L"losing focus closes it");
    }


    //  A click pins the tip with no time limit; it is up until a second click
    //  on the "i", a press elsewhere, or Escape.
    TEST_METHOD (AClickPinsIt_UntilAClickAnywhereElseOrEscape)
    {
        DxuiInfoTip   tip;
        DxuiKeyEvent  escape;



        tip.SetText (L"Some tools read only 8-bit WAV files.");
        tip.SetRect (RECT { 100, 10, 120, 30 });
        tip.SetVisible (true);
        escape.vk = VK_ESCAPE;

        tip.OnMouse (Move (110, 20, DxuiMouseEventKind::Down));
        Assert::IsTrue  (tip.GetTooltip().IsVisible());
        Assert::IsFalse (tip.GetTooltip().WantsTick(), L"no lifetime to run out");

        tip.OnMouse (Move (300, 300));
        Assert::IsTrue (tip.GetTooltip().IsVisible(), L"a pinned tip is up after the pointer moves off");

        DxuiInfoTip::CloseOpenOutside (POINT { 110, 20 });
        Assert::IsTrue (tip.GetTooltip().IsVisible(), L"a press on its own \"i\" is the tip's to handle");

        tip.OnMouse (Move (110, 20, DxuiMouseEventKind::Down));
        Assert::IsFalse (tip.GetTooltip().IsVisible(), L"a second click closes it");

        tip.OnMouse (Move (110, 20, DxuiMouseEventKind::Down));
        DxuiInfoTip::CloseOpenOutside (POINT { 300, 300 });
        Assert::IsFalse (tip.GetTooltip().IsVisible(), L"a press elsewhere closes it");

        tip.OnMouse (Move (110, 20, DxuiMouseEventKind::Down));
        Assert::IsTrue  (DxuiInfoTip::CloseOpen(), L"Escape at the window closes it");
        Assert::IsFalse (tip.GetTooltip().IsVisible());
        Assert::IsFalse (DxuiInfoTip::CloseOpen(), L"and then nothing is open, so Escape cancels");

        tip.OnMouse (Move (110, 20, DxuiMouseEventKind::Down));
        Assert::IsTrue  (tip.OnKey (escape), L"Escape on the focused tip is consumed");
        Assert::IsFalse (tip.GetTooltip().IsVisible());
        Assert::IsFalse (tip.OnKey (escape), L"and passes on when it is closed");
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
