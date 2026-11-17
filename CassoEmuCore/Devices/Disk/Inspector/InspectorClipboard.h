#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorClipboard
//
//  The text the inspector puts on the clipboard (FR-057): sector bytes as
//  hex digits, 16 to a line, or as characters with the high bit masked and
//  "." for what does not print; a whole sector as a hex dump of offset,
//  bytes and text; nibbles as hex; and table rows tab-separated under their
//  header. Lines end in CR LF, as Windows text does. Pasted hex text is
//  read back into bytes. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorClipboard
{
public:
    static constexpr size_t  kBytesPerLine = 16;

    enum class HexParse
    {
        Bytes,
        Empty,
        NotHex,
        OddDigits,
    };

    static std::wstring  FormatHex       (std::span<const Byte> bytes);
    static std::wstring  FormatText      (std::span<const Byte> bytes);
    static std::wstring  FormatHexDump   (std::span<const Byte> bytes);
    static std::wstring  FormatNibbles   (const TrackAnalysis & track, int firstNibble, int nibbleCount);
    static std::wstring  FormatTable     (const vector<std::wstring> & header, const vector<vector<std::wstring>> & rows);

    static wchar_t       GetTextChar     (Byte value);
    static HexParse      ParseHex        (const std::wstring & text, vector<Byte> & outBytes);
    static std::wstring  GetHexPasteNote (HexParse result);

private:
    static std::wstring  FormatRow       (const vector<std::wstring> & cells);
    static int           GetHexValue     (wchar_t ch);
    static bool          IsSpace         (wchar_t ch);
};
