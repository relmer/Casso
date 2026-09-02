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
        Assert::AreEqual (-1, st.headQuarterTrack.load(),
                          L"head position defaults to unknown, not track 0");
    }


    TEST_METHOD (HeadQuarterTrack_UnknownIsDistinctFromPositionZero)
    {
        DriveWidgetState  st;

        // A machine with no Disk ][ controller never has a head position, and
        // a readout that drew that as track 0 would park a marker at the outer
        // edge of a drive that does not exist. The sentinel has to survive the
        // round trip rather than being clamped on the way through.
        Assert::AreEqual (-1, st.headQuarterTrack.load(),
                          L"unknown is the initial value");

        st.headQuarterTrack.store (0);
        Assert::AreEqual (0, st.headQuarterTrack.load(),
                          L"quarter-track 0 is a real position, stored as itself");

        st.headQuarterTrack.store (139);
        Assert::AreEqual (139, st.headQuarterTrack.load(),
                          L"the outermost quarter-track survives, so the units "
                          L"are the engine's rather than whole tracks");
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



    //  A disk replaced under the running machine: the door opens, then closes
    //  on its own. This is the one door move the source-path poll cannot see,
    //  because the file in the drive did not change -- only its contents did.
    TEST_METHOD (BeginReinsert_FromClosed_OpensThenClosesOnItsOwn)
    {
        DriveWidgetState  st;
        const int64_t     kAnim = DriveWidgetState::kDoorAnimationMs;

        // Settle to Closed (a mounted drive at rest).
        st.BeginInsert (L"work.dsk", 0);
        st.TickDoorAnimation (kAnim);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed);

        // The swap: opens first.
        st.BeginReinsert (1000);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Opening,
                        L"a swap opens the door first");

        // Mid-open it is still opening, not resting.
        st.TickDoorAnimation (1000 + kAnim - 1);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Opening);

        // The open half completing rolls straight into closing -- it never
        // rests Open, because the disk is going back in.
        st.TickDoorAnimation (1000 + kAnim);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closing,
                        L"the open half rolls into the close half, no rest between");

        // And the close half settles Closed.
        st.TickDoorAnimation (1000 + kAnim + kAnim);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed,
                        L"the door ends closed with the new disk seated");
    }



    //  An eject that lands during a swap's open half must leave the empty
    //  drive resting OPEN, not roll the pending close shut on nothing. The
    //  pick-up runs on the CPU thread and the door poll on the UI thread, so
    //  an eject inside the ~350 ms open window is a real interleaving.
    TEST_METHOD (EjectDuringReinsertOpen_LeavesEmptyDriveOpen)
    {
        DriveWidgetState  st;
        const int64_t     kAnim = DriveWidgetState::kDoorAnimationMs;

        st.BeginInsert (L"work.dsk", 0);
        st.TickDoorAnimation (kAnim);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed);

        // Swap begins: door opening, close queued.
        st.BeginReinsert (1000);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Opening);

        // The user ejects mid-open.
        st.BeginEject (1000 + kAnim / 2);

        // Run past both animation halves.
        st.TickDoorAnimation (1000 + kAnim);
        st.TickDoorAnimation (1000 + kAnim + kAnim);

        Assert::IsFalse (st.IsMounted(), L"the drive is empty after the eject");
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Open,
                        L"an empty drive rests open -- the queued close must not seal it");
    }



    //  A mount of a different disk during a swap's open half must not strand
    //  the pending close, or a much-later eject of that disk would seal the
    //  emptied drive shut.
    TEST_METHOD (InsertDuringReinsertOpen_DoesNotStrandThePendingClose)
    {
        DriveWidgetState  st;
        const int64_t     kAnim = DriveWidgetState::kDoorAnimationMs;

        st.BeginInsert (L"a.dsk", 0);
        st.TickDoorAnimation (kAnim);

        st.BeginReinsert (1000);                 // swap A: opening, close queued
        st.BeginInsert (L"b.dsk", 1000 + kAnim / 2);   // mount B mid-open
        st.TickDoorAnimation (1000 + kAnim + kAnim);   // settle: B closed

        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed);

        // Much later, eject B. The empty drive must rest open.
        st.BeginEject (9000);
        st.TickDoorAnimation (9000 + kAnim);

        Assert::IsFalse (st.IsMounted());
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Open,
                        L"a stranded reinsert must not seal a later eject shut");
    }



    //  A swap while the door is already open (an empty drive that somehow took
    //  one) has only the close half left to do -- it must not re-open.
    TEST_METHOD (BeginReinsert_FromOpen_JustCloses)
    {
        DriveWidgetState  st;   // default door is Open



        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Open);

        st.BeginReinsert (0);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closing,
                        L"an already-open door only needs to close");

        st.TickDoorAnimation (DriveWidgetState::kDoorAnimationMs);
        Assert::IsTrue (st.doorState == DriveWidgetState::Door::Closed);
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

    TEST_METHOD (IsSupportedDiskImageExtension_AcceptsEveryMountableType)
    {
        // Named for the property, not a count. It used to say "the four",
        // which stopped being true the moment a fifth container mounted -- a
        // name that must be revised alongside the list it describes is a name
        // that will one day disagree with it silently.
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.dsk"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.do"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.woz"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.po"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.nib"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.nb2"));
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

    TEST_METHOD (IsSupportedDiskImageExtension_AcceptsNibbleImages)
    {
        // INVERTED, NOT DELETED. This asserted the opposite for as long as
        // nothing could load a nibble image, and it was right to: the filter
        // once offered .nib, the drop was taken, and the mount then failed
        // with no message at all. The capability now belongs asserted exactly
        // where its absence was, so the same seam keeps the same guard.
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.nib"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"a.nb2"));
        Assert::IsTrue (IsSupportedDiskImageExtension (L"C:\\Disks\\LODE.NIB"),
                        L"the .nib match must be case-insensitive");
        Assert::IsTrue (IsSupportedDiskImageExtension (L"C:\\Disks\\LODE.NB2"),
                        L"and so must .nb2");
    }

    TEST_METHOD (IsSupportedDiskImageExtension_AnswersExactlyWhatTheLoaderRoutes)
    {
        // The regression this locks down is a SECOND LIST. The filter and the
        // mount router disagreed over .nib for as long as both existed, and
        // the only durable fix is that there is now one answer. Sweeping a
        // corpus that contains both accepted and rejected types would pass
        // against a reintroduced private list only if that list were exactly
        // right -- which is the property worth asserting.
        static const wchar_t * const  kCandidates[] =
        {
            L"disk.dsk", L"disk.do",  L"disk.po",  L"disk.woz",
            L"disk.nib", L"disk.nb2", L"disk.2mg", L"disk.img",
            L"disk.txt",
            L"disk.DSK", L"disk.WoZ", L"disk",     L"disk.",
        };

        DiskFormat  fmt        = DiskFormat::Dsk;
        size_t      candidates = std::size (kCandidates);
        bool        routed     = false;
        bool        offered    = false;

        // A sweep over an empty corpus passes while checking nothing, and
        // reads identically to a full one.
        Assert::IsTrue (candidates > 0, L"the corpus must not be empty");

        for (const wchar_t * candidate : kCandidates)
        {
            std::wstring  wide   = candidate;
            std::string   narrow = std::filesystem::path (wide).string();
            HRESULT       hr     = DiskImageStore::GetSourceFormatByExtension (narrow, fmt);

            routed  = SUCCEEDED (hr);
            offered = IsSupportedDiskImageExtension (wide);

            Assert::IsTrue (routed == offered,
                            (L"filter and loader must agree about " + wide).c_str());
        }
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

    TEST_METHOD (WriteProtectTooltip_DamageSuppressesTheOtherCauses)
    {
        // Damage is the whole story, so it tells it alone.
        //
        // These previously appended: an image could be reported damaged AND
        // flagged AND behind the drive preference, all in one tooltip. That
        // reads as a list of things to go fix, and none of them can be fixed
        // -- the disk is unwritable because it is damaged, and clearing a flag
        // or a preference changes nothing while it stays that way. Naming them
        // sends the user hunting for a remedy that does not exist.
        WriteProtectInfo  wp;

        wp.checksumMismatch = true;
        wp.imageFlag        = true;
        wp.readOnlyFile     = true;
        wp.userSetting      = true;

        Assert::AreEqual (
            std::wstring (L"\"a.woz\" is damaged: its stored checksum does not match its "
                          L"contents. Casso will not write to it, because rewriting the file "
                          L"would hide the damage."),
            ComposeWriteProtectTooltip (1, L"a.woz", wp),
            L"damage stands alone, whatever else happens to be true");
    }


    TEST_METHOD (WriteProtectTooltip_UndamagedStillListsEveryCause)
    {
        // The suppression is specific to damage. With no damage the tooltip
        // still has to account for every reason the disk will not take a
        // write, because those the user CAN act on.
        WriteProtectInfo  wp;

        wp.imageFlag   = true;
        wp.userSetting = true;

        Assert::AreEqual (
            std::wstring (L"\"a.woz\" is write-protected (WOZ write-protect flag). "
                          L"Drive 1 is also write-protected in Settings > Disk."),
            ComposeWriteProtectTooltip (1, L"a.woz", wp),
            L"an undamaged image still lists its causes and still includes 'also'");
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
