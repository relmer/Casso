#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DhcpServer
//
//  The virtual network's DHCP server: one guest, one fixed lease. A DISCOVER
//  produces an OFFER and a REQUEST an ACK, both with the guest address,
//  the subnet mask, the router, the DNS server and the lease time. Anything
//  else produces no reply.
//
////////////////////////////////////////////////////////////////////////////////

class DhcpServer
{
public:

    static constexpr uint16_t  kServerPort   = 67;
    static constexpr uint16_t  kClientPort   = 68;
    static constexpr uint32_t  kLeaseSeconds = 86400;

    struct Lease
    {
        uint32_t  clientIp   = 0;
        uint32_t  subnetMask = 0;
        uint32_t  router     = 0;
        uint32_t  dnsServer  = 0;
    };

    explicit DhcpServer (const Lease & lease) : m_lease (lease) {}

    // The BOOTP payload of a client datagram in; the reply payload out, or
    // empty when there is none.
    std::vector<Byte>  Process (std::span<const Byte> request) const;

private:

    // BOOTP fixed fields (RFC 2131), then the magic cookie and the options
    // (RFC 2132): each a code, a length and the value; 0 pads, 255 ends.
    static constexpr size_t    kOpOffset          = 0;
    static constexpr size_t    kHtypeOffset       = 1;
    static constexpr size_t    kHlenOffset        = 2;
    static constexpr size_t    kXidOffset         = 4;
    static constexpr size_t    kXidLength         = 4;
    static constexpr size_t    kFlagsOffset       = 10;
    static constexpr size_t    kFlagsLength       = 2;
    static constexpr size_t    kYiaddrOffset      = 16;
    static constexpr size_t    kSiaddrOffset      = 20;
    static constexpr size_t    kChaddrOffset      = 28;
    static constexpr size_t    kChaddrLength      = 16;
    static constexpr size_t    kCookieOffset      = 236;
    static constexpr size_t    kOptionsOffset     = 240;
    static constexpr size_t    kMinimumReply      = 300;
    static constexpr uint32_t  kMagicCookie       = 0x63825363;
    static constexpr Byte      kBootRequest       = 1;
    static constexpr Byte      kBootReply         = 2;
    static constexpr Byte      kHtypeEthernet     = 1;
    static constexpr Byte      kHlenEthernet      = 6;
    static constexpr Byte      kMessageDiscover   = 1;
    static constexpr Byte      kMessageOffer      = 2;
    static constexpr Byte      kMessageRequest    = 3;
    static constexpr Byte      kMessageAck        = 5;
    static constexpr Byte      kOptionPad         = 0;
    static constexpr Byte      kOptionSubnetMask  = 1;
    static constexpr Byte      kOptionRouter      = 3;
    static constexpr Byte      kOptionDns         = 6;
    static constexpr Byte      kOptionLeaseTime   = 51;
    static constexpr Byte      kOptionMessageType = 53;
    static constexpr Byte      kOptionServerId    = 54;
    static constexpr Byte      kOptionEnd         = 255;

    static Byte  FindMessageType (std::span<const Byte> request);
    static void  AppendOption    (std::vector<Byte> & reply, Byte code, Byte value);
    static void  AppendOption    (std::vector<Byte> & reply, Byte code, uint32_t value);

    std::vector<Byte>  BuildReply (std::span<const Byte> request, Byte messageType) const;

    Lease  m_lease;
};
