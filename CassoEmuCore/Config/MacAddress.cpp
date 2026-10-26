#include "Pch.h"

#include "Config/MacAddress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddress::TryParse
//
//  Six hex pairs with one separator style throughout: "02:CA:55:12:34:56"
//  or "02-CA-55-12-34-56". Case does not matter.
//
////////////////////////////////////////////////////////////////////////////////

bool MacAddress::TryParse (
    std::string_view   text,
    MacAddress       & outAddress)
{
    constexpr size_t  kPairLength   = 2;
    constexpr size_t  kGroupLength  = kPairLength + 1;
    constexpr size_t  kTextLength   = kLength * kGroupLength - 1;
    Octets            octets        = {};
    char              separator     = 0;
    size_t            i             = 0;



    if (text.size() != kTextLength)
    {
        return false;
    }

    separator = text[kPairLength];

    if (separator != ':' && separator != '-')
    {
        return false;
    }

    for (i = 0; i < kLength; i++)
    {
        if (i > 0 && text[i * kGroupLength - 1] != separator)
        {
            return false;
        }

        if (!TryParseHexPair (text.substr (i * kGroupLength, kPairLength), octets[i]))
        {
            return false;
        }
    }

    outAddress = MacAddress (octets);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddress::TryParseHexPair
//
////////////////////////////////////////////////////////////////////////////////

bool MacAddress::TryParseHexPair (
    std::string_view   text,
    Byte             & outValue)
{
    constexpr int           kHexBase = 16;
    unsigned int            value    = 0;
    const char            * end      = text.data() + text.size();
    std::from_chars_result  parsed   = {};



    parsed = std::from_chars (text.data(), end, value, kHexBase);

    if (parsed.ec != std::errc() || parsed.ptr != end)
    {
        return false;
    }

    outValue = (Byte) value;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddress::MakeRandomLocal
//
////////////////////////////////////////////////////////////////////////////////

MacAddress MacAddress::MakeRandomLocal (const Octets & random)
{
    Octets  octets = random;



    octets[0] = (Byte) ((octets[0] & ~kMulticastBit) | kLocalBit);

    return MacAddress (octets);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddress::Format
//
//  Uppercase, colon-separated.
//
////////////////////////////////////////////////////////////////////////////////

std::string MacAddress::Format() const
{
    return std::format ("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                        m_octets[0],
                        m_octets[1],
                        m_octets[2],
                        m_octets[3],
                        m_octets[4],
                        m_octets[5]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddress::IsUnicast
//
////////////////////////////////////////////////////////////////////////////////

bool MacAddress::IsUnicast() const
{
    return (m_octets[0] & kMulticastBit) == 0;
}
