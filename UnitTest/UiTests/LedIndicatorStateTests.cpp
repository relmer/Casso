#include "Pch.h"

#include "Ui/Chrome/LedIndicator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





TEST_CLASS (LedIndicatorStateTests)
{
public:

    TEST_METHOD (State_Colors_And_Dimensions_Are_Stable)
    {
        CassoTheme         theme = CassoTheme::Skeuomorphic();
        LedIndicator        led;
        LedIndicatorLayout  layout = {};



        led.PositionAt (20, 30, 96);
        layout = led.GetLayout();

        Assert::IsTrue (layout.coreRect.right - layout.coreRect.left >= 6);
        Assert::IsTrue (layout.coreRect.left - layout.haloRect.left >= 2);
        Assert::AreEqual ((unsigned int) theme.ledIdle, (unsigned int) led.CoreArgb (theme));

        led.SetState (LedState::Present);
        Assert::AreEqual ((unsigned int) theme.ledPresent, (unsigned int) led.CoreArgb (theme));

        led.SetState (LedState::Active);
        Assert::AreEqual ((unsigned int) theme.ledActive, (unsigned int) led.CoreArgb (theme));
        Assert::AreEqual ((unsigned int) theme.ledHalo, (unsigned int) led.HaloArgb (theme));
    }
};
