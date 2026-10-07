#include "Pch.h"

#include "Update/UpdateSchedule.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateScheduleTests
//
//  When the automatic check is due, and when the title-bar indicator shows.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UpdateScheduleTests)
{
public:

    static constexpr std::int64_t  kNow   = 1790000000;
    static constexpr std::int64_t  kDay   = UpdateSchedule::kCheckIntervalSeconds;



    TEST_METHOD (Automatic_DueWhenNeverChecked)
    {
        Assert::IsTrue (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, 0, kNow));
    }



    TEST_METHOD (Automatic_DueOnlyAfterFullInterval)
    {
        Assert::IsFalse (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, kNow - kDay + 1, kNow));
        Assert::IsTrue  (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, kNow - kDay,     kNow));
        Assert::IsTrue  (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, kNow - kDay * 9, kNow));
    }



    TEST_METHOD (Automatic_DueWhenClockMovedBackward)
    {
        Assert::IsTrue (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, kNow + 60, kNow));
    }



    //  A local update feed is a test of this build against that feed, so a
    //  check recorded against GitHub (or another feed) an hour ago does not
    //  stand in for it; turned off, it still does not run.
    TEST_METHOD (Automatic_DueAtEveryStartWithALocalFeed)
    {
        Assert::IsFalse (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, 1000, 1000 + 3600));
        Assert::IsTrue  (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, true, 1000, 1000 + 3600, true));
        Assert::IsFalse (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, false, 0, 1000, true));
    }



    TEST_METHOD (Automatic_NeverDueWhenTurnedOff)
    {
        Assert::IsFalse (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, false, 0,            kNow));
        Assert::IsFalse (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Automatic, false, kNow - kDay * 9, kNow));
    }



    TEST_METHOD (Manual_IgnoresSwitchAndThrottle)
    {
        Assert::IsTrue (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Manual, false, kNow - 1, kNow));
        Assert::IsTrue (UpdateSchedule::IsCheckDue (UpdateCheckTrigger::Manual, true,  kNow,     kNow));
    }



    TEST_METHOD (Indicator_ShowsOnlyForNewerUnskipped)
    {
        ReleaseVersion  running { 1, 30, 0 };



        Assert::IsTrue  (UpdateSchedule::ShouldShowIndicator (running, "1.31.0", ""));
        Assert::IsTrue  (UpdateSchedule::ShouldShowIndicator (running, "1.30.1", "1.30.0"));
        Assert::IsFalse (UpdateSchedule::ShouldShowIndicator (running, "1.30.0", ""),       L"same version");
        Assert::IsFalse (UpdateSchedule::ShouldShowIndicator (running, "1.29.5", ""),       L"older version");
        Assert::IsFalse (UpdateSchedule::ShouldShowIndicator (running, "1.31.0", "1.31.0"), L"skipped");
        Assert::IsTrue  (UpdateSchedule::ShouldShowIndicator (running, "1.32.0", "1.31.0"), L"a release after the skipped one");
        Assert::IsFalse (UpdateSchedule::ShouldShowIndicator (running, "",       ""),       L"nothing known yet");
        Assert::IsFalse (UpdateSchedule::ShouldShowIndicator (running, "junk",   ""));
    }



    TEST_METHOD (OfferRelease_ManualIgnoresSkip)
    {
        ReleaseVersion  running { 1, 30, 0 };
        ReleaseVersion  latest  { 1, 31, 0 };



        Assert::IsFalse (UpdateSchedule::ShouldOfferRelease (UpdateCheckTrigger::Automatic, running, latest, "1.31.0"));
        Assert::IsTrue  (UpdateSchedule::ShouldOfferRelease (UpdateCheckTrigger::Manual,    running, latest, "1.31.0"));
        Assert::IsFalse (UpdateSchedule::ShouldOfferRelease (UpdateCheckTrigger::Manual,    latest,  running, ""),
                         L"a manual check still offers only a newer release");
    }
};
