#include "Pch.h"

#include "Shell/NormalPlacementTracker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NormalPlacementTrackerTests
//
//  The main window lost its invisible border on every maximize and restart:
//  saved while maximized, the border measured on the maximized window was
//  wider than the restored one, and restored at startup, the border measured
//  on the still-hidden window was zero. The rect saved is now the one the
//  window had restored, and the saved one is applied only once the window is
//  shown restored.
//
////////////////////////////////////////////////////////////////////////////////

namespace NormalPlacementTrackerTests
{
    static constexpr RECT  s_kGiven    = { -1700, -500, -500,  700 };
    static constexpr RECT  s_kMonitor  = { -2160, -827,    0, 3013 };
    static constexpr RECT  s_kUnframed = { -1691, -500, -509,  691 };
    static constexpr RECT  s_kMoved    = { -1600, -400, -300,  900 };




    static void AssertRectEqual (const RECT & expected, const RECT & actual)
    {
        Assert::AreEqual (expected.left,   actual.left,   L"left");
        Assert::AreEqual (expected.top,    actual.top,    L"top");
        Assert::AreEqual (expected.right,  actual.right,  L"right");
        Assert::AreEqual (expected.bottom, actual.bottom, L"bottom");
    }




    TEST_CLASS (NormalPlacementTrackerTests)
    {
    public:

        TEST_METHOD (AWindowRestartedMaximizedSavesTheRectItWasGiven)
        {
            NormalPlacementTracker  tracker;
            RECT                    saved   = {};
            bool                    found   = false;



            tracker.SetExpectedRect (s_kGiven);
            found = tracker.TryGetNormalRect (false, s_kMonitor, saved);

            Assert::IsTrue  (found);
            AssertRectEqual (s_kGiven, saved);
        }


        TEST_METHOD (AMeasurementBeforeTheRectIsAppliedDoesNotReplaceIt)
        {
            NormalPlacementTracker  tracker;
            RECT                    saved   = {};
            bool                    found   = false;



            //  Created at the visible rect with no border yet: what it
            //  measures is short of what was saved.
            tracker.SetExpectedRect (s_kGiven);
            found = tracker.TryGetNormalRect (true, s_kUnframed, saved);

            Assert::IsTrue  (found);
            AssertRectEqual (s_kGiven, saved);
        }


        TEST_METHOD (TheExpectedRectIsAppliedOnceTheWindowIsRestored)
        {
            NormalPlacementTracker  tracker;
            RECT                    applied        = {};
            bool                    whileMaximized = false;
            bool                    onRestore      = false;
            bool                    afterwards     = false;



            tracker.SetExpectedRect (s_kGiven);
            whileMaximized = tracker.TryTakePending (false, applied);
            onRestore      = tracker.TryTakePending (true,  applied);

            Assert::IsFalse (whileMaximized);
            Assert::IsTrue  (onRestore);
            AssertRectEqual (s_kGiven, applied);

            afterwards = tracker.TryTakePending (true, applied);
            Assert::IsFalse (afterwards);
        }


        TEST_METHOD (AMaximizedWindowSavesItsLastRestoredRect)
        {
            NormalPlacementTracker  tracker;
            RECT                    saved   = {};
            bool                    found   = false;



            found = tracker.TryGetNormalRect (true,  s_kGiven,   saved);
            found = tracker.TryGetNormalRect (false, s_kMonitor, saved);

            Assert::IsTrue  (found);
            AssertRectEqual (s_kGiven, saved);
        }


        TEST_METHOD (AMoveOfARestoredWindowIsSaved)
        {
            NormalPlacementTracker  tracker;
            RECT                    applied = {};
            RECT                    saved   = {};
            bool                    taken   = false;
            bool                    found   = false;



            tracker.SetExpectedRect (s_kGiven);
            taken = tracker.TryTakePending   (true, applied);
            found = tracker.TryGetNormalRect (true, s_kMoved, saved);

            Assert::IsTrue  (taken);
            Assert::IsTrue  (found);
            AssertRectEqual (s_kMoved, saved);
        }


        TEST_METHOD (NothingIsSavedBeforeAnyRestoredRect)
        {
            NormalPlacementTracker  tracker;
            RECT                    saved   = {};
            bool                    found   = false;



            found = tracker.TryGetNormalRect (false, s_kMonitor, saved);

            Assert::IsFalse (found);
        }
    };
}
