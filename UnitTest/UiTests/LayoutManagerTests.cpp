#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/Chrome/LayoutManager.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

namespace
{
    // Pure-data edge contributor for tests.
    class StubEdge : public IEdgeContributor
    {
    public:
        StubEdge (ChromeEdge edge, int dp) : m_edge (edge), m_dp (dp) {}
        ChromeEdge  Edge               () const override { return m_edge; }
        int         DesiredThicknessDp () const override { return m_dp;   }

        void  SetThickness (int dp) { m_dp = dp; }

    private:
        ChromeEdge m_edge;
        int        m_dp;
    };


    class StubCenter : public ICenterLayer
    {
    public:
        StubCenter (int t, int b, int l, int r)
          : m_t (t), m_b (b), m_l (l), m_r (r) {}
        int  TopPadDp    () const override { return m_t; }
        int  BottomPadDp () const override { return m_b; }
        int  LeftPadDp   () const override { return m_l; }
        int  RightPadDp  () const override { return m_r; }

    private:
        int  m_t, m_b, m_l, m_r;
    };
}



TEST_CLASS (LayoutManagerTests)
{
public:

    TEST_METHOD (Empty_ZeroInsets_CenterRectIsFullClient)
    {
        LayoutManager        layout;
        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (0,   r.topInsetPx);
        Assert::AreEqual (0,   r.bottomInsetPx);
        Assert::AreEqual (0,   r.leftInsetPx);
        Assert::AreEqual (0,   r.rightInsetPx);
        Assert::AreEqual (0L,  r.centerRect.left);
        Assert::AreEqual (0L,  r.centerRect.top);
        Assert::AreEqual (800L, r.centerRect.right);
        Assert::AreEqual (600L, r.centerRect.bottom);
    }


    TEST_METHOD (SingleTopEdge_ReservesTopInsetOnly)
    {
        LayoutManager  layout;
        StubEdge      top (ChromeEdge::Top, 32);

        layout.Register (&top);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (32,  r.topInsetPx);
        Assert::AreEqual (0,   r.bottomInsetPx);
        Assert::AreEqual (32L, r.centerRect.top);
        Assert::AreEqual (600L, r.centerRect.bottom);
    }


    TEST_METHOD (TwoTopEdges_StackThicknesses)
    {
        LayoutManager  layout;
        StubEdge      titleBar (ChromeEdge::Top, 32);
        StubEdge      navStrip (ChromeEdge::Top, 32);

        layout.Register (&titleBar);
        layout.Register (&navStrip);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (64,  r.topInsetPx);
        Assert::AreEqual (64L, r.centerRect.top);
    }


    TEST_METHOD (FourEdges_AllSidesReserved)
    {
        LayoutManager  layout;
        StubEdge      top    (ChromeEdge::Top,    10);
        StubEdge      bottom (ChromeEdge::Bottom, 20);
        StubEdge      left   (ChromeEdge::Left,   30);
        StubEdge      right  (ChromeEdge::Right,  40);

        layout.Register (&top);
        layout.Register (&bottom);
        layout.Register (&left);
        layout.Register (&right);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (10,  r.topInsetPx);
        Assert::AreEqual (20,  r.bottomInsetPx);
        Assert::AreEqual (30,  r.leftInsetPx);
        Assert::AreEqual (40,  r.rightInsetPx);

        Assert::AreEqual (30L,  r.centerRect.left);
        Assert::AreEqual (10L,  r.centerRect.top);
        Assert::AreEqual (760L, r.centerRect.right);
        Assert::AreEqual (580L, r.centerRect.bottom);
    }


    TEST_METHOD (DpiScaling_DoublesAtDpi192)
    {
        LayoutManager  layout;
        StubEdge      top    (ChromeEdge::Top,    32);
        StubEdge      bottom (ChromeEdge::Bottom, 192);

        layout.Register (&top);
        layout.Register (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600, 192);

        Assert::AreEqual (64,  r.topInsetPx);
        Assert::AreEqual (384, r.bottomInsetPx);
    }


    TEST_METHOD (DpiScaling_150Percent_AtDpi144)
    {
        LayoutManager  layout;
        StubEdge      bottom (ChromeEdge::Bottom, 192);

        layout.Register (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600, 144);

        Assert::AreEqual (288, r.bottomInsetPx);
    }


    TEST_METHOD (DpiZero_TreatsAsBaseDpi)
    {
        LayoutManager  layout;
        StubEdge      top (ChromeEdge::Top, 32);

        layout.Register (&top);

        LayoutManagerResult  r = layout.Resolve (800, 600, 0);

        Assert::AreEqual (32, r.topInsetPx);
    }


    TEST_METHOD (CenterLayer_AddsPaddingInsideChromeInsets)
    {
        LayoutManager  layout;
        StubEdge      bottom  (ChromeEdge::Bottom, 100);
        StubCenter    monitor (8, 16, 4, 4);

        layout.Register (&bottom);
        layout.Register (&monitor);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (100, r.bottomInsetPx);
        Assert::AreEqual (16,  r.bottomCenterPadPx);
        Assert::AreEqual (8,   r.topCenterPadPx);
        Assert::AreEqual (4,   r.leftCenterPadPx);
        Assert::AreEqual (4,   r.rightCenterPadPx);

        Assert::AreEqual (8L,            r.centerRect.top);
        Assert::AreEqual (600L - 116L,   r.centerRect.bottom);
        Assert::AreEqual (4L,            r.centerRect.left);
        Assert::AreEqual (800L - 4L,     r.centerRect.right);
    }


    TEST_METHOD (CenterRect_OverAllocated_ClampsNonNegative)
    {
        LayoutManager  layout;
        StubEdge      top    (ChromeEdge::Top,    400);
        StubEdge      bottom (ChromeEdge::Bottom, 400);

        layout.Register (&top);
        layout.Register (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        // Window is 600 tall; top+bottom = 800. centerRect must not invert.
        Assert::IsTrue (r.centerRect.bottom >= r.centerRect.top);
        Assert::IsTrue (r.centerRect.right  >= r.centerRect.left);
    }


    TEST_METHOD (ClientSizeForCenter_AddsAllEdgeAndCenterPadding)
    {
        LayoutManager  layout;
        StubEdge      top      (ChromeEdge::Top,    32);
        StubEdge      navStrip (ChromeEdge::Top,    32);
        StubEdge      bottom   (ChromeEdge::Bottom, 192);
        StubCenter    monitor  (8, 16, 4, 4);

        layout.Register (&top);
        layout.Register (&navStrip);
        layout.Register (&bottom);
        layout.Register (&monitor);

        // Emulator framebuffer is 560x384 at 1x scale; total chrome cost is
        // top(64) + bottom(192) + center top(8) + center bottom(16) = 280
        // vertical, and center left(4) + center right(4) = 8 horizontal.
        SIZE  s = layout.ClientSizeForCenter (560, 384, 96);

        Assert::AreEqual (568L, s.cx);
        Assert::AreEqual (664L, s.cy);
    }


    TEST_METHOD (ClientSizeForCenter_DpiScales)
    {
        LayoutManager  layout;
        StubEdge      top    (ChromeEdge::Top,    32);
        StubEdge      bottom (ChromeEdge::Bottom, 192);

        layout.Register (&top);
        layout.Register (&bottom);

        // At 192 DPI, top=64 + bottom=384. Framebuffer doubles to 1120x768.
        SIZE  s = layout.ClientSizeForCenter (1120, 768, 192);

        Assert::AreEqual (1120L, s.cx);
        Assert::AreEqual (1216L, s.cy);
    }


    TEST_METHOD (ClientSizeForCenter_InvariantUnderRoundtrip)
    {
        LayoutManager  layout;
        StubEdge      top    (ChromeEdge::Top,    32);
        StubEdge      bottom (ChromeEdge::Bottom, 192);

        layout.Register (&top);
        layout.Register (&bottom);

        // ClientSizeForCenter then Resolve should report a centerRect that
        // exactly matches the desired emulator size we asked to host.
        SIZE                client = layout.ClientSizeForCenter (560, 384, 96);
        LayoutManagerResult  r      = layout.Resolve ((int) client.cx, (int) client.cy, 96);

        Assert::AreEqual (560L, r.centerRect.right  - r.centerRect.left);
        Assert::AreEqual (384L, r.centerRect.bottom - r.centerRect.top);
    }


    TEST_METHOD (Unregister_RemovesContribution)
    {
        LayoutManager  layout;
        StubEdge      top    (ChromeEdge::Top,    32);
        StubEdge      bottom (ChromeEdge::Bottom, 192);

        layout.Register   (&top);
        layout.Register   (&bottom);
        layout.Unregister (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (32, r.topInsetPx);
        Assert::AreEqual (0,  r.bottomInsetPx);
    }


    TEST_METHOD (Register_NullIsNoOp)
    {
        LayoutManager  layout;

        layout.Register ((IEdgeContributor *) nullptr);
        layout.Register ((ICenterLayer     *) nullptr);

        Assert::AreEqual ((size_t) 0, layout.Edges().size());
        Assert::AreEqual ((size_t) 0, layout.CenterLayers().size());
    }


    TEST_METHOD (ContributorMutation_NextResolveReflects)
    {
        LayoutManager  layout;
        StubEdge      driveBar (ChromeEdge::Bottom, 192);

        layout.Register (&driveBar);

        LayoutManagerResult  before = layout.Resolve (800, 600, 96);

        Assert::AreEqual (192, before.bottomInsetPx);

        driveBar.SetThickness (48);    // simulate compact-drives theme swap

        LayoutManagerResult  after = layout.Resolve (800, 600, 96);

        Assert::AreEqual (48, after.bottomInsetPx);
    }


    TEST_METHOD (CtrlZeroParity_FramebufferTimesScalePlusInsets)
    {
        // Regression test for the Ctrl+0 pillarbox bug. The historical bug
        // was that WindowCommandManager computed the desired client size
        // without including the command-bar inset; with LayoutManager, the
        // same call yields the right value automatically because both call
        // sites share the same contributor list.
        constexpr int  fbW       = 560;
        constexpr int  fbH       = 384;
        constexpr int  scale     = 2;

        LayoutManager  layout;
        StubEdge      titleBar (ChromeEdge::Top,    32);
        StubEdge      navStrip (ChromeEdge::Top,    32);
        StubEdge      driveBar (ChromeEdge::Bottom, 192);

        layout.Register (&titleBar);
        layout.Register (&navStrip);
        layout.Register (&driveBar);

        SIZE  s = layout.ClientSizeForCenter (fbW * scale, fbH * scale, 96);

        // Width: framebuffer * scale, no L/R contributors.
        Assert::AreEqual ((LONG) (fbW * scale),           s.cx);
        // Height: framebuffer * scale + top(64) + bottom(192).
        Assert::AreEqual ((LONG) (fbH * scale + 64 + 192), s.cy);
    }


    TEST_METHOD (SimpleEdgeContributor_AsValueType_Works)
    {
        LayoutManager           layout;
        SimpleEdgeContributor  bottom (ChromeEdge::Bottom, 100);

        layout.Register (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (100, r.bottomInsetPx);

        bottom.SetEdge        (ChromeEdge::Top);
        bottom.SetThicknessDp (50);

        r = layout.Resolve (800, 600, 96);

        Assert::AreEqual (0,  r.bottomInsetPx);
        Assert::AreEqual (50, r.topInsetPx);
    }
};

}   // namespace UiTests
