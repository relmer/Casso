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
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerStatusText::GetBudgetColor (float fill)
{
    return DxuiColor::Lerp (kEmptyArgb, kFullArgb, std::clamp (fill, 0.0f, 1.0f));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetBeamText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetBeamText (const std::optional<DebuggerViewSnapshot::BeamState> & beam)
{
    if (!beam.has_value())
    {
        return {};
    }

    return std::format (L"Scanline:cycle {}:{} (${:03X}:{:02X})", beam->scanline, beam->cycle, beam->scanline, beam->cycle);
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
