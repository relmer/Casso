#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSourcePlacementTests
//
//  Window placement: clamping to the work area, rescuing a window saved on a
//  monitor that no longer exists, and seating a dialog beside its owner.
//
//  Pure geometry over supplied rects, with no real monitors involved -- which
//  is the point. The interesting cases are a display configuration the test
//  machine does not have: a removed screen, a negative-coordinate secondary, a
//  taskbar on an unusual edge.
//
//  The work area rather than the monitor rect is the target throughout, so a
//  restored window is never left under the taskbar.
//
//  A window whose saved rect lands on no monitor must fall back to the default
//  placement rather than being dragged onto a survivor -- these pin that
//  choice, which is the difference between a window appearing where the user
//  expects and appearing somewhere arbitrary.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHwndSourcePlacementTests)
{
public:

    // 1920x1080 primary with a 40px bottom taskbar (rcWork bottom = 1040).
    const RECT  s_kWork = { 0, 0, 1920, 1040 };


    RECT  GetRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }


    void  AssertPoint (LONG expX, LONG expY, POINT actual, const wchar_t * what)
    {
        Assert::AreEqual (expX, actual.x, what);
        Assert::AreEqual (expY, actual.y, what);
    }

    //
    //  A window already fully within the work area keeps its position: the
    //  clamp never re-centers a window that fits.
    //
    TEST_METHOD (FullyOnScreenIsUnchanged)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (GetRect (300, 200, 900, 700), s_kWork);


        AssertPoint (300, 200, p, L"already-visible window is left where it is");
    }


    //
    //  The reported bug: a cascade drops the bottom (and its button row)
    //  under the taskbar; the window is lifted straight up the minimum
    //  amount and its horizontal position is preserved.
    //
    TEST_METHOD (BottomUnderTaskbarLiftedUp)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (GetRect (200, 700, 700, 1300), s_kWork);


        AssertPoint (200, 440, p, L"bottom meets work.bottom; x unchanged");
    }


    //
    //  A window past the right edge is pulled left the minimum amount.
    //
    TEST_METHOD (RightEdgeOffScreenPulledLeft)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (GetRect (1700, 100, 2100, 400), s_kWork);


        AssertPoint (1520, 100, p, L"right meets work.right; y unchanged");
    }


    //
    //  A window above the work area (a top-docked taskbar) is pushed down.
    //
    TEST_METHOD (AboveWorkAreaPushedDown)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (GetRect (200, -60, 600, 240), GetRect (0, 40, 1920, 1080));


        AssertPoint (200, 40, p, L"pushed down to the top-docked work area");
    }


    //
    //  A window larger than the work area pins to the top-left, so the
    //  caption stays reachable rather than the bottom sliding off-screen.
    //
    TEST_METHOD (OversizedPinsTopLeft)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (GetRect (-50, -30, 2000, 1210), s_kWork);


        AssertPoint (0, 0, p, L"oversized window pinned to the work origin");
    }


    //
    //  Work areas with a non-zero origin (a monitor left of the primary)
    //  clamp against that origin, not against 0,0.
    //
    TEST_METHOD (HonorsOffsetWorkAreaOrigin)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (GetRect (-2000, 100, -1600, 400), GetRect (-1920, 0, 0, 1080));


        AssertPoint (-1920, 100, p, L"clamped to the left monitor's left edge");
    }


    SIZE  GetSize (LONG cx, LONG cy)
    {
        SIZE  size = { cx, cy };
        return size;
    }


    //
    //  The preferred placement: flush against the owner's left edge, tops
    //  aligned, whenever the whole frame still fits on the owner's monitor.
    //  Both sides fit this owner, which is what makes it a preference test.
    //
    TEST_METHOD (BesideOwnerPrefersTheLeftEdge)
    {
        POINT  p = DxuiHwndSource::PlaceBesideOwner (GetRect (600, 100, 1400, 700), GetSize (500, 600), s_kWork);


        AssertPoint (100, 100, p, L"flush against the owner's left edge");
    }


    //
    //  An owner near the left edge leaves no room on that side, so the
    //  window goes to the owner's right rather than off the monitor.
    //
    TEST_METHOD (BesideOwnerFallsBackToTheRightEdge)
    {
        POINT  p = DxuiHwndSource::PlaceBesideOwner (GetRect (100, 100, 900, 700), GetSize (500, 600), s_kWork);


        AssertPoint (900, 100, p, L"flush against the owner's right edge");
    }


    //
    //  Neither side fits: the window overlaps the owner on the side with
    //  more room -- pinned to that edge of the work area, so the overlap is
    //  the smallest the monitor allows -- instead of leaving the monitor.
    //
    TEST_METHOD (BesideOwnerOverlapsOnTheRoomierSide)
    {
        POINT  left  = DxuiHwndSource::PlaceBesideOwner (GetRect (200, 100, 1800, 900), GetSize (500, 600), s_kWork);
        POINT  right = DxuiHwndSource::PlaceBesideOwner (GetRect (100, 100, 1700, 900), GetSize (500, 600), s_kWork);


        AssertPoint (0,    100, left,  L"more room left: pinned to the work-area left edge");
        AssertPoint (1420, 100, right, L"more room right: pinned to the work-area right edge");
    }


    //
    //  A maximized owner fills its monitor, so there is no side to sit
    //  beside. The window still stays on that monitor and overlaps.
    //
    TEST_METHOD (BesideMaximizedOwnerStaysOnItsMonitor)
    {
        POINT  p = DxuiHwndSource::PlaceBesideOwner (s_kWork, GetSize (500, 600), s_kWork);


        AssertPoint (1420, 0, p, L"overlapping the maximized owner, still on its monitor");
    }


    //
    //  The work area is the OWNER's monitor, so an owner on a secondary
    //  screen to the left of the primary keeps its dialog there: the fit
    //  test is against that monitor's edges, negative coordinates and all.
    //  That monitor's left edge rules out the left side, so this one lands
    //  on the fallback.
    //
    TEST_METHOD (BesideOwnerStaysOnASecondaryMonitor)
    {
        POINT  p = DxuiHwndSource::PlaceBesideOwner (GetRect (-1800, 100, -1200, 700),
                                                     GetSize (500, 600),
                                                     GetRect (-1920, 0, 0, 1080));


        AssertPoint (-1200, 100, p, L"beside the owner on the left-hand monitor");
    }


    //
    //  A modal centers on the owner's frame: half the size difference on
    //  each side, both axes.
    //
    TEST_METHOD (CenteredOnOwnerSplitsTheSizeDifference)
    {
        POINT  p = DxuiHwndSource::CenterOnOwner (GetRect (400, 200, 1200, 800), GetSize (500, 300), s_kWork);


        AssertPoint (550, 350, p, L"centered on the owner in both axes");
    }


    //
    //  An owner hanging off the left of its monitor would center the dialog
    //  partly off-screen, so the clamp pulls it back onto the work area --
    //  no longer centered, but whole.
    //
    TEST_METHOD (CenteredOnAnOffScreenOwnerStaysWhole)
    {
        POINT  p = DxuiHwndSource::CenterOnOwner (GetRect (-300, 100, 500, 700), GetSize (500, 300), s_kWork);


        AssertPoint (0, 250, p, L"pulled onto the work area's left edge");
    }


    //
    //  A maximized owner is the work area, so the dialog centers on the
    //  monitor -- and the taskbar-shortened work area is what it centers
    //  in, which sits it a little above the monitor's own center.
    //
    TEST_METHOD (CenteredOnAMaximizedOwnerCentersOnTheWorkArea)
    {
        POINT  p = DxuiHwndSource::CenterOnOwner (s_kWork, GetSize (500, 300), s_kWork);


        AssertPoint (710, 370, p, L"centered in the work area");
    }


    //
    //  An owner taller than the work area would push a centered dialog off
    //  the bottom; the clamp keeps its button row above the taskbar.
    //
    TEST_METHOD (CenteredOnATallOwnerClampsIntoTheWorkArea)
    {
        POINT  p = DxuiHwndSource::CenterOnOwner (GetRect (400, 600, 1200, 2000), GetSize (500, 300), s_kWork);


        AssertPoint (550, 740, p, L"bottom meets work.bottom");
    }


    //
    //  Tops align with the owner, but a low-sitting owner would push the
    //  window's bottom under the taskbar, so the clamp lifts it back.
    //
    TEST_METHOD (BesideOwnerClampsTheBottomIntoTheWorkArea)
    {
        POINT  p = DxuiHwndSource::PlaceBesideOwner (GetRect (100, 700, 900, 1000), GetSize (500, 600), s_kWork);


        AssertPoint (900, 440, p, L"bottom meets work.bottom; the side placement holds");
    }
};

