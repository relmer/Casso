#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextCharset
//
//  Bytes as Unicode in a character set. Apple text drops the high bit, and
//  a control character other than a tab becomes a space; Mac OS Roman and
//  ISO 8859-1 go through Windows' code pages for them. A carriage return
//  ends a line in each.
//
////////////////////////////////////////////////////////////////////////////////

class TextCharset
{
public:
    //  Default resolved for a format: Mac OS Roman for a IIgs format.
    static Charset  Resolve (Charset charset, bool iigsFormat) { return (charset != Charset::Default) ? charset : (iigsFormat ? Charset::MacOsRoman : Charset::AppleText); }

    static std::wstring               DecodeLine  (std::span<const Byte> bytes, Charset charset);
    static std::vector<std::wstring>  DecodeLines (std::span<const Byte> bytes, Charset charset);

    //  A MouseText character, 0 to 31, as Unicode, two UTF-16 units for one
    //  outside the Basic Multilingual Plane.
    static std::wstring  FromMouseText (Byte character);

private:
    static constexpr Byte          kCarriageReturn = 0x0D;
    static constexpr Byte          kTab            = 0x09;
    static constexpr unsigned      kMacOsRomanPage = 10000;
    static constexpr unsigned      kLatin1Page     = 28591;
};
