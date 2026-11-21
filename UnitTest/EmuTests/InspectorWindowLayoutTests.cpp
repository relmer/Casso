#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorWindowLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorWindowLayoutTests
//
//  Where the disk inspector's platter column, splitter, track column and
//  strips go, at 96 dpi.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorWindowLayoutTests)
{
public:

    static InspectorWindowLayout::Inputs MakeInputs()
    {
        InspectorWindowLayout::Inputs  in;



        in.bounds    = { 0, 0, 1280, 800 };
        in.toolbarPx = 40;
        in.sectorsPx = 500;

        return in;
    }


    static DxuiDpiScaler MakeScaler()
    {
        DxuiDpiScaler  scaler;



        scaler.SetDpi (96);

        return scaler;
    }


    TEST_METHOD (Splitter_SitsBetweenTheColumns)
    {
        InspectorWindowLayout::Frames  f = InspectorWindowLayout::Plan (MakeInputs(), MakeScaler());



        Assert::IsTrue (f.platterArea.right == f.splitter.left);
        Assert::IsTrue (f.trackColumn.left > f.splitter.right);
        Assert::IsTrue (f.platterColumn.right < f.splitter.left);
    }


    TEST_METHOD (PlatterColumn_LeavesTheSectorBytesTheirWidth)
    {
        InspectorWindowLayout::Inputs  in = MakeInputs();
        InspectorWindowLayout::Frames  f;



        in.splitFraction = InspectorWindowLayout::kMaxSplit;
        f                = InspectorWindowLayout::Plan (in, MakeScaler());

        Assert::IsTrue (f.trackColumn.right - f.trackColumn.left >= in.sectorsPx);
    }


    TEST_METHOD (PlatterColumn_IsAtLeastItsControlRow)
    {
        InspectorWindowLayout::Inputs  in = MakeInputs();
        InspectorWindowLayout::Frames  f;



        in.splitFraction = InspectorWindowLayout::kMinSplit;
        in.minColumnPx   = 480;
        f                = InspectorWindowLayout::Plan (in, MakeScaler());

        Assert::IsTrue (f.platterColumn.right - f.platterColumn.left >= in.minColumnPx);
    }


    TEST_METHOD (Comparing_AddsTheBarAndBsStrip)
    {
        InspectorWindowLayout::Inputs  in = MakeInputs();
        InspectorWindowLayout::Frames  one;
        InspectorWindowLayout::Frames  two;



        one            = InspectorWindowLayout::Plan (in, MakeScaler());
        in.isComparing = true;
        two            = InspectorWindowLayout::Plan (in, MakeScaler());

        Assert::AreEqual (0L, one.compareBar.bottom - one.compareBar.top);
        Assert::IsTrue   (two.compareBar.bottom > two.compareBar.top);
        Assert::AreEqual (0L, one.stripB.bottom - one.stripB.top);
        Assert::IsTrue   (two.stripB.top >= two.strip.bottom);
        Assert::IsTrue   (two.stripB.bottom > two.stripB.top);
        Assert::IsTrue   (two.stripArea.top < one.stripArea.top);
    }


    TEST_METHOD (Platter_StaysAboveTheCornerBand)
    {
        InspectorWindowLayout::Inputs  in = MakeInputs();
        InspectorWindowLayout::Frames  f;



        in.cornerBandPx = 60;
        f               = InspectorWindowLayout::Plan (in, MakeScaler());

        Assert::IsTrue (f.platter.bottom <= f.platterColumn.bottom - in.cornerBandPx);
        Assert::IsTrue (f.stripControls.bottom <= in.bounds.bottom);
    }
};
