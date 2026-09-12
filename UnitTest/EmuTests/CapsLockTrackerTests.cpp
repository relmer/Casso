#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Shell/Input/CapsLockTracker.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CapsLockTrackerTests
//
//  The emulated Caps Lock reads as down until the first press in the emulator
//  window, then follows the host's. Only the first press, and only when it
//  turns the host's Caps Lock on, asks for the notice. A typed letter loses
//  the host's Caps Lock and gains the emulated one, and only letters are
//  touched. The last test runs the shell's full order -- host Caps Lock out,
//  //c Dvorak remap, emulated Caps Lock in -- over the keys where the order
//  matters.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CapsLockTrackerTests)
{
public:

    TEST_METHOD (DownBeforeAnyPressWhateverTheHostSays)
    {
        CapsLockTracker  tracker;

        Assert::IsFalse (tracker.IsFollowingHost());
        Assert::IsTrue  (tracker.IsOn (false), L"the host's Caps Lock is off, the //e's key is down");
        Assert::IsTrue  (tracker.IsOn (true));
    }


    TEST_METHOD (FirstPressTurningHostOnAsksForTheNotice)
    {
        CapsLockTracker  tracker;

        Assert::IsTrue (tracker.OnCapsLockPressed (true));
        Assert::IsTrue (tracker.IsFollowingHost());
        Assert::IsTrue (tracker.IsOn (true));
    }


    TEST_METHOD (FirstPressTurningHostOffTurnsItOffWithoutTheNotice)
    {
        CapsLockTracker  tracker;

        Assert::IsFalse (tracker.OnCapsLockPressed (false), L"the change is visible, so nothing needs explaining");
        Assert::IsTrue  (tracker.IsFollowingHost());
        Assert::IsFalse (tracker.IsOn (false));
    }


    TEST_METHOD (OnlyTheFirstPressAsksForTheNotice)
    {
        CapsLockTracker  tracker;

        Assert::IsTrue  (tracker.OnCapsLockPressed (true));
        Assert::IsFalse (tracker.OnCapsLockPressed (false));
        Assert::IsFalse (tracker.OnCapsLockPressed (true), L"the notice is shown once a session");
    }


    TEST_METHOD (FollowingHostTracksEitherWay)
    {
        CapsLockTracker  tracker;

        (void) tracker.OnCapsLockPressed (false);

        Assert::IsFalse (tracker.IsOn (false));
        Assert::IsTrue  (tracker.IsOn (true), L"a toggle made in another window is followed too");
    }


    TEST_METHOD (OnRaisesLowerCaseLetters)
    {
        Assert::AreEqual ((int) 'A', (int) CapsLockTracker::ApplyCapsLock ('a', true));
        Assert::AreEqual ((int) 'Z', (int) CapsLockTracker::ApplyCapsLock ('z', true));
        Assert::AreEqual ((int) 'Q', (int) CapsLockTracker::ApplyCapsLock ('Q', true), L"an upper-case letter stays");
    }


    TEST_METHOD (OnLeavesEverythingButLettersAlone)
    {
        Assert::AreEqual ((int) '1',  (int) CapsLockTracker::ApplyCapsLock ('1',  true));
        Assert::AreEqual ((int) '`',  (int) CapsLockTracker::ApplyCapsLock ('`',  true), L"one below 'a'");
        Assert::AreEqual ((int) '{',  (int) CapsLockTracker::ApplyCapsLock ('{',  true), L"one above 'z'");
        Assert::AreEqual ((int) '\'', (int) CapsLockTracker::ApplyCapsLock ('\'', true));
        Assert::AreEqual (0x0D,       (int) CapsLockTracker::ApplyCapsLock (0x0D, true));
    }


    TEST_METHOD (OffPassesLettersThrough)
    {
        Assert::AreEqual ((int) 'a', (int) CapsLockTracker::ApplyCapsLock ('a', false));
        Assert::AreEqual ((int) 'A', (int) CapsLockTracker::ApplyCapsLock ('A', false));
    }


    TEST_METHOD (HostCapsLockOffChangesNothing)
    {
        Assert::AreEqual ((int) 'a', (int) CapsLockTracker::RemoveHostCapsLock ('a', false));
        Assert::AreEqual ((int) 'A', (int) CapsLockTracker::RemoveHostCapsLock ('A', false));
    }


    TEST_METHOD (HostCapsLockOnInvertsLettersBack)
    {
        // No Shift under host Caps Lock arrives upper case; Shift arrives lower.
        Assert::AreEqual ((int) 'a', (int) CapsLockTracker::RemoveHostCapsLock ('A', true));
        Assert::AreEqual ((int) 'Z', (int) CapsLockTracker::RemoveHostCapsLock ('z', true));
    }


    TEST_METHOD (HostCapsLockOnLeavesEverythingButLettersAlone)
    {
        Assert::AreEqual ((int) '@', (int) CapsLockTracker::RemoveHostCapsLock ('@', true), L"one below 'A'");
        Assert::AreEqual ((int) '[', (int) CapsLockTracker::RemoveHostCapsLock ('[', true), L"one above 'Z'");
        Assert::AreEqual ((int) '`', (int) CapsLockTracker::RemoveHostCapsLock ('`', true), L"one below 'a'");
        Assert::AreEqual ((int) '{', (int) CapsLockTracker::RemoveHostCapsLock ('{', true), L"one above 'z'");
        Assert::AreEqual ((int) '5', (int) CapsLockTracker::RemoveHostCapsLock ('5', true));
    }


    TEST_METHOD (FollowingHostTypesTheCaseTheHostKeySets)
    {
        // With the host's Caps Lock off, the emulated key is up and an
        // unshifted Q is 'q'. With it on, Shift+Q arrives as 'q', but the
        // //e's key is down too, and Shift under Caps Lock is still 'Q'.
        CapsLockTracker  tracker;
        Byte             ch = 0;

        (void) tracker.OnCapsLockPressed (false);

        ch = CapsLockTracker::RemoveHostCapsLock ('q', false);
        ch = CapsLockTracker::ApplyCapsLock      (ch, tracker.IsOn (false));
        Assert::AreEqual ((int) 'q', (int) ch);

        ch = CapsLockTracker::RemoveHostCapsLock ('q', true);
        ch = CapsLockTracker::ApplyCapsLock      (ch, tracker.IsOn (true));
        Assert::AreEqual ((int) 'Q', (int) ch, L"Shift under Caps Lock types upper case on a //e");
    }


    TEST_METHOD (DvorakRemapSitsBetweenTheTwoCapsLocks)
    {
        // The QWERTY Q key is Dvorak's apostrophe key. With the host's Caps
        // Lock on it arrives as 'Q'; taking the host's Caps Lock out first
        // makes it the unshifted key, so the remap yields the apostrophe and
        // the emulated Caps Lock, which only raises letters, leaves it alone.
        // Remapping before undoing the host's Caps Lock would type a quote.
        Byte  ch = CapsLockTracker::RemoveHostCapsLock ('Q', true);

        ch = CapsLockTracker::ApplyCapsLock (Apple2eKeyboard::QwertyToDvorak (ch), true);
        Assert::AreEqual ((int) '\'', (int) ch);

        // The QWERTY S key is Dvorak's O: a letter, so Caps Lock raises it.
        ch = CapsLockTracker::RemoveHostCapsLock ('S', true);
        ch = CapsLockTracker::ApplyCapsLock (Apple2eKeyboard::QwertyToDvorak (ch), true);
        Assert::AreEqual ((int) 'O', (int) ch);
    }
};
