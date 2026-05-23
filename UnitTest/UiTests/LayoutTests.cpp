#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/Layout.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (LayoutTests)
{
public:

    TEST_METHOD (Stack_Horizontal_EqualWeights_SplitsEvenly)
    {
        RECT                          container = { 0, 0, 100, 50 };
        std::vector<LayoutChildSize>  children;

        children.push_back ({ 0, 0, 1 });
        children.push_back ({ 0, 0, 1 });
        children.push_back ({ 0, 0, 1 });

        std::vector<RECT>  result = Layout::Stack (container, LayoutOrientation::Horizontal, 0, children);

        Assert::AreEqual ((size_t) 3,                       result.size());
        Assert::AreEqual ((LONG) 0,                          result[0].left);
        Assert::AreEqual ((LONG) (100 / 3),                  result[0].right);
        Assert::AreEqual ((LONG) (100 / 3) * 2,              result[1].right);
        Assert::AreEqual ((LONG) 50,                         result[0].bottom);
    }


    TEST_METHOD (Stack_Vertical_FixedSizes_StackTopToBottom)
    {
        RECT                          container = { 0, 0, 80, 200 };
        std::vector<LayoutChildSize>  children;

        children.push_back ({ 0, 30, 0 });
        children.push_back ({ 0, 40, 0 });
        children.push_back ({ 0, 50, 0 });

        std::vector<RECT>  result = Layout::Stack (container, LayoutOrientation::Vertical, 10, children);

        Assert::AreEqual ((size_t) 3,            result.size());
        Assert::AreEqual ((LONG) 0,              result[0].top);
        Assert::AreEqual ((LONG) 30,             result[0].bottom);
        Assert::AreEqual ((LONG) 40,             result[1].top);
        Assert::AreEqual ((LONG) 80,             result[1].bottom);
        Assert::AreEqual ((LONG) 90,             result[2].top);
        Assert::AreEqual ((LONG) 140,            result[2].bottom);
    }


    TEST_METHOD (Grid_FillsRowMajor)
    {
        RECT  container = { 0, 0, 200, 100 };

        std::vector<RECT>  result = Layout::Grid (container, 4, 2, 0, 8);

        Assert::AreEqual ((size_t) 8,  result.size());
        Assert::AreEqual ((LONG) 50,   result[0].right);
        Assert::AreEqual ((LONG) 50,   result[0].bottom);
        Assert::AreEqual ((LONG) 50,   result[1].left);
        Assert::AreEqual ((LONG) 50,   result[4].top);
    }


    TEST_METHOD (ScalePx_AtBaseDpiIsIdentity)
    {
        Assert::AreEqual (16, Layout::ScalePx (16, 96));
    }


    TEST_METHOD (ScalePx_At150PercentScalesUp)
    {
        Assert::AreEqual (24, Layout::ScalePx (16, 144));
    }


    TEST_METHOD (ScalePx_ZeroDpiTreatsAsBaseDpi)
    {
        Assert::AreEqual (16, Layout::ScalePx (16, 0));
    }
};

}   // namespace UiTests
