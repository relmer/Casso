#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDockEdgesTests
//
//  The edge a pinned pane's group hides against, in a 1000 by 600 docked
//  area: the nearest, and where a group touches a side and the top or
//  bottom, the side for a side bar no wider than half the area and the top
//  or bottom for a wider group.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiDockEdgesTests)
{
public:

    static constexpr RECT  kArea = { 0, 0, 1000, 600 };


    TEST_METHOD (NearestEdge_LeftColumnHidesLeft)
    {
        Assert::IsTrue (DxuiDockEdges::GetNearestEdge (RECT { 0, 0, 300, 600 }, kArea) == DxuiDockSide::Left);
    }


    TEST_METHOD (NearestEdge_TopOfTheRightColumnHidesRight)
    {
        Assert::IsTrue (DxuiDockEdges::GetNearestEdge (RECT { 700, 0, 1000, 300 }, kArea) == DxuiDockSide::Right,
                        L"a side bar touching the top hides to its side");
    }


    TEST_METHOD (NearestEdge_WideBottomRowHidesToTheBottom)
    {
        Assert::IsTrue (DxuiDockEdges::GetNearestEdge (RECT { 0, 400, 1000, 600 }, kArea) == DxuiDockSide::Bottom,
                        L"a group wider than half the area touching a side hides to the bottom");
    }


    TEST_METHOD (NearestEdge_GroupInTheMiddleHidesToItsNearestEdge)
    {
        Assert::IsTrue (DxuiDockEdges::GetNearestEdge (RECT { 300, 100, 700, 450 }, kArea) == DxuiDockSide::Top);
        Assert::IsTrue (DxuiDockEdges::GetNearestEdge (RECT { 300, 200, 700, 550 }, kArea) == DxuiDockSide::Bottom);
    }
};
