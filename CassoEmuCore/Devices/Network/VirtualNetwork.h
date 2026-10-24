#pragma once

#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Devices/Network/DhcpServer.h"
#include "Devices/Network/IGuestPacketSink.h"
#include "Devices/Network/PacketCodec.h"
#include "Devices/Network/PortForwarder.h"
#include "Devices/Network/TcpNat.h"
#include "Devices/Network/UdpNat.h"

class IHostNetwork;





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork
//
//  The far side of the emulated Ethernet wire for socket 0 in MACRAW mode:
//  a gateway with ARP replies for every address but the guest's, ICMP echo
//  replies, a DHCP server, a DNS relay to the PC's DNS server, and UDP and
//  TCP relayed through host sockets (VirtualSubnet has the addressing). Frames for the guest queue here until socket 0's RX buffer
//  has room for them.
//
////////////////////////////////////////////////////////////////////////////////

class VirtualNetwork : public IGuestPacketSink
{
public:

    static constexpr size_t  kMaxQueuedFrames = 256;

    VirtualNetwork ();

    void  SetHostNetwork (IHostNetwork * network);

    // LAN access and port forwards; opens the forwards' host ports.
    void  Configure      (bool lanAccess, const std::vector<PortForward> & forwards);

    void  OnGuestFrame   (std::span<const Byte> frame, uint64_t now);

    bool                       HasFrame  () const { return !m_outbound.empty(); }
    const std::vector<Byte>  & PeekFrame () const { return m_outbound.front(); }
    void                       PopFrame  ()       { m_outbound.pop_front(); }

    // `guestReady`: socket 0 is open in MACRAW, so inbound connections have
    // a guest to go to.
    void  Pump           (uint64_t now, bool guestReady);

    // Ends every relayed flow and connection and drops queued frames; the
    // port forwards stay open.
    void  Reset          ();

    bool  IsActive       () const;

    const std::vector<std::string> & GetErrors () const { return m_forwarder.GetErrors(); }

    //  The address translation's flows that other computers send to while
    //  LAN access is on.
    void  GetInboundPorts (std::vector<FirewallPort> & outPorts) const { m_udp.GetInboundPorts (outPorts); }

    // The DHCP server, for DHCP that arrives through a hardware UDP socket.
    std::vector<Byte>  ProcessDhcp (std::span<const Byte> request) const { return m_dhcp.Process (request); }

    void      SendUdpToGuest (uint32_t                sourceIp,
                              uint16_t                sourcePort,
                              uint16_t                guestPort,
                              std::span<const Byte>   payload)   override;
    void      SendTcpToGuest (const TcpSegment & segment)        override;
    uint32_t  GetGuestIp     () const                            override { return m_guestIp; }

private:

    void  OnArp       (const EthernetFrame & frame);
    void  OnIpv4      (const EthernetFrame & frame, uint64_t now);
    void  OnIcmp      (const Ipv4Packet & packet);
    void  OnUdp       (const Ipv4Packet & packet, uint64_t now);
    void  QueueIpv4   (uint32_t source, uint32_t destination, Byte protocol, std::span<const Byte> payload);
    void  QueueFrame  (std::vector<Byte> frame);

    DhcpServer                      m_dhcp;
    UdpNat                          m_udp;
    TcpNat                          m_tcp;
    PortForwarder                   m_forwarder;
    IHostNetwork                  * m_network      = nullptr;
    std::deque<std::vector<Byte>>   m_outbound;
    MacAddress::Octets              m_guestMac     = PacketCodec::kBroadcastMac;
    uint32_t                        m_guestIp;
    uint16_t                        m_nextIpId     = 0;
};
