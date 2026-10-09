#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetricsTests
//
//  The measures a pane's frame and its text share, at each scale from 100%
//  to 200%. Every one is a whole number of pixels, so the frame's edges and
//  the text's origin land on the same pixel grid. Visual Studio shows an
//  outer corner of 6 px at 125% and 7 px at 150%, and puts a title's
//  origin 11 px in from the pane's outer edge at 125% and 14 px at 150%.
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
            {  96, 1, 5,  9,  8 },
            { 120, 1, 6, 11, 10 },
            { 144, 2, 7, 14, 12 },
            { 168, 2, 8, 16, 14 },
            { 192, 2, 9, 18, 16 },
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
