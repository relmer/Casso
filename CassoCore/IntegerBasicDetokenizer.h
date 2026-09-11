#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicDetokenizer
//
//  The tokenized form Integer BASIC keeps in memory and DOS 3.3 stores in a
//  type I file, rendered back as host text.
//
//  ONE DIRECTION ONLY. Nothing here tokenizes: an Integer listing arriving from
//  the host is stored as text, because the machine that ran Integer BASIC
//  never saved anything else and the guest would have to be booted into it to
//  RUN the result.
//
//  THE STORED FORM, from the ROM's own conventions:
//
//      Each line is a length byte counting itself, a little-endian line
//      number, the tokens, and $01. Bytes below $80 are tokens; bytes at $80
//      and above are characters with the high bit set. A numeric constant is
//      its first digit, high bit set, followed by the little-endian value, and
//      it is told from a digit inside a variable name by what precedes it: a
//      digit continuing an identifier follows a letter or another digit. A
//      string is the open-quote token, characters, and the close-quote token.
//      REM keeps the rest of its line verbatim.
//
//  A MALFORMED LINE REFUSES THE WHOLE PROGRAM rather than rendering what came
//  before it. A partial listing reads as a short program, and a short program
//  is a plausible thing to ship, so the refusal carries the offset the walk
//  stopped at instead.
//
////////////////////////////////////////////////////////////////////////////////

//
//  Where a refusal happened. The offset is into the program bytes as handed
//  over, which is what a hex view of the file shows.
//
struct IntegerBasicListingError
{
    std::string  reason;
    size_t       offset        = 0;
    uint32_t     lineNumber    = 0;
    bool         hasLineNumber = false;
};



class IntegerBasicDetokenizer
{
public:
    static HRESULT  Detokenize (const std::vector<Byte>   & programBytes,
                                std::string               & outListing,
                                IntegerBasicListingError  & outError);

    //  What a token byte writes, or nullptr when the byte is not a token.
    static const char *  GetKeyword (Byte token);

    static constexpr Byte  kTokenEndOfLine  = 0x01;
    static constexpr Byte  kTokenQuoteOpen  = 0x28;
    static constexpr Byte  kTokenQuoteClose = 0x29;
    static constexpr Byte  kTokenRem        = 0x5D;
    static constexpr Byte  kLastToken       = 0x7F;
    static constexpr Byte  kHighBit         = 0x80;

    //  The ROM's own ceiling.
    static constexpr uint32_t  kMaxLineNumber = 32767;

    //  The smallest line that can exist: its length, its number, and $01.
    static constexpr size_t  kLineHeaderBytes  = 3;
    static constexpr size_t  kMinimumLineBytes = kLineHeaderBytes + 1;

    //  What a stored constant costs beyond its leading digit.
    static constexpr size_t  kConstantValueBytes = 2;

private:
    static HRESULT  RenderOneLine (const std::vector<Byte>  & programBytes,
                                   size_t                     bodyAt,
                                   size_t                     endAt,
                                   std::string              & outText,
                                   std::string              & outReason,
                                   size_t                   & outFaultAt);

    //  Whether a token writes as a word rather than a symbol, which is what
    //  decides the spacing around it.
    static bool  IsWordKeyword (const char * keyword);
    static bool  IsHighDigit   (Byte value);
    static bool  IsHighLetter  (Byte value);
};
