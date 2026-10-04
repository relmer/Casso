#pragma once

#include "Pch.h"
#include "Debugger/Reverse/HistoryStatus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText
//
//  What the debugger window's status bar shows, worked out from the history
//  status the CPU thread published and the window's own text zoom: the zoom
//  as a percentage, how full the history budget is and the color its meter
//  takes, the emulated time history begins at, and a note while a replay is
//  running to reach a point in history.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerStatusText
{
public:
    //  The part of the budget left at which the meter is the warning color,
    //  and at which it is the error color.
    static constexpr float   kWarningLeft     = 0.2f;
    static constexpr float   kErrorLeft       = 0.1f;
    static constexpr double  kCyclesPerSecond = 1020484.0;
    static constexpr float   kPercent         = 100.0f;

    static std::wstring  GetZoomText     (float zoom);

    //  The fraction of the budget history holds, or -1 while not recording.
    static float         GetBudgetFill   (const HistoryStatus & status);

    //  The meter's color for a fill: the healthy color while empty, blending
    //  to the warning color at kWarningLeft left, then to the error color at
    //  kErrorLeft left, and the error color past that.
    static uint32_t      GetBudgetColor  (float fill, uint32_t healthy, uint32_t warning, uint32_t error);

    static std::wstring  GetBudgetText   (const HistoryStatus & status);
    static std::wstring  GetBeginText    (const HistoryStatus & status);
    static std::wstring  GetReplayText   (bool isReplaying);

    //  Emulated time for a cycle count: seconds to a tenth under a minute,
    //  then minutes and seconds, then hours, minutes and seconds.
    static std::wstring  FormatTime      (uint64_t cycles);
};
