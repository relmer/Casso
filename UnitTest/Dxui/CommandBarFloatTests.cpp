#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarFloatTests
//
//  The command bar floating: when a drag tears it off, when a drop docks
//  it again, and the text its floating place is saved as.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CommandBarFloatTests)
{
public:

    static constexpr RECT  kArea = { 0, 100, 1000, 800 };


    TEST_METHOD (APointerNearAnEdgeIsInTheDockBand)
    {
        Assert::IsTrue (DxuiToolbarDock::IsInDockBand (POINT { 500, 120 }, kArea, 40), L"near the top");
        Assert::IsTrue (DxuiToolbarDock::IsInDockBand (POINT { 980, 400 }, kArea, 40), L"near the right");
        Assert::IsTrue (DxuiToolbarDock::IsInDockBand (POINT { 500, 800 }, kArea, 40), L"on the bottom edge");
    }


    TEST_METHOD (APointerInTheMiddleOrOutsideTearsTheBarOff)
    {
        Assert::IsFalse (DxuiToolbarDock::IsInDockBand (POINT { 500, 400 }, kArea, 40), L"the middle");
        Assert::IsFalse (DxuiToolbarDock::IsInDockBand (POINT { 500,  80 }, kArea, 40), L"over the menu bar");
        Assert::IsFalse (DxuiToolbarDock::IsInDockBand (POINT { -5, 400 },  kArea, 40), L"off the window");
    }


    TEST_METHOD (AFloatingPlaceRoundTripsThroughText)
    {
        DxuiToolbarDock  dock;


        dock.floating = true;
        dock.floatPx  = POINT { 300, -40 };

        Assert::AreEqual (std::wstring (L"float 300 -40"), dock.ToText());
        Assert::IsTrue   (DxuiToolbarDock::FromText (dock.ToText()) == dock);
        Assert::IsTrue   (DxuiToolbarDock::FromText (L"float 300 -40").floating);
    }


    TEST_METHOD (FloatingTextThatDoesNotReadGivesTheTop)
    {
        for (const wchar_t * text : { L"float", L"float 3", L"float 3 x", L"float 3 4 5", L"float x 4" })
        {
            Assert::IsTrue (DxuiToolbarDock::FromText (text) == DxuiToolbarDock {}, text);
        }
    }


    TEST_METHOD (AFloatingBarIsNotADockedOne)
    {
        DxuiToolbarDock  floating;


        floating.floating = true;

        Assert::IsFalse (floating == DxuiToolbarDock {});
    }
};
