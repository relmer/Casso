#pragma once

#include "Pch.h"

#include "Config/MacAddress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EthernetFrame
//
//  A parsed Ethernet II header; `payload` refers into the parsed buffer.
//
////////////////////////////////////////////////////////////////////////////////

struct EthernetFrame
{
    MacAddress::Octets     destination = {};
    MacAddress::Octets     source      = {};
    uint16_t               etherType   = 0;
    std::span<const Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ArpPacket
//
//  IPv4-over-Ethernet ARP.
//
////////////////////////////////////////////////////////////////////////////////

struct ArpPacket
{
    uint16_t            operation = 0;
    MacAddress::Octets  senderMac = {};
    uint32_t            senderIp  = 0;
    MacAddress::Octets  targetMac = {};
    uint32_t            targetIp  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Ipv4Packet
//
////////////////////////////////////////////////////////////////////////////////

struct Ipv4Packet
{
    uint32_t               source      = 0;
    uint32_t               destination = 0;
    Byte                   protocol    = 0;
    std::span<const Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  UdpDatagram
//
////////////////////////////////////////////////////////////////////////////////

struct UdpDatagram
{
    uint16_t               sourcePort      = 0;
    uint16_t               destinationPort = 0;
    std::span<const Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSegment
//
//  A TCP segment with addresses, so the NAT can pass one around whole. Only
//  the MSS option is understood; `mss` 0 means the option is absent.
//
////////////////////////////////////////////////////////////////////////////////

struct TcpSegment
{
    uint32_t           sourceIp        = 0;
    uint32_t           destinationIp   = 0;
    uint16_t           sourcePort      = 0;
    uint16_t           destinationPort = 0;
    uint32_t           sequence        = 0;
    uint32_t           acknowledgment  = 0;
    Byte               flags           = 0;
    uint16_t           window          = 0;
    uint16_t           mss             = 0;
    std::vector<Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec
//
//  Parsing and building of the headers the virtual network uses: Ethernet II,
//  ARP, IPv4 without options or fragments, ICMP echo, UDP and TCP. Parsers
//  return false for anything truncated, malformed or failing its checksum.
//  Multi-byte fields are big-endian on the wire and host order in the
//  structures.
//
////////////////////////////////////////////////////////////////////////////////

class PacketCodec
{
public:

    static constexpr uint16_t  kEtherTypeIpv4      = 0x0800;
    static constexpr uint16_t  kEtherTypeArp       = 0x0806;
    static constexpr uint16_t  kArpRequest         = 1;
    static constexpr uint16_t  kArpReply           = 2;
    static constexpr Byte      kProtocolIcmp       = 1;
    static constexpr Byte      kProtocolTcp        = 6;
    static constexpr Byte      kProtocolUdp        = 17;
    static constexpr Byte      kIcmpEchoReply      = 0;
    static constexpr Byte      kIcmpEchoRequest    = 8;
    static constexpr Byte      kTcpFin             = 0x01;
    static constexpr Byte      kTcpSyn             = 0x02;
    static constexpr Byte      kTcpRst             = 0x04;
    static constexpr Byte      kTcpPsh             = 0x08;
    static constexpr Byte      kTcpAck             = 0x10;
    static constexpr size_t    kEthernetHeader     = 14;
    static constexpr size_t    kMinimumFrame       = 60;
    static constexpr size_t    kIpv4Header         = 20;
    static constexpr size_t    kUdpHeader          = 8;
    static constexpr size_t    kTcpHeader          = 20;
    static constexpr size_t    kTcpMssOption       = 4;
    static constexpr size_t    kIcmpHeader         = 8;
    static constexpr size_t    kArpLength          = 28;
    static constexpr Byte      kDefaultTtl         = 64;

    static constexpr MacAddress::Octets  kBroadcastMac = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    // Internet checksum (RFC 1071) of `bytes`, continuing from a partial
    // one's-complement `sum`; returns the final complemented value.
    static uint16_t  Checksum (std::span<const Byte> bytes, uint32_t sum = 0);

    static bool  TryParseEthernet (std::span<const Byte> frame,  EthernetFrame & out);
    static bool  TryParseArp      (std::span<const Byte> payload, ArpPacket & out);
    static bool  TryParseIpv4     (std::span<const Byte> payload, Ipv4Packet & out);
    static bool  TryParseUdp      (const Ipv4Packet & packet,     UdpDatagram & out);
    static bool  TryParseTcp      (const Ipv4Packet & packet,     TcpSegment & out);

    static std::vector<Byte>  BuildEthernet (const MacAddress::Octets  & destination,
                                             const MacAddress::Octets  & source,
                                             uint16_t                    etherType,
                                             std::span<const Byte>       payload);
    static std::vector<Byte>  BuildArp      (const ArpPacket & packet);
    static std::vector<Byte>  BuildIpv4     (uint32_t                source,
                                             uint32_t                destination,
                                             Byte                    protocol,
                                             uint16_t                identification,
                                             std::span<const Byte>   payload);
    static std::vector<Byte>  BuildUdp      (uint32_t                sourceIp,
                                             uint32_t                destinationIp,
                                             uint16_t                sourcePort,
                                             uint16_t                destinationPort,
                                             std::span<const Byte>   payload);
    static std::vector<Byte>  BuildTcp      (const TcpSegment & segment);
    static std::vector<Byte>  BuildIcmpEchoReply (std::span<const Byte> request);

    static uint16_t  ReadU16  (std::span<const Byte> bytes, size_t offset);
    static uint32_t  ReadU32  (std::span<const Byte> bytes, size_t offset);
    static void      WriteU16 (std::span<Byte> bytes, size_t offset, uint16_t value);
    static void      WriteU32 (std::span<Byte> bytes, size_t offset, uint32_t value);

private:

    static uint32_t  SumPseudoHeader (uint32_t source, uint32_t destination, Byte protocol, size_t length);
    static uint32_t  SumWords        (std::span<const Byte> bytes, uint32_t sum);
};
