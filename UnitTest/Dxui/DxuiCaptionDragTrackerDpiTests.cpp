#include "Pch.h"

#include "Window/DxuiCaptionDragTracker.h"

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
    };
}
