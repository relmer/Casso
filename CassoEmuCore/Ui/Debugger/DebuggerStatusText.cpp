#include "Pch.h"

#include "Ui/Debugger/DebuggerStatusText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetZoomText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetZoomText (float zoom)
{
    return std::format (L"{}%", std::lround (zoom * kPercent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetBudgetFill
//
////////////////////////////////////////////////////////////////////////////////

float DebuggerStatusText::GetBudgetFill (const HistoryStatus & status)
{
    if (!status.isRecording || status.budgetBytes == 0)
    {
        return -1.0f;
    }

    return std::clamp ((float) ((double) status.usedBytes / (double) status.budgetBytes), 0.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetBudgetColor
//
//  Two blends end to end, so the color never jumps: healthy to warning over
//  the budget from empty to kWarningLeft left, then warning to error over
//  the next stretch to kErrorLeft left.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerStatusText::GetBudgetColor (
    float     fill,
    uint32_t  healthy,
    uint32_t  warning,
    uint32_t  error)
{
    float  used        = std::clamp (fill, 0.0f, 1.0f);
    float  warningFill = 1.0f - kWarningLeft;
    float  errorFill   = 1.0f - kErrorLeft;



    if (used <= warningFill)
    {
        return DxuiColor::Lerp (healthy, warning, used / warningFill);
    }

    if (used < errorFill)
    {
        return DxuiColor::Lerp (warning, error, (used - warningFill) / (errorFill - warningFill));
    }

    return error;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetBudgetText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetBudgetText (const HistoryStatus & status)
{
    float  fill = GetBudgetFill (status);



    if (fill < 0.0f)
    {
        return L"History off";
    }

    return std::format (L"History {}% full", std::lround (fill * kPercent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetBeginText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetBeginText (const HistoryStatus & status)
{
    if (!status.isRecording || !status.hasHistory)
    {
        return {};
    }

    return L"Begins at " + FormatTime (status.beginCycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetReplayText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetReplayText (bool isReplaying)
{
    return isReplaying ? std::wstring (L"Replaying history") + s_kchEllipsis : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::FormatTime
//
//  Tenths of a second, rounded down, so a time never reads later than the
//  machine has run.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::FormatTime (uint64_t cycles)
{
    constexpr uint64_t  kTenthsPerSecond = 10;
    constexpr uint64_t  kSecondsPerMin   = 60;
    constexpr uint64_t  kSecondsPerHour  = 3600;
    uint64_t            tenths           = (uint64_t) ((double) cycles * (double) kTenthsPerSecond / kCyclesPerSecond);
    uint64_t            seconds          = tenths / kTenthsPerSecond;
    uint64_t            tenth            = tenths % kTenthsPerSecond;



    if (seconds < kSecondsPerMin)
    {
        return std::format (L"{}.{} s", seconds, tenth);
    }

    if (seconds < kSecondsPerHour)
    {
        return std::format (L"{}:{:02}.{}", seconds / kSecondsPerMin, seconds % kSecondsPerMin, tenth);
    }

    return std::format (L"{}:{:02}:{:02}", seconds / kSecondsPerHour, (seconds % kSecondsPerHour) / kSecondsPerMin, seconds % kSecondsPerMin);
}
