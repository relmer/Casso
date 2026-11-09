#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorClipboard.h"





static constexpr LPCWSTR  s_kpszLineEnd   = L"\r\n";
static constexpr Byte     s_kAsciiMask    = 0x7F;
static constexpr Byte     s_kFirstPrinted = 0x20;
static constexpr Byte     s_kDelete       = 0x7F;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::FormatHex (std::span<const Byte> bytes)
{
    std::wstring  text;
    size_t        i    = 0;



    for (i = 0; i < bytes.size(); i++)
    {
        text += std::format (L"{:02X}", bytes[i]);
        text += (i + 1 == bytes.size()) ? L"" : ((i + 1) % kBytesPerLine == 0) ? s_kpszLineEnd : L" ";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::FormatText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::FormatText (std::span<const Byte> bytes)
{
    std::wstring  text;
    size_t        i    = 0;



    for (i = 0; i < bytes.size(); i++)
    {
        text += GetTextChar (bytes[i]);
        text += (i + 1 < bytes.size() && (i + 1) % kBytesPerLine == 0) ? s_kpszLineEnd : L"";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::FormatHexDump
//
//  "Copy sector": per line the offset, the 16 bytes and the same bytes as
//  text.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::FormatHexDump (std::span<const Byte> bytes)
{
    std::wstring  text;
    size_t        at   = 0;
    size_t        n    = 0;



    for (at = 0; at < bytes.size(); at += kBytesPerLine)
    {
        n     = std::min (kBytesPerLine, bytes.size() - at);
        text += std::format (L"{:02X}  ", at) + FormatHex (bytes.subspan (at, n));
        text += std::wstring ((kBytesPerLine - n) * 3, L' ') + L"  " + FormatText (bytes.subspan (at, n)) + s_kpszLineEnd;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::FormatNibbles
//
//  A run of framed nibbles, which may cross the index.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::FormatNibbles (const TrackAnalysis & track, int firstNibble, int nibbleCount)
{
    vector<Byte>  values;
    size_t        n      = track.framed.nibbles.size();
    int           i      = 0;



    for (i = 0; n > 0 && firstNibble >= 0 && i < nibbleCount; i++)
    {
        values.push_back (track.framed.nibbles[(static_cast<size_t> (firstNibble) + i) % n].value);
    }

    return FormatHex (values);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::FormatTable
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::FormatTable (const vector<std::wstring> & header, const vector<vector<std::wstring>> & rows)
{
    std::wstring  text = FormatRow (header);



    for (const vector<std::wstring> & row : rows)
    {
        text += FormatRow (row);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::FormatRow
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::FormatRow (const vector<std::wstring> & cells)
{
    std::wstring  text;
    size_t        i    = 0;



    for (i = 0; i < cells.size(); i++)
    {
        text += (i == 0 ? L"" : L"\t") + cells[i];
    }

    return text + s_kpszLineEnd;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::GetTextChar
//
////////////////////////////////////////////////////////////////////////////////

wchar_t InspectorClipboard::GetTextChar (Byte value)
{
    Byte  plain = static_cast<Byte> (value & s_kAsciiMask);



    return (plain < s_kFirstPrinted || plain == s_kDelete) ? L'.' : static_cast<wchar_t> (plain);
}
