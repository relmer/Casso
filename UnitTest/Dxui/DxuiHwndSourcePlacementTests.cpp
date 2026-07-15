#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace
{
    // 1920x1080 primary with a 40px bottom taskbar (rcWork bottom = 1040).
    const RECT  s_kWork = { 0, 0, 1920, 1040 };


    RECT  Rect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  rc = { l, t, r, b };
        return rc;
    }


    SIZE  Size (LONG cx, LONG cy)
    {
        SIZE  sz = { cx, cy };
        return sz;
    }


    void  AssertPoint (LONG expX, LONG expY, POINT actual, const wchar_t * what)
    {
        Assert::AreEqual (expX, actual.x, what);
        Assert::AreEqual (expY, actual.y, what);
    }
}





TEST_CLASS (DxuiHwndSourcePlacementTests)
{
public:

    //
    //  A window that fits centers on the owner rect and the work-area
    //  clamp leaves it untouched.
    //
    TEST_METHOD (CentersOnOwnerWhenItFits)
    {
        POINT  p = DxuiHwndSource::CenterAndClamp (Rect (100, 100, 900, 700), s_kWork, Size (400, 300));


        AssertPoint (300, 250, p, L"centered on the 800x600 owner");
    }


    //
    //  An empty anchor (ownerless, or a minimized owner) centers on the
    //  work area itself.
    //
    TEST_METHOD (CentersOnWorkAreaWhenAnchorEmpty)
    {
        POINT  p = DxuiHwndSource::CenterAndClamp (Rect (0, 0, 0, 0), s_kWork, Size (400, 300));


        AssertPoint (760, 370, p, L"centered on the 1920x1040 work area");
    }


    //
    //  The reported bug: centering on a low owner pushes the bottom (and
    //  its command-button row) under the taskbar; the clamp lifts it back
    //  fully on-screen.
    //
    TEST_METHOD (ClampsBottomUpFromUnderTaskbar)
    {
        POINT  p = DxuiHwndSource::CenterAndClamp (Rect (0, 800, 500, 1000), s_kWork, Size (400, 600));


        AssertPoint (50, 440, p, L"bottom pinned to work.bottom - height");
    }


    //
    //  A window wider than the space to its owner's right is pulled left so
    //  its right edge stops at the work area.
    //
    TEST_METHOD (ClampsRightEdgeInward)
    {
        POINT  p = DxuiHwndSource::CenterAndClamp (Rect (1600, 100, 1900, 400), s_kWork, Size (600, 200));


        AssertPoint (1320, 150, p, L"right pinned to work.right - width");
    }


    //
    //  A window larger than the work area pins to the top-left, so the
    //  caption stays reachable rather than the bottom sliding off-screen.
    //
    TEST_METHOD (OversizedWindowPinsTopLeft)
    {
        POINT  p = DxuiHwndSource::CenterAndClamp (Rect (0, 0, 0, 0), s_kWork, Size (2000, 1200));


        AssertPoint (0, 0, p, L"oversized window pinned to the work origin");
    }


    //
    //  Work areas with a non-zero origin (a monitor left of the primary, or
    //  a top / left taskbar) clamp against that origin, not against 0,0.
    //
    TEST_METHOD (HonorsOffsetWorkAreaOrigin)
    {
        POINT  p = DxuiHwndSource::CenterAndClamp (Rect (0, 0, 0, 0), Rect (-1920, 0, 0, 1080), Size (400, 300));


        AssertPoint (-1160, 390, p, L"centered on the left-hand monitor");
    }
};
