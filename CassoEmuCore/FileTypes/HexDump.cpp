#include "Pch.h"

#include "FileTypes/HexDump.h"
#include "FileTypes/TextCharset.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HexDump::Decode
//
//  An empty file is an empty dump.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HexDump::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (outError);

    outFile.kind  = DecodedFile::Kind::Lines;
    outFile.lines = Format (bytes, TextCharset::Resolve (options.charset, false));

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HexDump::Format
//
//  A short last line is padded, so its characters line up with the lines
//  above it. A space between the eighth and ninth bytes splits each line
//  into halves.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> HexDump::Format (std::span<const Byte> bytes, Charset charset)
{
    std::vector<std::wstring>  lines;
    int                        digits = (bytes.size() > 0x10000) ? 6 : 4;
    size_t                     start  = 0;



    for (start = 0; start < bytes.size(); start += kBytesPerLine)
    {
        std::span<const Byte>  row   = bytes.subspan (start, (std::min) (kBytesPerLine, bytes.size() - start));
        std::wstring           line  = std::format (L"{:0{}X}: ", start, digits);
        std::wstring           chars;
        size_t                 i     = 0;

        for (i = 0; i < kBytesPerLine; i++)
        {
            line += (i == kBytesPerLine / 2) ? L" " : L"";
            line += (i < row.size()) ? std::format (L"{:02X} ", row[i]) : std::wstring (L"   ");
        }

        for (Byte b : row)
        {
            chars += IsControl (b, charset) ? std::wstring (L".") : TextCharset::DecodeLine (std::span<const Byte> (&b, 1), charset);
        }

        lines.push_back (line + L" " + chars);
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HexDump::IsControl
//
//  Apple text reads a byte without its high bit. ISO 8859-1 has a second
//  set of controls, $80 to $9F.
//
////////////////////////////////////////////////////////////////////////////////

bool HexDump::IsControl (Byte b, Charset charset)
{
    Byte  low = (charset == Charset::AppleText) ? (Byte) (b & 0x7F) : b;



    return low < 0x20 || low == 0x7F || (charset == Charset::Iso8859_1 && low >= 0x80 && low < 0xA0);
}
