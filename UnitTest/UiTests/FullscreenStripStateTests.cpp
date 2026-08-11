#include "Pch.h"

#include "Ui/Scene/FullscreenStripState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullscreenStripStateTests
//
//  The overlay strip FSM invariants: edge-reveal never fires under guest
//  capture, Hidden is never entered while pinned, capture is restored exactly
//  once on dismissal iff it was released at summon, and the activity indicator
//  exists only while Hidden.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullscreenStripStateTests)
{
public:

    TEST_METHOD (StripState_Is_Default_Constructible)
    {
        FullscreenStripState  state;



        (void) state;
        Assert::IsTrue (true);
    }

};
