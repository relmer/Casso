#include "Pch.h"

#include "Ui/Dialogs/SizeGrip.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SizeGripTests
//
//  Where the update dialog's resize gripper goes, what it draws, and what a
//  click on it means.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SizeGripTests)
{
public:

    TEST_METHOD (GripRect_IsFlushWithTheBottomRightCorner)
    {
        RECT  grip = SizeGrip::GetGripRect (RECT { 0, 0, 600, 500 }, 12);



        Assert::AreEqual (588L, grip.left);
        Assert::AreEqual (488L, grip.top);
        Assert::AreEqual (600L, grip.right);
        Assert::AreEqual (500L, grip.bottom);
    }



    TEST_METHOD (Grip_StaysInsideTheButtonRowsEdgePad)
    {
        //  The bottom-right button and the nudge both stop the edge pad short
        //  of the right edge, so a gripper no wider than the pad cannot meet
        //  them.
        Assert::IsTrue (SizeGrip::kSizeDip <= DxuiButtonRow::kEdgePadDip);
    }



    TEST_METHOD (Dots_FormA45DegreeTriangleInsideTheGrip)
    {
        RECT                          grip = { 88, 88, 100, 100 };
        std::vector<SizeGrip::Dot>    dots = SizeGrip::GetDots (grip, 2.0f, 4.0f);
        size_t                        i    = 0;



        Assert::AreEqual ((size_t) 6, dots.size(), L"one, two, then three dots");

        for (i = 0; i < dots.size(); i++)
        {
            Assert::IsTrue (dots[i].x >= (float) grip.left && dots[i].x + dots[i].size <= (float) grip.right);
            Assert::IsTrue (dots[i].y >= (float) grip.top  && dots[i].y + dots[i].size <= (float) grip.bottom);
        }

        //  The top row is the single dot at the right; each dot sits on or
        //  below the 45-degree line from the grip's bottom-left corner.
        Assert::AreEqual (dots[0].x, dots[3].x, L"every row ends at the same right column");
        Assert::IsTrue   (dots[0].y < dots[1].y && dots[1].y < dots[3].y);

        for (const SizeGrip::Dot & dot : dots)
        {
            Assert::IsTrue (dot.x + dot.size - (float) grip.left >= (float) grip.bottom - (dot.y + dot.size) - 0.01f,
                            L"no dot above the diagonal");
        }
    }



    TEST_METHOD (Hit_IsTheBottomRightResizeCorner)
    {
        SizeGrip  grip;



        Assert::IsTrue (grip.ClassifyHit (POINT { 5, 5 }) == DxuiHitTestKind::ResizeCornerBR);
    }
};
