#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogText.h"
#include "Ui/Debugger/DebuggerStatusText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCycle
//
//  With the locale's thousands separators.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::FormatCycle (uint64_t cycle)
{
    return DebuggerStatusText::FormatCount (cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatTime
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::FormatTime (uint64_t fileTime, LPCWSTR locale)
{
    HRESULT       hr       = S_OK;
    FILETIME      utc      = {};
    SYSTEMTIME    utcTime  = {};
    SYSTEMTIME    local    = {};
    std::wstring  text;
    BOOL          isOk     = FALSE;



    BAIL_OUT_IF (fileTime == 0, S_OK);

    utc.dwLowDateTime  = (DWORD) (fileTime & 0xFFFFFFFF);
    utc.dwHighDateTime = (DWORD) (fileTime >> 32);

    isOk = FileTimeToSystemTime (&utc, &utcTime);
    CWR (isOk);

    isOk = SystemTimeToTzSpecificLocalTime (nullptr, &utcTime, &local);
    CWR (isOk);

    text = FormatClockTime (local, locale);

Error:
    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatClockTime
//
//  The locale's long time format with the milliseconds added after the
//  seconds; GetTimeFormatEx has no field for them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::FormatClockTime (const SYSTEMTIME & localTime, LPCWSTR locale)
{
    constexpr int  kMaxChars          = 128;
    HRESULT        hr                 = S_OK;
    WCHAR          picture[kMaxChars] = {};
    WCHAR          decimal[kMaxChars] = {};
    WCHAR          text[kMaxChars]    = {};
    std::wstring   withMilliseconds;
    int            length             = 0;



    length = GetLocaleInfoEx (locale, LOCALE_STIMEFORMAT, picture, ARRAYSIZE (picture));
    CWR (length > 0);

    length = GetLocaleInfoEx (locale, LOCALE_SDECIMAL, decimal, ARRAYSIZE (decimal));
    CWR (length > 0);

    withMilliseconds = AddMilliseconds (picture, decimal, localTime.wMilliseconds);

    length = GetTimeFormatEx (locale, 0, &localTime, withMilliseconds.c_str(), text, ARRAYSIZE (text));
    CWR (length > 0);

Error:
    return SUCCEEDED (hr) ? std::wstring (text) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddMilliseconds
//
//  Text between apostrophes in a picture is literal, so an "s" there is
//  not a seconds field.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::AddMilliseconds (const std::wstring & picture, const std::wstring & decimal, WORD milliseconds)
{
    std::wstring  result    = picture;
    size_t        afterLast = std::wstring::npos;
    bool          isQuoted  = false;
    size_t        i         = 0;



    for (i = 0; i < picture.size(); i++)
    {
        if (picture[i] == L'\'')
        {
            isQuoted = !isQuoted;
        }
        else if (!isQuoted && picture[i] == L's')
        {
            afterLast = i + 1;
        }
    }

    if (afterLast != std::wstring::npos)
    {
        result.insert (afterLast, std::format (L"'{}{:03}'", decimal, milliseconds));
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTimeFitTexts
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> EventLogText::GetTimeFitTexts (LPCWSTR locale)
{
    constexpr WORD             kHoursPerDay = 24;
    constexpr WORD             kLongMinute  = 58;
    constexpr WORD             kLongMs      = 888;
    constexpr WORD             kAnyYear     = 2026;
    std::vector<std::wstring>  texts;
    SYSTEMTIME                 time         = {};



    time.wYear         = kAnyYear;
    time.wMonth        = 1;
    time.wDay          = 1;
    time.wMinute       = kLongMinute;
    time.wSecond       = kLongMinute;
    time.wMilliseconds = kLongMs;

    for (WORD hour = 0; hour < kHoursPerDay; hour++)
    {
        time.wHour = hour;
        texts.push_back (FormatClockTime (time, locale));
    }

    return texts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatByteChar
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::FormatByteChar (uint8_t value)
{
    constexpr uint8_t  kAscii      = 0x7F;
    constexpr uint8_t  kFirstPrint = 0x20;
    uint8_t            ascii       = value & kAscii;



    if (ascii < kFirstPrint || ascii == kAscii)
    {
        return std::format (L"${:02X}", value);
    }

    return std::format (L"${:02X} '{}'", value, (wchar_t) ascii);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPrintable
//
////////////////////////////////////////////////////////////////////////////////

wchar_t EventLogText::GetPrintable (uint8_t value)
{
    constexpr uint8_t  kAscii      = 0x7F;
    constexpr uint8_t  kFirstPrint = 0x20;
    uint8_t            ascii       = value & kAscii;



    return (ascii < kFirstPrint || ascii == kAscii) ? L'.' : (wchar_t) ascii;
}
