#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace
{
    constexpr LONG  s_kMonLeft      = 0;
    constexpr LONG  s_kMonTop       = 0;
    constexpr LONG  s_kMonRight     = 1920;
    constexpr LONG  s_kMonBottom    = 1080;
    constexpr LONG  s_kPopupW       = 200;
    constexpr LONG  s_kPopupH       = 150;


    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }


    SIZE  MakeSize (LONG cx, LONG cy)
    {
        SIZE  out = {};
        out.cx = cx; out.cy = cy;
        return out;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHostPlacementTests
//
//  Drives the pure-function ComputePlacementForTest seam to verify
//  each placement direction lands the popup where expected and that
//  flipIfOffscreen flips Below -> Above (and friends) when the
//  preferred edge would push the popup past the monitor work area.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupHostPlacementTests)
{
public:

    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (Below_PlacesPopupUnderneathAnchor)
    {
        RECT  anchor = MakeRect (100, 200, 300, 230);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Below,
                                                               size,
                                                               true);

        Assert::AreEqual ((LONG) 100, placed.left);
        Assert::AreEqual ((LONG) 230, placed.top);
        Assert::AreEqual ((LONG) 300, placed.right);
        Assert::AreEqual ((LONG) 380, placed.bottom);
    }


    TEST_METHOD (Above_PlacesPopupAboveAnchor)
    {
        RECT  anchor = MakeRect (100, 400, 300, 430);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Above,
                                                               size,
                                                               true);

        Assert::AreEqual ((LONG) 100, placed.left);
        Assert::AreEqual ((LONG) 250, placed.top);
        Assert::AreEqual ((LONG) 400, placed.bottom);
    }


    TEST_METHOD (Right_PlacesPopupToRightOfAnchor)
    {
        RECT  anchor = MakeRect (100, 200, 300, 400);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Right,
                                                               size,
                                                               true);

        Assert::AreEqual ((LONG) 300, placed.left);
        Assert::AreEqual ((LONG) 200, placed.top);
        Assert::AreEqual ((LONG) 500, placed.right);
    }


    TEST_METHOD (Left_PlacesPopupToLeftOfAnchor)
    {
        RECT  anchor = MakeRect (500, 200, 700, 400);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Left,
                                                               size,
                                                               true);

        Assert::AreEqual ((LONG) 300, placed.left);
        Assert::AreEqual ((LONG) 200, placed.top);
        Assert::AreEqual ((LONG) 500, placed.right);
    }


    TEST_METHOD (AtCursor_PlacesPopupAtAnchorTopLeft)
    {
        RECT  cursor = MakeRect (640, 480, 640, 480);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (cursor,
                                                               work,
                                                               DxuiPopupPlacement::AtCursor,
                                                               size,
                                                               true);

        Assert::AreEqual ((LONG) 640, placed.left);
        Assert::AreEqual ((LONG) 480, placed.top);
        Assert::AreEqual ((LONG) 840, placed.right);
        Assert::AreEqual ((LONG) 630, placed.bottom);
    }


    //
    //  SC-008 — the dropdown-near-bottom-of-window case. The
    //  anchor is parked 20 px from the bottom of the work area and
    //  the popup is 150 px tall; placing Below would push the popup
    //  off-screen so flipIfOffscreen must select Above.
    //
    TEST_METHOD (Below_FlipsToAboveWhenAnchorIsNearBottom)
    {
        RECT  anchor = MakeRect (100, 1040, 300, 1060);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Below,
                                                               size,
                                                               true);

        // Top edge ended up ABOVE the anchor top — i.e. it flipped.
        Assert::IsTrue (placed.bottom <= anchor.top,
                        L"Popup should flip Above when Below would clip the bottom of the work area");
        Assert::AreEqual ((LONG) 890, placed.top);
        Assert::AreEqual ((LONG) 1040, placed.bottom);
    }


    TEST_METHOD (Above_FlipsToBelowWhenAnchorIsNearTop)
    {
        RECT  anchor = MakeRect (100, 20, 300, 50);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Above,
                                                               size,
                                                               true);

        Assert::IsTrue (placed.top >= anchor.bottom,
                        L"Popup should flip Below when Above would clip the top of the work area");
    }


    TEST_METHOD (Right_FlipsToLeftWhenAnchorIsNearRight)
    {
        RECT  anchor = MakeRect (1850, 200, 1900, 400);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Right,
                                                               size,
                                                               true);

        Assert::IsTrue (placed.right <= anchor.left,
                        L"Popup should flip Left when Right would clip the right of the work area");
    }


    TEST_METHOD (Left_FlipsToRightWhenAnchorIsNearLeft)
    {
        RECT  anchor = MakeRect (10, 200, 60, 400);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Left,
                                                               size,
                                                               true);

        Assert::IsTrue (placed.left >= anchor.right,
                        L"Popup should flip Right when Left would clip the left of the work area");
    }


    TEST_METHOD (FlipIfOffscreenFalse_DoesNotFlip)
    {
        RECT  anchor = MakeRect (100, 1040, 300, 1060);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Below,
                                                               size,
                                                               false);

        Assert::AreEqual ((LONG) 1060, placed.top);
    }


    //
    //  Orthogonal clamping: Below placement near the right edge
    //  should slide the popup horizontally inside the work area
    //  even when it doesn't need to flip vertically.
    //
    TEST_METHOD (Below_ClampsHorizontallyToWorkArea)
    {
        RECT  anchor = MakeRect (1800, 100, 1900, 130);
        RECT  work   = MakeRect (s_kMonLeft, s_kMonTop, s_kMonRight, s_kMonBottom);
        SIZE  size   = MakeSize (s_kPopupW, s_kPopupH);

        RECT  placed = DxuiPopupHost::ComputePlacementForTest (anchor,
                                                               work,
                                                               DxuiPopupPlacement::Below,
                                                               size,
                                                               true);

        Assert::IsTrue (placed.right <= s_kMonRight,
                        L"Popup right edge must stay inside the work area");
        Assert::AreEqual ((LONG) (s_kMonRight - s_kPopupW), placed.left);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHostDismissPolicyTests
//
//  Exercises the ShouldDismissForTest pure-function classifier for
//  every combination of policy + reason.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupHostDismissPolicyTests)
{
public:

    TEST_METHOD (OnClickOutside_DismissesOnlyOnOutsideChainClick)
    {
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickOutside,
                                                              DxuiPopupDismissReason::ClickInsidePopup));
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickOutside,
                                                              DxuiPopupDismissReason::ClickInsideChainAncestor));
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickOutside,
                                                              DxuiPopupDismissReason::ClickOutsideChain));
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickOutside,
                                                              DxuiPopupDismissReason::PointerLeftPopup));
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickOutside,
                                                              DxuiPopupDismissReason::Manual));
    }


    TEST_METHOD (OnClickAnywhere_DismissesOnEveryClick)
    {
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickAnywhere,
                                                              DxuiPopupDismissReason::ClickInsidePopup));
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickAnywhere,
                                                              DxuiPopupDismissReason::ClickInsideChainAncestor));
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickAnywhere,
                                                              DxuiPopupDismissReason::ClickOutsideChain));
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnClickAnywhere,
                                                              DxuiPopupDismissReason::PointerLeftPopup));
    }


    TEST_METHOD (OnPointerLeave_DismissesOnlyOnPointerLeave)
    {
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnPointerLeave,
                                                              DxuiPopupDismissReason::ClickInsidePopup));
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnPointerLeave,
                                                              DxuiPopupDismissReason::ClickOutsideChain));
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::OnPointerLeave,
                                                              DxuiPopupDismissReason::PointerLeftPopup));
    }


    TEST_METHOD (Manual_NeverDismissesExceptOnExplicitManualReason)
    {
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::Manual,
                                                              DxuiPopupDismissReason::ClickInsidePopup));
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::Manual,
                                                              DxuiPopupDismissReason::ClickOutsideChain));
        Assert::IsFalse (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::Manual,
                                                              DxuiPopupDismissReason::PointerLeftPopup));
        Assert::IsTrue  (DxuiPopupHost::ShouldDismissForTest (DxuiPopupDismiss::Manual,
                                                              DxuiPopupDismissReason::Manual));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHostChainTests
//
//  Owner-chain bookkeeping for cascading submenus. SetParentPopup
//  links a child popup to its parent so a click landing inside the
//  parent doesn't dismiss the chain. Verifies ActiveChildPopup
//  reflects the link and that Close() unwires the back-pointer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupHostChainTests)
{
public:

    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (SetParentPopup_LinksChildToParent)
    {
        DxuiPopupHost  parent;
        DxuiPopupHost  child;


        parent.InitializeForTest();
        child.InitializeForTest();

        child.SetParentPopup (&parent);

        Assert::AreEqual (static_cast<void *> (&parent),
                          static_cast<void *> (child.ParentPopup()));
        Assert::AreEqual (static_cast<void *> (&child),
                          static_cast<void *> (parent.ActiveChildPopup()));
    }


    TEST_METHOD (SetParentPopup_Reparent_DetachesPriorParent)
    {
        DxuiPopupHost  parentA;
        DxuiPopupHost  parentB;
        DxuiPopupHost  child;


        parentA.InitializeForTest();
        parentB.InitializeForTest();
        child.InitializeForTest();

        child.SetParentPopup (&parentA);
        child.SetParentPopup (&parentB);

        Assert::IsNull (parentA.ActiveChildPopup());
        Assert::AreEqual (static_cast<void *> (&child),
                          static_cast<void *> (parentB.ActiveChildPopup()));
    }


    TEST_METHOD (Close_DetachesChildFromParent)
    {
        DxuiPopupHost              parent;
        DxuiPopupHost              child;
        DxuiPopupHost::ShowParams  showParams;


        parent.InitializeForTest();
        child.InitializeForTest();

        showParams.anchorRectScreen = MakeRect (0, 0, 10, 10);
        Assert::AreEqual (S_OK, child.Show (std::move (showParams)));
        child.SetParentPopup (&parent);

        Assert::AreEqual (static_cast<void *> (&child),
                          static_cast<void *> (parent.ActiveChildPopup()));

        child.Close (42);

        Assert::IsNull (parent.ActiveChildPopup());
        Assert::IsNull (child.ParentPopup());
        Assert::IsFalse (child.IsOpen());
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHostCompletionTests
//
//  Verifies the std::future<int> returned by Completion() resolves
//  with the value passed to Close().
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupHostCompletionTests)
{
public:

    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (Close_FulfilsCompletionFutureWithResultCode)
    {
        DxuiPopupHost              popup;
        DxuiPopupHost::ShowParams  showParams;
        std::future<int>           future;


        popup.InitializeForTest();
        showParams.anchorRectScreen = MakeRect (0, 0, 10, 10);
        Assert::AreEqual (S_OK, popup.Show (std::move (showParams)));

        future = popup.Completion();

        popup.Close (7);

        Assert::IsTrue (future.wait_for (std::chrono::seconds (1)) == std::future_status::ready);
        Assert::AreEqual (7, future.get());
    }


    TEST_METHOD (Show_TwiceClosesPriorCompletionBeforeArmingNewOne)
    {
        DxuiPopupHost              popup;
        DxuiPopupHost::ShowParams  firstParams;
        DxuiPopupHost::ShowParams  secondParams;
        std::future<int>           firstFuture;
        std::future<int>           secondFuture;


        popup.InitializeForTest();
        firstParams.anchorRectScreen = MakeRect (0, 0, 10, 10);
        Assert::AreEqual (S_OK, popup.Show (std::move (firstParams)));
        firstFuture = popup.Completion();

        secondParams.anchorRectScreen = MakeRect (0, 0, 10, 10);
        Assert::AreEqual (S_OK, popup.Show (std::move (secondParams)));

        Assert::IsTrue (firstFuture.wait_for (std::chrono::seconds (1)) == std::future_status::ready);
        Assert::AreEqual (0, firstFuture.get());

        secondFuture = popup.Completion();
        popup.Close (99);
        Assert::AreEqual (99, secondFuture.get());
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupHostPoolTests
//
//  Pool reuse + hit/miss counter verification. Drives AcquirePopup
//  on a synthetic DxuiHwndSource (no D3D device, no HWND) so popups
//  are seeded via InitializeForTest. Asserts T082's "PopupHits >= 4"
//  guarantee after multi-popup churn.
//
////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

namespace
{
    std::unique_ptr<DxuiHwndSource>  BuildSyntheticHostForPool ()
    {
        RECT  bounds = MakeRect (0, 0, 1024, 768);


        return std::make_unique<DxuiHwndSource> (bounds,
                                                 6.0f,
                                                 std::make_unique<DxuiPanel>());
    }
}


TEST_CLASS (DxuiPopupHostPoolTests)
{
public:

    TEST_METHOD_INITIALIZE (ResetUiThread)
    {
        DxuiResetUiThreadIdForTest();
    }


    TEST_METHOD (FirstAcquire_SeedsPoolToInitialSize3)
    {
        std::unique_ptr<DxuiHwndSource>  host  = BuildSyntheticHostForPool();
        DxuiPopupHost                  * popup = host->AcquirePopup();

        Assert::IsNotNull (popup);
        Assert::AreEqual ((size_t) 3, host->PopupPoolSize());
        Assert::AreEqual ((size_t) 1, host->PopupActiveCount());
        Assert::AreEqual ((size_t) 1, host->PopupHits());
        Assert::AreEqual ((size_t) 0, host->PopupMisses());
    }


    TEST_METHOD (RepeatedAcquireRelease_ReusesPooledInstance)
    {
        std::unique_ptr<DxuiHwndSource>  host = BuildSyntheticHostForPool();
        DxuiPopupHost                  * a    = nullptr;
        DxuiPopupHost                  * b    = nullptr;


        a = host->AcquirePopup();
        host->ReleasePopup (a);
        b = host->AcquirePopup();

        Assert::AreEqual (static_cast<void *> (a), static_cast<void *> (b),
                          L"Pool should return the same instance after release");
        Assert::AreEqual ((size_t) 2, host->PopupHits());
    }


    TEST_METHOD (OverAcquire_GrowsPoolOnDemandAndCountsMiss)
    {
        std::unique_ptr<DxuiHwndSource>  host  = BuildSyntheticHostForPool();
        DxuiPopupHost                  * p[4]  = {};

        for (int i = 0; i < 4; ++i)
        {
            p[i] = host->AcquirePopup();
            Assert::IsNotNull (p[i]);
        }

        Assert::AreEqual ((size_t) 4, host->PopupPoolSize(),
                          L"Pool should have grown from 3 -> 4 on demand");
        Assert::AreEqual ((size_t) 3, host->PopupHits());
        Assert::AreEqual ((size_t) 1, host->PopupMisses());
    }


    //
    //  T082 verification: after open/close churn 5 times, the pool
    //  should report at least 4 hits (one acquire seeds the pool;
    //  subsequent ones reuse the same slot).
    //
    TEST_METHOD (FiveOpenCloseCycles_ProduceFourOrMorePoolHits)
    {
        std::unique_ptr<DxuiHwndSource>  host = BuildSyntheticHostForPool();

        for (int i = 0; i < 5; ++i)
        {
            DxuiPopupHost  * popup = host->AcquirePopup();
            Assert::IsNotNull (popup);
            host->ReleasePopup (popup);
        }

        Assert::IsTrue (host->PopupHits() >= 4,
                        L"Five sequential acquire/release cycles must yield >= 4 pool hits (FR-055 / SC-008)");
        Assert::AreEqual ((size_t) 0, host->PopupActiveCount());
    }
};

#endif // _DEBUG
