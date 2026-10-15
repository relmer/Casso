#include "Pch.h"

#include "RecordingPacketSink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingPacketSink::SendUdpToGuest
//
////////////////////////////////////////////////////////////////////////////////

void RecordingPacketSink::SendUdpToGuest (
    uint32_t                sourceIp,
    uint16_t                sourcePort,
    uint16_t                guestPort,
    std::span<const Byte>   payload)
{
    UdpPacket  packet;



    packet.sourceIp   = sourceIp;
    packet.sourcePort = sourcePort;
    packet.guestPort  = guestPort;
    packet.payload.assign (payload.begin(), payload.end());

    udp.push_back (std::move (packet));
}
