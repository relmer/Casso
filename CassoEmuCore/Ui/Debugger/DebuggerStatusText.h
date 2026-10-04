#pragma once

#include "Pch.h"
#include "Debugger/Reverse/HistoryStatus.h"
#include "Debugger/Reverse/ReplayControl.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText
//
//  What the debugger window's status bar shows, worked out from the history
//  status the CPU thread published and the window's own text zoom: the zoom
//  as a percentage, how full the history budget is and the color its meter
//  takes, the host's time and the emulated time history begins at, and a
//  note while a replay is running to reach a point in history.
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

    //  How long a replay runs before the note adds its progress.
    static constexpr uint64_t  kProgressDelayMs = 200;

    static std::wstring  GetZoomText     (float zoom);

    //  Where the beam is: "Scanline:cycle 192:1 ($0C0:01)", the place in
    //  decimal and then in hex as VIDEOINFO gives it, or empty with no beam.
    static std::wstring  GetBeamText     (const std::optional<DebuggerViewSnapshot::BeamState> & beam, bool isPaused);

    //  The fraction of the budget history holds, or -1 while not recording.
    static float         GetBudgetFill   (const HistoryStatus & status);

    //  The meter's color for a fill: kEmptyArgb while empty, blending evenly
    //  to kFullArgb when full.
    static uint32_t      GetBudgetColor  (float fill);

    static std::wstring  GetBudgetText   (const HistoryStatus & status);
    static std::wstring  GetBeginText    (const HistoryStatus & status) { return GetBeginText (status, LOCALE_NAME_USER_DEFAULT); }
    static std::wstring  GetBeginText    (const HistoryStatus & status, LPCWSTR locale);
    static std::wstring  GetReplayText   (bool isReplaying);
    static std::wstring  GetReplayText   (const ReplayProgress & progress);

    //  Emulated time for a cycle count: seconds to a tenth under a minute,
    //  then minutes and seconds, then hours, minutes and seconds, with the
    //  locale's decimal separator.
    static std::wstring  FormatTime      (uint64_t cycles) { return FormatTime (cycles, LOCALE_NAME_USER_DEFAULT); }
    static std::wstring  FormatTime      (uint64_t cycles, LPCWSTR locale);

    //  A local time of day in the locale's own time format, with seconds.
    static std::wstring  FormatClock     (const SYSTEMTIME & localTime, LPCWSTR locale);

private:
    static constexpr size_t  kMaxFormattedChars = 80;
    static constexpr size_t  kMaxSeparator      = 8;

    static std::wstring  FormatTenths    (uint64_t whole, uint64_t tenth, LPCWSTR locale);
};
