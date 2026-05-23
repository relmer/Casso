#include "Pch.h"

#include "Ui/TitleBar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TitleBarLayoutTests
//
//  Pure-logic tests for `TitleBarLayout::Compute`. The layout function
//  computes the title-bar rect, the three system-button rects, and the
//  drag-region rect from a client width + title height + button width.
//  No Win32 / no painter.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TitleBarLayoutTests)
{
public:

    static TitleBarLayoutInput MakeStandard (int width = 1000)
    {
        TitleBarLayoutInput  in = {};
        in.clientWidth = width;
        in.titleHeight = 32;
        in.buttonWidth = 46;
        return in;
    }


    TEST_METHOD (TitleBar_Rect_Spans_Full_Width)
    {
        TitleBarLayoutOutput  out = TitleBarLayout::Compute (MakeStandard (1234));

        Assert::AreEqual (0L,    out.titleBar.left);
        Assert::AreEqual (0L,    out.titleBar.top);
        Assert::AreEqual (1234L, out.titleBar.right);
        Assert::AreEqual (32L,   out.titleBar.bottom);
    }


    TEST_METHOD (Buttons_Stack_Right_To_Left_Close_Max_Min)
    {
        TitleBarLayoutOutput  out = TitleBarLayout::Compute (MakeStandard (1000));

        // Right edge of the close button kisses the right edge of the
        // client rect.
        Assert::AreEqual (1000L, out.closeButton.right);
        // Min sits to the left of Max; Max to the left of Close.
        Assert::IsTrue (out.minButton.right == out.maxButton.left,
                        L"Min.right must touch Max.left");
        Assert::IsTrue (out.maxButton.right == out.closeButton.left,
                        L"Max.right must touch Close.left");
        // Each button is buttonWidth wide.
        Assert::AreEqual (46L, out.minButton.right   - out.minButton.left);
        Assert::AreEqual (46L, out.maxButton.right   - out.maxButton.left);
        Assert::AreEqual (46L, out.closeButton.right - out.closeButton.left);
    }


    TEST_METHOD (Drag_Region_Excludes_Button_Strip)
    {
        TitleBarLayoutOutput  out = TitleBarLayout::Compute (MakeStandard (1000));

        // Drag region starts at 0 and ends at the LEFT edge of the
        // leftmost (min) button.
        Assert::AreEqual (0L,                  out.dragRegion.left);
        Assert::AreEqual (out.minButton.left,  out.dragRegion.right);
        Assert::AreEqual (0L,                  out.dragRegion.top);
        Assert::AreEqual (32L,                 out.dragRegion.bottom);
    }


    TEST_METHOD (Buttons_Track_Width_Changes_For_Various_Sizes)
    {
        const int kWidths[] = { 400, 800, 1280, 1920, 3840 };

        for (int width : kWidths)
        {
            TitleBarLayoutOutput  out = TitleBarLayout::Compute (MakeStandard (width));

            Assert::AreEqual ((LONG) width,           out.closeButton.right);
            Assert::AreEqual ((LONG) (width - 46),    out.closeButton.left);
            Assert::AreEqual ((LONG) (width - 92),    out.maxButton.left);
            Assert::AreEqual ((LONG) (width - 138),   out.minButton.left);
        }
    }


    TEST_METHOD (Window_Narrower_Than_Button_Strip_Collapses_Buttons)
    {
        TitleBarLayoutInput   in  = MakeStandard (60);  // < 3 * 46
        TitleBarLayoutOutput  out = TitleBarLayout::Compute (in);

        // All buttons collapse to zero rects; drag region == full strip.
        Assert::AreEqual (0L, out.minButton.right   - out.minButton.left);
        Assert::AreEqual (0L, out.maxButton.right   - out.maxButton.left);
        Assert::AreEqual (0L, out.closeButton.right - out.closeButton.left);
        Assert::AreEqual (60L, out.dragRegion.right);
    }


    TEST_METHOD (Zero_Height_Or_Width_Yields_Empty_Layout)
    {
        TitleBarLayoutInput  inZeroH = {};
        inZeroH.clientWidth = 1000;
        inZeroH.titleHeight = 0;
        inZeroH.buttonWidth = 46;

        TitleBarLayoutOutput  out = TitleBarLayout::Compute (inZeroH);

        Assert::AreEqual (0L, out.minButton.right   - out.minButton.left);
        Assert::AreEqual (0L, out.maxButton.right   - out.maxButton.left);
        Assert::AreEqual (0L, out.closeButton.right - out.closeButton.left);
    }


    TEST_METHOD (Custom_Button_Width_Is_Honored)
    {
        TitleBarLayoutInput  in = MakeStandard (1000);
        in.buttonWidth = 60;

        TitleBarLayoutOutput  out = TitleBarLayout::Compute (in);

        Assert::AreEqual (60L, out.closeButton.right - out.closeButton.left);
        Assert::AreEqual (1000L - 60L,  out.closeButton.left);
        Assert::AreEqual (1000L - 120L, out.maxButton.left);
        Assert::AreEqual (1000L - 180L, out.minButton.left);
    }


    TEST_METHOD (Buttons_All_Have_Full_Title_Height)
    {
        TitleBarLayoutInput  in = MakeStandard (1000);
        in.titleHeight = 40;

        TitleBarLayoutOutput  out = TitleBarLayout::Compute (in);

        Assert::AreEqual (40L, out.minButton.bottom   - out.minButton.top);
        Assert::AreEqual (40L, out.maxButton.bottom   - out.maxButton.top);
        Assert::AreEqual (40L, out.closeButton.bottom - out.closeButton.top);
    }


    TEST_METHOD (Default_Title_Height_Never_Returns_Zero)
    {
        // The fallback path returns 32 when GetSystemMetricsForDpi
        // returns 0 (which happens in some hostless environments).
        // Pass a known-valid DPI and just assert non-zero.
        int  h = TitleBarLayout::DefaultTitleHeight (96);
        Assert::IsTrue (h > 0, L"DefaultTitleHeight must be positive");
    }


    TEST_METHOD (Default_Button_Width_Scales_With_DPI)
    {
        int  w96  = TitleBarLayout::DefaultButtonWidth (96);
        int  w192 = TitleBarLayout::DefaultButtonWidth (192);

        Assert::AreEqual (46, w96);
        Assert::AreEqual (92, w192);
    }


    TEST_METHOD (TitleBar_FontPolicy_UsesWindowsSystemUiFont)
    {
        Assert::AreEqual (std::wstring (L"Segoe UI"),
                          std::wstring (TitleBarLayout::WindowsUiFontFamily()));
        Assert::AreEqual (400, TitleBarLayout::WindowsUiFontWeight());
    }
};
