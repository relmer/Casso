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
//  ReverseBudget::EstimateMinutes
//
//  History grows with the snapshots taken, so a longer interval stretches
//  the same memory over proportionally more time. That is roughly true: a
//  snapshot further from the one before differs from it in more bytes.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseBudget::EstimateMinutes (
    int       budgetMb,
    int       intervalFrames,
    double  & outLow,
    double  & outHigh)
{
    double  scale = (double) budgetMb / kMeasuredBudgetMb * (double) intervalFrames / (double) ReverseOptions::kDefaultIntervalFrames;



    outLow  = kBusyMinutesPer64Mb  * scale;
    outHigh = kQuietMinutesPer64Mb * scale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::GetEstimateText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ReverseBudget::GetEstimateText (int budgetMb)
{
    double  low  = 0.0;
    double  high = 0.0;



    EstimateMinutes (budgetMb, ReverseOptions::kDefaultIntervalFrames, low, high);

    //  The unit follows each figure only when the two differ: 24 to 44
    //  minutes, but 95 minutes to 3 hours.
    return std::format (L"Room for about {} to {} of history; a busy program fills it sooner.",
                        FormatDuration (low, (low < kMinutesShownUnder) != (high < kMinutesShownUnder)),
                        FormatDuration (high, true));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget::FormatDuration
//
//  Whole minutes below two hours, whole hours from there on.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ReverseBudget::FormatDuration (double minutes, bool withUnit)
{
    bool    isHours = minutes >= kMinutesShownUnder;
    double  value   = isHours ? minutes / kMinutesPerHour : minutes;



    return std::format (L"{:.0f}{}", value, !withUnit ? L"" : isHours ? L" hours" : L" minutes");
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
    return (int) std::ceil (hours * kMinutesPerHour / kBusyMinutesPer64Mb * kMeasuredBudgetMb);
}





