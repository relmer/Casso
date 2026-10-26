#pragma once

#include "Pch.h"

#include "Config/MacAddress.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualSubnet
//
//  The fixed addressing of the virtual network behind the Uthernet II's
//  MACRAW socket, in host byte order. Fixed so one guest configuration works on
//  every PC. Traffic to the gateway goes to the PC's loopback
//  address; DNS queries to the DNS address go to the PC's DNS server.
//
////////////////////////////////////////////////////////////////////////////////

class VirtualSubnet
{
public:

    static constexpr uint32_t  kGatewayIp   = 0x0A004101;   // 10.0.65.1
    static constexpr uint32_t  kDnsIp       = 0x0A004103;   // 10.0.65.3
    static constexpr uint32_t  kGuestIp     = 0x0A00410F;   // 10.0.65.15
    static constexpr uint32_t  kSubnetMask  = 0xFFFFFF00;   // 255.255.255.0
    static constexpr uint32_t  kLoopbackIp  = 0x7F000001;   // 127.0.0.1
    static constexpr uint32_t  kAnyIp       = 0;
    static constexpr uint32_t  kBroadcastIp = 0xFFFFFFFF;
    static constexpr uint16_t  kDnsPort     = 53;

    static constexpr MacAddress::Octets  kGatewayMac = { 0x02, 0xCA, 0x55, 0x0A, 0x00, 0x01 };

    // Where traffic the guest sends to `ip` goes on the PC.
    static uint32_t  GuestToHost (uint32_t ip) { return ip == kGatewayIp ? kLoopbackIp : ip; }

    // How a PC address appears to the guest.
    static uint32_t  HostToGuest (uint32_t ip) { return ip == kLoopbackIp ? kGatewayIp : ip; }

    static bool      IsLoopback  (uint32_t ip) { return (ip >> 24) == (kLoopbackIp >> 24); }
};
