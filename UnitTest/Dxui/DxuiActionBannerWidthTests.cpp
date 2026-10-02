#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBannerWidthTests
//
//  Each action is as wide as its label and the standard margin, as a dialog
//  button is, so a longer label does not touch the button's sides.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiActionBannerWidthTests
{
    TEST_CLASS (DxuiActionBannerWidthTests)
    {
    public:

        TEST_METHOD (ALongLabelWidensItsButton)
        {
            DxuiActionBanner  banner;
            DxuiDpiScaler     scaler;
            RECT              bounds = { 0, 0, 600, 40 };
            RECT              action = {};



            scaler.SetDpi (96);
            banner.SetText    (L"Showing the macro body.");
            banner.SetActions ({ L"Show body" });
            banner.GetAction (0)->SetLabel (L"Show invocation");
            banner.Layout     (bounds, scaler);
            action = banner.GetAction (0)->GetBounds();

            Assert::AreEqual ((LONG) DxuiButtonRow::GetWidthForLabel (L"Show invocation"), action.right - action.left);
            Assert::IsTrue   (banner.GetActionReservePx (scaler) >= (float) (action.right - action.left));
        }



        TEST_METHOD (AShortLabelKeepsTheStandardWidth)
        {
            DxuiActionBanner  banner;
            DxuiDpiScaler     scaler;
            RECT              bounds = { 0, 0, 600, 40 };
            RECT              action = {};



            scaler.SetDpi (96);
            banner.SetActions ({ L"Restart" });
            banner.Layout     (bounds, scaler);
            action = banner.GetAction (0)->GetBounds();

            Assert::AreEqual ((LONG) DxuiButtonRow::kButtonWidthDip, action.right - action.left);
        }
    };
}
