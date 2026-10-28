#include "Pch.h"

#include "Ui/Chrome/BranchPreview.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace BranchPreviewTests
{
    static constexpr RECT   kArea   = { 0, 0, 800, 400 };
    static constexpr int    kGap    = 8;
    static constexpr float  kAspect = 1.5f;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchPreviewTests
    //
    //  Where a branch's frame goes beside the rewind bar's flyout: on the
    //  side with more room, level with the bottom of the screen above the
    //  bar, or across that whole screen when neither side has room.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BranchPreviewTests)
    {
    public:

        static void  AssertRect (const RECT & expected, const RECT & actual, const wchar_t * what)
        {
            Assert::IsTrue (EqualRect (&expected, &actual) != FALSE, what);
        }


        TEST_METHOD (AFlyoutAtTheLeftPutsTheFrameAtTheRight)
        {
            RECT  flyout = { 40, 100, 440, 392 };



            AssertRect ({ 448, 163, 792, 392 }, BranchPreview::FitBeside (kArea, flyout, kAspect, kGap), L"the width right of the flyout, level with the bottom");
        }


        TEST_METHOD (AFlyoutAtTheRightPutsTheFrameAtTheLeft)
        {
            RECT  flyout = { 360, 100, 760, 392 };



            AssertRect ({ 8, 163, 352, 392 }, BranchPreview::FitBeside (kArea, flyout, kAspect, kGap), L"the width left of the flyout, level with the bottom");
        }


        TEST_METHOD (WithNoRoomBesideTheFlyoutTheFrameFillsTheScreenAboveTheBar)
        {
            RECT  flyout = { 100, 100, 700, 392 };



            AssertRect ({ 112, 8, 688, 392 }, BranchPreview::FitBeside (kArea, flyout, kAspect, kGap), L"the height of the area, centered across it");
        }
    };
}
