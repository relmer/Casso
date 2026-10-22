#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkAnnouncement
//
//  What a universal boot disk's Uthernet II broadcasts to UDP port 6510
//  while it waits for the PC: CASSOLINK in ASCII, a version byte, its IPv4
//  address, its TCP port, high byte first, and its MAC address.
//
////////////////////////////////////////////////////////////////////////////////

struct LinkAnnouncement
{
    static constexpr Byte    kVersion = 1;
    static constexpr size_t  kLength  = 22;

    std::array<Byte, 4>  address = {};
    uint16_t             port    = 0;
    std::array<Byte, 6>  mac     = {};

    bool  operator== (const LinkAnnouncement &) const = default;

    //  False for a datagram that is not an announcement of this version.
    static bool  Parse (std::span<const Byte> datagram, LinkAnnouncement & outAnnouncement);

    //  The address and port, as 192.168.1.20:6502.
    std::string  GetAddressText () const;
};
