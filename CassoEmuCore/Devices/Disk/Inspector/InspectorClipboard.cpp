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





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::ParseHex
//
//  Values are separated by spaces, tabs or line breaks. A value is pairs of
//  hex digits, read in order, and may start with "$" or "0x". A value with a
//  prefix may also be one digit. For any other text, the call returns an
//  error and an empty byte list, so nothing is pasted.
//
////////////////////////////////////////////////////////////////////////////////

InspectorClipboard::HexParse InspectorClipboard::ParseHex (const std::wstring & text, vector<Byte> & outBytes)
{
    constexpr int  kNibbleBits = 4;



    HexParse  result     = HexParse::Bytes;
    size_t    i          = 0;
    size_t    first      = 0;
    size_t    count      = 0;
    size_t    k          = 0;
    bool      isPrefixed = false;



    outBytes.clear();

    while (result == HexParse::Bytes && i < text.size())
    {
        isPrefixed = text[i] == L'$' || (text[i] == L'0' && i + 1 < text.size() && (text[i + 1] == L'x' || text[i + 1] == L'X'));
        i         += isPrefixed ? ((text[i] == L'$') ? 1 : 2) : 0;
        first      = i;

        while (i < text.size() && !IsSpace (text[i]) && GetHexValue (text[i]) >= 0)
        {
            i++;
        }

        count = i - first;

        if (count == 0 && !isPrefixed)
        {
            //  A space between values, or a character that isn't hex.
            result = IsSpace (text[i]) ? HexParse::Bytes : HexParse::NotHex;
            i++;
        }
        else if ((i < text.size() && !IsSpace (text[i])) || count == 0)
        {
            result = HexParse::NotHex;
        }
        else if (count == 1 && isPrefixed)
        {
            outBytes.push_back (static_cast<Byte> (GetHexValue (text[first])));
        }
        else if (count % 2 != 0)
        {
            result = HexParse::OddDigits;
        }
        else
        {
            for (k = first; k < i; k += 2)
            {
                outBytes.push_back (static_cast<Byte> ((GetHexValue (text[k]) << kNibbleBits) | GetHexValue (text[k + 1])));
            }
        }
    }

    if (result == HexParse::Bytes && outBytes.empty())
    {
        result = HexParse::Empty;
    }

    if (result != HexParse::Bytes)
    {
        outBytes.clear();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::GetHexPasteNote
//
//  Why a hex paste wrote nothing, or empty after one that worked.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring InspectorClipboard::GetHexPasteNote (HexParse result)
{
    switch (result)
    {
        case HexParse::Empty:      return L"Nothing to paste: the clipboard has no hex digits";
        case HexParse::NotHex:     return L"Paste rejected because the clipboard contains non-hex characters";
        case HexParse::OddDigits:  return L"Paste rejected because a hex value on the clipboard has an odd number of digits";
        default:                   return std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::GetHexValue
//
//  The digit's value, or -1 for a character that isn't a hex digit.
//
////////////////////////////////////////////////////////////////////////////////

int InspectorClipboard::GetHexValue (wchar_t ch)
{
    constexpr int  kDecimal = 10;



    return (ch >= L'0' && ch <= L'9') ? ch - L'0'
         : (ch >= L'a' && ch <= L'f') ? ch - L'a' + kDecimal
         : (ch >= L'A' && ch <= L'F') ? ch - L'A' + kDecimal
                                      : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard::IsSpace
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorClipboard::IsSpace (wchar_t ch)
{
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
}
