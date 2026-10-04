#include "Pch.h"

#include "Shell/HeldHostInputs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HeldHostInputReleaseTests
//
//  While the machine is behind live the host's keys and mouse button do not
//  reach it, so a key the user lets go of then would stay down once the
//  machine is live again. The tracker gives the releases to inject on going
//  live: each key that reached the machine down and was let go of while
//  behind, and nothing for a key still held.
//
////////////////////////////////////////////////////////////////////////////////

namespace HeldHostInputRelease
{
    TEST_CLASS (HeldHostInputReleaseTests)
    {
    public:

        TEST_METHOD (AKeyLetGoOfBehindLiveIsReleasedOnGoingLive)
        {
            HeldHostInputs       held;
            std::vector<WPARAM>  releases;



            held.OnPress   ('A', true);
            held.OnRelease ('A', false);

            releases = held.TakeReleases();

            Assert::AreEqual ((size_t) 1, releases.size());
            Assert::AreEqual ((WPARAM) 'A', releases[0]);
            Assert::IsTrue   (held.TakeReleases().empty(), L"each release is given once");
        }


        TEST_METHOD (AKeyStillHeldIsLeftAlone)
        {
            HeldHostInputs  held;



            held.OnPress ('A', true);

            Assert::IsTrue (held.TakeReleases().empty());
        }


        TEST_METHOD (AKeyPressedAgainBeforeGoingLiveIsLeftAlone)
        {
            HeldHostInputs  held;



            held.OnPress   ('A', true);
            held.OnRelease ('A', false);
            held.OnPress   ('A', false);

            Assert::IsTrue (held.TakeReleases().empty(), L"held again, so still down");
        }


        TEST_METHOD (AKeyThatNeverReachedTheMachineGetsNoRelease)
        {
            HeldHostInputs  held;



            held.OnPress   ('B', false);
            held.OnRelease ('B', false);

            Assert::IsTrue (held.TakeReleases().empty(), L"the machine never had it down");
        }


        TEST_METHOD (AReleaseThatReachedTheMachineNeedsNoOther)
        {
            HeldHostInputs  held;



            held.OnPress   ('A', true);
            held.OnRelease ('A', true);

            Assert::IsTrue (held.TakeReleases().empty());
        }


        TEST_METHOD (TheMouseButtonIsTrackedLikeAKey)
        {
            HeldHostInputs       held;
            std::vector<WPARAM>  releases;



            held.OnPress   (HeldHostInputs::kMouseButton, true);
            held.OnRelease (HeldHostInputs::kMouseButton, false);

            releases = held.TakeReleases();

            Assert::AreEqual ((size_t) 1, releases.size());
            Assert::AreEqual (HeldHostInputs::kMouseButton, releases[0]);
        }


        TEST_METHOD (LosingFocusBehindLiveReleasesEveryHeldKey)
        {
            HeldHostInputs       held;
            std::vector<WPARAM>  releases;



            held.OnPress      ('A', true);
            held.OnPress      (VK_SHIFT, true);
            held.OnReleaseAll (false);

            releases = held.TakeReleases();

            Assert::AreEqual ((size_t) 2, releases.size());
        }


        TEST_METHOD (LosingFocusWhileLiveLeavesNothingToRelease)
        {
            HeldHostInputs  held;



            held.OnPress      ('A', true);
            held.OnReleaseAll (true);
            held.OnRelease    ('A', false);

            Assert::IsTrue (held.TakeReleases().empty());
        }
    };
}
