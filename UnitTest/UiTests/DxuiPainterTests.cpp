#include "Pch.h"

#include "CppUnitTest.h"


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


    //  A fill wholly outside the clip draws nothing, one partly inside draws,
    //  and the clip ends at its PopClip.
    TEST_METHOD (PushClip_DropsWhatFallsOutside)
    {
        constexpr int  kVerticesPerQuad = 6;

        DxuiPainter  painter;



        painter.PushClip (0.0f, 0.0f, 10.0f, 10.0f);

        painter.FillRect (20.0f, 20.0f, 5.0f, 5.0f, 0xFFFFFFFF);
        Assert::AreEqual (0, painter.GetPendingVertexCount(), L"outside the clip");

        painter.FillRect (5.0f, 5.0f, 10.0f, 10.0f, 0xFFFFFFFF);
        Assert::AreEqual (kVerticesPerQuad, painter.GetPendingVertexCount(), L"partly inside");

        painter.PopClip();

        painter.FillRect (20.0f, 20.0f, 5.0f, 5.0f, 0xFFFFFFFF);
        Assert::AreEqual (2 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"after the clip is popped");
    }


    TEST_METHOD (TryClipRect_CutsToTheOverlap)
    {
        D2D1_RECT_F  clip = { 0.0f, 0.0f, 10.0f, 10.0f };
        float        x0   = 5.0f;
        float        y0   = -5.0f;
        float        x1   = 15.0f;
        float        y1   = 5.0f;



        Assert::IsTrue   (DxuiPainter::TryClipRect (clip, x0, y0, x1, y1));
        Assert::AreEqual (5.0f,  x0);
        Assert::AreEqual (0.0f,  y0);
        Assert::AreEqual (10.0f, x1);
        Assert::AreEqual (5.0f,  y1);

        x0 = 11.0f;
        y0 = 0.0f;
        x1 = 12.0f;
        y1 = 5.0f;

        Assert::IsFalse  (DxuiPainter::TryClipRect (clip, x0, y0, x1, y1), L"no overlap");
    }
};

}   // namespace UiTests
