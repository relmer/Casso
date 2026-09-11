#include "Pch.h"

#include "Shell/Input/CapsLockLatch.h"

#include "FakeHostCapsLock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CapsLockLatchTests
//
//  The emulator-owned Caps Lock latch against a fake host toggle: focus
//  drives the host to the latch and back, the user's key-up moves the latch,
//  and the key-up from our own flip does not.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CapsLockLatchTests)
{
public:

    TEST_METHOD (StartsLatchedOnWithoutTouchingTheHost)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        Assert::IsTrue  (latch.IsLatched());
        Assert::IsFalse (latch.HasFocus());
        Assert::AreEqual (0, host.toggles, L"nothing moves until focus arrives");
    }


    TEST_METHOD (GainingFocusTurnsAHostThatWasOffOn)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();

        Assert::IsTrue (host.isOn);
        Assert::AreEqual (1, host.toggles);
    }


    TEST_METHOD (GainingFocusLeavesAHostAlreadyMatchingAlone)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = true;
        latch.OnGainedFocus();

        Assert::IsTrue (host.isOn);
        Assert::AreEqual (0, host.toggles, L"a flip would have turned it off");
    }


    TEST_METHOD (LosingFocusRestoresTheParkedHostValue)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();
        latch.OnLostFocus();

        Assert::IsFalse (host.isOn);
        Assert::AreEqual (2, host.toggles);
    }


    TEST_METHOD (LosingFocusTwiceRestoresOnce)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();
        latch.OnLostFocus();
        latch.OnLostFocus();

        Assert::IsFalse (host.isOn, L"a second restore would flip it back on");
        Assert::AreEqual (2, host.toggles);
    }


    TEST_METHOD (GainingFocusTwiceParksTheHostValueOnce)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();
        latch.OnGainedFocus();
        latch.OnLostFocus();

        Assert::IsFalse (host.isOn, L"the second gain must not park our own value");
    }


    TEST_METHOD (ParkingHappensOnEveryGain)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();
        latch.OnLostFocus();

        // The user turns Caps Lock on in another program while we are away.
        host.isOn = true;
        latch.OnGainedFocus();
        latch.OnLostFocus();

        Assert::IsTrue (host.isOn, L"the value parked on the second gain is the one restored");
    }


    TEST_METHOD (TheUsersKeyUpBecomesTheLatch)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();

        // The user presses Caps Lock: the host has already flipped off.
        host.isOn = false;
        latch.OnCapsLockKeyUp();

        Assert::IsFalse (latch.IsLatched());
    }


    TEST_METHOD (TheLatchSurvivesAFocusRoundTrip)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();
        host.isOn = false;
        latch.OnCapsLockKeyUp();
        latch.OnLostFocus();

        host.isOn = true;
        latch.OnGainedFocus();

        Assert::IsFalse (host.isOn, L"the latch the user turned off is what focus applies");
    }


    TEST_METHOD (OurOwnKeyUpLeavesTheLatchAlone)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnGainedFocus();

        // Restore on focus loss flips the host off; that press echoes back.
        latch.OnLostFocus();
        host.lastKeyWasOurs = true;
        latch.OnCapsLockKeyUp();

        Assert::IsTrue (latch.IsLatched());
    }


    TEST_METHOD (AKeyUpWithoutFocusLeavesTheLatchAlone)
    {
        FakeHostCapsLock  host;
        CapsLockLatch     latch (host);

        host.isOn = false;
        latch.OnCapsLockKeyUp();

        Assert::IsTrue (latch.IsLatched());
    }
};
