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
//  DebuggerStatusText::GetHistoryFill
//
////////////////////////////////////////////////////////////////////////////////

float DebuggerStatusText::GetHistoryFill (const HistoryStatus & status)
{
    if (!status.isRecording || status.isFull || status.budgetBytes == 0)
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
//  DebuggerStatusText::GetHistoryText
//
//  While filling, the host's local time of day the oldest snapshot was taken
//  at and how much of the budget is left. Once full, that time, then the
//  emulated time since power-on and the cycle count there, in parentheses.
//  Without a host time, each form leaves the time out.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetHistoryText (
    const HistoryStatus  & status,
    LPCWSTR                locale)
{
    float         fill  = GetHistoryFill (status);
    std::wstring  clock;
    std::wstring  emulated;
    std::wstring  cycle;
    long          left  = 0;



    if (!status.isRecording)
    {
        return L"History off";
    }

    clock = GetBeginClock (status, locale);

    if (fill >= 0.0f)
    {
        left = std::lround (kPercent) - std::lround (fill * kPercent);

        if (clock.empty())
        {
            return std::format (L"History buffer remaining {}%", left);
        }

        return std::format (L"History since {}, buffer remaining {}%", clock, left);
    }

    emulated = FormatTime (status.beginCycle, locale);
    cycle    = FormatCount (status.beginCycle, locale);

    if (clock.empty())
    {
        return std::format (L"History begins at Power + {} (cycle {})", emulated, cycle);
    }

    return std::format (L"History begins at {} (Power + {}, cycle {})", clock, emulated, cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetBeginClock
//
//  The host's local time of day the oldest snapshot was taken at, or empty
//  without one.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetBeginClock (
    const HistoryStatus  & status,
    LPCWSTR                locale)
{
    return FormatWallClock (status.beginWallTime, locale);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetHistoryFitTexts
//
//  Every hour of the day at 58:58, so the locale's widest clock is among
//  them whatever its markers and digits; with each, the full form at a
//  long emulated time and a thirteen-digit cycle count, and the filling form
//  at 100%; then "History off".
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DebuggerStatusText::GetHistoryFitTexts (LPCWSTR locale)
{
    constexpr WORD             kHoursPerDay = 24;
    constexpr WORD             kLongMinute  = 58;
    constexpr WORD             kAnyYear     = 2026;
    constexpr uint64_t         kLongSeconds = 359999;              // 99:59:59
    constexpr uint64_t         kLongCycle   = 9999999999999ull;
    std::vector<std::wstring>  texts;
    SYSTEMTIME                 time         = {};
    std::wstring               emulated     = FormatTime ((uint64_t) ((double) kLongSeconds * kCyclesPerSecond) + 1, locale);
    std::wstring               cycle        = FormatCount (kLongCycle, locale);
    std::wstring               clock;



    time.wYear   = kAnyYear;
    time.wMonth  = 1;
    time.wDay    = 1;
    time.wMinute = kLongMinute;
    time.wSecond = kLongMinute;

    for (WORD hour = 0; hour < kHoursPerDay; hour++)
    {
            time.wHour = hour;
        clock      = FormatClock (time, locale);

        texts.push_back (std::format (L"History begins at {} (Power + {}, cycle {})", clock, emulated, cycle));
        texts.push_back (std::format (L"History since {}, buffer remaining 100%", clock));
    }

    texts.push_back (L"History off");
    return texts;
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
//  DebuggerStatusText::FormatWallClock
//
//  The status bar asks for the same time every frame, and looking up the
//  time zone is most of the cost, so the last time converted on each thread
//  is kept and converted again only when the time asked for changes.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::FormatWallClock (
    uint64_t  wallTime,
    LPCWSTR   locale)
{
    thread_local uint64_t    s_lastWallTime  = 0;
    thread_local SYSTEMTIME  s_lastLocalTime = {};
    HRESULT                  hr              = S_OK;
    ULARGE_INTEGER           wall            = {};
    FILETIME                 utc             = {};
    SYSTEMTIME               utcTime         = {};
    SYSTEMTIME               localTime       = {};
    BOOL                     converted       = FALSE;
    std::wstring             clock;



    CBR (wallTime != 0);

    if (wallTime == s_lastWallTime)
    {
        localTime = s_lastLocalTime;
    }
    else
    {
        wall.QuadPart      = wallTime;
        utc.dwLowDateTime  = wall.LowPart;
        utc.dwHighDateTime = wall.HighPart;

        converted = FileTimeToSystemTime (&utc, &utcTime);
        CWR (converted);

        converted = SystemTimeToTzSpecificLocalTime (nullptr, &utcTime, &localTime);
        CWR (converted);

        s_lastWallTime  = wallTime;
        s_lastLocalTime = localTime;
    }

    clock = FormatClock (localTime, locale);

Error:
    return SUCCEEDED (hr) ? clock : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetPowerText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetPowerText (
    uint64_t  cycle,
    LPCWSTR   locale)
{
    return std::format (L"(Power + {})", FormatTime (cycle, locale));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::GetReplayCaption
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::GetReplayCaption (
    uint64_t  wallTime,
    uint64_t  cycle,
    LPCWSTR   locale)
{
    std::wstring  clock = FormatWallClock (wallTime, locale);



    if (clock.empty())
    {
        return std::format (L" [Replaying @ Power + {}]", FormatTime (cycle, locale));
    }

    return std::format (L" [Replaying @ {}]", clock);
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





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerStatusText::FormatCount
//
//  The locale's separator and grouping with no decimals, or the plain digits
//  when the locale gives none. A grouping such as "3;2;0" repeats its last
//  group and becomes 32; one without the closing ";0" stops grouping after
//  its last group, which NUMBERFMT takes as a trailing 0 digit.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerStatusText::FormatCount (
    uint64_t  count,
    LPCWSTR   locale)
{
    HRESULT       hr                        = S_OK;
    std::wstring  plain                     = std::to_wstring (count);
    WCHAR         separator[kMaxSeparator]  = {};
    WCHAR         decimal[kMaxSeparator]    = {};
    WCHAR         grouping[kMaxSeparator]   = {};
    WCHAR         text[kMaxFormattedChars]  = {};
    NUMBERFMTW    format                    = {};
    UINT          groups                    = 0;
    bool          stops                     = true;
    int           length                    = 0;



    length = GetLocaleInfoEx (locale, LOCALE_STHOUSAND, separator, ARRAYSIZE (separator));
    CWR (length > 0);

    length = GetLocaleInfoEx (locale, LOCALE_SDECIMAL, decimal, ARRAYSIZE (decimal));
    CWR (length > 0);

    length = GetLocaleInfoEx (locale, LOCALE_SGROUPING, grouping, ARRAYSIZE (grouping));
    CWR (length > 0);

    for (const WCHAR * ch = grouping; *ch != L'\0'; ch++)
    {
        if (*ch >= L'1' && *ch <= L'9')
        {
            groups  = groups * 10 + (UINT) (*ch - L'0');
            stops   = true;
        }
        else if (*ch == L'0')
        {
            stops   = false;
        }
    }

    format.NumDigits     = 0;
    format.LeadingZero   = 0;
    format.Grouping      = stops ? groups * 10 : groups;
    format.lpDecimalSep  = decimal;
    format.lpThousandSep = separator;
    format.NegativeOrder = 1;

    length = GetNumberFormatEx (locale, 0, plain.c_str(), &format, text, ARRAYSIZE (text));
    CWR (length > 0);

Error:
    return SUCCEEDED (hr) ? std::wstring (text) : plain;
}
