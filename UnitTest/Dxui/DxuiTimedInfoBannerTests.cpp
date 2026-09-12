#include "Pch.h"

#include "Widgets/DxuiTimedInfoBanner.h"
#include "Core/DxuiDpiScaler.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTimedInfoBannerTests
//
//  The message bar that takes itself down: when it shows, when it expires,
//  and that a newer notice replaces an older one with a fresh countdown.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTimedInfoBannerTests
{
    TEST_CLASS (DxuiTimedInfoBannerTests)
    {
    public:

        TEST_METHOD (StartsHidden)
        {
            DxuiTimedInfoBanner  notice;



            Assert::IsFalse (notice.IsVisible());
            Assert::IsFalse (notice.IsShowing (0));
        }



        TEST_METHOD (ShowIsVisibleUntilTheDurationRunsOut)
        {
            constexpr int64_t  kStartMs    = 1000;
            constexpr int64_t  kDurationMs = 500;

            DxuiTimedInfoBanner  notice;



            notice.SetDurationMs (kDurationMs);
            notice.Show (L"Saved", kStartMs);

            Assert::IsTrue (notice.IsVisible());
            Assert::AreEqual (std::wstring (L"Saved"), notice.GetText());

            notice.Tick (kStartMs + kDurationMs - 1);
            Assert::IsTrue (notice.IsVisible(), L"still inside the countdown");

            notice.Tick (kStartMs + kDurationMs);
            Assert::IsFalse (notice.IsVisible(), L"the tick at expiry takes it down");
            Assert::IsFalse (notice.IsShowing (kStartMs + kDurationMs));
        }



        TEST_METHOD (ASecondShowReplacesTheTextAndRestartsTheCountdown)
        {
            constexpr int64_t  kDurationMs = 500;
            constexpr int64_t  kSecondMs   = 400;

            DxuiTimedInfoBanner  notice;



            notice.SetDurationMs (kDurationMs);
            notice.Show (L"First", 0);
            notice.Show (L"Second", kSecondMs);

            Assert::AreEqual (std::wstring (L"Second"), notice.GetText());

            notice.Tick (kDurationMs);
            Assert::IsTrue (notice.IsVisible(), L"the first notice's expiry does not end the second");

            notice.Tick (kSecondMs + kDurationMs);
            Assert::IsFalse (notice.IsVisible());
        }



        TEST_METHOD (DismissEndsTheCountdown)
        {
            DxuiTimedInfoBanner  notice;



            notice.Show (L"Saved", 0);
            notice.Dismiss();

            Assert::IsFalse (notice.IsVisible());
            Assert::IsFalse (notice.IsShowing (1));
        }



        TEST_METHOD (LayoutFillsTheGivenBounds)
        {
            constexpr UINT  kDpi = 96;

            DxuiTimedInfoBanner  notice;
            DxuiDpiScaler        scaler;
            RECT                 bounds = { 0, 40, 800, 80 };
            RECT                 laid   = {};



            scaler.SetDpi (kDpi);
            notice.Layout (bounds, scaler);
            laid = notice.GetBounds();

            Assert::AreEqual (bounds.left,   laid.left);
            Assert::AreEqual (bounds.top,    laid.top);
            Assert::AreEqual (bounds.right,  laid.right);
            Assert::AreEqual (bounds.bottom, laid.bottom);
            Assert::IsTrue (notice.GetPreferredHeightPx (800.0f, scaler) > 0.0f);
        }
    };
}
