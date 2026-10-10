#include "Pch.h"

#include "Ui/DiskInspector/FollowHead.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FollowHeadTests
//
//  "Follow head" (FR-067): the head's track is selected once the head has
//  stayed on it for 0.3 s and never while it only passes; turning it on
//  selects at once; a hand selection turns it off.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FollowHeadTests)
{
public:

    TEST_METHOD (TurningItOnSelectsTheHeadsTrack)
    {
        FollowHead  follow;



        Assert::AreEqual (12, follow.SetOn (true, 12, 1000));
        Assert::IsTrue   (follow.IsOn());
        Assert::AreEqual (-1, follow.Tick (12, 2000), L"already selected");
        Assert::AreEqual (-1, follow.SetOn (false, 12, 2000));
    }



    TEST_METHOD (ATrackIsSelectedAfterTheHeadSettles)
    {
        FollowHead  follow;



        (void) follow.SetOn (true, 0, 0);

        Assert::AreEqual (-1, follow.Tick (8,  100), L"just arrived");
        Assert::AreEqual (-1, follow.Tick (8,  399), L"not yet 0.3 s");
        Assert::AreEqual (8,  follow.Tick (8,  400));
        Assert::AreEqual (-1, follow.Tick (8,  900), L"selected once");
    }



    TEST_METHOD (ATrackTheHeadPassesOverIsNeverSelected)
    {
        FollowHead  follow;



        (void) follow.SetOn (true, 0, 0);

        for (int k = 1; k <= 10; k++)
        {
            Assert::AreEqual (-1, follow.Tick (k * 2, k * 100));
        }

        Assert::AreEqual (20, follow.Tick (20, 1300));
    }



    TEST_METHOD (AHandSelectionTurnsItOff)
    {
        FollowHead  follow;



        (void) follow.SetOn (true, 0, 0);
        follow.OnHandSelection();

        Assert::IsFalse  (follow.IsOn());
        Assert::AreEqual (-1, follow.Tick (8, 1000));
    }
};
