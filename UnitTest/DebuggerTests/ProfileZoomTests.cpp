#include "Pch.h"

#include "Debugger/RoutineProfile.h"
#include "Ui/Debugger/Panes/ProfileZoom.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProfileZoomTests
//
//  The Profiler's zoom: the stack back and forward move through, where a
//  profile from history goes by the cycles it covers, and the zoom and pan
//  of the timeline's range within the profile's.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ProfileZoomTests)
    {
    public:

        using Range = ProfileZoom::Range;

        static constexpr int64_t  kFrame = (int64_t) RoutineProfile::kCyclesPerFrame;


        //  Back and forward move through the states pushed; a push after going
        //  back drops the states that were ahead.
        TEST_METHOD (BackAndForwardMoveThroughTheStates)
        {
            ProfileZoom         zoom;
            ProfileZoom::State  first;
            ProfileZoom::State  second;
            ProfileZoom::State  third;



            first.view  = Range { 0, 100 * kFrame };
            second.view = Range { 0, 50 * kFrame };
            third.view  = Range { 50 * kFrame, 100 * kFrame };

            Assert::IsFalse (zoom.CanGoBack());

            zoom.Push (first);
            zoom.Push (second);
            Assert::IsTrue  (zoom.Back());
            Assert::IsTrue  (zoom.GetState().view == first.view);
            Assert::IsTrue  (zoom.Back());
            Assert::IsFalse (zoom.GetState().view.has_value(), L"the whole profile");
            Assert::IsFalse (zoom.Back(), L"nothing before the first state");
            Assert::IsTrue  (zoom.Forward());
            Assert::IsTrue  (zoom.GetState().view == first.view);

            zoom.Push (third);
            Assert::IsFalse (zoom.CanGoForward(), L"the second state was dropped");
            Assert::IsTrue  (zoom.Back() && zoom.GetState().view == first.view);
        }


        //  A profile is taken once, by its build, and goes where the request
        //  for its range was for; a build the pane didn't request is a whole
        //  new profile and empties the stack.
        TEST_METHOD (AProfileGoesWhereItsRequestWasFor)
        {
            using Target = ProfileZoom::Target;

            ProfileZoom         zoom;
            ProfileZoom::State  zoomed;
            ProfileZoom::State  selected;
            Range               view      = { 100, 900 };
            Range               selection = { 300, 400 };



            Assert::IsTrue (zoom.Route (std::nullopt, 0) == Target::Whole, L"a profile counted as the machine ran");
            Assert::IsTrue (zoom.Route (CycleRange (0, 1000), 1) == Target::Whole, L"the period's profile");
            Assert::IsTrue (zoom.Route (CycleRange (0, 1000), 1) == Target::None,  L"the same build in the next snapshot");

            zoomed.view = view;
            zoom.Push       (zoomed);
            zoom.SetRequest ({ view, false });
            Assert::IsTrue (zoom.Route (CycleRange (0, 1000), 1) == Target::None, L"a build from before the request");
            Assert::IsTrue (zoom.Route (CycleRange (100, 900), 2) == Target::Both);

            selected.view      = view;
            selected.selection = selection;
            zoom.Push       (selected);
            zoom.SetRequest ({ selection, true });
            Assert::IsTrue (zoom.Route (CycleRange (300, 400), 3) == Target::Lists, L"the selection's profile is the lists'");

            zoom.SetRequest ({ view, false });
            Assert::IsTrue (zoom.Route (CycleRange (100, 900), 4) == Target::Timeline, L"with a selection, a view's profile is the timeline's");
            Assert::IsTrue (zoom.Route (CycleRange (300, 400), 5) == Target::None, L"a range requested before the last request");

            zoom.SetRequest ({ selection, true });
            zoom.Back();
            Assert::IsTrue (zoom.Route (CycleRange (300, 400), 6) == Target::None, L"the selection is gone");

            Assert::IsTrue  (zoom.Route (CycleRange (500, 2000), 7) == Target::Whole, L"a build the pane didn't request");
            Assert::IsFalse (zoom.CanGoBack(), L"a whole new profile empties the stack");
        }


        //  After a zoom the cycle under the pointer is at the same place, the
        //  view is within the profile's range and at least a few frames
        //  wide, and a zoom out to all of it, or past, is no zoom.
        TEST_METHOD (AZoomKeepsTheAnchorAndStaysInTheProfile)
        {
            Range                 limit = { 0, 1000 * kFrame };
            std::optional<Range>  in    = ProfileZoom::ZoomAround (limit, 500 * kFrame, 0.5, limit);
            std::optional<Range>  edge  = ProfileZoom::ZoomAround (limit, 0, 0.5, limit);
            std::optional<Range>  tiny  = ProfileZoom::ZoomAround (limit, 500 * kFrame, 0.0001, limit);



            Assert::IsTrue  (in == Range { 250 * kFrame, 750 * kFrame }, L"half as wide, centered on the anchor");
            Assert::IsTrue  (edge == Range { 0, 500 * kFrame }, L"at the left edge, the anchor stays at the left");
            Assert::AreEqual (ProfileZoom::kMinFrames * kFrame, tiny->second - tiny->first, L"no narrower than the least");
            Assert::IsFalse (ProfileZoom::ZoomAround (*in, 500 * kFrame, 4.0, limit).has_value(), L"zoomed out past the profile");

            Assert::IsTrue  (ProfileZoom::Pan (*in, 1000 * kFrame, limit) == Range { 500 * kFrame, 1000 * kFrame }, L"a pan stops at the profile's end");
            Assert::IsFalse (ProfileZoom::Pan (limit, kFrame, limit).has_value(), L"nothing to pan at no zoom");
        }


        //  A drag selects the whole frames it crosses, dragged either way,
        //  within the profile.
        TEST_METHOD (ASelectionIsWholeFrames)
        {
            Range  limit = { 10 * kFrame, 100 * kFrame };



            Assert::IsTrue (ProfileZoom::GetFramesBetween (20 * kFrame + 5, 22 * kFrame + 5, limit) == Range { 20 * kFrame, 23 * kFrame });
            Assert::IsTrue (ProfileZoom::GetFramesBetween (22 * kFrame + 5, 20 * kFrame + 5, limit) == Range { 20 * kFrame, 23 * kFrame }, L"dragged right to left");
            Assert::IsTrue (ProfileZoom::GetFramesBetween (30 * kFrame, 30 * kFrame, limit) == Range { 30 * kFrame, 31 * kFrame }, L"at least one frame");
            Assert::IsTrue (ProfileZoom::GetFramesBetween (0, 200 * kFrame, limit) == limit, L"within the profile");
        }
    };
}
