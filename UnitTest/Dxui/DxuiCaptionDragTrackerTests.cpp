#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionDragTrackerTests
//
//  A floating pane's window shows the drop zones while it is moved by its
//  title bar, and they come down when the move ends, whether it ended in a
//  drop, a resize, or a change of size on the way.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiCaptionDragTrackerTests
{
    using Event = DxuiCaptionDragTracker::Event;

    static constexpr SIZE  s_kSize  = { 400, 300 };
    static constexpr SIZE  s_kOther = { 420, 300 };



    TEST_CLASS (DxuiCaptionDragTrackerTests)
    {
    public:

        TEST_METHOD (TheEndOfTheMoveLoopEndsAMove)
        {
            DxuiCaptionDragTracker  drag;



            Assert::IsTrue (drag.OnTick (true, s_kSize) == Event::Moved);
            Assert::IsTrue (drag.OnLoopEnd (s_kSize) == Event::Ended);
            Assert::IsFalse (drag.IsDragging());
            Assert::IsTrue (drag.OnPoll (false, s_kSize) == Event::None, L"ended once");
        }


        TEST_METHOD (AMoveThatTurnsIntoAResizeIsCanceled)
        {
            DxuiCaptionDragTracker  drag;



            Assert::IsTrue (drag.OnTick (true, s_kSize)  == Event::Moved);
            Assert::IsTrue (drag.OnTick (true, s_kOther) == Event::Canceled, L"the zones the first tick showed come down");
            Assert::IsTrue (drag.OnTick (true, s_kOther) == Event::None);
            Assert::IsTrue (drag.OnLoopEnd (s_kOther)    == Event::None, L"nothing left to end");
        }


        TEST_METHOD (AMoveThatEndsAtAnotherSizeIsCanceled)
        {
            DxuiCaptionDragTracker  drag;



            Assert::IsTrue (drag.OnTick (true, s_kSize) == Event::Moved);
            Assert::IsTrue (drag.OnPoll (false, s_kOther) == Event::Canceled);
        }


        TEST_METHOD (APollAfterTheButtonIsUpEndsAMoveOnce)
        {
            DxuiCaptionDragTracker  drag;



            Assert::IsTrue (drag.OnTick (true, s_kSize)  == Event::Moved);
            Assert::IsTrue (drag.OnPoll (true, s_kSize)  == Event::None, L"still held");
            Assert::IsTrue (drag.OnPoll (false, s_kSize) == Event::Ended);
            Assert::IsTrue (drag.OnLoopEnd (s_kSize)     == Event::None);
        }
    };
}
