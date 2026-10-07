#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DragTestSite
//
//  A desk for a toolbar drag: an owner window at (0, 0) to (1000, 800) on a
//  monitor at 100%, and a second monitor at 150% to its right, from x 1000
//  on. A window's DPI is the DPI of the monitor holding most of it, and a
//  window moved onto the other monitor has its DPI changed under it, the
//  WM_DPICHANGED asking the session for the rectangle it takes there, as
//  Windows does from inside the SetWindowPos that moved it.
//
//  Docking goes by the pointer: within 64 of the owner's top, in a band
//  there, or within 64 of its right side. A pointer within 40 of the left
//  side stands the floating toolbar up, and within 40 of the top lays it
//  down. The toolbar floats 300 DIPs long and 40 thick.
//
////////////////////////////////////////////////////////////////////////////////

class DragTestSite : public IDxuiToolbarDragSite
{
public:
    static constexpr int   kSplitX      = 1000;
    static constexpr UINT  kLeftDpi     = 96;
    static constexpr UINT  kRightDpi    = 144;
    static constexpr RECT  kOwner       = { 0, 0, 1000, 800 };
    static constexpr int   kReach       = 64;
    static constexpr int   kTurn        = 40;
    static constexpr int   kPull        = 72;
    static constexpr int   kThickDip    = 40;

    DxuiToolbarDragSession  * session      = nullptr;
    int                       lengthDip    = 300;
    int                       insetDip     = 0;
    bool                      floating     = false;
    bool                      canTearOff   = true;
    bool                      vertical     = false;
    RECT                      window       = {};
    UINT                      windowDpi    = kLeftDpi;
    int                       slides       = 0;
    int                       placements   = 0;
    int                       dpiChanges   = 0;
    int                       drops        = 0;
    int                       leaves       = 0;
    int                       putDowns     = 0;
    int                       restores     = 0;
    bool                      previewShown = false;
    DxuiToolbarDock           preview;
    DxuiToolbarDock           dropped;
    std::vector<POINT>        dropQueries;


    static UINT GetMonitorDpi (const RECT & rect)
    {
        long  left  = (std::max) (0L, (std::min) (rect.right, (long) kSplitX) - rect.left);
        long  right = (std::max) (0L, rect.right - (std::max) (rect.left, (long) kSplitX));

        return (right > left) ? kRightDpi : kLeftDpi;
    }


    bool IsPulledOut (POINT screenPx) override
    {
        return screenPx.y > kPull || screenPx.x < kOwner.left || screenPx.x >= kOwner.right;
    }


    void SlideDocked (POINT) override
    {
        slides++;
    }


    bool TryTearOff (const RECT & rectPx, bool isVertical) override
    {
        if (!canTearOff)
        {
            return false;
        }

        floating  = true;
        vertical  = isVertical;
        window    = rectPx;
        windowDpi = GetMonitorDpi (rectPx);
        placements++;
        return true;
    }


    UINT GetFloatDpi() override
    {
        return floating ? windowDpi : kLeftDpi;
    }


    SIZE GetFloatSizePx (bool isVertical, UINT dpi) override
    {
        int  length = MulDiv (lengthDip, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        int  thick  = MulDiv (kThickDip, (int) dpi, USER_DEFAULT_SCREEN_DPI);

        return isVertical ? SIZE { thick, length } : SIZE { length, thick };
    }


    int GetFloatInsetPx (UINT dpi) override
    {
        return MulDiv (insetDip, (int) dpi, USER_DEFAULT_SCREEN_DPI);
    }


    bool PickVertical (POINT screenPx, bool current) override
    {
        bool  inside = screenPx.x >= kOwner.left && screenPx.x < kOwner.right && screenPx.y >= kOwner.top && screenPx.y < kOwner.bottom;

        if (!inside)
        {
            return current;
        }

        if (screenPx.x < kOwner.left + kTurn)
        {
            return true;
        }

        if (screenPx.y < kOwner.top + kTurn)
        {
            return false;
        }

        return current;
    }


    void PlaceFloat (const RECT & rectPx, bool isVertical) override
    {
        UINT  dpi       = GetMonitorDpi (rectPx);
        RECT  suggested = {};

        vertical = isVertical;
        window   = rectPx;
        placements++;

        if (dpi == windowDpi)
        {
            return;
        }

        //  Windows suggests the same rectangle scaled about its top left.
        windowDpi = dpi;
        dpiChanges++;
        suggested = RECT { rectPx.left, rectPx.top,
                           rectPx.left + MulDiv (rectPx.right  - rectPx.left, (int) dpi, (int) GetOtherDpi (dpi)),
                           rectPx.top  + MulDiv (rectPx.bottom - rectPx.top,  (int) dpi, (int) GetOtherDpi (dpi)) };

        if (!session->TryGetDpiRect (dpi, suggested))
        {
            Assert::Fail (L"a floating drag gives the window its rectangle at the new scale");
        }

        window = suggested;
        placements++;
    }


    static UINT GetOtherDpi (UINT dpi)
    {
        return (dpi == kLeftDpi) ? kRightDpi : kLeftDpi;
    }


    bool TryPickDrop (POINT screenPx, int, DxuiToolbarDock & outDock, bool & outNewBand) override
    {
        bool  inside = screenPx.x >= kOwner.left && screenPx.x < kOwner.right && screenPx.y >= kOwner.top && screenPx.y < kOwner.bottom;

        dropQueries.push_back (screenPx);
        outNewBand = false;

        if (!inside)
        {
            return false;
        }

        if (screenPx.y < kOwner.top + kReach)
        {
            outDock = DxuiToolbarDock::FromText (L"top 0 band 0");
            return true;
        }

        if (screenPx.x >= kOwner.right - kReach)
        {
            outDock = DxuiToolbarDock::FromText (L"right 0 band 0");
            return true;
        }

        return false;
    }


    void ShowDropPreview (bool show, const DxuiToolbarDock & dock, bool) override
    {
        previewShown = show;
        preview      = dock;
    }


    void DropDocked (const DxuiToolbarDock & dock, bool) override
    {
        drops++;
        dropped  = dock;
        floating = false;
    }


    void LeaveFloating() override { leaves++;   }
    void PutDown()       override { putDowns++; }
    void Restore()       override { restores++; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDragSessionTests
//
//  Recorded drags of a toolbar by its grab handle, fed to the session as the
//  messages the owner window sees, with the left button's state as of each:
//  the press-to-release drag is one drag however the toolbar tears off; the
//  floating window stays under the pointer where it was grabbed, in physical
//  pixels at every scale; nothing docks while the button is down; docking
//  goes by the pointer alone; and Escape, or losing the mouse, puts the
//  toolbar back.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarDragSessionTests)
{
public:

    using  Msg   = DxuiToolbarDragSession::Message;
    using  Phase = DxuiToolbarDragSession::Phase;

    static constexpr RECT  kDockedBar = { 8, 0, 308, 40 };
    static constexpr POINT kGrip      = { 58, 20 };


    struct Rig
    {
        DragTestSite            site;
        DxuiToolbarDragSession  session { site };

        Rig() { site.session = &session; }

        void Play (std::initializer_list<Msg> messages)
        {
            for (const Msg & msg : messages)
            {
                session.OnMessage (msg);
            }
        }

        //  A press on the docked toolbar's handle, 50 along it and 20 down.
        void PressDocked() { session.BeginDocked (kGrip, kDockedBar, false, DragTestSite::kLeftDpi); }

        //  The same press, then a pull well below the band, which floats it.
        void TearOff (POINT at)
        {
            PressDocked();
            Play ({ Move (58, 30), Move (at.x, at.y) });

            Assert::IsTrue (session.IsFloating(), L"the pull tears the toolbar off");
        }
    };


    static Msg Move (int x, int y)                { return Msg { WM_MOUSEMOVE,      POINT { x, y }, true,  0 }; }
    static Msg MoveUp (int x, int y)              { return Msg { WM_MOUSEMOVE,      POINT { x, y }, false, 0 }; }
    static Msg Up (int x, int y)                  { return Msg { WM_LBUTTONUP,      POINT { x, y }, false, 0 }; }
    static Msg Frame (bool down)                  { return Msg { WM_TIMER,          POINT {},       down,  0 }; }
    static Msg CaptureLost (bool down)            { return Msg { WM_CAPTURECHANGED, POINT {},       down,  0 }; }
    static Msg Key (WPARAM vk)                    { return Msg { WM_KEYDOWN,        POINT {},       true,  vk }; }


    static void AssertRect (const RECT & expected, const RECT & actual, const wchar_t * msg)
    {
        std::wstring  text = std::format (L"{} expected {{{}, {}, {}, {}}} actual {{{}, {}, {}, {}}}", msg,
                                          expected.left, expected.top, expected.right, expected.bottom,
                                          actual.left, actual.top, actual.right, actual.bottom);

        Assert::IsTrue (EqualRect (&expected, &actual) != FALSE, text.c_str());
    }


    TEST_METHOD (TheWindowRectHoldsTheGrabUnderThePointer)
    {
        AssertRect (RECT { 450, 280, 750, 320 }, DxuiToolbarDragSession::GetWindowRect (POINT { 500, 300 }, POINT { 50, 20 }, SIZE { 300, 40 }, 0, false, 96),
                    L"lying down at 100%");
        AssertRect (RECT { 417, 270, 867, 330 }, DxuiToolbarDragSession::GetWindowRect (POINT { 500, 300 }, POINT { 50, 20 }, SIZE { 450, 60 }, 8, false, 144),
                    L"at 150% the grab is 75 along, past an end 8 long, and 30 across");
        AssertRect (RECT { 470, 250, 510, 550 }, DxuiToolbarDragSession::GetWindowRect (POINT { 500, 300 }, POINT { 50, 30 }, SIZE { 40, 300 }, 0, true, 96),
                    L"standing up, along runs down and across runs right");
    }


    TEST_METHOD (ATearOffKeepsTheToolbarUnderThePointerWhereItWasGrabbed)
    {
        Rig  rig;

        rig.PressDocked();
        rig.Play ({ Move (58, 30), Move (70, 40) });

        Assert::IsTrue   (rig.session.GetPhase() == Phase::Docked, L"inside the band it slides");
        Assert::AreEqual (2, rig.site.slides);

        rig.Play ({ Move (58, 100) });

        Assert::IsTrue (rig.session.IsFloating(), L"pulled well away, it floats");
        AssertRect (RECT { 8, 80, 308, 120 }, rig.site.window, L"with the handle where the pointer took it");

        rig.Play ({ Move (400, 300) });
        AssertRect (RECT { 350, 280, 650, 320 }, rig.site.window, L"and the window follows the pointer");
    }


    TEST_METHOD (NothingDocksWhileTheButtonIsDown)
    {
        Rig  rig;

        rig.TearOff (POINT { 300, 300 });
        rig.Play ({ Move (300, 20) });

        Assert::IsTrue   (rig.site.previewShown,                                     L"over the top band, where it would dock is shown");
        Assert::IsTrue   (rig.site.preview.edge == DxuiToolbarDock::Edge::Top);
        Assert::AreEqual (0, rig.site.drops,                                          L"but nothing docks");
        Assert::IsTrue   (rig.session.IsFloating(),                                  L"and the drag goes on floating");
        AssertRect (RECT { 250, 0, 550, 40 }, rig.site.window,                       L"under the pointer");

        rig.Play ({ Move (320, 10), Move (300, 300) });

        Assert::AreEqual (0, rig.site.drops);
        Assert::IsFalse  (rig.site.previewShown, L"away from the band the preview goes");

        rig.Play ({ Move (300, 20), Up (300, 20) });

        Assert::AreEqual (1, rig.site.drops,     L"the release docks it");
        Assert::IsTrue   (rig.site.dropped.edge == DxuiToolbarDock::Edge::Top);
        Assert::IsFalse  (rig.site.previewShown, L"and the preview goes");
        Assert::IsTrue   (rig.session.GetPhase() == Phase::Idle);
    }


    //  A floating bar left over a band is pressed and let go where it is,
    //  with a move to the same point between, as Windows sends: a click on
    //  its handle is no drop.
    TEST_METHOD (AClickOnAFloatingHandleOverABandDoesNotDockIt)
    {
        Rig  rig;

        rig.site.floating = true;
        rig.site.window   = RECT { 250, 0, 550, 40 };

        rig.session.BeginFloating (POINT { 300, 20 }, rig.site.window, false, DragTestSite::kLeftDpi);
        rig.Play ({ Move (300, 20), Up (300, 20) });

        Assert::AreEqual (0, rig.site.drops,  L"no dock");
        Assert::AreEqual (1, rig.site.leaves, L"it stays floating where it was");
    }


    TEST_METHOD (AReleaseAwayFromEveryBandLeavesItFloating)
    {
        Rig  rig;

        rig.TearOff (POINT { 300, 300 });
        rig.Play ({ Move (500, 500), Up (500, 500) });

        Assert::AreEqual (1, rig.site.leaves);
        Assert::AreEqual (0, rig.site.drops);
        Assert::IsTrue   (rig.session.GetPhase() == Phase::Idle);
    }


    //  A long toolbar held at its left end: its right end runs across the
    //  right side's band while the pointer is in the middle of the window.
    //  Only the pointer decides.
    TEST_METHOD (TheFarEndOfALongToolbarDoesNotDockIt)
    {
        Rig  rig;

        rig.site.lengthDip = 800;
        rig.site.floating  = true;
        rig.site.window    = RECT { 100, 300, 900, 340 };

        rig.session.BeginFloating (POINT { 110, 320 }, rig.site.window, false, DragTestSite::kLeftDpi);
        rig.Play ({ Move (200, 320), Move (260, 320), Move (300, 330) });

        Assert::IsTrue  (rig.site.window.right > DragTestSite::kOwner.right - DragTestSite::kReach, L"the far end is over the right side's band");
        Assert::IsFalse (rig.site.previewShown, L"no preview");

        rig.Play ({ Up (300, 330) });

        Assert::AreEqual (0, rig.site.drops,  L"and no dock");
        Assert::AreEqual (1, rig.site.leaves, L"it stays floating");

        for (POINT queried : rig.site.dropQueries)
        {
            Assert::IsTrue (queried.x <= 300, L"every drop question was about the pointer");
        }
    }


    TEST_METHOD (EscapePutsTheToolbarBackAndSwallowsTheRestOfThePress)
    {
        Rig  rig;
        int  placed = 0;

        rig.TearOff (POINT { 300, 300 });
        rig.Play ({ Move (300, 20), Key (VK_ESCAPE) });

        Assert::AreEqual (1, rig.site.restores,  L"Escape puts it back");
        Assert::IsFalse  (rig.site.previewShown, L"with no preview left");
        Assert::IsTrue   (rig.session.GetPhase() == Phase::Canceled);

        placed = rig.site.placements;

        Assert::IsTrue   (rig.session.OnMessage (Move (400, 400)), L"the rest of the press is swallowed");
        Assert::AreEqual (placed, rig.site.placements,            L"and moves nothing");
        Assert::IsTrue   (rig.session.OnMessage (Up (300, 20)),     L"nor does the release");
        Assert::AreEqual (0, rig.site.drops);
        Assert::AreEqual (0, rig.site.leaves);
        Assert::IsTrue   (rig.session.GetPhase() == Phase::Idle);
    }


    TEST_METHOD (EscapeCancelsADockedCarryToo)
    {
        Rig  rig;

        rig.PressDocked();
        rig.Play ({ Move (200, 30), Key (VK_ESCAPE), Up (200, 30) });

        Assert::AreEqual (1, rig.site.restores);
        Assert::AreEqual (0, rig.site.putDowns, L"a canceled carry is not put down where it was carried");
    }


    TEST_METHOD (LosingTheMouseWhileTheButtonIsDownCancels)
    {
        UINT  kinds[] = { WM_CAPTURECHANGED, WM_CANCELMODE, WM_ENTERSIZEMOVE };

        for (UINT kind : kinds)
        {
            Rig  rig;

            rig.TearOff (POINT { 300, 300 });
            rig.Play ({ Msg { kind, POINT {}, true, 0 } });

            Assert::AreEqual (1, rig.site.restores, L"put back");
            Assert::AreEqual (0, rig.site.leaves);

            rig.Play ({ Frame (false) });
            Assert::IsTrue (rig.session.GetPhase() == Phase::Idle, L"and done once the button is up");
        }
    }


    //  The window releases the mouse before the release reaches the drag.
    TEST_METHOD (TheReleaseThatLetsGoOfTheMouseFirstStillDrops)
    {
        Rig  rig;

        rig.TearOff (POINT { 300, 300 });
        rig.Play ({ Move (300, 20), CaptureLost (false), Up (300, 20) });

        Assert::AreEqual (0, rig.site.restores);
        Assert::AreEqual (1, rig.site.drops);
    }


    TEST_METHOD (AReleaseThatNeverArrivesEndsTheDragOnTheNextFrame)
    {
        Rig  rig;
        Rig  moved;

        rig.TearOff (POINT { 300, 300 });
        rig.Play ({ Move (300, 20), Frame (true) });

        Assert::IsTrue (rig.session.IsFloating(), L"a frame with the button down changes nothing");

        rig.Play ({ Frame (false) });

        Assert::AreEqual (1, rig.site.drops, L"the button found up drops it where the pointer last was");
        Assert::IsTrue   (rig.session.GetPhase() == Phase::Idle);

        moved.TearOff (POINT { 300, 300 });
        moved.Play ({ MoveUp (500, 500) });

        Assert::AreEqual (1, moved.site.leaves, L"so does a move with the button up");
    }


    TEST_METHOD (CrossingOntoAMonitorAtAnotherScaleKeepsTheGrabInPhysicalPixels)
    {
        Rig  rig;
        int  placed  = 0;

        rig.TearOff (POINT { 300, 300 });

        placed = rig.site.placements;
        rig.Play ({ Move (1200, 300) });

        Assert::AreEqual (1, rig.site.dpiChanges, L"the window crossed onto the 150% monitor");
        AssertRect (RECT { 1125, 270, 1575, 330 }, rig.site.window, L"sized for 150% with the grab 75 along and 30 down");
        Assert::AreEqual (placed + 2, rig.site.placements, L"one move and one size for the new scale, no more");

        placed = rig.site.placements;
        rig.Play ({ Frame (true) });
        Assert::AreEqual (placed, rig.site.placements, L"a frame does not size it again");

        rig.Play ({ Move (1300, 300) });
        AssertRect (RECT { 1225, 270, 1675, 330 }, rig.site.window, L"on that monitor it moves at that scale");
        Assert::AreEqual (placed + 1, rig.site.placements);

        rig.Play ({ Move (500, 300) });
        Assert::AreEqual (2, rig.site.dpiChanges, L"and back");
        AssertRect (RECT { 450, 280, 750, 320 }, rig.site.window, L"at 100% again, still held 50 along and 20 down");
    }


    TEST_METHOD (ATearOffOntoAMonitorAtAnotherScaleTakesThatScale)
    {
        Rig  rig;

        rig.PressDocked();
        rig.Play ({ Move (1100, 300) });

        Assert::IsTrue   (rig.session.IsFloating());
        Assert::AreEqual (DragTestSite::kRightDpi, rig.site.windowDpi);
        AssertRect (RECT { 1025, 270, 1475, 330 }, rig.site.window, L"the window made there is sized for its scale, under the pointer");
    }


    TEST_METHOD (StandingUpNearASideKeepsTheGrabUnderThePointer)
    {
        Rig  rig;

        rig.TearOff (POINT { 300, 300 });
        rig.Play ({ Move (20, 400) });

        Assert::IsTrue (rig.site.vertical, L"near the left side it stands up");
        AssertRect (RECT { 0, 350, 40, 650 }, rig.site.window, L"held 50 down its length and 20 across");
    }


    TEST_METHOD (APressOnAFloatingHandleCountsFromTheToolbarNotTheWindowsEnd)
    {
        Rig  rig;

        rig.site.insetDip = 8;
        rig.site.floating = true;
        rig.site.window   = RECT { 100, 300, 416, 340 };

        rig.session.BeginFloating (POINT { 158, 320 }, rig.site.window, false, DragTestSite::kLeftDpi);

        Assert::AreEqual (50L, rig.session.GetGrabDip().x, L"50 along the toolbar, past the window's end");

        rig.Play ({ Move (500, 500) });
        AssertRect (RECT { 442, 480, 742, 520 }, rig.site.window, L"so the window follows with the toolbar where it was held");
    }


    TEST_METHOD (ADockedCarryReleasedIsPutDownWhereItWasCarried)
    {
        Rig  rig;

        rig.PressDocked();
        rig.Play ({ Move (200, 30), Up (200, 30) });

        Assert::AreEqual (1, rig.site.putDowns);
        Assert::AreEqual (0, rig.site.placements, L"it never floated");
        Assert::IsTrue   (rig.session.GetPhase() == Phase::Idle);
    }


    TEST_METHOD (ATearOffWithNoWindowGoesOnSlidingDocked)
    {
        Rig  rig;

        rig.site.canTearOff = false;
        rig.PressDocked();
        rig.Play ({ Move (58, 300) });

        Assert::IsTrue   (rig.session.GetPhase() == Phase::Docked);
        Assert::AreEqual (1, rig.site.slides);
    }


    TEST_METHOD (OnlyAFloatingDragSizesTheWindowForANewScale)
    {
        Rig   rig;
        RECT  rect = {};

        Assert::IsFalse (rig.session.TryGetDpiRect (144, rect), L"no drag");

        rig.PressDocked();
        Assert::IsFalse (rig.session.TryGetDpiRect (144, rect), L"a docked carry has no window");
    }
};
