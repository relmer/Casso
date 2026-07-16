#include "Pch.h"

#include "Devices/Printer/PrinterViewport.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Live-preview follow adapter (FR-033): tracks the print head and decides when
// the view rides the live row versus stays where the user parked it, and
// reports the scroll bounds DxuiPanZoom clamps panY to. The eased scroll
// position itself lives in DxuiPanZoom (see DxuiPanZoomTests); this class is
// pure follow/snap policy. Clock-injected so every transition is deterministic.
namespace PrinterViewportTests
{
    // Small viewport (100 rows), short snap (2000 ms), small overscroll (25
    // rows) keep the arithmetic readable; the production defaults only
    // change the constants.
    static PrinterViewport::Config Cfg (int rows = 100, int64_t snapMs = 2000)
    {
        PrinterViewport::Config   c;
        c.viewportRows   = rows;
        c.snapDelayMs    = snapMs;
        c.overscrollRows = 25;
        return c;
    }


    TEST_CLASS (PrinterViewportTests)
    {
    public:

        TEST_METHOD (FollowsLiveRowByDefault)
        {
            PrinterViewport   v (Cfg());

            Assert::IsTrue (v.FollowingLive());

            v.Advance (500);

            Assert::AreEqual (500, v.LiveRow());
            Assert::IsTrue   (v.FollowingLive());
        }


        TEST_METHOD (AdvanceIsMonotonic)
        {
            PrinterViewport   v (Cfg());

            v.Advance (500);
            v.Advance (200);   // stale lesser value must not regress the live row

            Assert::AreEqual (500, v.LiveRow());
        }


        TEST_METHOD (NotifyUserScrollLeavesFollowMode)
        {
            PrinterViewport   v (Cfg());

            v.Advance (500);
            v.NotifyUserScroll (0);

            Assert::IsFalse (v.FollowingLive());
        }


        TEST_METHOD (BoundsSpanFullPageOfScrollbackPlusOverscroll)
        {
            PrinterViewport   v (Cfg());   // 100-row viewport, 25 overscroll

            v.Advance (500);

            // Furthest back = a full viewport against the top of the strip
            // (bottom row 99 -> first row 0); furthest forward = live + 25.
            Assert::AreEqual (99,  v.MinBottomRow());
            Assert::AreEqual (525, v.MaxBottomRow());
        }


        TEST_METHOD (ShortStripPinsMinBottomToLiveRow)
        {
            PrinterViewport   v (Cfg());

            v.Advance (30);   // strip shorter than the viewport: nowhere to scroll back

            Assert::AreEqual (30, v.MinBottomRow());   // == live row
            Assert::AreEqual (55, v.MaxBottomRow());   // live + overscroll still allowed
        }


        TEST_METHOD (SnapsBackToLiveOnceIdleAndPrintingContinued)
        {
            PrinterViewport   v (Cfg (100, 2000));

            v.Advance (500);
            v.NotifyUserScroll (1000);
            v.Advance (600);   // the print keeps going

            v.Tick (2999);   // 1999 ms idle: not yet
            Assert::IsFalse (v.FollowingLive());

            v.Tick (3000);   // 2000 ms idle: snap
            Assert::IsTrue  (v.FollowingLive());
        }


        TEST_METHOD (FinishedPrintStaysWhereTheUserScrolledIt)
        {
            // No new rows since the scroll: there is no "currently printing
            // row" to return to, so idling must NOT yank the view away.
            PrinterViewport   v (Cfg (100, 2000));

            v.Advance (500);
            v.NotifyUserScroll (1000);

            v.Tick (1000000);
            Assert::IsFalse (v.FollowingLive());
        }


        TEST_METHOD (ContinuedScrollingDefersTheSnap)
        {
            PrinterViewport   v (Cfg (100, 2000));

            v.Advance (500);
            v.NotifyUserScroll (1000);
            v.NotifyUserScroll (2500);   // keeps interacting
            v.Advance (900);             // print continues past the last scroll

            v.Tick (3500);               // only 1000 ms since the last scroll
            Assert::IsFalse (v.FollowingLive());

            v.Tick (4500);
            Assert::IsTrue  (v.FollowingLive());
        }


        TEST_METHOD (ResetReturnsToFollowingAtTop)
        {
            PrinterViewport   v (Cfg());

            v.Advance (500);
            v.NotifyUserScroll (0);
            v.Reset   ();

            Assert::IsTrue   (v.FollowingLive());
            Assert::AreEqual (0, v.LiveRow());
            Assert::AreEqual (0, v.MinBottomRow());
        }
    };
}
