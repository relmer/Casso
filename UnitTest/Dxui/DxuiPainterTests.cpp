#include "Pch.h"

#include "CppUnitTest.h"

#include "../EmuTests/WarpRenderHarness.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{





////////////////////////////////////////////////////////////////////////////////
//
//  TEST_CLASS
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPainterTests)
{
public:

    TEST_METHOD (End_WithNullRtvNoOpsCleanly)
    {
        DxuiPainter  painter;

        HRESULT  hr = painter.End (nullptr);

        Assert::AreEqual ((HRESULT) S_OK, hr);
    }


    TEST_METHOD (Shutdown_IsIdempotent)
    {
        DxuiPainter  painter;

        painter.Shutdown();
        painter.Shutdown();

        Assert::AreEqual (0, painter.GetPendingVertexCount());
    }


    // Curved and diagonal primitives take their edges from a distance
    // function in the pixel shader, so each is one quad regardless of size.
    // Scanline slicing emitted a quad per row or step, dozens at these sizes.
    TEST_METHOD (ShapedPrimitives_EmitOneQuadEach)
    {
        constexpr int    kVerticesPerQuad = 6;
        constexpr float  kSizePx          = 40.0f;
        constexpr float  kRadiusPx        = 8.0f;

        DxuiPainter  painter;



        painter.FillCircle (kSizePx, kSizePx, kSizePx, 0xFFFFFFFF);
        Assert::AreEqual (kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillCircle");

        painter.FillEllipse (kSizePx, kSizePx, kSizePx, kRadiusPx, 0xFFFFFFFF);
        Assert::AreEqual (2 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillEllipse");

        painter.FillRoundedRect (0.0f, 0.0f, kSizePx, kSizePx, kRadiusPx, 0xFFFFFFFF);
        Assert::AreEqual (3 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillRoundedRect");

        painter.OutlineRoundedRect (0.0f, 0.0f, kSizePx, kSizePx, kRadiusPx, 1.0f, 0xFFFFFFFF);
        Assert::AreEqual (4 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"OutlineRoundedRect");

        painter.DrawLine (0.0f, 0.0f, kSizePx, kSizePx, 1.0f, 0xFFFFFFFF);
        Assert::AreEqual (5 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"DrawLine");

        painter.FillConvexQuad (0.0f, 0.0f, kSizePx, 0.0f, kSizePx, kSizePx, 0.0f, kRadiusPx, 0xFFFFFFFF);
        Assert::AreEqual (6 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillConvexQuad");
    }


    // A clip trims shapes that cross it and drops shapes wholly outside it,
    // which is what keeps a scrolled page's controls off the tab strip and
    // the button row.
    TEST_METHOD (ClipRect_DropsShapesOutsideAndKeepsCrossingOnes)
    {
        constexpr int  kVerticesPerQuad = 6;

        DxuiPainter  painter;
        RECT         clip = { 0, 100, 400, 300 };



        painter.SetClipRect (&clip);

        painter.FillRect (10.0f, 20.0f, 50.0f, 40.0f, 0xFFFFFFFF);
        Assert::AreEqual (0, painter.GetPendingVertexCount(), L"above the clip");

        painter.FillRect (10.0f, 320.0f, 50.0f, 40.0f, 0xFFFFFFFF);
        Assert::AreEqual (0, painter.GetPendingVertexCount(), L"below the clip");

        painter.FillRoundedRect (10.0f, 80.0f, 50.0f, 40.0f, 8.0f, 0xFFFFFFFF);
        Assert::AreEqual (kVerticesPerQuad, painter.GetPendingVertexCount(), L"crossing the top edge");

        painter.SetClipRect (nullptr);

        painter.FillRect (10.0f, 20.0f, 50.0f, 40.0f, 0xFFFFFFFF);
        Assert::AreEqual (2 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"clip cleared");
    }


    // Stretches of a ring that abut on hard ends cover every pixel between
    // them, even with the disk's center hundreds of thousands of pixels off
    // the target and the clip trimming each stretch's quad, as on a platter
    // zoomed far in. A pixel no stretch covers stays black.
    TEST_METHOD (RingSectors_FarFromTheCenter_LeaveNoGapsAtTheirEnds)
    {
        constexpr int       kSize      = 256;
        constexpr int       kClipInset = 16;
        constexpr int       kPieces    = 16;
        constexpr float     kRadiusPx  = 400000.0f;
        constexpr float     kHalfBand  = 400.0f;
        constexpr float     kEyeRatio  = 3.0f;    // the platter's eye distance, in disk radii
        constexpr double    kPieceArc  = 24.0;
        constexpr double    kMarginPx  = 1.0;
        constexpr double    kDirection = 0.785398163397448;   // 45 degrees, so both of the center's coordinates are far off
        constexpr uint32_t  kFill      = 0xFF00FF00;

        WarpRenderHarness               harness;
        ComPtr<ID3D11Texture2D>         texture;
        ComPtr<ID3D11RenderTargetView>  rtv;
        DxuiPainter                     painter;
        std::vector<uint32_t>           pixels;
        RECT                            clip     = { kClipInset, kClipInset, kSize - kClipInset, kSize - kClipInset };
        const float                     black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float                           mid      = kSize / 2.0f + 0.5f;
        float                           cx       = mid - kRadiusPx * (float) std::sin (kDirection);
        float                           cy       = mid + kRadiusPx * (float) std::cos (kDirection);
        double                          step     = kPieceArc / kRadiusPx;
        double                          first    = kDirection - step * kPieces / 2;
        double                          dx       = 0;
        double                          dy       = 0;
        double                          r        = 0;
        double                          angle    = 0;
        int                             checked  = 0;
        int                             gaps     = 0;



        Assert::IsTrue (harness.IsAvailable(), L"no WARP device");

        DxuiResetUiThreadIdForTest();

        AssertSucceeded (harness.MakeTarget (kSize, kSize, texture, rtv));
        AssertSucceeded (painter.Initialize (harness.GetDevice(), harness.GetContext()));
        harness.GetContext()->ClearRenderTargetView (rtv.Get(), black);

        AssertSucceeded (painter.Begin (kSize, kSize));
        painter.SetClipRect (&clip);

        for (int k = 0; k < kPieces; k++)
        {
            painter.FillRingSector (cx, cy, kRadiusPx * kEyeRatio, 0.0f, kRadiusPx - kHalfBand, kRadiusPx + kHalfBand,
                                    first + step * k, first + step * (k + 1), (k > 0 ? 1 : 0) | (k < kPieces - 1 ? 2 : 0), kFill);
        }

        AssertSucceeded (painter.End (rtv.Get()));
        AssertSucceeded (harness.ReadBack (texture.Get(), pixels));

        //  Every pixel in the clip a pixel or more inside the run of stretches.
        for (int y = clip.top; y < clip.bottom; y++)
        {
            for (int x = clip.left; x < clip.right; x++)
            {
                dx    = x + 0.5 - cx;
                dy    = y + 0.5 - cy;
                r     = std::sqrt (dx * dx + dy * dy);
                angle = std::atan2 (dx, -dy);

                if (std::abs (r - kRadiusPx) < kHalfBand - kMarginPx &&
                    angle > first + kMarginPx / kRadiusPx && angle < first + step * kPieces - kMarginPx / kRadiusPx)
                {
                    checked++;
                    gaps += (pixels[(size_t) y * kSize + (size_t) x] != kFill) ? 1 : 0;
                }
            }
        }

        Assert::IsTrue (checked > 0, L"no pixel lies inside the stretches");
        Assert::AreEqual (0, gaps, std::format (L"uncovered pixels of {}", checked).c_str());
    }
};

}   // namespace UiTests
