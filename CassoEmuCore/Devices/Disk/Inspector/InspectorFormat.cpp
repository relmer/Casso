#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatQuarterTrack
//
//  17, 17.25, 17.5 or 17.75.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatQuarterTrack (int quarterTrack)
{
    static constexpr int             kPerTrack     = 4;
    static constexpr const wchar_t * kpszFractions[kPerTrack] = { L"", L".25", L".5", L".75" };



    return std::to_wstring (quarterTrack / kPerTrack) + kpszFractions[quarterTrack % kPerTrack];
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatQuarterTrackList
//
//  Quarter tracks one step apart join into a run, as "2.25-2.75"; other runs
//  are separated by commas. The list is taken as given, so the caller sorts.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatQuarterTrackList (const vector<int> & quarterTracks)
{
    std::wstring  text;
    size_t        first = 0;
    size_t        last  = 0;



    while (first < quarterTracks.size())
    {
        last = first;

        while (last + 1 < quarterTracks.size() && quarterTracks[last + 1] == quarterTracks[last] + 1)
        {
            last++;
        }

        if (!text.empty())
        {
            text += L", ";
        }

        text += FormatQuarterTrack (quarterTracks[first]);

        if (last > first)
        {
            text += L"-" + FormatQuarterTrack (quarterTracks[last]);
        }

        first = last + 1;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatSector
//
//  $5, $F, $1A: hex with a "$", no leading zero.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatSector (int sector)
{
    return std::format (L"${:X}", sector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatHexByte
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatHexByte (int value)
{
    return std::format (L"${:02X}", value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatHexOffset
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatHexOffset (int offset)
{
    return std::format (L"${:X}", offset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatByteSequence
//
//  D5 AA 96: bare hex, one space apart.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatByteSequence (std::span<const Byte> bytes)
{
    std::wstring  text;



    for (Byte b : bytes)
    {
        if (!text.empty())
        {
            text += L' ';
        }

        text += std::format (L"{:02X}", b);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatCount
//
//  51,007: decimal with a comma every three digits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatCount (uint64_t count)
{
    static constexpr int  kGroup = 3;



    std::wstring  digits = std::to_wstring (count);
    std::wstring  text;
    int           i      = 0;
    int           length = static_cast<int> (digits.size());



    for (i = 0; i < length; i++)
    {
        if (i > 0 && (length - i) % kGroup == 0)
        {
            text += L',';
        }

        text += digits[i];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatDegrees
//
//  A fraction of a turn as whole degrees clockwise from the index.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatDegrees (double fractionOfTurn)
{
    static constexpr double  kDegreesPerTurn = 360.0;



    return std::format (L"{:.0f}{}", fractionOfTurn * kDegreesPerTurn, s_kpszDegree);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatMicroseconds
//
//  125 ns flux ticks as microseconds to two places, as "3.91µs".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatMicroseconds (double ticks)
{
    static constexpr double  kTicksPerMicrosecond = 8.0;



    return std::format (L"{:.2f}{}s", ticks / kTicksPerMicrosecond, s_kpszMicro);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::FormatPercent
//
//  A signed deviation as "+4.8%" or "-5.0%".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorFormat::FormatPercent (double fraction)
{
    static constexpr double  kPercent = 100.0;



    return std::format (L"{:+.1f}%", fraction * kPercent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::TryParseQuarterTrack
//
//  A whole track, or one with .25, .5 or .75, as the views show it.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorFormat::TryParseQuarterTrack (std::wstring_view text, int & outQuarterTrack)
{
    static constexpr int  kPerTrack  = 4;
    static constexpr int  kMaxTracks = 40;



    size_t            dot      = text.find (L'.');
    std::wstring_view whole    = text.substr (0, dot);
    std::wstring_view fraction = (dot == std::wstring_view::npos) ? std::wstring_view() : text.substr (dot);
    int               track    = 0;
    int               quarter  = -1;
    bool              isValid  = TryParseDecimal (whole, track) && track >= 0 && track < kMaxTracks;



    if (fraction.empty() || fraction == L".0" || fraction == L".00")
    {
        quarter = 0;
    }
    else if (fraction == L".25")
    {
        quarter = 1;
    }
    else if (fraction == L".5" || fraction == L".50")
    {
        quarter = 2;
    }
    else if (fraction == L".75")
    {
        quarter = 3;
    }

    isValid = isValid && quarter >= 0;

    if (isValid)
    {
        outQuarterTrack = track * kPerTrack + quarter;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::TryParseHex
//
//  Hex digits with or without a leading "$".
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorFormat::TryParseHex (std::wstring_view text, int & outValue)
{
    static constexpr int  kHexBase = 16;



    std::wstring_view  digits  = (!text.empty() && text[0] == L'$') ? text.substr (1) : text;
    std::string        narrow;
    int                value   = 0;
    bool               isValid = !digits.empty();



    for (wchar_t c : digits)
    {
        isValid = isValid && ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f'));
        narrow += static_cast<char> (c);
    }

    isValid = isValid && std::from_chars (narrow.data(), narrow.data() + narrow.size(), value, kHexBase).ec == std::errc();

    if (isValid)
    {
        outValue = value;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFormat::TryParseDecimal
//
//  Digits, with or without thousands commas.
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorFormat::TryParseDecimal (std::wstring_view text, int & outValue)
{
    static constexpr int  kDecimalBase = 10;



    std::string  narrow;
    int          value   = 0;
    bool         isValid = !text.empty();



    for (wchar_t c : text)
    {
        if (c == L',')
        {
            continue;
        }

        isValid = isValid && c >= L'0' && c <= L'9';
        narrow += static_cast<char> (c);
    }

    isValid = isValid && !narrow.empty() && std::from_chars (narrow.data(), narrow.data() + narrow.size(), value, kDecimalBase).ec == std::errc();

    if (isValid)
    {
        outValue = value;
    }

    return isValid;
}
