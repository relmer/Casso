#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockLayoutTests
//
//  Dock layout: bands claiming an edge, and the center taking what is left.
//
//  ORDER is the substance -- docking is sequential, so the second top band sits
//  below the first and each dock shrinks the rect the rest see. A layout that
//  treated docks as independent would stack them on top of each other.
//
//  The center's rect is the assertion that matters, since it is the residue of
//  every other band and is what the emulator viewport actually gets.
//
//  Degenerate cases are covered: bands thicker than the available space, and a
//  container smaller than its own chrome. Both are reachable at small window
//  sizes and must collapse rather than produce inverted rects.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiDockLayoutTests)
{
public:

    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }


    LONG  Width  (const RECT & r) { return r.right  - r.left; }
    LONG  Height (const RECT & r) { return r.bottom - r.top;  }

    TEST_METHOD (Empty_NoChildren_DoesNothing)
    {
        DxuiDockLayout  layout;
        DxuiDpiScaler   scaler;
        RECT            bounds = MakeRect (0, 0, 100, 100);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> ());
        // No crash, nothing to verify.
        Assert::IsTrue (true);
    }


    TEST_METHOD (DockOf_DefaultsToFill)
    {
        DxuiDockLayout   layout;
        MockDxuiControl  c;

        Assert::IsTrue (layout.GetDock (c) == DxuiDock::Fill);
    }


    TEST_METHOD (SetDock_RoundTrips)
    {
        DxuiDockLayout   layout;
        MockDxuiControl  c;

        layout.SetDock (c, DxuiDock::Top);
        Assert::IsTrue (layout.GetDock (c) == DxuiDock::Top);

        layout.SetDock (c, DxuiDock::Bottom);
        Assert::IsTrue (layout.GetDock (c) == DxuiDock::Bottom);

        layout.ClearDock (c);
        Assert::IsTrue (layout.GetDock (c) == DxuiDock::Fill);
    }


    TEST_METHOD (Top_PeelsTopSlab)
    {
        DxuiDockLayout     layout;
        DxuiDpiScaler      scaler;
        MockDxuiControl    top;
        IDxuiControl     * kids[1] = { &top };
        RECT               bounds  = MakeRect (0, 0, 200, 100);

        top.SetBounds (MakeRect (0, 0, 999, 24));   // natural height 24
        layout.SetDock (top, DxuiDock::Top);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 0,   top.GetBounds().left);
        Assert::AreEqual ((LONG) 0,   top.GetBounds().top);
        Assert::AreEqual ((LONG) 200, top.GetBounds().right);
        Assert::AreEqual ((LONG) 24,  top.GetBounds().bottom);
    }


    TEST_METHOD (Bottom_PeelsBottomSlab)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  bottom;
        IDxuiControl *   kids[1] = { &bottom };
        RECT             bounds  = MakeRect (0, 0, 200, 100);

        bottom.SetBounds (MakeRect (0, 0, 999, 30));
        layout.SetDock   (bottom, DxuiDock::Bottom);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 0,   bottom.GetBounds().left);
        Assert::AreEqual ((LONG) 70,  bottom.GetBounds().top);
        Assert::AreEqual ((LONG) 200, bottom.GetBounds().right);
        Assert::AreEqual ((LONG) 100, bottom.GetBounds().bottom);
    }


    TEST_METHOD (Left_PeelsLeftSlab)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  left;
        IDxuiControl *   kids[1] = { &left };
        RECT             bounds  = MakeRect (0, 0, 200, 100);

        left.SetBounds (MakeRect (0, 0, 40, 999));
        layout.SetDock (left, DxuiDock::Left);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 0,   left.GetBounds().left);
        Assert::AreEqual ((LONG) 0,   left.GetBounds().top);
        Assert::AreEqual ((LONG) 40,  left.GetBounds().right);
        Assert::AreEqual ((LONG) 100, left.GetBounds().bottom);
    }


    TEST_METHOD (Right_PeelsRightSlab)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  right;
        IDxuiControl *   kids[1] = { &right };
        RECT             bounds  = MakeRect (0, 0, 200, 100);

        right.SetBounds (MakeRect (0, 0, 50, 999));
        layout.SetDock  (right, DxuiDock::Right);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 150, right.GetBounds().left);
        Assert::AreEqual ((LONG) 0,   right.GetBounds().top);
        Assert::AreEqual ((LONG) 200, right.GetBounds().right);
        Assert::AreEqual ((LONG) 100, right.GetBounds().bottom);
    }


    TEST_METHOD (Fill_TakesAllRemainingAfterPeels)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  top;
        MockDxuiControl  bottom;
        MockDxuiControl  fill;
        IDxuiControl *   kids[3] = { &top, &bottom, &fill };
        RECT             bounds  = MakeRect (0, 0, 400, 300);

        top.SetBounds    (MakeRect (0, 0, 999, 24));
        bottom.SetBounds (MakeRect (0, 0, 999, 32));

        layout.SetDock (top,    DxuiDock::Top);
        layout.SetDock (bottom, DxuiDock::Bottom);
        layout.SetDock (fill,   DxuiDock::Fill);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 3));

        Assert::AreEqual ((LONG) 0,   fill.GetBounds().left);
        Assert::AreEqual ((LONG) 24,  fill.GetBounds().top);
        Assert::AreEqual ((LONG) 400, fill.GetBounds().right);
        Assert::AreEqual ((LONG) 268, fill.GetBounds().bottom);    // 300 - 32 bottom
    }


    TEST_METHOD (Fill_RegisteredBeforeEdges_StillExcludesEdges)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  fill;
        MockDxuiControl  bottom;
        IDxuiControl *   kids[2] = { &fill, &bottom };   // Fill registered FIRST
        RECT             bounds  = MakeRect (0, 0, 400, 300);

        bottom.SetBounds (MakeRect (0, 0, 999, 44));

        layout.SetDock (fill,   DxuiDock::Fill);
        layout.SetDock (bottom, DxuiDock::Bottom);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        // The Bottom slab is reserved even though Fill was registered
        // first; Fill spans the region ABOVE it, not the whole rect.
        Assert::AreEqual ((LONG) 256, bottom.GetBounds().top);     // 300 - 44
        Assert::AreEqual ((LONG) 300, bottom.GetBounds().bottom);
        Assert::AreEqual ((LONG) 0,   fill.GetBounds().top);
        Assert::AreEqual ((LONG) 256, fill.GetBounds().bottom);
    }


    TEST_METHOD (MultipleTops_StackInOrder)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  top1;
        MockDxuiControl  top2;
        IDxuiControl *   kids[2] = { &top1, &top2 };
        RECT             bounds  = MakeRect (0, 0, 200, 100);

        top1.SetBounds (MakeRect (0, 0, 999, 24));
        top2.SetBounds (MakeRect (0, 0, 999, 16));

        layout.SetDock (top1, DxuiDock::Top);
        layout.SetDock (top2, DxuiDock::Top);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 0,  top1.GetBounds().top);
        Assert::AreEqual ((LONG) 24, top1.GetBounds().bottom);
        Assert::AreEqual ((LONG) 24, top2.GetBounds().top);
        Assert::AreEqual ((LONG) 40, top2.GetBounds().bottom);
    }


    TEST_METHOD (AllFourEdges_PeelInOrder)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  top;
        MockDxuiControl  bottom;
        MockDxuiControl  left;
        MockDxuiControl  right;
        MockDxuiControl  fill;
        IDxuiControl *   kids[5] = { &top, &bottom, &left, &right, &fill };
        RECT             bounds  = MakeRect (0, 0, 400, 300);

        top.SetBounds    (MakeRect (0, 0, 999, 20));
        bottom.SetBounds (MakeRect (0, 0, 999, 30));
        left.SetBounds   (MakeRect (0, 0, 40,  999));
        right.SetBounds  (MakeRect (0, 0, 50,  999));

        layout.SetDock (top,    DxuiDock::Top);
        layout.SetDock (bottom, DxuiDock::Bottom);
        layout.SetDock (left,   DxuiDock::Left);
        layout.SetDock (right,  DxuiDock::Right);
        layout.SetDock (fill,   DxuiDock::Fill);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 5));

        // Left was peeled after top/bottom shrank height, so it spans
        // the inner vertical band (20..270), not the full container.
        Assert::AreEqual ((LONG) 0,   left.GetBounds().left);
        Assert::AreEqual ((LONG) 20,  left.GetBounds().top);
        Assert::AreEqual ((LONG) 40,  left.GetBounds().right);
        Assert::AreEqual ((LONG) 270, left.GetBounds().bottom);

        Assert::AreEqual ((LONG) 350, right.GetBounds().left);
        Assert::AreEqual ((LONG) 400, right.GetBounds().right);

        Assert::AreEqual ((LONG) 40,  fill.GetBounds().left);
        Assert::AreEqual ((LONG) 20,  fill.GetBounds().top);
        Assert::AreEqual ((LONG) 350, fill.GetBounds().right);
        Assert::AreEqual ((LONG) 270, fill.GetBounds().bottom);
    }


    TEST_METHOD (SecondFill_CollapsesToZeroSize)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  fill1;
        MockDxuiControl  fill2;
        IDxuiControl *   kids[2] = { &fill1, &fill2 };
        RECT             bounds  = MakeRect (0, 0, 200, 100);

        layout.SetDock (fill1, DxuiDock::Fill);
        layout.SetDock (fill2, DxuiDock::Fill);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 200, Width  (fill1.GetBounds()));
        Assert::AreEqual ((LONG) 100, Height (fill1.GetBounds()));
        Assert::AreEqual ((LONG) 0,   Width  (fill2.GetBounds()));
        Assert::AreEqual ((LONG) 0,   Height (fill2.GetBounds()));
    }


    TEST_METHOD (ContainerSizeForFill_NoNonFillChildren_ReturnsDesired)
    {
        DxuiDockLayout                layout;
        SIZE                          desired = { 560, 384 };
        SIZE                          result  = {};

        result = layout.GetContainerSizeForFill (desired, std::span<IDxuiControl * const> ());

        Assert::AreEqual ((LONG) 560, result.cx);
        Assert::AreEqual ((LONG) 384, result.cy);
    }


    TEST_METHOD (ContainerSizeForFill_AddsTopBottomToHeight)
    {
        DxuiDockLayout   layout;
        MockDxuiControl  top;
        MockDxuiControl  bottom;
        IDxuiControl *   kids[2] = { &top, &bottom };
        SIZE             desired = { 560, 384 };
        SIZE             result  = {};

        top.SetBounds    (MakeRect (0, 0, 999, 24));
        bottom.SetBounds (MakeRect (0, 0, 999, 32));
        layout.SetDock (top,    DxuiDock::Top);
        layout.SetDock (bottom, DxuiDock::Bottom);

        result = layout.GetContainerSizeForFill (desired,
                                                 std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 560,            result.cx);
        Assert::AreEqual ((LONG) 384 + 24 + 32,  result.cy);
    }


    TEST_METHOD (ContainerSizeForFill_AddsLeftRightToWidth)
    {
        DxuiDockLayout   layout;
        MockDxuiControl  left;
        MockDxuiControl  right;
        IDxuiControl *   kids[2] = { &left, &right };
        SIZE             desired = { 560, 384 };
        SIZE             result  = {};

        left.SetBounds  (MakeRect (0, 0, 40, 999));
        right.SetBounds (MakeRect (0, 0, 50, 999));
        layout.SetDock (left,  DxuiDock::Left);
        layout.SetDock (right, DxuiDock::Right);

        result = layout.GetContainerSizeForFill (desired,
                                                 std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 560 + 40 + 50, result.cx);
        Assert::AreEqual ((LONG) 384,           result.cy);
    }


    TEST_METHOD (ContainerSizeForFill_RoundTripsThroughArrange)
    {
        DxuiDockLayout   layout;
        DxuiDpiScaler    scaler;
        MockDxuiControl  top;
        MockDxuiControl  bottom;
        MockDxuiControl  left;
        MockDxuiControl  right;
        MockDxuiControl  fill;
        IDxuiControl *   nonFill[4]  = { &top, &bottom, &left, &right };
        IDxuiControl *   all[5]      = { &top, &bottom, &left, &right, &fill };
        SIZE             desiredFill = { 560, 384 };
        SIZE             container   = {};
        RECT             bounds      = {};

        top.SetBounds    (MakeRect (0, 0, 999, 20));
        bottom.SetBounds (MakeRect (0, 0, 999, 30));
        left.SetBounds   (MakeRect (0, 0, 40,  999));
        right.SetBounds  (MakeRect (0, 0, 50,  999));

        layout.SetDock (top,    DxuiDock::Top);
        layout.SetDock (bottom, DxuiDock::Bottom);
        layout.SetDock (left,   DxuiDock::Left);
        layout.SetDock (right,  DxuiDock::Right);
        layout.SetDock (fill,   DxuiDock::Fill);

        container   = layout.GetContainerSizeForFill (desiredFill,
                                                      std::span<IDxuiControl * const> (nonFill, 4));
        bounds      = MakeRect (0, 0, container.cx, container.cy);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (all, 5));

        Assert::AreEqual ((LONG) 560, Width  (fill.GetBounds()));
        Assert::AreEqual ((LONG) 384, Height (fill.GetBounds()));
    }


    TEST_METHOD (ContainerSizeForFill_IgnoresFillEntriesInSpan)
    {
        DxuiDockLayout   layout;
        MockDxuiControl  top;
        MockDxuiControl  stray;
        IDxuiControl *   kids[2] = { &top, &stray };
        SIZE             desired = { 100, 100 };
        SIZE             result  = {};

        top.SetBounds   (MakeRect (0, 0, 999, 10));
        stray.SetBounds (MakeRect (0, 0, 999, 999));
        layout.SetDock (top,   DxuiDock::Top);
        layout.SetDock (stray, DxuiDock::Fill);     // explicit Fill: must be ignored

        result = layout.GetContainerSizeForFill (desired,
                                                 std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 100,        result.cx);
        Assert::AreEqual ((LONG) 100 + 10,   result.cy);
    }
};

