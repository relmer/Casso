#include "Pch.h"

#include "DiskMarkPattern.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::MakeExact
//
//  A two-nibble pattern, such as the DE AA that every epilogue starts with.
//
////////////////////////////////////////////////////////////////////////////////

DiskMarkPattern DiskMarkPattern::MakeExact (Byte first, Byte second)
{
    static constexpr int  kPairLength = 2;



    DiskMarkPattern  pattern;



    pattern.m_values[0] = first;
    pattern.m_values[1] = second;
    pattern.m_length    = kPairLength;

    return pattern;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::MakeExact
//
//  A three-nibble pattern, such as the D5 AA 96 address prologue.
//
////////////////////////////////////////////////////////////////////////////////

DiskMarkPattern DiskMarkPattern::MakeExact (Byte first, Byte second, Byte third)
{
    DiskMarkPattern  pattern;



    pattern.m_values[0]               = first;
    pattern.m_values[1]               = second;
    pattern.m_values[kMaxNibbles - 1] = third;
    pattern.m_length                  = kMaxNibbles;

    return pattern;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::TryParse
//
//  Reads one to three space-separated tokens, each two hex digits or ??, in
//  either case. On failure the pattern is left as it was and outError holds a
//  sentence giving the rule.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskMarkPattern::TryParse (std::string_view text, DiskMarkPattern & outPattern, std::string & outError)
{
    static constexpr std::string_view  kpszRule = "A mark is one to three nibbles, each two hex digits or ??.";



    DiskMarkPattern   pattern;
    size_t            pos       = 0;
    size_t            end       = 0;
    bool              isValid   = true;
    bool              isAny     = false;
    Byte              value     = 0;
    std::string_view  token;



    while (isValid && pos < text.size())
    {
        if (text[pos] == ' ')
        {
            pos++;
            continue;
        }

        end = text.find (' ', pos);

        if (end == std::string_view::npos)
        {
            end = text.size();
        }

        token   = text.substr (pos, end - pos);
        isValid = pattern.m_length < kMaxNibbles && TryParseToken (token, value, isAny);

        if (isValid)
        {
            pattern.m_values[pattern.m_length] = value;
            pattern.m_isAny[pattern.m_length]  = isAny;
            pattern.m_length++;
        }

        pos = end;
    }

    isValid = isValid && pattern.m_length > 0;

    if (isValid)
    {
        outPattern = pattern;
    }
    else
    {
        outError = kpszRule;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::TryParseToken
//
//  One token: ?? for any nibble, or exactly two hex digits.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskMarkPattern::TryParseToken (std::string_view token, Byte & outValue, bool & outIsAny)
{
    static constexpr int  kTokenLength = 2;
    static constexpr int  kHexBase     = 16;



    bool                    isValid = false;
    unsigned int            parsed  = 0;
    std::from_chars_result  result  = {};



    outIsAny = false;
    outValue = 0;

    if (token == "??")
    {
        outIsAny = true;
        isValid  = true;
    }
    else if (token.size() == kTokenLength)
    {
        result  = std::from_chars (token.data(), token.data() + token.size(), parsed, kHexBase);
        isValid = result.ec == std::errc() && result.ptr == token.data() + token.size();

        if (isValid)
        {
            outValue = static_cast<Byte> (parsed);
        }
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::Matches
//
//  True when the first GetLength() nibbles match, position by position, with
//  "any" matching every value. A span shorter than the pattern never matches.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskMarkPattern::Matches (std::span<const Byte> nibbles) const
{
    bool  isMatch = m_length > 0 && nibbles.size() >= static_cast<size_t> (m_length);
    int   i       = 0;



    for (i = 0; isMatch && i < m_length; i++)
    {
        isMatch = m_isAny[i] || nibbles[i] == m_values[i];
    }

    return isMatch;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::ToText
//
//  The pattern as TryParse reads it: upper-case hex, ?? for any, one space
//  between nibbles.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskMarkPattern::ToText() const
{
    std::string  text;
    int          i    = 0;



    for (i = 0; i < m_length; i++)
    {
        if (i > 0)
        {
            text += ' ';
        }

        text += m_isAny[i] ? std::string ("??") : std::format ("{:02X}", m_values[i]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern::operator==
//
//  Equal when the lengths match and every position is the same kind with the
//  same value; the value of an "any" position takes no part.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskMarkPattern::operator== (const DiskMarkPattern & other) const
{
    bool  isEqual = m_length == other.m_length;
    int   i       = 0;



    for (i = 0; isEqual && i < m_length; i++)
    {
        isEqual = m_isAny[i] == other.m_isAny[i] && (m_isAny[i] || m_values[i] == other.m_values[i]);
    }

    return isEqual;
}
