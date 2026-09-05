#include "Pch.h"

#include "Ui/Chrome/DriveWidget.h"
#include "Devices/Disk/Disk2NibbleEngine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetHitTests
//
//  Which REGION of a drive widget a point falls in: the body, the eject door,
//  or neither.
//
//  The two regions do different things -- the body browses for a disk, the door
//  ejects -- so a boundary that is one pixel out swaps a mount for an eject.
//  That is why the region matters more than the hit.
//
//  Points on each boundary are tested explicitly, since the door sits inside
//  the widget's outer rect and the two rects share an edge.
//
//  A HIDDEN widget must report a miss everywhere. Its rects are collapsed to
//  nothing on a machine with no controller, and a hit test that ignored that
//  would make an invisible drive clickable.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetHitTests)
{
public:

    TEST_METHOD (HitTest_Returns_Expected_Regions)
    {
        DriveWidget    drive;
        RECT           body   = {};
        RECT           eject  = {};
        DxuiDpiScaler  scaler;
        RECT           anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);
        body  = drive.GetBodyRect();
        eject = drive.GetEjectRect();

        Assert::IsTrue (drive.HitTest ((body.left + body.right) / 2,
                                       body.top + 5) == DriveWidgetRegion::Body);
        Assert::IsTrue (drive.HitTest (eject.right + 1,
                                       (body.top + body.bottom) / 2) == DriveWidgetRegion::Body);
        Assert::IsTrue (drive.HitTest ((eject.left + eject.right) / 2,
                                       (eject.top + eject.bottom) / 2) == DriveWidgetRegion::Eject);
        Assert::IsTrue (drive.HitTest (body.left - 1, body.top - 1) == DriveWidgetRegion::None);
    }


    TEST_METHOD (Led_Uses_Active_State_When_Motor_Is_On)
    {
        DriveWidget       drive;
        DriveWidgetState  state;
        DxuiDpiScaler     scaler;
        RECT              anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);
        state.mountedImagePath = L"boot.dsk";
        state.motorOn.store (true, std::memory_order_relaxed);
        state.diskActive.store (false, std::memory_order_relaxed);

        drive.SyncFromState (state);

        Assert::IsTrue (drive.GetLed() == LedState::Active);
    }


    TEST_METHOD (Led_Uses_Active_State_When_Nibble_Counters_Move)
    {
        DriveWidget       drive;
        DriveWidgetState  state;
        DxuiDpiScaler     scaler;
        RECT              anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.Layout (anchor, scaler);
        state.mountedImagePath = L"boot.dsk";
        state.motorOn.store (false, std::memory_order_relaxed);
        state.diskActive.store (true, std::memory_order_relaxed);

        drive.SyncFromState (state);

        Assert::IsTrue (drive.GetLed() == LedState::Active);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetNameRollTests
//
//  The 2D drive's name ROLL: a mount or an eject slides the old label out of
//  the name row and the new one in behind it, because a flat theme has no door
//  to swing.
//
//  Worth testing rather than eyeballing. The outgoing name has to be captured
//  at SYNC time, since BeginEject clears mountedImagePath the instant it
//  starts, so by the time anything paints, the name being rolled away is
//  already gone from the state.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetNameRollTests)
{
public:

    static void Sync (DriveWidget & w, const wchar_t * path)
    {
        DriveWidgetState  st;

        st.mountedImagePath = (path != nullptr) ? path : L"";
        w.SyncFromState (st);
    }


    TEST_METHOD (FirstSync_doesNotRoll)
    {
        DriveWidget  w;

        w.SetCompact (true);

        // A drive that already has a disk when the chrome is built has not
        // just been handed one, so nothing should slide.
        Sync (w, L"C:\\disks\\game.dsk");

        Assert::IsFalse (w.IsNameRolling (1),
                         L"the first label appears rather than rolling in");
    }


    TEST_METHOD (Eject_rollsTheNameAway)
    {
        DriveWidget  w;

        w.SetCompact (true);

        Sync (w, L"C:\\disks\\game.dsk");
        Sync (w, nullptr);

        int64_t  now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                           std::chrono::steady_clock::now().time_since_epoch()).count();

        Assert::IsTrue (w.IsNameRolling (now),
                        L"an eject starts a roll");
        Assert::IsFalse (w.IsNameRolling (now + DriveWidgetState::kDoorAnimationMs + 1),
                         L"and it is over after the door animation's own duration");
    }


    TEST_METHOD (Mount_rollsTheNameIn)
    {
        DriveWidget  w;

        w.SetCompact (true);

        Sync (w, nullptr);
        Sync (w, L"C:\\disks\\game.dsk");

        int64_t  now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                           std::chrono::steady_clock::now().time_since_epoch()).count();

        Assert::IsTrue (w.IsNameRolling (now),
                        L"a mount starts a roll too");
    }


    TEST_METHOD (SameDiskResynced_doesNotRoll)
    {
        DriveWidget  w;

        w.SetCompact (true);

        Sync (w, L"C:\\disks\\game.dsk");
        Sync (w, L"C:\\disks\\game.dsk");

        int64_t  now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                           std::chrono::steady_clock::now().time_since_epoch()).count();

        // SyncFromState runs every UI frame. If an unchanged label armed a
        // roll, the row would slide continuously and never settle.
        Assert::IsFalse (w.IsNameRolling (now),
                         L"the per-frame resync of an unchanged label is not a roll");
    }


    TEST_METHOD (FullPathChangeWithSameBasename_doesNotRoll)
    {
        DriveWidget  w;

        w.SetCompact (true);

        Sync (w, L"C:\\a\\game.dsk");
        Sync (w, L"C:\\b\\game.dsk");

        int64_t  now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                           std::chrono::steady_clock::now().time_since_epoch()).count();

        // The row shows a basename. Rolling one name out and the identical
        // name in reads as a glitch, not as a disk change.
        Assert::IsFalse (w.IsNameRolling (now),
                         L"the roll follows what the row SHOWS, not the path behind it");
    }


    TEST_METHOD (BandHover_reportsOnlyTheEDGES)
    {
        DriveWidget  w;

        w.SetCompact (true);

        // The shell calls this on EVERY mouse move. The compact band draws a
        // button treatment for hover, and the only thing that asks for a
        // repaint is this returning true, so a stationary pointer must report
        // false or the drive band would repaint on every mouse move forever.
        Assert::IsTrue  (w.UpdateMarqueeHover (true,  100), L"entering is a change");
        Assert::IsFalse (w.UpdateMarqueeHover (true,  110), L"staying is not");
        Assert::IsFalse (w.UpdateMarqueeHover (true,  120), L"still not");
        Assert::IsTrue  (w.UpdateMarqueeHover (false, 130), L"leaving is a change");
        Assert::IsFalse (w.UpdateMarqueeHover (false, 140), L"staying away is not");
    }


    TEST_METHOD (SkeuoWidget_neverRolls)
    {
        DriveWidget  w;   // not compact: the skeuo drive swings a real door

        Sync (w, L"C:\\disks\\game.dsk");
        Sync (w, nullptr);

        int64_t  now = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                           std::chrono::steady_clock::now().time_since_epoch()).count();

        Assert::IsFalse (w.IsNameRolling (now),
                         L"the full widget animates its door instead");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetCompactGeometryTests
//
//  The 2D widget's RECTS, rather than what it paints into them.
//
//  These cover faults that four green build configurations and the whole suite
//  went straight past, because every one of them is a rectangle of the wrong
//  size rather than code that is never reached.
//
//  Two rects decide behavior between them and are read by different callers.
//  The shell takes hover from the OUTER rect and the click from HitTest, which
//  reads the eject and body rects, so a column inside one and outside the other
//  is a control that lights up and then does nothing. And the compact branch
//  owns two rects the modeled branch never writes, so a widget that changes
//  theme carries them across unless the other branch clears them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetCompactGeometryTests)
{
public:

    static void LayOut (DriveWidget & drive, bool compact, const RECT & anchor)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        drive.Initialize (6, 0, nullptr);
        drive.SetCompact (compact);
        drive.Layout (anchor, scaler);
    }


    TEST_METHOD (Compact_CaptionColumnIsPartOfTheControl)
    {
        DriveWidget  drive;
        RECT         anchor = { 100, 200, 100, 200 };
        RECT         outer  = {};
        RECT         body   = {};

        LayOut (drive, true, anchor);
        outer = drive.GetOuterRect();
        body  = drive.GetBodyRect();

        Assert::IsTrue (outer.left < body.left,
                        L"the compact widget hangs its caption column off the left of the band");

        // The caption used to sit inside the outer rect and outside every rect
        // HitTest reads, so pointing at "DRIVE 1" lit the band's button
        // treatment and clicking it did nothing at all.
        Assert::IsTrue (drive.HitTest (outer.left + 1, (body.top + body.bottom) / 2)
                        == DriveWidgetRegion::Eject,
                        L"a click on the caption ejects, like a click anywhere else on the stack");
    }


    TEST_METHOD (Compact_EveryColumnTheShellHoversIsClickable)
    {
        DriveWidget  drive;
        RECT         anchor = { 100, 200, 100, 200 };
        RECT         outer  = {};
        int          midY   = 0;
        int          x      = 0;

        LayOut (drive, true, anchor);
        outer = drive.GetOuterRect();
        midY  = (drive.GetBodyRect().top + drive.GetBodyRect().bottom) / 2;

        // The shell decides hover from the OUTER rect and the click from
        // HitTest. The two have to agree across the whole width, or the widget
        // advertises a target somewhere it will not honor.
        for (x = outer.left; x < outer.right; x++)
        {
            Assert::IsTrue (drive.HitTest (x, midY) != DriveWidgetRegion::None,
                            L"a column the hover treatment lights must answer a click");
        }
    }


    TEST_METHOD (ModeledAfterCompact_LaysOutLikeItNeverWasCompact)
    {
        DriveWidget  switched;
        DriveWidget  reference;
        RECT         compactAnchor = { 400, 700, 400, 700 };
        RECT         anchor        = { 100, 200, 100, 200 };
        RECT         a             = {};
        RECT         b             = {};

        // Compact FIRST, and at a position far from the origin, which is what
        // gives the caption rect coordinates that stand out if they survive.
        LayOut (switched,  true,  compactAnchor);
        LayOut (switched,  false, anchor);
        LayOut (reference, false, anchor);

        a = switched.GetOuterRect();
        b = reference.GetOuterRect();

        // The head bar and the caption belong to the compact branch, and
        // nothing in the modeled branch writes them. Carried across, the
        // caption inflated GetOuterRect -- which is the widget's own bounds,
        // the probe that sizes the drive row, and the shell's hover test.
        Assert::AreEqual (b.left,   a.left,   L"no caption column on a drive that draws no caption");
        Assert::AreEqual (b.top,    a.top,    L"top unchanged by the theme it came from");
        Assert::AreEqual (b.right,  a.right,  L"right unchanged by the theme it came from");
        Assert::AreEqual (b.bottom, a.bottom, L"no stale caption stretching the box downward");
    }


    TEST_METHOD (HideAfterCompact_CollapsesEveryRect)
    {
        DriveWidget  drive;
        RECT         anchor = { 400, 700, 400, 700 };
        RECT         outer  = {};

        LayOut (drive, true, anchor);
        drive.Hide();
        outer = drive.GetOuterRect();

        // Hide zeroes the rects so a machine with no controller shows no disk
        // UI. It has to reach the compact-only rects too: GetOuterRect folds a
        // non-empty caption in, so one left behind would keep a hidden widget
        // occupying space and answering the hover test.
        Assert::AreEqual (0L, outer.left,   L"a hidden widget occupies nothing");
        Assert::AreEqual (0L, outer.top,    L"a hidden widget occupies nothing");
        Assert::AreEqual (0L, outer.right,  L"a hidden widget occupies nothing");
        Assert::AreEqual (0L, outer.bottom, L"a hidden widget occupies nothing");
    }


    TEST_METHOD (HeadCore_StaysOnTheRailAcrossTheEnginesWholeRange)
    {
        const int    barLeft  = 100;
        const int    barW     = 140;
        const float  coreHalf = 6.0f;
        float        lowest   = (float) barLeft + coreHalf;
        float        highest  = (float) (barLeft + barW) - coreHalf;
        int          q        = 0;

        // Walked over the ENGINE's range rather than the rail's. The rail
        // spans 35 tracks and Disk2NibbleEngine clamps to its own wider
        // maximum, so a disk that steps the head outward past track 35 reports
        // a position the scale has no room for. Reading kMaxTrack from the
        // engine means this fails if that range ever widens again.
        for (q = Disk2NibbleEngine::kMinTrack; q <= Disk2NibbleEngine::kMaxTrack; q++)
        {
            float  cx = DriveWidget::GetHeadCoreCenterX (q, barLeft, barW, coreHalf);

            Assert::IsTrue (cx >= lowest,
                            L"the core never runs off the left end of the rail");
            Assert::IsTrue (cx <= highest,
                            L"the core never runs off the right end of the rail");
        }
    }


    TEST_METHOD (HeadCore_InsetAtBothEndsAndParkedWhenOverStepped)
    {
        const int    barLeft  = 100;
        const int    barW     = 140;
        const float  coreHalf = 6.0f;
        float        last     = DriveWidget::GetHeadCoreCenterX (139, barLeft, barW, coreHalf);

        // Inset by the core's own half-width, so the lit spot is whole at the
        // ends rather than clipped in half by the rail it rides on.
        Assert::AreEqual (106.0f, DriveWidget::GetHeadCoreCenterX (0, barLeft, barW, coreHalf),
                          0.01f, L"track 0 sits a core half-width in from the left");
        Assert::AreEqual (234.0f, last,
                          0.01f, L"the rail's last track sits a core half-width in from the right");

        // An over-stepped head parks at the end of the scale instead of
        // carrying on past it, which is what put the core outside the widget.
        Assert::AreEqual (last,
                          DriveWidget::GetHeadCoreCenterX (Disk2NibbleEngine::kMaxTrack,
                                                           barLeft, barW, coreHalf),
                          0.01f, L"the engine's outermost position parks on the rail's last track");
    }
};
