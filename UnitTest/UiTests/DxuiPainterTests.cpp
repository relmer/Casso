#include "Pch.h"

#include "CppUnitTest.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

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

        Assert::AreEqual (0, painter.PendingVertexCount());
    }
};

}   // namespace UiTests
