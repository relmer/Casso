#include "Pch.h"

#include "Ui/Debugger/Panes/EventLogText.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatCycle
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::FormatCycle (uint64_t cycle)
{
    constexpr size_t  kGroup  = 3;
    std::wstring      digits  = std::format (L"{}", cycle);
    std::wstring      grouped;
    size_t            i       = 0;



    for (i = 0; i < digits.size(); i++)
    {
        if (i > 0 && (digits.size() - i) % kGroup == 0)
        {
            grouped.push_back (L',');
        }

        grouped.push_back (digits[i]);
    }

    return grouped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatWallTime
//
////////////////////////////////////////////////////////////////////////////////

std::wstring EventLogText::FormatWallTime (uint64_t fileTime)
{
    FILETIME      utc      = {};
    SYSTEMTIME    utcTime  = {};
    SYSTEMTIME    local    = {};
    std::wstring  text;
    BOOL          isOk     = FALSE;



    if (fileTime == 0)
    {
        return text;
    }

    utc.dwLowDateTime  = (DWORD) (fileTime & 0xFFFFFFFF);
    utc.dwHighDateTime = (DWORD) (fileTime >> 32);

    isOk = FileTimeToSystemTime (&utc, &utcTime) && SystemTimeToTzSpecificLocalTime (nullptr, &utcTime, &local);

    if (isOk)
    {
        text = std::format (L"{:02}:{:02}:{:02}.{:03}", local.wHour, local.wMinute, local.wSecond, local.wMilliseconds);
    }

    return text;
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
