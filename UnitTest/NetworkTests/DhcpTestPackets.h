#pragma once

#include "Pch.h"

#include "Config/MacAddress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpTestPackets
//
//  Client-side DHCP messages for tests, built from RFC 2131 directly rather
//  than through the server under test, and the fields tests read back from a
//  reply.
//
////////////////////////////////////////////////////////////////////////////////

class DhcpTestPackets
{
public:

    static constexpr Byte  kDiscover = 1;
    static constexpr Byte  kOffer    = 2;
    static constexpr Byte  kRequest  = 3;
    static constexpr Byte  kAck      = 5;

    static std::vector<Byte>  MakeRequest   (Byte messageType, const MacAddress::Octets & mac, uint32_t xid);

    static Byte      GetMessageType (std::span<const Byte> reply);
    static uint32_t  GetYourAddress (std::span<const Byte> reply);
    static uint32_t  GetXid         (std::span<const Byte> reply);

    // The value of a four-byte option, or 0 when absent.
    static uint32_t  GetOption32    (std::span<const Byte> reply, Byte code);
};
