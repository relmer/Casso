#include "Pch.h"

#include "Window/DxuiPropertySheet.h"
#include "Core/DxuiDpiScaler.h"

#include <array>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace
{
    RECT  MakeRect (int l, int t, int r, int b) { return RECT{ l, t, r, b }; }

    void  AssertRect (const RECT & expected, const RECT & actual, const wchar_t * msg)
    {
        Assert::AreEqual (expected.left,   actual.left,   msg);
        Assert::AreEqual (expected.top,    actual.top,    msg);
        Assert::AreEqual (expected.right,  actual.right,  msg);
        Assert::AreEqual (expected.bottom, actual.bottom, msg);
    }
}




//
//  Covers the button-row customization added for the SettingsPanel ->
//  DxuiPropertySheet migration (T162): the hideable Apply button + custom
//  OK label / width that let the sheet express Casso's no-Apply /
//  "OK (reboot)" model. The reflow math is exercised through the pure
//  LayoutButtonRow helper so no window is required.
//
TEST_CLASS (DxuiPropertySheetTests)
{
public:
    TEST_METHOD (Defaults_ApplyVisibleOkTextOkWidth)
    {
        DxuiPropertySheet  sheet;

        Assert::IsTrue   (sheet.ApplyVisible());
        Assert::AreEqual (L"OK", sheet.OkText().c_str());
        Assert::AreEqual (0, sheet.OkWidthDip());
    }


    TEST_METHOD (Setters_UpdateState)
    {
        DxuiPropertySheet  sheet;

        sheet.SetApplyVisible (false);
        sheet.SetOkText       (L"OK (reboot)");
        sheet.SetOkWidthDip   (140);

        Assert::IsFalse  (sheet.ApplyVisible());
        Assert::AreEqual (L"OK (reboot)", sheet.OkText().c_str());
        Assert::AreEqual (140, sheet.OkWidthDip());
    }


    TEST_METHOD (LayoutButtonRow_ThreeEqual_RightAligned96Dpi)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[3] = { 96, 96, 96 };
        std::array<RECT, 3>  rects     = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (80,  256, 176, 284), rects[0], L"ok");
        AssertRect (MakeRect (184, 256, 280, 284), rects[1], L"cancel");
        AssertRect (MakeRect (288, 256, 384, 284), rects[2], L"apply");

        // Rightmost button hugs the edge pad (16 DIP @ 96 dpi).
        Assert::AreEqual (bounds.right - 16, rects[2].right);
    }


    TEST_METHOD (LayoutButtonRow_ApplyHidden_TwoButtonsReflowRight)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[2] = { 96, 96 };
        std::array<RECT, 2>  rects     = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (184, 256, 280, 284), rects[0], L"ok");
        AssertRect (MakeRect (288, 256, 384, 284), rects[1], L"cancel");

        // Cancel is now the rightmost button and still hugs the edge pad.
        Assert::AreEqual (bounds.right - 16, rects[1].right);
    }


    TEST_METHOD (LayoutButtonRow_CustomOkWidth_WidensOkShiftsRow)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[3] = { 140, 96, 96 };
        std::array<RECT, 3>  rects     = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (36,  256, 176, 284), rects[0], L"ok");
        AssertRect (MakeRect (184, 256, 280, 284), rects[1], L"cancel");
        AssertRect (MakeRect (288, 256, 384, 284), rects[2], L"apply");

        Assert::AreEqual (140, (int) (rects[0].right - rects[0].left));
        Assert::AreEqual ((int) (bounds.right - 16), (int) rects[2].right);
    }


    TEST_METHOD (LayoutButtonRow_192Dpi_DoublesMetrics)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 800, 600);
        int                  widths[3] = { 96, 96, 96 };
        std::array<RECT, 3>  rects     = {};

        scaler.SetDpi (192);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (160, 512, 352, 568), rects[0], L"ok");
        AssertRect (MakeRect (368, 512, 560, 568), rects[1], L"cancel");
        AssertRect (MakeRect (576, 512, 768, 568), rects[2], L"apply");

        Assert::AreEqual (bounds.right - 32, rects[2].right);
    }
};
