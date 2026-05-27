#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/HitTester.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UiTests
{

TEST_CLASS (NcHitTestTests)
{
public:

    TEST_METHOD (Classify_DefaultsToClient)
    {
        NcHitTestInput  in;

        in.windowRectScreen = { 100, 100, 900, 700 };
        in.mouseXScreen     = 500;
        in.mouseYScreen     = 400;

        Assert::AreEqual ((LRESULT) HTCLIENT, HitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_TopLeftResizeEdge)
    {
        NcHitTestInput  in;

        in.windowRectScreen = { 100, 100, 900, 700 };
        in.resizeBorderPx   = 6;
        in.mouseXScreen     = 102;
        in.mouseYScreen     = 102;

        Assert::AreEqual ((LRESULT) HTTOPLEFT, HitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_BottomRightResizeEdge)
    {
        NcHitTestInput  in;

        in.windowRectScreen = { 100, 100, 900, 700 };
        in.resizeBorderPx   = 6;
        in.mouseXScreen     = 897;
        in.mouseYScreen     = 697;

        Assert::AreEqual ((LRESULT) HTBOTTOMRIGHT, HitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_ButtonRectBeatsCaption)
    {
        NcHitTestInput  in;

        in.windowRectScreen = { 0, 0, 800, 600 };
        in.resizeBorderPx   = 6;
        in.captionRect      = { 0, 0, 800, 32 };
        in.closeButtonRect  = { 750, 0, 800, 32 };
        in.mouseXScreen     = 770;
        in.mouseYScreen     = 16;

        Assert::AreEqual ((LRESULT) HTCLOSE, HitTester::ClassifyNcHit (in));
    }


    TEST_METHOD (Classify_CaptionInsideTitleStripAwayFromButtons)
    {
        NcHitTestInput  in;

        in.windowRectScreen = { 0, 0, 800, 600 };
        in.resizeBorderPx   = 6;
        in.captionRect      = { 0, 0, 800, 32 };
        in.mouseXScreen     = 300;
        in.mouseYScreen     = 16;

        Assert::AreEqual ((LRESULT) HTCAPTION, HitTester::ClassifyNcHit (in));
    }
};

}   // namespace UiTests
