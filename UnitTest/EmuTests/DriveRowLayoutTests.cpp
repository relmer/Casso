#include "Pch.h"

#include "Shell/Layout/DriveRowLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveRowLayoutTests
//
//  Where the drive widgets sit along the bottom of the window.
//
//  Every part of this arithmetic has been wrong at least once, and each time it
//  read as a row hanging slightly off to one side rather than as anything
//  obviously broken -- the kind of thing that ships because it looks like a
//  design choice.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveRowLayoutTests)
{
public:

    TEST_METHOD (ASingleDriveIsCentered)
    {
        //  Not offset with a gap where a second drive would sit. A //c with its
        //  external drive unconnected lays out both and hides the second right
        //  after, so centering on the ARRAY left the one visible drive sitting
        //  left of center by half a widget and a gap.
        int  originX = DriveRowLayout::ComputeRowOriginX (1000, 200, 40, 1);

        Assert::AreEqual (400, originX, L"(1000 - 200) / 2");
    }


    TEST_METHOD (TwoDrivesCenterOnThePair)
    {
        //  200 + 40 + 200 = 440 wide, so (1000 - 440) / 2.
        int  originX = DriveRowLayout::ComputeRowOriginX (1000, 200, 40, 2);

        Assert::AreEqual (280, originX);
    }


    TEST_METHOD (TheRowNeverStartsOffTheLeftEdge)
    {
        //  A window narrower than the drives it carries overflows to the right,
        //  where it costs visibility. Overflowing left would put the row where
        //  it cannot be seen at all.
        Assert::AreEqual (0, DriveRowLayout::ComputeRowOriginX (100, 200, 40, 1));
        Assert::AreEqual (0, DriveRowLayout::ComputeRowOriginX (100, 200, 40, 2));
        Assert::AreEqual (0, DriveRowLayout::ComputeRowOriginX (0,   200, 40, 2));
    }


    TEST_METHOD (AVisibleCountOutsideTheRangeIsClamped)
    {
        //  Zero drives still lays out one: the widget exists and has to go
        //  somewhere sensible. Three is not a machine Casso has.
        Assert::AreEqual (DriveRowLayout::ComputeRowOriginX (1000, 200, 40, 1),
                          DriveRowLayout::ComputeRowOriginX (1000, 200, 40, 0));
        Assert::AreEqual (DriveRowLayout::ComputeRowOriginX (1000, 200, 40, 2),
                          DriveRowLayout::ComputeRowOriginX (1000, 200, 40, 5));
    }


    TEST_METHOD (OnlyALoneDriveIsNudgedForItsCaption)
    {
        //  The 2D widget hangs its "DRIVE 1" caption off to the left, so a lone
        //  drive centers on the part carrying the weight. A pair does not: the
        //  caption then reads as part of a repeating unit.
        Assert::AreEqual (380, DriveRowLayout::ApplyLoneDriveCaptionOffset (400, 40, 1));
        Assert::AreEqual (280, DriveRowLayout::ApplyLoneDriveCaptionOffset (280, 40, 2));
    }


    TEST_METHOD (AWidgetWithNoCaptionLeadIsNotMoved)
    {
        //  The full skeuomorphic drive's body starts at its own left edge, so
        //  it subtracts nothing. The offset is measured off the widget rather
        //  than assumed, which is what keeps the two presentations in step.
        Assert::AreEqual (400, DriveRowLayout::ApplyLoneDriveCaptionOffset (400, 0, 1));
    }


    TEST_METHOD (TheCaptionNudgeNeverPushesTheRowOffScreen)
    {
        Assert::AreEqual (0, DriveRowLayout::ApplyLoneDriveCaptionOffset (10, 400, 1));
    }


    TEST_METHOD (WidgetsSitOneGapApart)
    {
        Assert::AreEqual (280, DriveRowLayout::ComputeWidgetX (280, 0, 200, 40));
        Assert::AreEqual (520, DriveRowLayout::ComputeWidgetX (280, 1, 200, 40));
    }


    TEST_METHOD (SkewLeansEachDriveTowardTheClientCenter)
    {
        //  Drives left of center lean right and drives right of center lean
        //  left, so a pair reads as two objects on one desk under one monitor
        //  rather than as the same sprite drawn twice.
        int  left  = DriveRowLayout::ComputePerspectiveSkewPx (1000, 100, 200);
        int  right = DriveRowLayout::ComputePerspectiveSkewPx (1000, 700, 200);

        Assert::IsTrue (left  > 0, L"a drive left of center leans toward it");
        Assert::IsTrue (right < 0, L"and one right of center leans back the other way");
    }


    TEST_METHOD (ADriveOnTheCenterLineDoesNotLean)
    {
        //  Centered widget: its center is the vanishing point, so there is
        //  nowhere to lean.
        Assert::AreEqual (0, DriveRowLayout::ComputePerspectiveSkewPx (1000, 400, 200));
    }
};
