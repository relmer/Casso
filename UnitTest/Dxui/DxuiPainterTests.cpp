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
};

}   // namespace UiTests
