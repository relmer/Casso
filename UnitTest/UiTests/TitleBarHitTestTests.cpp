#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarHitTestTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TitleBarHitTestTests)
{
public:

    // Build a fully populated input describing a 1000x600 window with a
    // 36px-tall title bar and three 40x36 system buttons on the right.
    static DxuiTitleBarHitTestInput MakeStandard()
    {
        DxuiTitleBarHitTestInput in;

        in.clientWidth  = 1000;
        in.clientHeight = 600;
        in.titleLeft    = 0;
        in.titleTop     = 0;
        in.titleRight   = 1000;
        in.titleBottom  = 36;

        in.minLeft   = 880; in.minTop   = 0; in.minRight   = 920; in.minBottom   = 36;
        in.maxLeft   = 920; in.maxTop   = 0; in.maxRight   = 960; in.maxBottom   = 36;
        in.closeLeft = 960; in.closeTop = 0; in.closeRight = 1000; in.closeBottom = 36;

        in.resizeBorderPx = 8;
        return in;
    }


    TEST_METHOD (Center_Of_Title_Returns_HTCAPTION)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 500;
        in.mouseY = 18;
        Assert::AreEqual ((LRESULT) HTCAPTION, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Center_Of_Client_Returns_HTCLIENT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 500;
        in.mouseY = 300;
        Assert::AreEqual ((LRESULT) HTCLIENT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Top_Edge_Returns_HTTOP)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 500;
        in.mouseY = 3;
        Assert::AreEqual ((LRESULT) HTTOP, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Bottom_Edge_Returns_HTBOTTOM)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 500;
        in.mouseY = 597;
        Assert::AreEqual ((LRESULT) HTBOTTOM, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Left_Edge_Returns_HTLEFT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 3;
        in.mouseY = 300;
        Assert::AreEqual ((LRESULT) HTLEFT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Right_Edge_Returns_HTRIGHT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 997;
        in.mouseY = 300;
        Assert::AreEqual ((LRESULT) HTRIGHT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (TopLeft_Corner_Returns_HTTOPLEFT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 3;
        in.mouseY = 3;
        Assert::AreEqual ((LRESULT) HTTOPLEFT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (TopRight_Corner_Returns_HTTOPRIGHT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 997;
        in.mouseY = 3;
        Assert::AreEqual ((LRESULT) HTTOPRIGHT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (BottomLeft_Corner_Returns_HTBOTTOMLEFT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 3;
        in.mouseY = 597;
        Assert::AreEqual ((LRESULT) HTBOTTOMLEFT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (BottomRight_Corner_Returns_HTBOTTOMRIGHT)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 997;
        in.mouseY = 597;
        Assert::AreEqual ((LRESULT) HTBOTTOMRIGHT, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Close_Button_Returns_HTCLOSE)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 980;
        in.mouseY = 18;
        Assert::AreEqual ((LRESULT) HTCLOSE, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Max_Button_Returns_HTMAXBUTTON)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 940;
        in.mouseY = 18;
        Assert::AreEqual ((LRESULT) HTMAXBUTTON, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Min_Button_Returns_HTMINBUTTON)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 900;
        in.mouseY = 18;
        Assert::AreEqual ((LRESULT) HTMINBUTTON, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Button_Beats_Caption_When_Overlapping)
    {
        // Verify button rects win even though they lie within the title rect.
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 980;
        in.mouseY = 18;
        Assert::AreEqual ((LRESULT) HTCLOSE, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Top_Edge_Beats_Close_Button_When_On_Resize_Border)
    {
        // y=3 is in the resize border AND the close-button rect; per
        // priority order, the resize edge wins so the user can resize
        // even when the close button is at the top edge.
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.mouseX = 980;
        in.mouseY = 3;
        Assert::AreEqual ((LRESULT) HTTOP, DxuiTitleBarHitTest::Test (in));
    }


    TEST_METHOD (Empty_TitleBar_Falls_Through_To_Client)
    {
        DxuiTitleBarHitTestInput  in = MakeStandard();
        in.titleBottom = in.titleTop;  // collapse title
        in.minRight = in.minLeft;
        in.maxRight = in.maxLeft;
        in.closeRight = in.closeLeft;
        in.mouseX = 500;
        in.mouseY = 18;
        Assert::AreEqual ((LRESULT) HTCLIENT, DxuiTitleBarHitTest::Test (in));
    }
};
