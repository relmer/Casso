#include "Pch.h"

#include "Cassque/Model/ContentSniffer.h"
#include "ApplesoftTokenizer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::IsPrintable
//
////////////////////////////////////////////////////////////////////////////////

bool ContentSniffer::IsPrintable (Byte value)
{
    return (value >= 0x20 && value < 0x7F) || value == '\t' || value == '\r' || value == '\n';
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::IsIdentifierStart
//
////////////////////////////////////////////////////////////////////////////////

bool ContentSniffer::IsIdentifierStart (char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::IsIdentifierChar
//
////////////////////////////////////////////////////////////////////////////////

bool ContentSniffer::IsIdentifierChar (char c)
{
    return IsIdentifierStart (c) || (c >= '0' && c <= '9');
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::SplitLines
//
//  CR, LF or CRLF end a line. Views into the caller's bytes, so nothing is
//  copied.
//
////////////////////////////////////////////////////////////////////////////////

void ContentSniffer::SplitLines (std::span<const Byte> bytes, std::vector<std::string_view> & outLines)
{
    const char *  text  = reinterpret_cast<const char *> (bytes.data());
    size_t        count = bytes.size();
    size_t        start = 0;
    size_t        at    = 0;



    outLines.clear();

    while (at < count)
    {
        if (bytes[at] == '\r' || bytes[at] == '\n')
        {
            outLines.emplace_back (text + start, at - start);

            if (bytes[at] == '\r' && at + 1 < count && bytes[at + 1] == '\n')
            {
                at++;
            }

            start = at + 1;
        }

        at++;
    }

    if (start < count)
    {
        outLines.emplace_back (text + start, count - start);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::IsApplesoftStatementStart
//
//  A keyword from Applesoft's own table, or `?` for PRINT, or an identifier
//  followed by `=`. Matched case-insensitively, since a listing typed on the
//  host is as likely lower case as upper.
//
////////////////////////////////////////////////////////////////////////////////

bool ContentSniffer::IsApplesoftStatementStart (std::string_view rest)
{
    size_t  at    = 0;
    size_t  end   = 0;
    Byte    token = 0;



    while (at < rest.size() && rest[at] == ' ')
    {
        at++;
    }

    if (at >= rest.size())
    {
        return false;
    }

    if (rest[at] == '?')
    {
        return true;
    }

    //  Only the word tokens: the table also holds the operators, and a line
    //  opening with one of those is no statement.
    for (token = ApplesoftTokenizer::kFirstToken; token <= ApplesoftTokenizer::kLastToken; token++)
    {
        const char *  keyword = ApplesoftTokenizer::GetKeyword (token);
        size_t        length  = (keyword != nullptr) ? strlen (keyword) : 0;
        bool          isWord  = length > 0 && IsIdentifierStart (keyword[0]);

        if (isWord && at + length <= rest.size()
         && _strnicmp (rest.data() + at, keyword, length) == 0)
        {
            return true;
        }
    }

    if (!IsIdentifierStart (rest[at]))
    {
        return false;
    }

    end = at;

    while (end < rest.size() && IsIdentifierChar (rest[end]))
    {
        end++;
    }

    if (end < rest.size() && (rest[end] == '$' || rest[end] == '%'))
    {
        end++;
    }

    while (end < rest.size() && rest[end] == ' ')
    {
        end++;
    }

    return end < rest.size() && rest[end] == '=';
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::LooksLikeApplesoft
//
////////////////////////////////////////////////////////////////////////////////

bool ContentSniffer::LooksLikeApplesoft (std::span<const Byte> bytes)
{
    std::vector<std::string_view>  lines;
    uint32_t                       previous = 0;
    bool                           seenLine = false;



    for (Byte value : bytes)
    {
        if (!IsPrintable (value))
        {
            return false;
        }
    }

    SplitLines (bytes, lines);

    for (std::string_view line : lines)
    {
        size_t    at     = 0;
        uint32_t  number = 0;
        size_t    digits = 0;

        while (at < line.size() && (line[at] == ' ' || line[at] == '\t'))
        {
            at++;
        }

        if (at >= line.size())
        {
            continue;   // blank
        }

        while (at < line.size() && line[at] >= '0' && line[at] <= '9')
        {
            number = number * 10 + (uint32_t) (line[at] - '0');
            digits++;
            at++;

            if (number > ApplesoftTokenizer::kMaxLineNumber)
            {
                return false;
            }
        }

        if (digits == 0 || (seenLine && number <= previous))
        {
            return false;
        }

        if (!IsApplesoftStatementStart (line.substr (at)))
        {
            return false;
        }

        previous = number;
        seenLine = true;
    }

    return seenLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::LooksLikeText
//
////////////////////////////////////////////////////////////////////////////////

bool ContentSniffer::LooksLikeText (std::span<const Byte> bytes)
{
    bool  hasLineEnding = false;



    for (Byte value : bytes)
    {
        if (!IsPrintable (value))
        {
            return false;
        }

        hasLineEnding = hasLineEnding || value == '\r' || value == '\n';
    }

    return hasLineEnding || bytes.size() <= kShortTextBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ContentSniffer::Classify
//
////////////////////////////////////////////////////////////////////////////////

ContentSniffer::Verdict ContentSniffer::Classify (std::span<const Byte> bytes, Word & outSuggestedAddress)
{
    outSuggestedAddress = (bytes.size() == kHiResLength) ? kHiResAddress : kDefaultAddress;

    if (LooksLikeApplesoft (bytes))
    {
        return Verdict::Applesoft;
    }

    if (LooksLikeText (bytes))
    {
        return Verdict::Text;
    }

    return Verdict::Binary;
}
