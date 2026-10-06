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
//  UpdateSchedule
//
//  When to check for a release and when to show the title-bar indicator.
//  Pure decisions over the persisted update state; times are Unix seconds.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateSchedule
{
public:
    static constexpr std::int64_t  kSecondsPerHour       = 3600;
    static constexpr std::int64_t  kHoursBetweenChecks   = 24;
    static constexpr std::int64_t  kCheckIntervalSeconds = kHoursBetweenChecks * kSecondsPerHour;

    static bool  IsCheckDue          (UpdateCheckTrigger  trigger,
                                      bool                autoUpdateCheck,
                                      std::int64_t        lastCheckUtc,
                                      std::int64_t        nowUtc);

    static bool  ShouldShowIndicator (const ReleaseVersion & running,
                                      std::string_view       latestKnownVersion,
                                      std::string_view       skippedVersion);

    static bool  ShouldOfferRelease  (UpdateCheckTrigger     trigger,
                                      const ReleaseVersion & running,
                                      const ReleaseVersion & latest,
                                      std::string_view       skippedVersion);
};
