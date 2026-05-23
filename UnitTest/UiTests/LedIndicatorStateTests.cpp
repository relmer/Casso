#include "Pch.h"

#include "Ui/Chrome/LedIndicator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (LedIndicatorStateTests)
{
public:

    TEST_METHOD (State_Colors_And_Dimensions_Are_Stable)
    {
        ChromeTheme         theme = ChromeTheme::Skeuomorphic();
        LedIndicator        led;
        LedIndicatorLayout  layout = {};



        led.Layout (20, 30, 96);
        layout = led.GetLayout();

        Assert::IsTrue (layout.coreRect.right - layout.coreRect.left >= 12);
        Assert::IsTrue (layout.coreRect.left - layout.haloRect.left >= 4);
        Assert::AreEqual ((unsigned int) theme.ledIdleArgb, (unsigned int) led.CoreArgb (theme));

        led.SetState (LedState::Present);
        Assert::AreEqual ((unsigned int) theme.ledPresentArgb, (unsigned int) led.CoreArgb (theme));

        led.SetState (LedState::Active);
        Assert::AreEqual ((unsigned int) theme.ledActiveArgb, (unsigned int) led.CoreArgb (theme));
        Assert::AreEqual ((unsigned int) theme.ledHaloArgb, (unsigned int) led.HaloArgb (theme));
    }
};
