#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Core/DxuiDpiScaler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBannerTests
//
//  The banner's renderer-free preferred-height heuristic -- the piece the page
//  relies on to reflow controls beneath it. No painter / text renderer needed;
//  height is estimated from the text length + width so Layout (which has no
//  renderer) can size the banner.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiInfoBannerTests
{
    static DxuiDpiScaler Scaler96()
    {
        DxuiDpiScaler  s;

        s.SetDpi (96);   // 1:1 DIP == px
        return s;
    }


    TEST_CLASS (DxuiInfoBannerTests)
    {
    public:

        TEST_METHOD (LongerTextWrapsTaller)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  shortBanner (L"Short.");
            DxuiInfoBanner  longBanner  (L"A considerably longer notice that has to wrap across several "
                                         L"lines when the banner is only a couple hundred pixels wide.");

            float  hShort = shortBanner.PreferredHeightPx (220.0f, scaler);
            float  hLong  = longBanner.PreferredHeightPx  (220.0f, scaler);

            Assert::IsTrue (hShort > 0.0f, L"a one-line banner has a positive height");
            Assert::IsTrue (hLong > hShort, L"a longer message wraps to a taller banner");
        }


        TEST_METHOD (WiderBannerWrapsShorter)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  banner (L"A considerably longer notice that has to wrap across several lines "
                                    L"when the banner is narrow, but fits far fewer when it is wide.");

            float  hNarrow = banner.PreferredHeightPx (200.0f, scaler);
            float  hWide   = banner.PreferredHeightPx (700.0f, scaler);

            Assert::IsTrue (hWide <= hNarrow, L"a wider banner wraps to fewer-or-equal lines");
        }


        TEST_METHOD (EmptyBannerStillClearsTheIcon)
        {
            DxuiDpiScaler   scaler = Scaler96();
            DxuiInfoBanner  banner (L"");

            // Even with no text the banner is at least one icon tall plus padding,
            // so a blank message never collapses to nothing.
            Assert::IsTrue (banner.PreferredHeightPx (300.0f, scaler) > 0.0f,
                            L"an empty banner keeps a positive, icon-clearing height");
        }
    };
}
