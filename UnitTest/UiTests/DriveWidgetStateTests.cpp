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
        DriveWidgetState        st;
        DriveWidgetState::Door  before = {};

        st.BeginInsert (L"same.dsk", 0);
        before = st.doorState;

        st.BeginInsert (L"same.dsk", 100);

        Assert::IsTrue (st.doorState == before,
                        L"re-inserting the same path while closed must not retrigger animation");
        Assert::AreEqual (std::wstring (L"same.dsk"), st.mountedImagePath);
    }

    TEST_METHOD (WriteProtectTooltip_EmptyWhenNotProtected)
    {
        WriteProtectInfo  wp;

        Assert::IsTrue (ComposeWriteProtectTooltip (1, L"a.woz", wp).empty(),
                        L"An unprotected disk yields no tooltip text");
    }

    TEST_METHOD (WriteProtectTooltip_NamesTheImageAndTheSpecificCause)
    {
        WriteProtectInfo  wpSetting;
        WriteProtectInfo  wpImage;
        WriteProtectInfo  wpReadOnly;
        WriteProtectInfo  wpNoPerm;

        wpSetting.userSetting   = true;
        wpImage.imageFlag       = true;
        wpReadOnly.readOnlyFile = true;
        wpNoPerm.noPermission   = true;

        Assert::AreEqual (std::wstring (L"Drive 2 is write-protected in Settings > Disk."),
                          ComposeWriteProtectTooltip (2, L"a.woz", wpSetting));
        Assert::AreEqual (std::wstring (L"\"Blank Disk.woz\" is write-protected (WOZ write-protect flag)."),
                          ComposeWriteProtectTooltip (1, L"Blank Disk.woz", wpImage));
        Assert::AreEqual (std::wstring (L"\"foo.dsk\" is write-protected (file is read-only)."),
                          ComposeWriteProtectTooltip (1, L"foo.dsk", wpReadOnly));
        Assert::AreEqual (std::wstring (L"\"foo.dsk\" is write-protected (no write permission)."),
                          ComposeWriteProtectTooltip (1, L"foo.dsk", wpNoPerm));
    }

    TEST_METHOD (WriteProtectTooltip_DamagedImageLeadsWithItsOwnSentence)
    {
        // A damaged image is not a setting anyone chose, so reporting it as
        // plain write-protection would send the user hunting for a toggle that
        // will refuse them. It says what is wrong and that Casso will not
        // write, and it leads.
        WriteProtectInfo  wp;

        wp.checksumMismatch = true;

        Assert::AreEqual (
            std::wstring (L"\"suspect.woz\" is damaged: its stored checksum does not match "
                          L"its contents. Casso will not write to it, because rewriting the "
                          L"file would hide the damage."),
            ComposeWriteProtectTooltip (1, L"suspect.woz", wp));

        Assert::AreEqual (
            std::wstring (L"This disk image is damaged: its stored checksum does not match "
                          L"its contents. Casso will not write to it, because rewriting the "
                          L"file would hide the damage."),
            ComposeWriteProtectTooltip (1, std::wstring(), wp),
            L"and it still reads as a sentence without an image name");
    }

    TEST_METHOD (WriteProtectTooltip_DamageDoesNotSwallowTheOtherCauses)
    {
        // The damage sentence is additional, not a replacement -- an image can
        // be damaged AND carry its own write-protect flag AND sit behind the
        // drive preference, and the tooltip has to survive saying all three.
        WriteProtectInfo  wp;

        wp.checksumMismatch = true;
        wp.imageFlag        = true;

        Assert::AreEqual (
            std::wstring (L"\"a.woz\" is damaged: its stored checksum does not match its "
                          L"contents. Casso will not write to it, because rewriting the file "
                          L"would hide the damage. \"a.woz\" is write-protected (WOZ "
                          L"write-protect flag)."),
            ComposeWriteProtectTooltip (1, L"a.woz", wp),
            L"the cause list must append to the damage sentence, not overwrite it");

        wp.userSetting = true;

        Assert::IsTrue (
            ComposeWriteProtectTooltip (1, L"a.woz", wp).find (
                L"Drive 1 is also write-protected in Settings > Disk.") != std::wstring::npos,
            L"the drive preference still appends, and still says 'also'");
    }

    TEST_METHOD (WriteProtectTooltip_DamageAloneStillSaysAlsoForTheDrivePreference)
    {
        // "also" is chosen from whether anything preceded it, and a damage
        // sentence counts even though it is not in the cause list.
        WriteProtectInfo  wp;

        wp.checksumMismatch = true;
        wp.userSetting      = true;

        Assert::IsTrue (
            ComposeWriteProtectTooltip (2, L"a.woz", wp).find (
                L"Drive 2 is also write-protected") != std::wstring::npos,
            L"a preceding damage sentence must make the drive clause say 'also'");
    }

    TEST_METHOD (WriteProtectTooltip_FallsBackWithoutAName)
    {
        WriteProtectInfo  wp;

        wp.readOnlyFile = true;

        Assert::AreEqual (std::wstring (L"Disk image is write-protected (file is read-only)."),
                          ComposeWriteProtectTooltip (1, std::wstring(), wp));
    }

    TEST_METHOD (WriteProtectTooltip_MergesDiskCausesAndAppendsTheDrive)
    {
        WriteProtectInfo  wp;

        wp.imageFlag    = true;
        wp.readOnlyFile = true;

        Assert::AreEqual (
            std::wstring (L"\"a.woz\" is write-protected (WOZ write-protect flag; file is read-only)."),
            ComposeWriteProtectTooltip (1, L"a.woz", wp),
            L"Disk-borne causes merge into one parenthetical");

        wp.userSetting = true;

        Assert::AreEqual (
            std::wstring (L"\"a.woz\" is write-protected (WOZ write-protect flag; file is read-only). "
                          L"Drive 1 is also write-protected in Settings > Disk."),
            ComposeWriteProtectTooltip (1, L"a.woz", wp),
            L"The drive-level pref appends its own sentence with \"also\"");
    }
};
