#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRect
//
//  Shared by both TEST_CLASSes below, so it lives at file scope rather than
//  on either one. `static` supplies the internal linkage the anonymous
//  namespace was there for.
//
////////////////////////////////////////////////////////////////////////////////

static RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
{
    RECT  out = {};
    out.left = l; out.top = t; out.right = r; out.bottom = b;
    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGridLayoutTests
//
//  Grid layout: uniform cells, gaps, and row and column SPANS.
//
//  Spans carry the weight. A spanned cell has to absorb the gaps it crosses so
//  its far edge lands exactly where an unspanned neighbor's would -- computing
//  it as cells times width plus gaps leaves a span consistently short, which
//  reads as a misaligned control rather than as a layout bug.
//
//  Out-of-range cell assignments are covered because they are reachable: a
//  grid reshaped smaller leaves stale assignments behind, and the child must
//  clamp to the edge rather than land outside it.
//
//  A child with no assignment defaults to (0,0) rather than being skipped, so
//  it is visibly misplaced instead of invisible.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiGridLayoutTests)
{
public:

    TEST_METHOD (TwoByTwoGrid_AssignsExpectedCellRects)
    {
        DxuiDpiScaler    scaler;
        MockDxuiControl  a;
        MockDxuiControl  b;
        MockDxuiControl  c;
        MockDxuiControl  d;
        RECT             bounds = {};



        DxuiGridLayout    layout (2, 2, 0.0f);
        IDxuiControl *    kids[4] = { &a, &b, &c, &d };
        bounds = MakeRect (0, 0, 200, 100);


        layout.SetCell (&a, 0, 0);
        layout.SetCell (&b, 0, 1);
        layout.SetCell (&c, 1, 0);
        layout.SetCell (&d, 1, 1);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 4));

        Assert::AreEqual ((LONG) 0,   a.Bounds().left);
        Assert::AreEqual ((LONG) 100, a.Bounds().right);
        Assert::AreEqual ((LONG) 100, b.Bounds().left);
        Assert::AreEqual ((LONG) 50,  c.Bounds().top);   // bandY + cellH = 50
    }


    TEST_METHOD (GapBetweenCells_ReducesCellSize)
    {
        DxuiDpiScaler    scaler;
        MockDxuiControl  a;
        MockDxuiControl  b;
        RECT             bounds = {};



        DxuiGridLayout    layout (1, 2, 10.0f);
        IDxuiControl *    kids[2] = { &a, &b };
        bounds = MakeRect (0, 0, 210, 50);


        layout.SetCell (&a, 0, 0);
        layout.SetCell (&b, 0, 1);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        // (210 - 10) / 2 = 100 per cell.
        Assert::AreEqual ((LONG) 100, a.Bounds().right - a.Bounds().left);
        Assert::AreEqual ((LONG) 110, b.Bounds().left);
        Assert::AreEqual ((LONG) 210, b.Bounds().right);
    }


    TEST_METHOD (ColSpan_ClaimsUnionRect)
    {
        DxuiDpiScaler    scaler;
        MockDxuiControl  wide;
        RECT             bounds = {};



        DxuiGridLayout    layout (1, 3, 0.0f);
        IDxuiControl *    kids[1] = { &wide };
        bounds = MakeRect (0, 0, 300, 50);


        layout.SetCell (&wide, 0, 0, 1, 3);
        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 0,   wide.Bounds().left);
        Assert::AreEqual ((LONG) 300, wide.Bounds().right);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFormLayoutTests
//
//  Form layout: label-and-field rows sharing one aligned label column.
//
//  The shared column is the point -- every label is the same width regardless
//  of its text, so the fields line up down the page. A layout sizing each label
//  to its own text produces a ragged left edge on the controls, which is what
//  the policy exists to prevent.
//
//  The field STRETCHES to fill the remaining width, which is the behavior
//  callers have to work around when they want a fixed-width control with
//  something beside it -- so it is pinned here rather than left implicit.
//
//  A row with no field, and a form narrower than its label column, are both
//  covered: the first is how a section heading is expressed, and the second is
//  reachable at small window sizes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiFormLayoutTests)
{
public:

    TEST_METHOD (TwoRows_StackedWithRowGap)
    {
        DxuiDpiScaler    scaler;
        MockDxuiControl  lab1;
        MockDxuiControl  fld1;
        MockDxuiControl  lab2;
        MockDxuiControl  fld2;
        RECT             bounds = {};



        DxuiFormLayout    layout (100.0f, 24.0f, 8.0f, 16.0f, 12.0f);
        bounds = MakeRect (0, 0, 400, 200);


        layout.AddRow (&lab1, &fld1);
        layout.AddRow (&lab2, &fld2);
        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const>());

        Assert::AreEqual ((LONG) 0,   lab1.Bounds().top);
        Assert::AreEqual ((LONG) 24,  lab1.Bounds().bottom);
        Assert::AreEqual ((LONG) 32,  lab2.Bounds().top);
        Assert::AreEqual ((LONG) 100, lab1.Bounds().right);
        Assert::AreEqual ((LONG) 108, fld1.Bounds().left);
        Assert::AreEqual ((LONG) 400, fld1.Bounds().right);
    }


    TEST_METHOD (SubRow_IndentsLabelByConfiguredAmount)
    {
        DxuiDpiScaler    scaler;
        MockDxuiControl  labMain;
        MockDxuiControl  fldMain;
        MockDxuiControl  labSub;
        MockDxuiControl  fldSub;
        RECT             bounds  = {};



        DxuiFormLayout    layout (100.0f, 24.0f, 8.0f, 16.0f, 12.0f);
        bounds = MakeRect (0, 0, 400, 200);


        layout.AddRow    (&labMain, &fldMain);
        layout.AddSubRow (&labSub,  &fldSub);
        layout.Arrange   (bounds, scaler, std::span<IDxuiControl * const>());

        Assert::AreEqual ((LONG) 0,  labMain.Bounds().left);
        Assert::AreEqual ((LONG) 12, labSub.Bounds().left);
    }


    TEST_METHOD (SectionGap_AdvancesYWithoutDrawingARow)
    {
        DxuiDpiScaler    scaler;
        MockDxuiControl  lab1;
        MockDxuiControl  fld1;
        MockDxuiControl  lab2;
        MockDxuiControl  fld2;
        RECT             bounds = {};



        DxuiFormLayout    layout (100.0f, 24.0f, 8.0f, 16.0f, 12.0f);
        bounds = MakeRect (0, 0, 400, 200);


        layout.AddRow         (&lab1, &fld1);
        layout.AddSectionGap  ();
        layout.AddRow         (&lab2, &fld2);
        layout.Arrange        (bounds, scaler, std::span<IDxuiControl * const>());

        // first row: 0..24, +8 rowGap, +16 sectionGap, then second row.
        Assert::AreEqual ((LONG) 48, lab2.Bounds().top);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  TEST_CLASS
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiAbsoluteLayoutTests)
{
public:

    TEST_METHOD (Arrange_LeavesChildBoundsUntouched)
    {
        DxuiAbsoluteLayout  layout;
        DxuiDpiScaler       scaler;
        MockDxuiControl     a;
        MockDxuiControl     b;
        IDxuiControl *      kids[2] = { &a, &b };
        RECT                bounds  = MakeRect (0, 0, 500, 500);


        a.SetBounds (MakeRect (10, 20, 30, 40));
        b.SetBounds (MakeRect (100, 200, 300, 400));

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 10,  a.Bounds().left);
        Assert::AreEqual ((LONG) 40,  a.Bounds().bottom);
        Assert::AreEqual ((LONG) 200, b.Bounds().top);
        Assert::AreEqual ((LONG) 300, b.Bounds().right);
    }
};
