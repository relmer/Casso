#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer
//
//  What a host file with no usable suffix should become on a disk, decided
//  from its bytes and never from its extension.
//
//  THE RULES, IN ORDER:
//
//      Applesoft  when every non-blank line is an integer no larger than
//                 Applesoft's ceiling, the integers strictly ascend, and what
//                 follows each is an Applesoft keyword or an identifier and
//                 an equals sign. That is the shape of a listing and nothing
//                 else; no grammar past the first token is checked, because
//                 the Apple never checked one either.
//
//      Text       when every byte is printable ASCII, a tab or a line
//                 ending, and the file has a line ending or is short.
//
//      Binary     otherwise, with the load address the graphics rule would
//                 want for an 8192-byte file and $0803 for anything else.
//
//  An Integer BASIC listing falls to Text by design: nothing tokenizes it.
//
////////////////////////////////////////////////////////////////////////////////

class ContentSniffer
{
public:
    enum class Verdict { Applesoft, Text, Binary };

    static Verdict  Classify (std::span<const Byte> bytes, Word & outSuggestedAddress);

    //  Whether one line, number already consumed, opens with something
    //  Applesoft would accept as a statement. Exposed so a test can state
    //  the rule line by line.
    static bool  IsApplesoftStatementStart (std::string_view rest);

    static constexpr Word    kHiResAddress     = 0x2000;
    static constexpr Word    kDefaultAddress   = 0x0803;
    static constexpr size_t  kHiResLength      = 8192;

    //  A file with no line ending at all is still text when it is no longer
    //  than one screen; past that a line that never ends is not prose.
    static constexpr size_t  kShortTextBytes   = 256;

private:
    static bool  IsPrintable    (Byte value);
    static bool  IsIdentifierStart (char c);
    static bool  IsIdentifierChar  (char c);
    static bool  LooksLikeApplesoft (std::span<const Byte> bytes);
    static bool  LooksLikeText      (std::span<const Byte> bytes);
    static void  SplitLines (std::span<const Byte> bytes, std::vector<std::string_view> & outLines);
};
