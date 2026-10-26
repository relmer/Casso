#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MacAddress
//
//  Six octets, written as hex pairs separated by colons or hyphens. Bit 0 of
//  the first octet set marks a multicast address; bit 1 set marks a locally
//  administered one.
//
////////////////////////////////////////////////////////////////////////////////

class MacAddress
{
public:

    static constexpr size_t  kLength = 6;

    using Octets = std::array<Byte, kLength>;

    MacAddress () = default;
    explicit MacAddress (const Octets & octets) : m_octets (octets), m_isSet (true) {}

    static bool        TryParse        (std::string_view text, MacAddress & outAddress);

    // `random` supplies the six octets; the multicast bit is cleared and the
    // locally administered bit set.
    static MacAddress  MakeRandomLocal (const Octets & random);

    std::string        Format          () const;
    bool               IsSet           () const { return m_isSet; }
    bool               IsUnicast       () const;
    const Octets     & GetOctets       () const { return m_octets; }

    bool operator== (const MacAddress & other) const = default;

private:

    static constexpr Byte  kMulticastBit = 0x01;
    static constexpr Byte  kLocalBit     = 0x02;

    static bool  TryParseHexPair (std::string_view text, Byte & outValue);

    Octets  m_octets = {};
    bool    m_isSet  = false;
};
