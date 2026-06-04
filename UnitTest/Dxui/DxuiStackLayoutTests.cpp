#include "Pch.h"

#include "MockDxuiControl.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace
{
    RECT  MakeRect (LONG l, LONG t, LONG r, LONG b)
    {
        RECT  out = {};
        out.left = l; out.top = t; out.right = r; out.bottom = b;
        return out;
    }


    LONG  Width  (const RECT & r) { return r.right  - r.left; }
    LONG  Height (const RECT & r) { return r.bottom - r.top;  }
}





TEST_CLASS (DxuiStackLayoutTests)
{
public:

    TEST_METHOD (HorizontalNoWeights_StacksAtNaturalWidths)
    {
        DxuiStackLayout       layout (DxuiStackLayout::Orientation::Horizontal,
                                      0.0f,
                                      DxuiStackLayout::Align::Stretch);
        DxuiDpiScaler         scaler;
        MockDxuiControl       a;
        MockDxuiControl       b;
        IDxuiControl *        kids[2] = { &a, &b };
        RECT                  bounds  = MakeRect (0, 0, 200, 50);


        a.SetBounds (MakeRect (0, 0, 60, 50));
        b.SetBounds (MakeRect (0, 0, 80, 50));

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 0,   a.Bounds().left);
        Assert::AreEqual ((LONG) 60,  a.Bounds().right);
        Assert::AreEqual ((LONG) 60,  b.Bounds().left);
        Assert::AreEqual ((LONG) 140, b.Bounds().right);
    }


    TEST_METHOD (HorizontalWithSpacing_AccountsForGaps)
    {
        DxuiStackLayout       layout (DxuiStackLayout::Orientation::Horizontal,
                                      10.0f,
                                      DxuiStackLayout::Align::Stretch);
        DxuiDpiScaler         scaler;
        MockDxuiControl       a;
        MockDxuiControl       b;
        IDxuiControl *        kids[2] = { &a, &b };
        RECT                  bounds  = MakeRect (0, 0, 200, 50);


        a.SetBounds (MakeRect (0, 0, 60, 50));
        b.SetBounds (MakeRect (0, 0, 80, 50));

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 2));

        Assert::AreEqual ((LONG) 0,  a.Bounds().left);
        Assert::AreEqual ((LONG) 60, a.Bounds().right);
        Assert::AreEqual ((LONG) 70, b.Bounds().left);
    }


    TEST_METHOD (HorizontalWithWeights_DistributesRemainder)
    {
        DxuiStackLayout       layout (DxuiStackLayout::Orientation::Horizontal,
                                      0.0f,
                                      DxuiStackLayout::Align::Stretch);
        DxuiDpiScaler         scaler;
        MockDxuiControl       fixed;
        MockDxuiControl       flex1;
        MockDxuiControl       flex2;
        IDxuiControl *        kids[3] = { &fixed, &flex1, &flex2 };
        RECT                  bounds  = MakeRect (0, 0, 200, 50);


        fixed.SetBounds (MakeRect (0, 0, 50, 50));
        layout.SetWeight (&flex1, 1);
        layout.SetWeight (&flex2, 3);

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 3));

        // Remaining 150 split 1:3 -> flex1 = 37, flex2 = 113 (last absorbs remainder).
        Assert::AreEqual ((LONG) 50,  Width (fixed.Bounds()));
        Assert::AreEqual ((LONG) 37,  Width (flex1.Bounds()));
        Assert::AreEqual ((LONG) 113, Width (flex2.Bounds()));
    }


    TEST_METHOD (VerticalCenterAlign_OffsetsByHalfRemainingCross)
    {
        DxuiStackLayout       layout (DxuiStackLayout::Orientation::Vertical,
                                      0.0f,
                                      DxuiStackLayout::Align::Center);
        DxuiDpiScaler         scaler;
        MockDxuiControl       a;
        IDxuiControl *        kids[1] = { &a };
        RECT                  bounds  = MakeRect (0, 0, 100, 80);


        a.SetBounds (MakeRect (0, 0, 40, 30));

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 30, a.Bounds().left);
        Assert::AreEqual ((LONG) 70, a.Bounds().right);
        Assert::AreEqual ((LONG) 0,  a.Bounds().top);
        Assert::AreEqual ((LONG) 30, a.Bounds().bottom);
    }


    TEST_METHOD (StretchAlign_ExpandsCrossAxisToBandSize)
    {
        DxuiStackLayout       layout (DxuiStackLayout::Orientation::Horizontal,
                                      0.0f,
                                      DxuiStackLayout::Align::Stretch);
        DxuiDpiScaler         scaler;
        MockDxuiControl       a;
        IDxuiControl *        kids[1] = { &a };
        RECT                  bounds  = MakeRect (0, 0, 100, 80);


        a.SetBounds (MakeRect (0, 0, 60, 20));

        layout.Arrange (bounds, scaler, std::span<IDxuiControl * const> (kids, 1));

        Assert::AreEqual ((LONG) 80, Height (a.Bounds()));
    }
};
