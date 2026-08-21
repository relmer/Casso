#include "Pch.h"

#include "Widgets/DxuiInfoBanner.h"
#include "Core/DxuiDpiScaler.h"
#include "MockDxuiTextRenderer.h"

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
            DxuiDpiScaler  scaler = Scaler96();
            float          hShort = 0.0f;
            float          hLong  = 0.0f;
            DxuiInfoBanner  shortBanner (L"Short.");
            DxuiInfoBanner  longBanner  (L"A considerably longer notice that has to wrap across several "
                                         L"lines when the banner is only a couple hundred pixels wide.");

            hShort = shortBanner.PreferredHeightPx (220.0f, scaler);
            hLong = longBanner.PreferredHeightPx  (220.0f, scaler);

            Assert::IsTrue (hShort > 0.0f, L"a one-line banner has a positive height");
            Assert::IsTrue (hLong > hShort, L"a longer message wraps to a taller banner");
        }


        TEST_METHOD (WiderBannerWrapsShorter)
        {
            DxuiDpiScaler  scaler  = Scaler96();
            float          hNarrow = 0.0f;
            float          hWide   = 0.0f;
            DxuiInfoBanner  banner (L"A considerably longer notice that has to wrap across several lines "
                                    L"when the banner is narrow, but fits far fewer when it is wide.");

            hNarrow = banner.PreferredHeightPx (200.0f, scaler);
            hWide = banner.PreferredHeightPx (700.0f, scaler);

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

        TEST_METHOD (MeasuredHeight_UsesTheRendererInsteadOfTheEstimate)
        {
            // PreferredHeightPx has no renderer, so it rounds up and never
            // clips -- harmless on an auto-sized surface, but on a fixed
            // dialog the slack shows as an empty line inside the box. A caller
            // that HAS a renderer can ask for the real height instead.
            DxuiInfoBanner        banner (L"a short warning");
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler    = Scaler96();
            SIZE                  oneLine   = { 200, 16 };
            float                 measured  = 0.0f;
            float                 estimated = 0.0f;

            text.SetCannedMetrics (L"a short warning", oneLine);

            measured  = banner.MeasuredHeightPx (text, 400.0f, scaler);
            estimated = banner.PreferredHeightPx (400.0f, scaler);

            Assert::IsTrue (measured > 0.0f, L"a measured height must be produced");
            Assert::IsTrue (measured <= estimated,
                L"measuring must never be taller than the deliberately generous estimate");
        }



        TEST_METHOD (MeasuredHeight_TallerTextNeedsMoreRoom)
        {
            DxuiInfoBanner        banner (L"tall");
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler = Scaler96();
            SIZE                  one    = { 200, 16 };
            SIZE                  three  = { 200, 48 };
            float                 hOne   = 0.0f;
            float                 hThree = 0.0f;

            text.SetCannedMetrics (L"tall", one);
            hOne = banner.MeasuredHeightPx (text, 400.0f, scaler);

            text.SetCannedMetrics (L"tall", three);
            hThree = banner.MeasuredHeightPx (text, 400.0f, scaler);

            Assert::IsTrue (hThree > hOne, L"three lines of text need more height than one");
        }



        TEST_METHOD (MeasuredHeight_FallsBackToTheEstimateWhenMeasurementFails)
        {
            // DirectWrite can transiently report a zero-width layout mid-resize.
            // Trusting that would collapse the banner to its padding, so a
            // failed measurement has to fall back rather than be believed.
            DxuiInfoBanner        banner (L"a warning long enough to wrap more than once in a narrow banner");
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler = Scaler96();

            text.SetMeasureReturnsZero (true);

            Assert::AreEqual (banner.PreferredHeightPx (200.0f, scaler),
                              banner.MeasuredHeightPx (text, 200.0f, scaler),
                              L"a zero measurement must fall back to the estimate");
        }

    };
}
