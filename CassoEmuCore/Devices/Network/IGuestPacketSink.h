#pragma once

#include "Pch.h"

struct TcpSegment;





////////////////////////////////////////////////////////////////////////////////
//
//  IGuestPacketSink
//
//  Where the virtual network's NAT puts traffic bound for the emulated Apple
//  II. The implementation wraps each packet in IPv4 and Ethernet headers and
//  queues the frame for socket 0. Addresses are the guest-side ones, in host
//  byte order.
//
////////////////////////////////////////////////////////////////////////////////

class IGuestPacketSink
{
public:

    virtual ~IGuestPacketSink () = default;

    virtual void      SendUdpToGuest (uint32_t                sourceIp,
                                      uint16_t                sourcePort,
                                      uint16_t                guestPort,
                                      std::span<const Byte>   payload)   = 0;
    virtual void      SendTcpToGuest (const TcpSegment & segment)        = 0;
    virtual uint32_t  GetGuestIp     () const                            = 0;
};
