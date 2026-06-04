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


    // Convenience: stack-allocated DxuiDpiScaler at the desired DPI.
    // LayoutManager binds to it by const-reference; mutations to the
    // scaler are picked up by subsequent layout queries.
    struct ScopedScaler
    {
        DxuiDpiScaler  scaler;
        explicit ScopedScaler (UINT dpi) { scaler.SetDpi (dpi); }
    };
}



TEST_CLASS (LayoutManagerTests)
{
public:

    TEST_METHOD (Empty_ZeroInsets_CenterRectIsFullClient)
    {
        ScopedScaler         s (96);
        LayoutManager        layout (s.scaler);
        LayoutManagerResult  r = layout.Resolve (800, 600);

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
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top (ChromeEdge::Top, 32);

        layout.RegisterEdge (&top);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (32,  r.topInsetPx);
        Assert::AreEqual (0,   r.bottomInsetPx);
        Assert::AreEqual (32L, r.centerRect.top);
        Assert::AreEqual (600L, r.centerRect.bottom);
    }


    TEST_METHOD (TwoTopEdges_StackThicknesses)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       titleBar (ChromeEdge::Top, 32);
        StubEdge       navStrip (ChromeEdge::Top, 32);

        layout.RegisterEdge (&titleBar);
        layout.RegisterEdge (&navStrip);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (64,  r.topInsetPx);
        Assert::AreEqual (64L, r.centerRect.top);
    }


    TEST_METHOD (FourEdges_AllSidesReserved)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    10);
        StubEdge       bottom (ChromeEdge::Bottom, 20);
        StubEdge       left   (ChromeEdge::Left,   30);
        StubEdge       right  (ChromeEdge::Right,  40);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);
        layout.RegisterEdge (&left);
        layout.RegisterEdge (&right);

        LayoutManagerResult  r = layout.Resolve (800, 600);

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
        ScopedScaler   s (192);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (64,  r.topInsetPx);
        Assert::AreEqual (384, r.bottomInsetPx);
    }


    TEST_METHOD (DpiScaling_150Percent_AtDpi144)
    {
        ScopedScaler   s (144);
        LayoutManager  layout (s.scaler);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (288, r.bottomInsetPx);
    }


    TEST_METHOD (DpiZero_TreatsAsBaseDpi)
    {
        ScopedScaler   s (0);   // DxuiDpiScaler::SetDpi(0) coerces to 96
        LayoutManager  layout (s.scaler);
        StubEdge       top (ChromeEdge::Top, 32);

        layout.RegisterEdge (&top);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (32, r.topInsetPx);
    }


    TEST_METHOD (CenterLayer_AddsPaddingInsideChromeInsets)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       bottom  (ChromeEdge::Bottom, 100);
        StubCenter     monitor (8, 16, 4, 4);

        layout.RegisterEdge (&bottom);
        layout.RegisterCenterLayer (&monitor);

        LayoutManagerResult  r = layout.Resolve (800, 600);

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
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    400);
        StubEdge       bottom (ChromeEdge::Bottom, 400);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        // Window is 600 tall; top+bottom = 800. centerRect must not invert.
        Assert::IsTrue (r.centerRect.bottom >= r.centerRect.top);
        Assert::IsTrue (r.centerRect.right  >= r.centerRect.left);
    }


    TEST_METHOD (ClientSizeForCenter_AddsAllEdgeAndCenterPadding)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top      (ChromeEdge::Top,    32);
        StubEdge       navStrip (ChromeEdge::Top,    32);
        StubEdge       bottom   (ChromeEdge::Bottom, 192);
        StubCenter     monitor  (8, 16, 4, 4);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&navStrip);
        layout.RegisterEdge (&bottom);
        layout.RegisterCenterLayer (&monitor);

        // Center pixels 560x384; chrome cost vertical = 32+32+192+8+16 = 280;
        // horizontal = 4+4 = 8.
        SIZE  sz = layout.ClientSizeForCenter (560, 384);

        Assert::AreEqual (568L, sz.cx);
        Assert::AreEqual (664L, sz.cy);
    }


    TEST_METHOD (ClientSizeForCenter_DpiScalesChromeInsetsOnly)
    {
        ScopedScaler   s (192);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        // ClientSizeForCenter expects PRE-SCALED center pixels. At 192
        // DPI chrome doubles: top=64 + bottom=384 = 448 added.
        SIZE  sz = layout.ClientSizeForCenter (1120, 768);

        Assert::AreEqual (1120L, sz.cx);
        Assert::AreEqual (1216L, sz.cy);
    }


    TEST_METHOD (ClientSizeForCenter_InvariantUnderRoundtrip)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        SIZE                 client = layout.ClientSizeForCenter (560, 384);
        LayoutManagerResult  r      = layout.Resolve ((int) client.cx, (int) client.cy);

        Assert::AreEqual (560L, r.centerRect.right  - r.centerRect.left);
        Assert::AreEqual (384L, r.centerRect.bottom - r.centerRect.top);
    }


    TEST_METHOD (ClientSizeForFramebuffer_LinearScaleAt96)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        SIZE  sz = layout.ClientSizeForFramebuffer (560, 384);

        Assert::AreEqual (560L, sz.cx);
        Assert::AreEqual ((LONG) (384 + 32 + 192), sz.cy);
    }


    TEST_METHOD (ClientSizeForFramebuffer_LinearScaleAt144)
    {
        ScopedScaler   s (144);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        // 1.5x: framebuffer = 840x576; chrome = top(48) + bottom(288) = 336.
        SIZE  sz = layout.ClientSizeForFramebuffer (560, 384);

        Assert::AreEqual (840L, sz.cx);
        Assert::AreEqual ((LONG) (576 + 48 + 288), sz.cy);
    }


    TEST_METHOD (ClientSizeForFramebuffer_LinearScaleAt192)
    {
        ScopedScaler   s (192);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);

        // 2x: framebuffer = 1120x768; chrome = top(64) + bottom(384) = 448.
        SIZE  sz = layout.ClientSizeForFramebuffer (560, 384);

        Assert::AreEqual (1120L, sz.cx);
        Assert::AreEqual ((LONG) (768 + 64 + 384), sz.cy);
    }


    TEST_METHOD (Unregister_RemovesContribution)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       top    (ChromeEdge::Top,    32);
        StubEdge       bottom (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&top);
        layout.RegisterEdge (&bottom);
        layout.UnregisterEdge (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (32, r.topInsetPx);
        Assert::AreEqual (0,  r.bottomInsetPx);
    }


    TEST_METHOD (Register_NullIsNoOp)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);

        layout.RegisterEdge ((IEdgeContributor *) nullptr);
        layout.RegisterCenterLayer ((ICenterLayer *) nullptr);

        Assert::AreEqual ((size_t) 0, layout.Edges().size());
        Assert::AreEqual ((size_t) 0, layout.CenterLayers().size());
    }


    TEST_METHOD (ContributorMutation_NextResolveReflects)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       driveBar (ChromeEdge::Bottom, 192);

        layout.RegisterEdge (&driveBar);

        LayoutManagerResult  before = layout.Resolve (800, 600);

        Assert::AreEqual (192, before.bottomInsetPx);

        driveBar.SetThickness (48);    // simulate compact-drives theme swap

        LayoutManagerResult  after = layout.Resolve (800, 600);

        Assert::AreEqual (48, after.bottomInsetPx);
    }


    TEST_METHOD (DpiScalerMutation_NextResolveReflects)
    {
        ScopedScaler   s (96);
        LayoutManager  layout (s.scaler);
        StubEdge       bottom (ChromeEdge::Bottom, 100);

        layout.RegisterEdge (&bottom);

        LayoutManagerResult  before = layout.Resolve (800, 600);

        Assert::AreEqual (100, before.bottomInsetPx);

        // Simulate WM_DPICHANGED bumping the underlying scaler.
        s.scaler.SetDpi (192);

        LayoutManagerResult  after = layout.Resolve (800, 600);

        Assert::AreEqual (200, after.bottomInsetPx);
    }


    TEST_METHOD (SimpleEdgeContributor_AsValueType_Works)
    {
        ScopedScaler           s (96);
        LayoutManager          layout (s.scaler);
        SimpleEdgeContributor  bottom (ChromeEdge::Bottom, 100);

        layout.RegisterEdge (&bottom);

        LayoutManagerResult  r = layout.Resolve (800, 600);

        Assert::AreEqual (100, r.bottomInsetPx);

        bottom.SetEdge        (ChromeEdge::Top);
        bottom.SetThicknessDp (50);

        r = layout.Resolve (800, 600);

        Assert::AreEqual (0,  r.bottomInsetPx);
        Assert::AreEqual (50, r.topInsetPx);
    }
};

}   // namespace UiTests
