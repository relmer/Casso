#include "Pch.h"

#include "Update/UpdateSchedule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateSchedule::IsCheckDue
//
//  A manual check always runs. The automatic one runs only when it is turned
//  on and either it has never run, a full interval has passed, or the clock
//  now reads earlier than the last check (a backward clock change would
//  otherwise suppress checks until it caught up).
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateSchedule::IsCheckDue (
    UpdateCheckTrigger  trigger,
    bool                autoUpdateCheck,
    std::int64_t        lastCheckUtc,
    std::int64_t        nowUtc)
{
    bool  isDue = false;



    if (trigger == UpdateCheckTrigger::Manual)
    {
        isDue = true;
    }
    else if (autoUpdateCheck)
    {
        isDue = lastCheckUtc == 0                               ||
                nowUtc < lastCheckUtc                           ||
                nowUtc - lastCheckUtc >= kCheckIntervalSeconds;
    }

    return isDue;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateSchedule::ShouldShowIndicator
//
//  The indicator shows when the newest release found is newer than the
//  running build and is not the one the user skipped. An empty or
//  unparsable latest version shows nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateSchedule::ShouldShowIndicator (
    const ReleaseVersion & running,
    std::string_view       latestKnownVersion,
    std::string_view       skippedVersion)
{
    ReleaseVersion  latest;
    bool            isKnown = false;



    isKnown = ReleaseVersion::TryParse (latestKnownVersion, latest);

    return isKnown && ShouldOfferRelease (UpdateCheckTrigger::Automatic, running, latest, skippedVersion);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateSchedule::ShouldOfferRelease
//
//  Whether a check's result is news. Only a newer release ever is. The
//  automatic check also stays quiet about the skipped release; a manual
//  check offers it anyway, since the user asked.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateSchedule::ShouldOfferRelease (
    UpdateCheckTrigger     trigger,
    const ReleaseVersion & running,
    const ReleaseVersion & latest,
    std::string_view       skippedVersion)
{
    ReleaseVersion  skipped;
    bool            isNewer     = latest > running;
    bool            isSkipped   = false;



    if (trigger == UpdateCheckTrigger::Automatic && ReleaseVersion::TryParse (skippedVersion, skipped))
    {
        isSkipped = skipped == latest;
    }

    return isNewer && !isSkipped;
}
