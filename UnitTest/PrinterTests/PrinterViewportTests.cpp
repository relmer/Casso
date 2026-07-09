#include "Pch.h"

#include "Devices/Printer/PrinterViewport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Live-preview viewport (FR-033): a ~1-page window over the fanfold strip that
// follows the print head, absorbs user scrollback, and snaps back to the live
// row after an idle gap. Clock-injected so every transition is deterministic.
namespace PrinterViewportTests
{
    // Small viewport (100 rows) + short snap (2000 ms) keeps the arithmetic
    // readable; the production defaults only change the constants.
    static PrinterViewport::Config Cfg (int rows = 100, int64_t snapMs = 2000)
    {
        PrinterViewport::Config   c;
        c.viewportRows = rows;
        c.snapDelayMs  = snapMs;
        return c;
    }


    TEST_CLASS (PrinterViewportTests)
    {
    public:

        TEST_METHOD (FollowsLiveRowByDefault)
        {
            PrinterViewport   v (Cfg ());

            Assert::IsTrue (v.FollowingLive ());

            v.Advance (500);

            PrinterViewport::Span   s = v.VisibleSpan ();
            Assert::AreEqual (500, s.lastRow);
            Assert::AreEqual (401, s.firstRow);   // exactly 100 rows tall
        }


        TEST_METHOD (SpanClampsAtTopOfStrip)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (30);   // strip shorter than the viewport

            PrinterViewport::Span   s = v.VisibleSpan ();
            Assert::AreEqual (0,  s.firstRow);
            Assert::AreEqual (30, s.lastRow);
        }


        TEST_METHOD (AdvanceIsMonotonic)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (500);
            v.Advance (200);   // stale lesser value must not regress the view

            Assert::AreEqual (500, v.VisibleSpan ().lastRow);
        }


        TEST_METHOD (ScrollBackLeavesFollowMode)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (500);
            v.Scroll  (-150, 0);

            Assert::IsFalse (v.FollowingLive ());
            Assert::AreEqual (350, v.VisibleSpan ().lastRow);
        }


        TEST_METHOD (ScrolledViewIsAnchoredWhilePrintingContinues)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (500);
            v.Scroll  (-150, 0);
            v.Advance (900);   // print keeps going

            // The reviewed content must not drift as new rows land.
            Assert::AreEqual (350, v.VisibleSpan ().lastRow);
        }


        TEST_METHOD (ScrollClampsAtTopOfStrip)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (500);
            v.Scroll  (-100000, 0);

            // Furthest back = one full viewport against the top of the paper.
            PrinterViewport::Span   s = v.VisibleSpan ();
            Assert::AreEqual (0,  s.firstRow);
            Assert::AreEqual (99, s.lastRow);
        }


        TEST_METHOD (ScrollForwardToLiveResumesFollowing)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (500);
            v.Scroll  (-150, 0);
            Assert::IsFalse (v.FollowingLive ());

            v.Scroll  (+500, 100);   // past the live row

            Assert::IsTrue (v.FollowingLive ());
            Assert::AreEqual (500, v.VisibleSpan ().lastRow);
        }


        TEST_METHOD (SnapsBackToLiveAfterIdleDelay)
        {
            PrinterViewport   v (Cfg (100, 2000));

            v.Advance (500);
            v.Scroll  (-150, 1000);

            v.Tick (2999);   // 1999 ms idle: not yet
            Assert::IsFalse (v.FollowingLive ());

            v.Tick (3000);   // 2000 ms idle: snap
            Assert::IsTrue  (v.FollowingLive ());
            Assert::AreEqual (500, v.VisibleSpan ().lastRow);
        }


        TEST_METHOD (ContinuedScrollingDefersTheSnap)
        {
            PrinterViewport   v (Cfg (100, 2000));

            v.Advance (500);
            v.Scroll  (-150, 1000);
            v.Scroll  (-10,  2500);   // keeps interacting

            v.Tick (3500);            // only 1000 ms since the last scroll
            Assert::IsFalse (v.FollowingLive ());

            v.Tick (4500);
            Assert::IsTrue  (v.FollowingLive ());
        }


        TEST_METHOD (ScrollOnShortStripStaysFollowing)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (30);        // nowhere to scroll: strip < one viewport
            v.Scroll  (-50, 0);

            Assert::IsTrue (v.FollowingLive ());
            Assert::AreEqual (30, v.VisibleSpan ().lastRow);
        }


        TEST_METHOD (ResetReturnsToFollowingAtTop)
        {
            PrinterViewport   v (Cfg ());

            v.Advance (500);
            v.Scroll  (-150, 0);
            v.Reset   ();

            Assert::IsTrue (v.FollowingLive ());
            Assert::AreEqual (0, v.VisibleSpan ().lastRow);
            Assert::AreEqual (0, v.VisibleSpan ().firstRow);
        }
    };
}
