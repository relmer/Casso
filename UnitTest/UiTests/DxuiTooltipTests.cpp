#include "Pch.h"

#include "Widgets/DxuiTooltip.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TooltipTests
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    RECT MakeRect (int l, int t, int r, int b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }
}


TEST_CLASS (TooltipTests)
{
public:

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
        Assert::AreEqual (std::wstring (L"locked"), t.Text());
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
        Assert::AreEqual (std::wstring (L"b"), t.Text());
        Assert::AreEqual ((LONG) 60, t.Anchor().left);
    }
};
