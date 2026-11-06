#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTrackerDpiTests
//
//  A floating window dragged onto a monitor at another scale is resized by
//  the system to keep its size in DIPs. That is still a move, so the drop
//  targets stay up until the drag ends.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiCaptionDragTrackerDpiTests
{
    using Event = DxuiCaptionDragTracker::Event;

    static constexpr SIZE  s_kSize   = { 400, 300 };
    static constexpr SIZE  s_kScaled = { 600, 450 };
    static constexpr SIZE  s_kOther  = { 620, 450 };



    TEST_CLASS (DxuiCaptionDragTrackerDpiTests)
    {
    public:

        TEST_METHOD (AScaleChangeKeepsTheMoveGoing)
        {
            DxuiCaptionDragTracker  drag;



            Assert::IsTrue (drag.OnTick (true, s_kSize) == Event::Moved);
            drag.Rebase (s_kScaled);
            Assert::IsTrue (drag.OnTick (true, s_kScaled) == Event::Moved, L"still a move on the other monitor");
            Assert::IsTrue (drag.OnLoopEnd (s_kScaled)    == Event::Ended, L"and it ends in a drop");
        }


        TEST_METHOD (AResizeAfterAScaleChangeIsStillCanceled)
        {
            DxuiCaptionDragTracker  drag;



            Assert::IsTrue (drag.OnTick (true, s_kSize) == Event::Moved);
            drag.Rebase (s_kScaled);
            Assert::IsTrue (drag.OnTick (true, s_kOther) == Event::Canceled);
        }


        TEST_METHOD (ARebaseWithNoDragDoesNothing)
        {
            DxuiCaptionDragTracker  drag;



            drag.Rebase (s_kScaled);
            Assert::IsFalse (drag.IsDragging());
            Assert::IsTrue  (drag.OnTick (true, s_kSize) == Event::Moved, L"the drag starts at its own size");
            Assert::IsTrue  (drag.OnLoopEnd (s_kSize)    == Event::Ended);
        }


        //  A window moved with the pointer and resized by a scale change has
        //  its grab point scaled with it, so the pointer is over the title
        //  bar at the same place in proportion.
        TEST_METHOD (TheGrabPointScalesWithTheWindow)
        {
            POINT  grown  = DxuiDockedWindow::ScaleGrab (POINT { 200, 10 }, s_kSize,   s_kScaled);
            POINT  shrunk = DxuiDockedWindow::ScaleGrab (POINT { 500, 15 }, s_kScaled, s_kSize);
            POINT  same   = DxuiDockedWindow::ScaleGrab (POINT { 200, 10 }, SIZE {},   s_kScaled);



            Assert::AreEqual (300L, grown.x,  L"halfway across the smaller window, halfway across the larger");
            Assert::AreEqual (15L,  grown.y);
            Assert::AreEqual (333L, shrunk.x, L"in a window that shrank, the grab point is inside it");
            Assert::AreEqual (10L,  shrunk.y);
            Assert::AreEqual (200L, same.x,   L"with no size to scale from, the grab point is the one given");
            Assert::AreEqual (10L,  same.y);
        }
    };
}
