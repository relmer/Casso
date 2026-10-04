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
//  The position only while the machine is stopped. While it runs, the view
//  is built at the end of each frame's run of cycles, which overshoots the
//  frame by a cycle or two, so a position read then creeps along by about a
//  scanline a second and says nothing about where the beam is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetBeamText (const std::optional<DebuggerViewSnapshot::BeamState> & beam, bool isPaused)
{
    if (!beam.has_value())
    {
        return {};
    }

    if (!isPaused)
    {
        return L"Scanline:cycle running";
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
//  The host's local time of day the oldest snapshot was taken at, then the
//  emulated time in parentheses: "Begins at 10:42:07 PM (0.0 s)". Without a
//  host time, the emulated time alone.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetBeginText (
    const HistoryStatus  & status,
    LPCWSTR                locale)
{
    HRESULT         hr        = S_OK;
    std::wstring    emulated;
    std::wstring    clock;
    ULARGE_INTEGER  wall      = {};
    FILETIME        utc       = {};
    SYSTEMTIME      utcTime   = {};
    SYSTEMTIME      localTime = {};
    BOOL            converted = FALSE;
    bool            hasClock  = false;



    CBR (status.isRecording && status.hasHistory);

    emulated = FormatTime (status.beginCycle, locale);
    CBR (status.beginWallTime != 0);

    wall.QuadPart       = status.beginWallTime;
    utc.dwLowDateTime   = wall.LowPart;
    utc.dwHighDateTime  = wall.HighPart;

    converted = FileTimeToSystemTime (&utc, &utcTime);
    CWR (converted);

    converted = SystemTimeToTzSpecificLocalTime (nullptr, &utcTime, &localTime);
    CWR (converted);

    clock    = FormatClock (localTime, locale);
    hasClock = !clock.empty();
    CBR (hasClock);

Error:
    if (emulated.empty())
    {
        return {};
    }

    if (FAILED (hr))
    {
        return L"Begins at " + emulated;
    }

    return std::format (L"Begins at {} ({})", clock, emulated);
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
//  DebuggerStatusText::GetReplayText
//
//  Once a replay has run long enough to notice, the note adds how much of
//  history it has covered and how to stop it, for a command that reports
//  its progress; a command that reports none cannot be stopped either.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetReplayText (const ReplayProgress & progress)
{
    std::wstring  text      = GetReplayText (progress.isReplaying);
    bool          isLong    = progress.elapsedMs >= kProgressDelayMs;
    bool          hasReport = progress.fraction >= 0.0f;



    if (!progress.isReplaying || !isLong || !hasReport)
    {
        return text;
    }

    return std::format (L"{} {}%. Press Escape or Pause to stop.", text, std::lround (std::min (progress.fraction, 1.0f) * kPercent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::FormatTime
//
//  Tenths of a second, rounded down, so a time never reads later than the
//  machine has run.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::FormatTime (
    uint64_t  cycles,
    LPCWSTR   locale)
{
    constexpr uint64_t  kTenthsPerSecond = 10;
    constexpr uint64_t  kSecondsPerMin   = 60;
    constexpr uint64_t  kSecondsPerHour  = 3600;
    uint64_t            tenths           = (uint64_t) ((double) cycles * (double) kTenthsPerSecond / kCyclesPerSecond);
    uint64_t            seconds          = tenths / kTenthsPerSecond;
    uint64_t            tenth            = tenths % kTenthsPerSecond;



    if (seconds < kSecondsPerMin)
    {
        return FormatTenths (seconds, tenth, locale) + L" s";
    }

    if (seconds < kSecondsPerHour)
    {
        return std::format (L"{}:{}{}", seconds / kSecondsPerMin, (seconds % kSecondsPerMin < 10) ? L"0" : L"", FormatTenths (seconds % kSecondsPerMin, tenth, locale));
    }

    return std::format (L"{}:{:02}:{:02}", seconds / kSecondsPerHour, (seconds % kSecondsPerHour) / kSecondsPerMin, seconds % kSecondsPerMin);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::FormatClock
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::FormatClock (
    const SYSTEMTIME  & localTime,
    LPCWSTR             locale)
{
    HRESULT  hr                       = S_OK;
    WCHAR    text[kMaxFormattedChars] = {};
    int      length                   = 0;



    // No flags and no picture: the locale's own long time format, which
    // holds the seconds, with its clock, markers, separators and zeros.
    length = GetTimeFormatEx (locale, 0, &localTime, nullptr, text, ARRAYSIZE (text));
    CWR (length > 0);

Error:
    return SUCCEEDED (hr) ? std::wstring (text) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::FormatTenths
//
//  A whole number and one decimal digit, with the locale's decimal
//  separator and no digit grouping, or a period when the locale gives none.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::FormatTenths (
    uint64_t  whole,
    uint64_t  tenth,
    LPCWSTR   locale)
{
    HRESULT       hr                       = S_OK;
    std::wstring  plain                    = std::format (L"{}.{}", whole, tenth);
    WCHAR         separator[kMaxSeparator] = {};
    WCHAR         text[kMaxFormattedChars] = {};
    NUMBERFMTW    format                   = {};
    int           length                   = 0;



    length = GetLocaleInfoEx (locale, LOCALE_SDECIMAL, separator, ARRAYSIZE (separator));
    CWR (length > 0);

    format.NumDigits     = 1;
    format.LeadingZero   = 1;
    format.Grouping      = 0;
    format.lpDecimalSep  = separator;
    format.lpThousandSep = const_cast<LPWSTR> (L"");
    format.NegativeOrder = 1;

    length = GetNumberFormatEx (locale, 0, plain.c_str(), &format, text, ARRAYSIZE (text));
    CWR (length > 0);

Error:
    return SUCCEEDED (hr) ? std::wstring (text) : plain;
}