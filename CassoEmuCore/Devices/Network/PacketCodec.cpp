#include "Pch.h"

#include "Devices/Network/PacketCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::ReadU16
//
////////////////////////////////////////////////////////////////////////////////

uint16_t PacketCodec::ReadU16 (
    std::span<const Byte>   bytes,
    size_t                  offset)
{
    return (uint16_t) ((bytes[offset] << 8) | bytes[offset + 1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::ReadU32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PacketCodec::ReadU32 (
    std::span<const Byte>   bytes,
    size_t                  offset)
{
    return ((uint32_t) ReadU16 (bytes, offset) << 16) | ReadU16 (bytes, offset + 2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::WriteU16
//
////////////////////////////////////////////////////////////////////////////////

void PacketCodec::WriteU16 (
    std::span<Byte>   bytes,
    size_t            offset,
    uint16_t          value)
{
    bytes[offset]     = (Byte) (value >> 8);
    bytes[offset + 1] = (Byte) (value & 0xFF);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::WriteU32
//
////////////////////////////////////////////////////////////////////////////////

void PacketCodec::WriteU32 (
    std::span<Byte>   bytes,
    size_t            offset,
    uint32_t          value)
{
    WriteU16 (bytes, offset,     (uint16_t) (value >> 16));
    WriteU16 (bytes, offset + 2, (uint16_t) (value & 0xFFFF));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::SumWords
//
//  The running one's-complement sum of 16-bit big-endian words; an odd final
//  byte is padded with zero, per RFC 1071.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PacketCodec::SumWords (
    std::span<const Byte>   bytes,
    uint32_t                sum)
{
    size_t  i = 0;



    for (i = 0; i + 1 < bytes.size(); i += 2)
    {
        sum += ReadU16 (bytes, i);
    }

    if ((bytes.size() & 1) != 0)
    {
        sum += (uint32_t) bytes.back() << 8;
    }

    return sum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::Checksum
//
////////////////////////////////////////////////////////////////////////////////

uint16_t PacketCodec::Checksum (
    std::span<const Byte>   bytes,
    uint32_t                sum)
{
    constexpr uint32_t  kLowWord = 0xFFFF;



    sum = SumWords (bytes, sum);

    while ((sum >> 16) != 0)
    {
        sum = (sum & kLowWord) + (sum >> 16);
    }

    return (uint16_t) (~sum & kLowWord);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::SumPseudoHeader
//
//  The IPv4 pseudo-header that UDP and TCP checksums cover.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PacketCodec::SumPseudoHeader (
    uint32_t  source,
    uint32_t  destination,
    Byte      protocol,
    size_t    length)
{
    uint32_t  sum = 0;



    sum += source >> 16;
    sum += source & 0xFFFF;
    sum += destination >> 16;
    sum += destination & 0xFFFF;
    sum += protocol;
    sum += (uint32_t) length;

    return sum;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::TryParseEthernet
//
////////////////////////////////////////////////////////////////////////////////

bool PacketCodec::TryParseEthernet (
    std::span<const Byte>   frame,
    EthernetFrame         & out)
{
    constexpr size_t  kSourceOffset = 6;
    constexpr size_t  kTypeOffset   = 12;



    if (frame.size() < kEthernetHeader)
    {
        return false;
    }

    std::copy_n (frame.begin(),                 MacAddress::kLength, out.destination.begin());
    std::copy_n (frame.begin() + kSourceOffset, MacAddress::kLength, out.source.begin());

    out.etherType = ReadU16 (frame, kTypeOffset);
    out.payload   = frame.subspan (kEthernetHeader);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::TryParseArp
//
//  Ethernet hardware (1) and IPv4 protocol addresses only.
//
////////////////////////////////////////////////////////////////////////////////

bool PacketCodec::TryParseArp (
    std::span<const Byte>   payload,
    ArpPacket             & out)
{
    constexpr uint16_t  kHardwareEthernet = 1;
    constexpr size_t    kOperation        = 6;
    constexpr size_t    kSenderMac        = 8;
    constexpr size_t    kSenderIp         = 14;
    constexpr size_t    kTargetMac        = 18;
    constexpr size_t    kTargetIp         = 24;



    if (payload.size() < kArpLength
        || ReadU16 (payload, 0) != kHardwareEthernet
        || ReadU16 (payload, 2) != kEtherTypeIpv4)
    {
        return false;
    }

    out.operation = ReadU16 (payload, kOperation);
    out.senderIp  = ReadU32 (payload, kSenderIp);
    out.targetIp  = ReadU32 (payload, kTargetIp);

    std::copy_n (payload.begin() + kSenderMac, MacAddress::kLength, out.senderMac.begin());
    std::copy_n (payload.begin() + kTargetMac, MacAddress::kLength, out.targetMac.begin());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::TryParseIpv4
//
//  Fragments are not reassembled; the guest stacks this serves do not send
//  them.
//
////////////////////////////////////////////////////////////////////////////////

bool PacketCodec::TryParseIpv4 (
    std::span<const Byte>   payload,
    Ipv4Packet            & out)
{
    constexpr Byte      kVersion4       = 4;
    constexpr size_t    kTotalLength    = 2;
    constexpr size_t    kFragment       = 6;
    constexpr uint16_t  kFragmentMask   = 0x3FFF;   // MF and the offset
    constexpr size_t    kProtocol       = 9;
    constexpr size_t    kSource         = 12;
    constexpr size_t    kDestination    = 16;
    size_t              headerLength    = 0;
    size_t              totalLength     = 0;



    if (payload.size() < kIpv4Header || (payload[0] >> 4) != kVersion4)
    {
        return false;
    }

    headerLength = (size_t) (payload[0] & 0x0F) * 4;
    totalLength  = ReadU16 (payload, kTotalLength);

    if (headerLength < kIpv4Header || totalLength < headerLength || totalLength > payload.size())
    {
        return false;
    }

    if (Checksum (payload.first (headerLength)) != 0 || (ReadU16 (payload, kFragment) & kFragmentMask) != 0)
    {
        return false;
    }

    out.protocol    = payload[kProtocol];
    out.source      = ReadU32 (payload, kSource);
    out.destination = ReadU32 (payload, kDestination);
    out.payload     = payload.subspan (headerLength, totalLength - headerLength);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::TryParseUdp
//
//  A zero checksum means the sender did not compute one, which UDP over
//  IPv4 allows.
//
////////////////////////////////////////////////////////////////////////////////

bool PacketCodec::TryParseUdp (
    const Ipv4Packet  & packet,
    UdpDatagram       & out)
{
    constexpr size_t  kLength      = 4;
    constexpr size_t  kChecksum    = 6;
    size_t            length       = 0;
    uint32_t          pseudo       = 0;



    if (packet.payload.size() < kUdpHeader)
    {
        return false;
    }

    length = ReadU16 (packet.payload, kLength);

    if (length < kUdpHeader || length > packet.payload.size())
    {
        return false;
    }

    if (ReadU16 (packet.payload, kChecksum) != 0)
    {
        pseudo = SumPseudoHeader (packet.source, packet.destination, kProtocolUdp, length);

        if (Checksum (packet.payload.first (length), pseudo) != 0)
        {
            return false;
        }
    }

    out.sourcePort      = ReadU16 (packet.payload, 0);
    out.destinationPort = ReadU16 (packet.payload, 2);
    out.payload         = packet.payload.subspan (kUdpHeader, length - kUdpHeader);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::TryParseTcp
//
////////////////////////////////////////////////////////////////////////////////

bool PacketCodec::TryParseTcp (
    const Ipv4Packet  & packet,
    TcpSegment        & out)
{
    constexpr size_t       kSequence       = 4;
    constexpr size_t       kAcknowledgment = 8;
    constexpr size_t       kDataOffset     = 12;
    constexpr size_t       kFlags          = 13;
    constexpr size_t       kWindow         = 14;
    constexpr Byte         kOptionEnd      = 0;
    constexpr Byte         kOptionNop      = 1;
    constexpr Byte         kOptionMss      = 2;
    std::span<const Byte>  segment         = packet.payload;
    size_t                 headerSize      = 0;
    size_t                 i               = 0;
    uint32_t               pseudo          = 0;



    if (segment.size() < kTcpHeader)
    {
        return false;
    }

    headerSize = (size_t) (segment[kDataOffset] >> 4) * 4;
    pseudo     = SumPseudoHeader (packet.source, packet.destination, kProtocolTcp, segment.size());

    if (headerSize < kTcpHeader || headerSize > segment.size() || Checksum (segment, pseudo) != 0)
    {
        return false;
    }

    out.sourceIp        = packet.source;
    out.destinationIp   = packet.destination;
    out.sourcePort      = ReadU16 (segment, 0);
    out.destinationPort = ReadU16 (segment, 2);
    out.sequence        = ReadU32 (segment, kSequence);
    out.acknowledgment  = ReadU32 (segment, kAcknowledgment);
    out.flags           = segment[kFlags];
    out.window          = ReadU16 (segment, kWindow);
    out.mss             = 0;
    out.payload.assign (segment.begin() + headerSize, segment.end());

    for (i = kTcpHeader; i < headerSize && segment[i] != kOptionEnd; )
    {
        if (segment[i] == kOptionNop)
        {
            i++;
            continue;
        }

        if (i + 1 >= headerSize || segment[i + 1] < 2)
        {
            break;
        }

        if (segment[i] == kOptionMss && segment[i + 1] == kTcpMssOption && i + kTcpMssOption <= headerSize)
        {
            out.mss = ReadU16 (segment, i + 2);
        }

        i += segment[i + 1];
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::BuildEthernet
//
//  Padded to the 60-byte minimum, as a frame on the wire is.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> PacketCodec::BuildEthernet (
    const MacAddress::Octets  & destination,
    const MacAddress::Octets  & source,
    uint16_t                    etherType,
    std::span<const Byte>       payload)
{
    constexpr size_t   kTypeOffset = 12;
    std::vector<Byte>  frame;



    frame.reserve (std::max (kMinimumFrame, kEthernetHeader + payload.size()));
    frame.insert (frame.end(), destination.begin(), destination.end());
    frame.insert (frame.end(), source.begin(), source.end());
    frame.resize (kEthernetHeader);
    WriteU16 (frame, kTypeOffset, etherType);
    frame.insert (frame.end(), payload.begin(), payload.end());

    if (frame.size() < kMinimumFrame)
    {
        frame.resize (kMinimumFrame, 0);
    }

    return frame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::BuildArp
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> PacketCodec::BuildArp (const ArpPacket & packet)
{
    constexpr uint16_t  kHardwareEthernet = 1;
    constexpr Byte      kMacSize          = 6;
    constexpr Byte      kIpSize           = 4;
    std::vector<Byte>   arp (kArpLength, 0);



    WriteU16 (arp, 0, kHardwareEthernet);
    WriteU16 (arp, 2, kEtherTypeIpv4);
    arp[4] = kMacSize;
    arp[5] = kIpSize;
    WriteU16 (arp, 6, packet.operation);
    std::copy (packet.senderMac.begin(), packet.senderMac.end(), arp.begin() + 8);
    WriteU32 (arp, 14, packet.senderIp);
    std::copy (packet.targetMac.begin(), packet.targetMac.end(), arp.begin() + 18);
    WriteU32 (arp, 24, packet.targetIp);

    return arp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::BuildIpv4
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> PacketCodec::BuildIpv4 (
    uint32_t                source,
    uint32_t                destination,
    Byte                    protocol,
    uint16_t                identification,
    std::span<const Byte>   payload)
{
    constexpr Byte      kVersionAndLength = 0x45;
    constexpr uint16_t  kDontFragment     = 0x4000;
    std::vector<Byte>   packet (kIpv4Header, 0);



    packet[0] = kVersionAndLength;
    WriteU16 (packet, 2, (uint16_t) (kIpv4Header + payload.size()));
    WriteU16 (packet, 4, identification);
    WriteU16 (packet, 6, kDontFragment);
    packet[8] = kDefaultTtl;
    packet[9] = protocol;
    WriteU32 (packet, 12, source);
    WriteU32 (packet, 16, destination);
    WriteU16 (packet, 10, Checksum (packet));

    packet.insert (packet.end(), payload.begin(), payload.end());

    return packet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::BuildUdp
//
//  A computed checksum of zero is sent as $FFFF, since zero on the wire
//  means no checksum.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> PacketCodec::BuildUdp (
    uint32_t                sourceIp,
    uint32_t                destinationIp,
    uint16_t                sourcePort,
    uint16_t                destinationPort,
    std::span<const Byte>   payload)
{
    constexpr uint16_t  kNoChecksum = 0xFFFF;
    std::vector<Byte>   datagram (kUdpHeader, 0);
    uint16_t            checksum    = 0;



    WriteU16 (datagram, 0, sourcePort);
    WriteU16 (datagram, 2, destinationPort);
    WriteU16 (datagram, 4, (uint16_t) (kUdpHeader + payload.size()));
    datagram.insert (datagram.end(), payload.begin(), payload.end());

    checksum = Checksum (datagram, SumPseudoHeader (sourceIp, destinationIp, kProtocolUdp, datagram.size()));
    WriteU16 (datagram, 6, checksum == 0 ? kNoChecksum : checksum);

    return datagram;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::BuildTcp
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> PacketCodec::BuildTcp (const TcpSegment & segment)
{
    constexpr Byte     kOptionMss  = 2;
    size_t             headerSize  = kTcpHeader + (segment.mss != 0 ? kTcpMssOption : 0);
    std::vector<Byte>  bytes (headerSize, 0);



    WriteU16 (bytes, 0,  segment.sourcePort);
    WriteU16 (bytes, 2,  segment.destinationPort);
    WriteU32 (bytes, 4,  segment.sequence);
    WriteU32 (bytes, 8,  segment.acknowledgment);
    bytes[12] = (Byte) ((headerSize / 4) << 4);
    bytes[13] = segment.flags;
    WriteU16 (bytes, 14, segment.window);

    if (segment.mss != 0)
    {
        bytes[kTcpHeader]     = kOptionMss;
        bytes[kTcpHeader + 1] = (Byte) kTcpMssOption;
        WriteU16 (bytes, kTcpHeader + 2, segment.mss);
    }

    bytes.insert (bytes.end(), segment.payload.begin(), segment.payload.end());
    WriteU16 (bytes, 16, Checksum (bytes, SumPseudoHeader (segment.sourceIp,
                                                           segment.destinationIp,
                                                           kProtocolTcp,
                                                           bytes.size())));

    return bytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PacketCodec::BuildIcmpEchoReply
//
//  The request with its type set to echo reply and its checksum recomputed;
//  the identifier, sequence number and data are copied from the request.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> PacketCodec::BuildIcmpEchoReply (std::span<const Byte> request)
{
    constexpr size_t   kChecksum = 2;
    std::vector<Byte>  reply (request.begin(), request.end());



    reply[0] = kIcmpEchoReply;
    WriteU16 (reply, kChecksum, 0);
    WriteU16 (reply, kChecksum, Checksum (reply));

    return reply;
}
