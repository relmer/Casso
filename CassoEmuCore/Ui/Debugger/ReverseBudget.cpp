#include "Pch.h"

#include "Ui/Debugger/ReverseBudget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::GetRange
//
////////////////////////////////////////////////////////////////////////////////

ReverseBudgetRange ReverseBudget::GetRange (uint64_t availableBytes)
{
    ReverseBudgetRange  range;
    uint64_t            availableMb = availableBytes / kBytesPerMb;
    uint64_t            usableMb    = availableMb * kPercentUsable / kPercent;
    uint64_t            reservedMb  = (availableMb > kReserveMb) ? availableMb - kReserveMb : 0;
    uint64_t            hoursMb     = (uint64_t) GetBudgetForHours (kMaxHistoryHours);
    uint64_t            maxMb       = std::max<uint64_t> (std::min ({ usableMb, reservedMb, hoursMb }), (uint64_t) range.minMb);



    range.maxMb  = (int) std::min<uint64_t> (maxMb, (uint64_t) INT_MAX);
    range.pageMb = std::max ((range.maxMb - range.minMb) / kTickCount, 1);

    return range;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::PlaceOnRange
//
////////////////////////////////////////////////////////////////////////////////

int ReverseBudget::PlaceOnRange (int budgetMb, const ReverseBudgetRange & range)
{
    return std::clamp (budgetMb, range.minMb, range.maxMb);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::GetAvailableBytes
//
//  Zero when GlobalMemoryStatusEx fails; GetRange treats zero as 1 MB.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ReverseBudget::GetAvailableBytes()
{
    MEMORYSTATUSEX  status = { sizeof (status) };



    return GlobalMemoryStatusEx (&status) ? status.ullAvailPhys : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::GetMinutes
//
////////////////////////////////////////////////////////////////////////////////

double ReverseBudget::GetMinutes (int budgetMb, double bytesPerSecond, double share)
{
    return (double) budgetMb * (double) kBytesPerMb * share / std::max (bytesPerSecond, 1.0) / kSecondsPerMinute;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::TryGetMeasuredRate
//
//  The snapshots' size over the emulated time from the oldest to the newest.
//  The budget also pays for the store's table and work state, so only the
//  room left to the snapshots fills at that rate.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseBudget::TryGetMeasuredRate (const HistoryStatus & status, double & outBytesPerSecond, double & outShare)
{
    uint64_t  endCycle = status.cycle + status.cyclesBehind;
    double    seconds  = (endCycle > status.beginCycle) ? (double) (endCycle - status.beginCycle) / HistoryStatus::kCyclesPerSecond : 0.0;



    if (!status.isRecording || !status.hasHistory || seconds < kLeastMeasuredSeconds ||
        status.storedBytes == 0 || status.budgetBytes == 0)
    {
        return false;
    }

    outBytesPerSecond = (double) status.storedBytes / seconds;
    outShare          = std::clamp ((double) status.arenaBytes / (double) status.budgetBytes, 0.0, 1.0);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::GetEstimateText
//

////////////////////////////////////////////////////////////////////////////////

std::wstring ReverseBudget::GetEstimateText (int budgetMb, const HistoryStatus & status)
{
    double  rate  = 0.0;
    double  share = 1.0;
    double  low   = GetMinutes (budgetMb, kBusyBytesPerSecond, share);
    double  high  = GetMinutes (budgetMb, kIdleBytesPerSecond, share);



    if (TryGetMeasuredRate (status, rate, share))
    {
        return std::format (L"At this program's rate so far, room for about {} of history.", FormatDuration (GetMinutes (budgetMb, rate, share)));
    }

    return std::format (L"Room for about {} of history for a program that redraws the whole screen, up to {} for one waiting for a key.",
                        FormatDuration (low), FormatDuration (high));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::FormatDuration
//
//  Whole minutes below two hours, never fewer than one, and whole hours
//  from there on.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ReverseBudget::FormatDuration (double minutes)
{
    bool    isHours = minutes >= kMinutesShownUnder;
    double  value   = std::max (std::round (isHours ? minutes / kMinutesPerHour : minutes), 1.0);



    if (isHours)
    {
        return std::format (L"{:.0f} hours", value);
    }

    return std::format (L"{:.0f} {}", value, (value == 1.0) ? L"minute" : L"minutes");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::GetBudgetForHours
//
//  Rounded up, so the budget has room for the whole time.
//
////////////////////////////////////////////////////////////////////////////////

int ReverseBudget::GetBudgetForHours (double hours)
{
    return (int) std::ceil (hours * kMinutesPerHour * kSecondsPerMinute * kBusyBytesPerSecond / (double) kBytesPerMb);
}





