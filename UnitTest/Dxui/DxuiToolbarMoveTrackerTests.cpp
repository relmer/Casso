#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarMoveTrackerTests
//
//  A toolbar torn off its dock floats in a window made, placed and fitted
//  while the button is still down, and the drag that tore it off goes on
//  moving that window. Every tick of that drag is reported, so the toolbar snaps
//  into a band it is dragged near from the moment it tears off, however the
//  window was placed or fitted on the way; a drop, or a resize the program
//  did not make, still ends it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiToolbarMoveTrackerTests
{
    using Event = DxuiToolbarMoveTracker::Event;

    static constexpr SIZE  s_kMade   = { 1125, 103 };
    static constexpr SIZE  s_kFitted = { 900,  82 };
    static constexpr SIZE  s_kOther  = { 950,  82 };



    TEST_CLASS (DxuiToolbarMoveTrackerTests)
    {
    public:

        //  The tear-off asks for the move, then the window is fitted before
        //  the move loop has started: the WM_SIZE that fitting sends arrives
        //  outside the loop, where a placement is otherwise a drop.
        TEST_METHOD (AWindowFittedBeforeItsLoopStartsKeepsTheTearOffDrag)
        {
            DxuiToolbarMoveTracker  move;



            move.Begin();
            move.BeginPlace();
            Assert::IsTrue (move.OnPlaced (s_kFitted) == Event::None, L"fitting the window is not a drop");
            move.EndPlace (s_kFitted);

            Assert::IsTrue (move.IsMoving(), L"the drag that tore it off goes on");
            Assert::IsTrue (move.OnTick (true, s_kFitted) == Event::Moved, L"the first tick is reported");
            Assert::IsTrue (move.OnTick (true, s_kFitted) == Event::Moved, L"and so is the tick near an edge");
            Assert::IsTrue (move.OnPlaced (s_kFitted)     == Event::Ended, L"the end of the loop drops it");
            Assert::IsFalse (move.IsMoving());
        }


        //  The window is fitted to the toolbar again on a frame inside the
        //  loop: the next tick at the new size is still a move, not a resize.
        TEST_METHOD (AWindowFittedDuringItsLoopKeepsTheTearOffDrag)
        {
            DxuiToolbarMoveTracker  move;



            move.Begin();
            Assert::IsTrue (move.OnTick (true, s_kMade) == Event::Moved, L"the drag starts at the size the window was made");

            move.BeginPlace();
            move.EndPlace (s_kFitted);

            Assert::IsTrue (move.OnTick (true, s_kFitted) == Event::Moved, L"a tick at the fitted size near an edge is still reported");
            Assert::IsTrue (move.OnPlaced (s_kFitted)     == Event::Ended, L"and the drop is a drop, not a cancel");
        }


        TEST_METHOD (AResizeTheProgramDidNotMakeStillCancels)
        {
            DxuiToolbarMoveTracker  move;



            move.Begin();
            Assert::IsTrue (move.OnTick (true, s_kFitted) == Event::Moved);
            Assert::IsTrue (move.OnTick (true, s_kOther)  == Event::Canceled, L"a size the program did not set");
            Assert::IsTrue (move.OnTick (true, s_kOther)  == Event::None,     L"reports nothing more");
        }


        TEST_METHOD (APlacementTheProgramDidNotMakeStillEndsTheMove)
        {
            DxuiToolbarMoveTracker  move;



            move.Begin();
            Assert::IsTrue  (move.OnTick (true, s_kFitted) == Event::Moved);
            Assert::IsTrue  (move.OnPlaced (s_kFitted)     == Event::Ended);
            Assert::IsFalse (move.IsMoving());
            Assert::IsTrue  (move.OnTick (true, s_kFitted) == Event::None, L"a loop the ends start is no move");
        }


        TEST_METHOD (AMoveAskedForAfterTheButtonCameUpNeverStarts)
        {
            DxuiToolbarMoveTracker  move;



            move.Begin();
            Assert::IsTrue  (move.OnPoll (false, s_kFitted) == Event::None);
            Assert::IsFalse (move.IsMoving());
        }
    };
}
