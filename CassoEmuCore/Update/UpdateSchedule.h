#pragma once

#include "Pch.h"

#include "Update/ReleaseVersion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateCheckTrigger
//
//  What started a check: the once-a-day background check, or the user
//  choosing Help > Check for updates.
//
////////////////////////////////////////////////////////////////////////////////

enum class UpdateCheckTrigger
{
    Automatic,
    Manual,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SharedCheckOutcome
//
//  What an instance that skipped the startup check does with the prefs
//  file's update record: keep waiting for a newer one, or adopt it and
//  hide or show the indicator.
//
////////////////////////////////////////////////////////////////////////////////

enum class SharedCheckOutcome
{
    Wait,
    Hide,
    Show,
};





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateSchedule
//
//  When to check for a release and when to show the title-bar indicator.
//  Pure decisions over the persisted update state; times are Unix seconds.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateSchedule
{
public:
    static constexpr std::int64_t   kSecondsPerHour       = 3600;
    static constexpr std::int64_t   kHoursBetweenChecks   = 24;
    static constexpr std::int64_t   kCheckIntervalSeconds = kHoursBetweenChecks * kSecondsPerHour;

    // How often, and for how long, an instance without the check lock
    // re-reads the prefs file for the check another instance runs.
    static constexpr std::uint32_t  kSharedCheckPollMs    = 5000;
    static constexpr std::uint32_t  kSharedCheckWaitMs    = 120000;
    static constexpr int            kSharedCheckPollLimit = (int) (kSharedCheckWaitMs / kSharedCheckPollMs);

    static bool  IsCheckDue          (UpdateCheckTrigger  trigger,
                                      bool                autoUpdateCheck,
                                      std::int64_t        lastCheckUtc,
                                      std::int64_t        nowUtc,
                                      bool                isLocalFeed = false);

    static bool  ShouldShowIndicator (const ReleaseVersion & running,
                                      std::string_view       latestKnownVersion,
                                      std::string_view       skippedVersion);

    static bool  ShouldOfferRelease  (UpdateCheckTrigger     trigger,
                                      const ReleaseVersion & running,
                                      const ReleaseVersion & latest,
                                      std::string_view       skippedVersion);

    static SharedCheckOutcome  DecideSharedCheck (std::int64_t           launchCheckUtc,
                                                  std::int64_t           storedCheckUtc,
                                                  bool                   autoUpdateCheck,
                                                  const ReleaseVersion & running,
                                                  std::string_view       latestKnownVersion,
                                                  std::string_view       skippedVersion,
                                                  bool                   isFinalPoll);
};
