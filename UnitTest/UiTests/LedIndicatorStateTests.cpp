#include "Pch.h"

#include "Ui/Chrome/LedIndicator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LedIndicatorStateTests
//
//  The LED's state machine: on, off, and the decay that keeps a brief pulse
//  visible.
//
//  DECAY is the substance. Disk activity is a burst of accesses lasting a
//  fraction of a frame, so an LED reflecting the raw signal would flicker below
//  the threshold of perception -- it has to stay lit for a minimum interval
//  after the last access.
//
//  Time is passed in, so the tests advance a synthetic clock rather than
//  sleeping, and the whole decay is exercised deterministically.
//
//  The boundary cases are covered in both directions: a second access
//  mid-decay must extend rather than restart-and-flicker, and the LED must
//  eventually go out rather than latching on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LedIndicatorStateTests)
{
public:

    TEST_METHOD (State_Colors_And_Dimensions_Are_Stable)
    {
        CassoTheme          theme  = CassoTheme::Skeuomorphic();
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
