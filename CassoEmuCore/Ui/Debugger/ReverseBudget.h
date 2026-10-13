#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptions
//
//  Reverse execution's settings: whether history recording is on, and the
//  memory budget for its snapshots. Snapshots are taken
//  kDefaultIntervalFrames video frames apart.
//
////////////////////////////////////////////////////////////////////////////////

struct ReverseOptions
{
    static constexpr int  kDefaultBudgetMb       = 64;
    static constexpr int  kDefaultIntervalFrames = 10;

    bool  isRecording = true;
    int   budgetMb    = kDefaultBudgetMb;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudgetRange
//
//  The History slider's range in MB, and its Page Up and Page Down step.
//
////////////////////////////////////////////////////////////////////////////////

struct ReverseBudgetRange
{
    int  minMb  = 1;
    int  maxMb  = 1;
    int  pageMb = 1;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseBudget
//
//  The arithmetic behind the History slider: its range from the memory
//  available, a saved budget placed on it, and the minutes of history a
//  budget has room for.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseBudget
{
public:
    static constexpr int     kTickCount       = 10;
    static constexpr double  kMaxHistoryHours = 48.0;

    //  1 MB to the lowest of 90% of availableBytes, availableBytes less 4 MB,
    //  and the memory a busy program fills in kMaxHistoryHours, never below
    //  1 MB; a page is a tenth of the range, at least 1 MB.
    static ReverseBudgetRange  GetRange (uint64_t availableBytes);

    //  Where a saved budget sits on the slider: at the maximum when above it.
    static int  PlaceOnRange (int budgetMb, const ReverseBudgetRange & range);

    //  The physical memory available now.
    static uint64_t  GetAvailableBytes ();

    //  Minutes of history a budget has room for at an interval: low for a
    //  busy program, high for a quiet one, from measured recordings.
    static void          EstimateMinutes (int budgetMb, int intervalFrames, double & outLow, double & outHigh);
    static std::wstring  GetEstimateText (int budgetMb);

    //  The budget a busy program fills in a number of hours.
    static int  GetBudgetForHours (double hours);

private:
    static constexpr uint64_t  kBytesPerMb       = 1024 * 1024;
    static constexpr uint64_t  kReserveMb        = 4;
    static constexpr uint64_t  kPercentUsable    = 90;
    static constexpr uint64_t  kPercent          = 100;

    //  Minutes 64 MB filled at the default interval, measured: a game, the
    //  busiest workload, and a program drawing hi-res, the quietest.
    static constexpr double  kBusyMinutesPer64Mb  = 24.1;
    static constexpr double  kQuietMinutesPer64Mb = 43.8;
    static constexpr double  kMeasuredBudgetMb    = 64.0;

    //  Below two hours a time is given in minutes.
    static constexpr double  kMinutesPerHour      = 60.0;
    static constexpr double  kMinutesShownUnder   = 120.0;

    static std::wstring  FormatDuration (double minutes, bool withUnit);
};
