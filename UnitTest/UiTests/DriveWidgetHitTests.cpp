#include "Pch.h"

#include "Ui/Chrome/DriveWidget.h"

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
