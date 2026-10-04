#pragma once

#include "Pch.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Ui/Debugger/DebuggerViewState.h"





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
    //  The meter's colors while empty and while full. They are fixed rather
    //  than the theme's: a full budget is not a fault, since history simply
    //  reuses its oldest snapshots, so the meter never turns a warning color.
    static constexpr uint32_t  kEmptyArgb       = 0xFF3FB950u;
    static constexpr uint32_t  kFullArgb        = 0xFF388BFDu;
    static constexpr double    kCyclesPerSecond = 1020484.0;
    static constexpr float     kPercent         = 100.0f;

    static std::wstring  GetZoomText     (float zoom);

    //  Where the beam is: "Scanline:cycle 192:1 ($0C0:01)", the place in
    //  decimal and then in hex as VIDEOINFO gives it, or empty with no beam.
    static std::wstring  GetBeamText     (const std::optional<DebuggerViewSnapshot::BeamState> & beam);

    //  The fraction of the budget history holds, or -1 while not recording.
    static float         GetBudgetFill   (const HistoryStatus & status);

    //  The meter's color for a fill: kEmptyArgb while empty, blending evenly
    //  to kFullArgb when full.
    static uint32_t      GetBudgetColor  (float fill);

    static std::wstring  GetBudgetText   (const HistoryStatus & status);
    static std::wstring  GetBeginText    (const HistoryStatus & status);
    static std::wstring  GetReplayText   (bool isReplaying);

    //  Emulated time for a cycle count: seconds to a tenth under a minute,
    //  then minutes and seconds, then hours, minutes and seconds.
    static std::wstring  FormatTime      (uint64_t cycles);
};
