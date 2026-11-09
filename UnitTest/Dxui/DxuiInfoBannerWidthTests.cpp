#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiInfoBannerWidthTests
//
//  Each action is as wide as its label and the standard margin, as a dialog
//  button is, so a longer label does not touch the button's sides.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiInfoBannerWidthTests
{
    TEST_CLASS (DxuiInfoBannerWidthTests)
    {
    public:

        TEST_METHOD (ALongLabelWidensItsButton)
        {
            DxuiInfoBanner  banner;
            DxuiDpiScaler   scaler;
            RECT            bounds = { 0, 0, 600, 40 };
            RECT            action = {};



            scaler.SetDpi (96);
            banner.SetText    (L"Showing the macro body.");
            banner.SetActions ({ L"Show body" });
            banner.GetAction (0)->SetLabel (L"Show invocation");
            banner.Layout     (bounds, scaler);
            action = banner.GetAction (0)->GetBounds();

            Assert::AreEqual ((LONG) DxuiButtonRow::GetWidthForLabel (L"Show invocation"), action.right - action.left);
            Assert::IsTrue   (banner.GetControlColumnPx (scaler) >= (float) (action.right - action.left));
        }



        TEST_METHOD (AShortLabelKeepsTheStandardWidth)
        {
            DxuiInfoBanner  banner;
            DxuiDpiScaler   scaler;
            RECT            bounds = { 0, 0, 600, 40 };
            RECT            action = {};



            scaler.SetDpi (96);
            banner.SetActions ({ L"Restart" });
            banner.Layout     (bounds, scaler);
            action = banner.GetAction (0)->GetBounds();

            Assert::AreEqual ((LONG) DxuiButtonRow::kButtonWidthDip, action.right - action.left);
        }
    };
}
