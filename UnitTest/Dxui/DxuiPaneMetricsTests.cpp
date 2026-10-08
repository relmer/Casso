#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetricsTests
//
//  The measures a pane's frame and its text share, at each scale from 100%
//  to 200%. Every one is a whole number of pixels, so the frame's edges and
//  the text's origin land on the same pixel grid.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPaneMetricsTests)
{
public:

    struct Expected
    {
        UINT  dpi          = 0;
        int   line         = 0;
        int   corner       = 0;
        int   textInset    = 0;
        int   contentInset = 0;
    };


    TEST_METHOD (MeasuresFollowTheScale)
    {
        DxuiDpiScaler   scaler;
        const Expected  expected[] =
        {
            {  96, 1,  5,  8,  7 },
            { 120, 1,  6, 10,  9 },
            { 144, 2,  8, 12, 10 },
            { 168, 2,  9, 14, 12 },
            { 192, 2, 10, 16, 14 },
        };



        for (const Expected & row : expected)
        {
            std::wstring  at = std::format (L"at {} dpi", row.dpi);

            scaler.SetDpi (row.dpi);

            Assert::AreEqual (row.line,         DxuiPaneMetrics::GetLinePx             (scaler), (L"line "               + at).c_str());
            Assert::AreEqual (row.corner,       DxuiPaneMetrics::GetCornerPx           (scaler), (L"corner "             + at).c_str());
            Assert::AreEqual (row.textInset,    DxuiPaneMetrics::GetTextInsetPx        (scaler), (L"text inset "         + at).c_str());
            Assert::AreEqual (row.contentInset, DxuiPaneMetrics::GetContentTextInsetPx (scaler), (L"content text inset " + at).c_str());
        }
    }
};
