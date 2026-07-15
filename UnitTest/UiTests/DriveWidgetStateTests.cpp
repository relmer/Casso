#include "Pch.h"

#include "Ui/DriveWidgetState.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetStateTests
//
//  Pure-logic state transitions for the per-drive widget
//  state. No chrome context required.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DriveWidgetStateTests)
{
public:

    TEST_METHOD (FreshState_IsEmpty_DoorOpen_LedIdle)
    {
        DriveWidgetState  st;

        Assert::IsFalse (st.IsMounted(),
                         L"newly constructed state must not report a mount");
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Open,
                        L"door should start open (empty drive visual)");
        Assert::IsFalse (st.motorOn.load(),
                         L"motor flag should default false");
        Assert::IsFalse (st.diskActive.load(),
                         L"diskActive should default false");
    }

    TEST_METHOD (BeginInsert_FromOpenDoor_TransitionsToClosing)
    {
        DriveWidgetState  st;

        // Force the door open first so insert has something to close.
        st.BeginEject (0);
        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Open,
                        L"door should be Open after ejecting + settling");

        st.BeginInsert (L"C:\\images\\boot.dsk", 1000);

        Assert::IsTrue (st.IsMounted(),
                        L"insert must set mounted state");
        Assert::AreEqual (std::wstring (L"C:\\images\\boot.dsk"), st.mountedImagePath);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closing,
                        L"door should be Closing right after insert from Open");
        Assert::AreEqual<int64_t> (1000, st.animationStartTimeMs);
    }

    TEST_METHOD (BeginEject_FromClosed_OpensDoor_ClearsPath)
    {
        DriveWidgetState  st;

        st.BeginInsert (L"a.woz", 0);   // door transitions Open -> Closing
        Assert::IsTrue (st.IsMounted());

        st.BeginEject (500);

        Assert::IsFalse (st.IsMounted(),
                         L"eject must clear the mounted path");
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Opening,
                        L"door should be Opening right after eject");
        Assert::AreEqual<int64_t> (500, st.animationStartTimeMs);
    }

    TEST_METHOD (TickDoorAnimation_OpeningSettlesAtKDoorAnimationMs)
    {
        DriveWidgetState  st;

        // Precondition: force the door closed (default is now Open).
        st.BeginInsert (L"warmup.dsk", 0);
        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed);

        st.BeginEject (DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Opening);

        // Before the deadline -- still opening.
        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs + DriveWidgetState::kDoorAnimationMs - 1);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Opening,
                        L"door should remain Opening before the deadline");

        // At exactly the deadline -- settles to Open.
        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs + DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Open,
                        L"door should settle to Open at kDoorAnimationMs");
    }

    TEST_METHOD (TickDoorAnimation_ClosingSettlesAtKDoorAnimationMs)
    {
        DriveWidgetState  st;

        // Open then re-close with a known timestamp.
        st.BeginEject (0);
        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs);
        st.BeginInsert (L"x.dsk", 1000);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closing);

        st.TickDoorAnimation (1000 + DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed,
                        L"door should settle to Closed at start+kDoorAnimationMs");
    }

    TEST_METHOD (BeginInsert_FromClosed_NoAnimation)
    {
        DriveWidgetState  st;

        // Force the door closed first (default is now Open).
        st.BeginInsert (L"warmup.dsk", 0);
        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed,
                        L"warmup insert + tick should settle door to Closed");

        st.BeginInsert (L"first.dsk", 42);

        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed,
                        L"insert into already-closed drive should not start an animation");
    }

    TEST_METHOD (MotorAndActiveFlags_RoundTripViaAtomics)
    {
        DriveWidgetState  st;

        st.motorOn.store    (true,  std::memory_order_relaxed);
        st.diskActive.store (true,  std::memory_order_relaxed);

        Assert::IsTrue (st.motorOn.load (std::memory_order_relaxed));
        Assert::IsTrue (st.diskActive.load (std::memory_order_relaxed));

        st.motorOn.store    (false, std::memory_order_relaxed);
        Assert::IsFalse (st.motorOn.load (std::memory_order_relaxed));
    }

    TEST_METHOD (IsSupportedDiskImageExtension_AcceptsAllFiveCanonical)
    {
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.dsk"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.do"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.nib"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.woz"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.po"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"C:\\path\\to\\BOOT.DSK"),
                        L"extension check must be case-insensitive");
        Assert::IsTrue (IsSupportedDiskImageExtension (L"C:\\Demos\\MousePaint.DO"),
                        L".do (DOS-ordered) must be accepted, case-insensitively");
    }

    TEST_METHOD (IsSupportedDiskImageExtension_RejectsUnknownAndBare)
    {
        Assert::IsFalse (IsSupportedDiskImageExtension (L""));
        Assert::IsFalse (IsSupportedDiskImageExtension (L"image"));
        Assert::IsFalse (IsSupportedDiskImageExtension (L"image.txt"));
        Assert::IsFalse (IsSupportedDiskImageExtension (L"image.dmg"));
        Assert::IsFalse (IsSupportedDiskImageExtension (L"foo.bar.exe"));
    }

    TEST_METHOD (DoubleInsert_SamePathLeavesDoorClosedWithoutGlitching)
    {
        DriveWidgetState  st;

        st.BeginInsert (L"same.dsk", 0);
        DriveWidgetState::Door  before = st.doorState;

        st.BeginInsert (L"same.dsk", 100);

        Assert::IsTrue (st.doorState == before,
                        L"re-inserting the same path while closed must not retrigger animation");
        Assert::AreEqual (std::wstring (L"same.dsk"), st.mountedImagePath);
    }
};
