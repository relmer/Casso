#pragma once

#include "Pch.h"

#include "Devices/Network/IGuestPacketSink.h"
#include "Devices/Network/PacketCodec.h"
#include "Devices/Network/VirtualSubnet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingPacketSink
//
//  IGuestPacketSink for NAT tests: each packet bound for the guest is
//  appended to a list, in arrival order, for the test to read.
//
////////////////////////////////////////////////////////////////////////////////

class RecordingPacketSink : public IGuestPacketSink
{
public:

    struct UdpPacket
    {
        uint32_t           sourceIp   = 0;
        uint16_t           sourcePort = 0;
        uint16_t           guestPort  = 0;
        std::vector<Byte>  payload;
    };

    void      SendUdpToGuest (uint32_t                sourceIp,
                              uint16_t                sourcePort,
                              uint16_t                guestPort,
                              std::span<const Byte>   payload)   override;
    void      SendTcpToGuest (const TcpSegment & segment)        override { tcp.push_back (segment); }
    uint32_t  GetGuestIp     () const                            override { return VirtualSubnet::kGuestIp; }

    std::vector<UdpPacket>   udp;
    std::vector<TcpSegment>  tcp;
};
