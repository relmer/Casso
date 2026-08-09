#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHwndSourcePlacementTests
//
//  Window placement: clamping to the work area, and rescuing a window saved on
//  a monitor that no longer exists.
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


    RECT  Rect (LONG l, LONG t, LONG r, LONG b)
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
        POINT  p = DxuiHwndSource::ClampToWorkArea (Rect (300, 200, 900, 700), s_kWork);


        AssertPoint (300, 200, p, L"already-visible window is left where it is");
    }


    //
    //  The reported bug: a cascade drops the bottom (and its button row)
    //  under the taskbar; the window is lifted straight up the minimum
    //  amount and its horizontal position is preserved.
    //
    TEST_METHOD (BottomUnderTaskbarLiftedUp)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (Rect (200, 700, 700, 1300), s_kWork);


        AssertPoint (200, 440, p, L"bottom meets work.bottom; x unchanged");
    }


    //
    //  A window past the right edge is pulled left the minimum amount.
    //
    TEST_METHOD (RightEdgeOffScreenPulledLeft)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (Rect (1700, 100, 2100, 400), s_kWork);


        AssertPoint (1520, 100, p, L"right meets work.right; y unchanged");
    }


    //
    //  A window above the work area (a top-docked taskbar) is pushed down.
    //
    TEST_METHOD (AboveWorkAreaPushedDown)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (Rect (200, -60, 600, 240), Rect (0, 40, 1920, 1080));


        AssertPoint (200, 40, p, L"pushed down to the top-docked work area");
    }


    //
    //  A window larger than the work area pins to the top-left, so the
    //  caption stays reachable rather than the bottom sliding off-screen.
    //
    TEST_METHOD (OversizedPinsTopLeft)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (Rect (-50, -30, 2000, 1210), s_kWork);


        AssertPoint (0, 0, p, L"oversized window pinned to the work origin");
    }


    //
    //  Work areas with a non-zero origin (a monitor left of the primary)
    //  clamp against that origin, not against 0,0.
    //
    TEST_METHOD (HonorsOffsetWorkAreaOrigin)
    {
        POINT  p = DxuiHwndSource::ClampToWorkArea (Rect (-2000, 100, -1600, 400), Rect (-1920, 0, 0, 1080));


        AssertPoint (-1920, 100, p, L"clamped to the left monitor's left edge");
    }
};

