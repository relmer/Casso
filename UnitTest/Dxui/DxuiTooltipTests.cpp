#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipTests
//
//  Tooltip dwell timing and placement: when a balloon appears, when it hides,
//  and where it lands.
//
//  Time is passed IN rather than read, which is what makes dwell behavior
//  testable at all -- the tests advance a synthetic clock instead of sleeping,
//  so the whole open-and-close cycle runs instantly and deterministically.
//
//  Show and hide are REQUESTS, and that is the behavior being pinned: sweeping
//  the pointer across several widgets must request several tooltips and produce
//  none, because the dwell never elapses. A widget that showed on hover would
//  flash a tooltip per widget crossed.
//
//  Placement is clamped to the viewport, so the cases near each edge are
//  covered -- that clamping is the in-window path's whole limitation, and the
//  reason the popup-hosted version exists.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (TooltipTests)
{
public:

    RECT MakeRect (int l, int t, int r, int b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }

    TEST_METHOD (Request_HiddenUntilDwellElapses)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs (500);

        t.RequestShow (MakeRect (0, 0, 50, 20), L"locked", 0);
        Assert::IsFalse (t.IsVisible());

        t.Tick (499);
        Assert::IsFalse (t.IsVisible());

        t.Tick (500);
        Assert::IsTrue (t.IsVisible());
        Assert::AreEqual (std::wstring (L"locked"), t.GetText());
    }

    TEST_METHOD (Request_HideAfterCloseDwell)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs  (100);
        t.SetDwellCloseMs (50);

        t.RequestShow (MakeRect (0, 0, 50, 20), L"x", 0);
        t.Tick (100);
        Assert::IsTrue (t.IsVisible());

        t.RequestHide (200);
        Assert::IsTrue (t.IsVisible(),
            L"DxuiTooltip must remain visible until the close-dwell elapses.");

        t.Tick (250);
        Assert::IsFalse (t.IsVisible());
    }

    TEST_METHOD (Request_HideBeforeShow_CancelsPending)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs (500);

        t.RequestShow (MakeRect (0, 0, 50, 20), L"x", 0);
        t.RequestHide (10);
        t.Tick (1000);

        Assert::IsFalse (t.IsVisible(),
            L"Hide before the show dwell fires must abort the pending show.");
    }

    TEST_METHOD (Request_SwapAnchorWhileVisible)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs (100);

        t.RequestShow (MakeRect (0, 0, 50, 20), L"a", 0);
        t.Tick (100);
        Assert::IsTrue (t.IsVisible());

        t.RequestShow (MakeRect (60, 0, 110, 20), L"b", 200);
        Assert::IsTrue (t.IsVisible());
        Assert::AreEqual (std::wstring (L"b"), t.GetText());
        Assert::AreEqual ((LONG) 60, t.GetAnchor().left);
    }

    //
    //  A tip left alone dismisses itself. Without this a pointer parked on a
    //  control -- or captured by the guest, which parks it by force -- left
    //  the balloon on screen for the rest of the session.
    //
    TEST_METHOD (Visible_DismissesAfterMaxLifetime)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs (100);

        t.RequestShow (MakeRect (0, 0, 50, 20), L"x", 0);
        t.Tick (100);
        Assert::IsTrue (t.IsVisible());

        t.Tick (100 + DxuiTooltip::kMaxVisibleMs - 1);
        Assert::IsTrue (t.IsVisible(),
            L"DxuiTooltip must stay up for its full lifetime.");

        t.Tick (100 + DxuiTooltip::kMaxVisibleMs);
        Assert::IsFalse (t.IsVisible(),
            L"DxuiTooltip must dismiss itself once its lifetime elapses.");
    }

    //
    //  ...and re-requesting the SAME tip does not renew it. Consumers reissue
    //  RequestShow on every mouse-move, so a deadline that reset on each one
    //  could never be reached: the lifetime existed and never once fired.
    //
    TEST_METHOD (Visible_RepeatedRequestDoesNotRenewLifetime)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs (100);

        t.RequestShow (MakeRect (0, 0, 50, 20), L"x", 0);
        t.Tick (100);

        for (int64_t now = 110; now < 100 + DxuiTooltip::kMaxVisibleMs; now += 10)
        {
            t.RequestShow (MakeRect (0, 0, 50, 20), L"x", now);
            t.Tick (now);
        }

        t.Tick (100 + DxuiTooltip::kMaxVisibleMs);
        Assert::IsFalse (t.IsVisible(),
            L"A resting pointer must not keep renewing the tooltip deadline.");
    }

    //  An instant tip shows on the request itself, with no dwell, and follows
    //  the pointer from one anchor to the next at once; the tooltip's dwell
    //  for an ordinary request is unchanged.
    TEST_METHOD (RequestShowNow_ShowsAtOnceAndLeavesTheDwellAlone)
    {
        DxuiTooltip  t;
        t.SetDwellOpenMs (500);

        t.RequestShowNow (MakeRect (0, 0, 10, 10), L"$0400", 0);
        Assert::IsTrue   (t.IsVisible(), L"no dwell, and no Tick needed");
        Assert::AreEqual (std::wstring (L"$0400"), t.GetText());

        t.RequestShowNow (MakeRect (10, 0, 20, 10), L"$0401", 1);
        Assert::AreEqual (std::wstring (L"$0401"), t.GetText(), L"the next byte's tip, at once");
        Assert::AreEqual (10L, t.GetAnchor().left);

        t.HideImmediate();
        t.RequestShow (MakeRect (0, 0, 50, 20), L"ordinary", 100);
        Assert::IsFalse (t.IsVisible(), L"an ordinary request still waits");

        t.Tick (599);
        Assert::IsFalse (t.IsVisible());

        t.Tick (600);
        Assert::IsTrue  (t.IsVisible());
    }

    //  A pointer twenty pixels tall below its hot spot, and two above it.
    static DxuiTooltip::PointerExtent  GetTallPointer() { return DxuiTooltip::PointerExtent { 2, 20 }; }

    //  An instant tip follows the pointer, so it is placed clear of the
    //  pointer's whole image plus a gap, below it, or above it where below
    //  would run off the work area. A dwelled tip keeps its control's anchor.
    TEST_METHOD (RequestShowNow_PlacesTheTipClearOfThePointer)
    {
        constexpr int  kGapPx   = 4;
        DxuiTooltip    t;
        RECT           cell     = MakeRect (100, 200, 120, 216);
        RECT           anchor   = {};
        RECT           work     = MakeRect (0, 0, 1000, 800);
        RECT           placed   = {};
        SIZE           tipPx    = { 60, 22 };



        t.SetPointerMeasurer (GetTallPointer);
        t.RequestShowNow (cell, L"$0400", 0);

        anchor = t.GetPlacementAnchor();

        Assert::AreEqual (cell.left,                 anchor.left,   L"along the byte, as before");
        Assert::AreEqual (cell.bottom + 20 + kGapPx, anchor.bottom, L"below the lowest the pointer's image reaches from anywhere in the byte, and a gap");
        Assert::AreEqual (cell.top - 2 - kGapPx,     anchor.top,    L"above the highest it reaches, and a gap");

        placed = DxuiPopupHost::ComputePlacementForTest (anchor, work, DxuiPopupPlacement::Below, tipPx, true);
        Assert::IsTrue (placed.top >= cell.bottom + 20, L"below the pointer, not over it");

        work.bottom = cell.bottom + 20;
        placed      = DxuiPopupHost::ComputePlacementForTest (anchor, work, DxuiPopupPlacement::Below, tipPx, true);
        Assert::IsTrue (placed.bottom <= cell.top - 2, L"above the pointer where below would leave the work area");

        t.HideImmediate();
        t.SetDwellOpenMs (0);
        t.RequestShow (cell, L"a control's tip", 0);
        t.Tick (0);
        anchor = t.GetPlacementAnchor();

        Assert::IsTrue (EqualRect (&anchor, &cell) != FALSE, L"a dwelled tip keeps its control's anchor");
    }

    //  A tip that follows the pointer keeps the one balloon it raised and
    //  moves it: hiding it and raising another at every step made it flicker
    //  from place to place. New text is put in place and starts its own
    //  lifetime; the same text moved along keeps the deadline it had.
    TEST_METHOD (RequestShowNow_MovesTheBalloonItHasUp)
    {
        DxuiHwndSource     host;
        DxuiTooltip        t;
        DxuiPopupHost    * popup  = nullptr;
        RECT               placed = {};



        t.SetPopupHost       (&host);
        t.SetPointerMeasurer (GetTallPointer);

        t.RequestShowNow (MakeRect (100, 200, 101, 201), L"$0400", 0);

        popup = t.GetActivePopup();
        Assert::IsNotNull (popup);
        Assert::AreEqual  (1, t.GetShowCount());

        placed = popup->GetPlacedRectScreenPx();

        t.RequestShowNow (MakeRect (130, 200, 131, 201), L"$0400", 10);
        Assert::AreEqual (1, t.GetShowCount(), L"moved along, not raised again");
        Assert::IsTrue   (t.GetActivePopup() == popup, L"the same balloon");
        Assert::AreEqual (placed.left + 30, popup->GetPlacedRectScreenPx().left, L"with the pointer");

        t.RequestShowNow (MakeRect (160, 200, 161, 201), L"$0401", 20);
        Assert::AreEqual (1, t.GetShowCount(), L"new text is put in place");
        Assert::AreEqual (std::wstring (L"$0401"), t.GetText());
        Assert::AreEqual (placed.left + 60, popup->GetPlacedRectScreenPx().left);

        t.HideImmediate();
    }
};
