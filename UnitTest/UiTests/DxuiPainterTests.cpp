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



        painter.FillCircleApprox (kSizePx, kSizePx, kSizePx, 0xFFFFFFFF);
        Assert::AreEqual (kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillCircleApprox");

        painter.FillEllipseApprox (kSizePx, kSizePx, kSizePx, kRadiusPx, 0xFFFFFFFF);
        Assert::AreEqual (2 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillEllipseApprox");

        painter.FillRoundedRect (0.0f, 0.0f, kSizePx, kSizePx, kRadiusPx, 0xFFFFFFFF);
        Assert::AreEqual (3 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillRoundedRect");

        painter.OutlineRoundedRect (0.0f, 0.0f, kSizePx, kSizePx, kRadiusPx, 1.0f, 0xFFFFFFFF);
        Assert::AreEqual (4 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"OutlineRoundedRect");

        painter.DrawLineApprox (0.0f, 0.0f, kSizePx, kSizePx, 1.0f, 0xFFFFFFFF);
        Assert::AreEqual (5 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"DrawLineApprox");

        painter.FillConvexQuad (0.0f, 0.0f, kSizePx, 0.0f, kSizePx, kSizePx, 0.0f, kRadiusPx, 0xFFFFFFFF);
        Assert::AreEqual (6 * kVerticesPerQuad, painter.GetPendingVertexCount(), L"FillConvexQuad");
    }
};

}   // namespace UiTests
