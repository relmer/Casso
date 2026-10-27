#pragma once

#include "Pch.h"

#include "Debugger/Reverse/HistoryStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptions
//
//  The history settings an options page edits: whether history recording
//  is on, the memory budget for its snapshots, and how many video frames
//  apart they are taken.
//
////////////////////////////////////////////////////////////////////////////////

struct ReverseOptions
{
    static constexpr int  kDefaultBudgetMb       = 64;
    static constexpr int  kDefaultIntervalFrames = 10;

    bool  isRecording    = true;
    int   budgetMb       = kDefaultBudgetMb;
    int   intervalFrames = kDefaultIntervalFrames;
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
//  available, a saved budget placed on it, and the time of history a budget
//  has room for, at the recording's own rate once it has measured one.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseBudget
{
public:
    static constexpr int     kTickCount            = 10;
    static constexpr double  kMaxHistoryHours      = 48.0;

    //  Bytes of history a second for a //e at the default interval, measured
    //  by HistoryRateTests: a machine-language loop rewriting the whole
    //  hi-res page, the busiest, and the DOS prompt waiting for a key, the
    //  idlest.
    static constexpr double  kBusyBytesPerSecond   = 69540.0;
    static constexpr double  kIdleBytesPerSecond   = 694.0;

    //  A recording's own rate is used once it covers this much time.
    static constexpr double  kLeastMeasuredSeconds = 30.0;

    //  1 MB to the lowest of 90% of availableBytes, availableBytes less 4 MB,
    //  and the memory a busy program fills in kMaxHistoryHours, never below
    //  1 MB; a page is a tenth of the range, at least 1 MB.
    static ReverseBudgetRange  GetRange (uint64_t availableBytes);

    //  Where a saved budget sits on the slider: at the maximum when above it.
    static int  PlaceOnRange (int budgetMb, const ReverseBudgetRange & range);

    //  The physical memory available now.
    static uint64_t  GetAvailableBytes ();

    //  Minutes of history a budget has room for at a rate, with share the
    //  part of the budget left to the snapshots.
    static double  GetMinutes (int budgetMb, double bytesPerSecond, double share);

    //  The recording's own rate, and the part of its budget left to the
    //  snapshots; false until it covers kLeastMeasuredSeconds.
    static bool  TryGetMeasuredRate (const HistoryStatus & status, double & outBytesPerSecond, double & outShare);

    //  At the recording's rate when it has one, otherwise from the busiest
    //  to the idlest measured rate.
    static std::wstring  GetEstimateText (int budgetMb, const HistoryStatus & status);

    //  The budget a busy program fills in a number of hours.
    static int  GetBudgetForHours (double hours);

private:
    static constexpr uint64_t  kBytesPerMb       = 1024 * 1024;
    static constexpr uint64_t  kReserveMb        = 4;
    static constexpr uint64_t  kPercentUsable    = 90;
    static constexpr uint64_t  kPercent          = 100;


    //  Below two hours a time is given in minutes.
    static constexpr double  kSecondsPerMinute    = 60.0;
    static constexpr double  kMinutesPerHour      = 60.0;
    static constexpr double  kMinutesShownUnder   = 120.0;

    static std::wstring  FormatDuration (double minutes);
};
