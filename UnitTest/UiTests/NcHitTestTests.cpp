#include "Pch.h"

#include "CppUnitTest.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (NcHitTestTests)
{
public:

    TEST_METHOD (Classify_DefaultsToClient)
    {
        DxuiNcHitTestInput  in;

        in.windowRectScreen = { 100, 100, 900, 700 };
        in.mouseXScreen     = 500;
        in.mouseYScreen     = 400;

        Assert::AreEqual ((LRESULT) HTCLIENT, DxuiHitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_TopLeftResizeEdge)
    {
        DxuiNcHitTestInput  in;

        in.windowRectScreen = { 100, 100, 900, 700 };
        in.resizeBorderPx   = 6;
        in.mouseXScreen     = 102;
        in.mouseYScreen     = 102;

        Assert::AreEqual ((LRESULT) HTTOPLEFT, DxuiHitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_BottomRightResizeEdge)
    {
        DxuiNcHitTestInput  in;

        in.windowRectScreen = { 100, 100, 900, 700 };
        in.resizeBorderPx   = 6;
        in.mouseXScreen     = 897;
        in.mouseYScreen     = 697;

        Assert::AreEqual ((LRESULT) HTBOTTOMRIGHT, DxuiHitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_ButtonRectBeatsCaption)
    {
        DxuiNcHitTestInput  in;

        in.windowRectScreen = { 0, 0, 800, 600 };
        in.resizeBorderPx   = 6;
        in.captionRect      = { 0, 0, 800, 32 };
        in.closeButtonRect  = { 750, 0, 800, 32 };
        in.mouseXScreen     = 770;
        in.mouseYScreen     = 16;

        Assert::AreEqual ((LRESULT) HTCLOSE, DxuiHitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_CaptionInsideTitleStripAwayFromButtons)
    {
        DxuiNcHitTestInput  in;

        in.windowRectScreen = { 0, 0, 800, 600 };
        in.resizeBorderPx   = 6;
        in.captionRect      = { 0, 0, 800, 32 };
        in.mouseXScreen     = 300;
        in.mouseYScreen     = 16;

        Assert::AreEqual ((LRESULT) HTCAPTION, DxuiHitTester::ClassifyNcHit (in));
    }
};

}   // namespace UiTests
