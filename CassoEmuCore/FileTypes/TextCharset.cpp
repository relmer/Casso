#include "Pch.h"

#include "FileTypes/TextCharset.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextCharset::DecodeLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TextCharset::DecodeLine (std::span<const Byte> bytes, Charset charset)
{
    std::wstring  text;
    int           length = 0;
    unsigned      page   = (charset == Charset::MacOsRoman) ? kMacOsRomanPage : kLatin1Page;



    if (charset == Charset::AppleText || charset == Charset::Default)
    {
        for (Byte b : bytes)
        {
            Byte  low = (Byte) (b & 0x7F);

            text += (low == kTab) ? L'\t' : (low < 0x20 || low == 0x7F) ? L' ' : (wchar_t) low;
        }

        return text;
    }

    if (bytes.empty())
    {
        return text;
    }

    length = MultiByteToWideChar (page, 0, (const char *) bytes.data(), (int) bytes.size(), nullptr, 0);
    text.resize ((size_t) std::max (length, 0));

    if (length > 0)
    {
        MultiByteToWideChar (page, 0, (const char *) bytes.data(), (int) bytes.size(), text.data(), length);
    }

    for (wchar_t & c : text)
    {
        c = (c != L'\t' && c < 0x20) ? L' ' : c;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextCharset::DecodeLines
//
//  A line for each carriage return, and the text after the last one when
//  there is any. In Apple text the carriage return is $0D or $8D.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> TextCharset::DecodeLines (std::span<const Byte> bytes, Charset charset)
{
    std::vector<std::wstring>  lines;
    size_t                     start = 0;
    bool                       apple = charset == Charset::AppleText || charset == Charset::Default;



    for (size_t at = 0; at < bytes.size(); at++)
    {
        Byte  b = apple ? (Byte) (bytes[at] & 0x7F) : bytes[at];

        if (b == kCarriageReturn)
        {
            lines.push_back (DecodeLine (bytes.subspan (start, at - start), charset));
            start = at + 1;
        }
    }

    if (start < bytes.size())
    {
        lines.push_back (DecodeLine (bytes.subspan (start), charset));
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextCharset::FromMouseText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring TextCharset::FromMouseText (Byte character)
{
    return (character < _countof (s_kpszMouseText)) ? std::wstring (s_kpszMouseText[character]) : std::wstring (L"?");
}
