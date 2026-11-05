#include "Pch.h"

#include "Devices/Network/VirtualNetwork.h"
#include "Devices/Network/VirtualSubnet.h"

using P = PacketCodec;





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::VirtualNetwork
//
////////////////////////////////////////////////////////////////////////////////

VirtualNetwork::VirtualNetwork()
    : m_dhcp      ({ VirtualSubnet::kGuestIp, VirtualSubnet::kSubnetMask, VirtualSubnet::kGatewayIp, VirtualSubnet::kDnsIp }),
      m_udp       (*this),
      m_tcp       (*this),
      m_forwarder (m_tcp, m_udp),
      m_guestIp   (VirtualSubnet::kGuestIp)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::SetHostNetwork
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::SetHostNetwork (IHostNetwork * network)
{
    m_network = network;
    m_udp.SetHostNetwork (network);
    m_tcp.SetHostNetwork (network);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::Configure
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::Configure (
    bool                              lanAccess,
    const std::vector<PortForward>  & forwards)
{
    m_udp.CloseAll();
    m_udp.SetLanAccess (lanAccess);
    m_forwarder.Start (m_network, forwards, lanAccess);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::OnGuestFrame
//
//  The source MAC of each frame the guest sends is where replies go.
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::OnGuestFrame (
    std::span<const Byte>   frame,
    uint64_t                now)
{
    EthernetFrame  ethernet;



    if (!P::TryParseEthernet (frame, ethernet))
    {
        return;
    }

    m_guestMac = ethernet.source;

    if (ethernet.etherType == P::kEtherTypeArp)
    {
        OnArp (ethernet);
    }
    else if (ethernet.etherType == P::kEtherTypeIpv4)
    {
        OnIpv4 (ethernet, now);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::OnArp
//
//  Every address but the guest's own resolves to the gateway, so traffic to
//  any destination, on the virtual subnet's numbering or not, goes to the
//  NAT. A probe (sender 0.0.0.0) or an announcement (sender = target) gets
//  no reply, or the guest would read it as an address conflict.
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::OnArp (const EthernetFrame & frame)
{
    ArpPacket          request;
    ArpPacket          reply;
    std::vector<Byte>  payload;



    if (!P::TryParseArp (frame.payload, request) || request.operation != P::kArpRequest)
    {
        return;
    }

    if (request.senderIp == VirtualSubnet::kAnyIp
        || request.senderIp == request.targetIp
        || request.targetIp == m_guestIp)
    {
        return;
    }

    reply.operation = P::kArpReply;
    reply.senderMac = VirtualSubnet::kGatewayMac;
    reply.senderIp  = request.targetIp;
    reply.targetMac = request.senderMac;
    reply.targetIp  = request.senderIp;

    payload = P::BuildArp (reply);
    QueueFrame (P::BuildEthernet (request.senderMac, VirtualSubnet::kGatewayMac, P::kEtherTypeArp, payload));
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::OnIpv4
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::OnIpv4 (
    const EthernetFrame  & frame,
    uint64_t               now)
{
    Ipv4Packet  packet;
    TcpSegment  segment;



    if (!P::TryParseIpv4 (frame.payload, packet))
    {
        return;
    }

    if (packet.source != VirtualSubnet::kAnyIp)
    {
        m_guestIp = packet.source;
    }

    switch (packet.protocol)
    {
        case P::kProtocolIcmp:
            OnIcmp (packet);
            break;

        case P::kProtocolUdp:
            OnUdp (packet, now);
            break;

        case P::kProtocolTcp:
            if (P::TryParseTcp (packet, segment))
            {
                m_tcp.OnGuestSegment (segment, now);
            }

            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::OnIcmp
//
//  Echo requests to the gateway get echo replies; nothing else is relayed.
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::OnIcmp (const Ipv4Packet & packet)
{
    std::vector<Byte>  reply;



    if (packet.destination != VirtualSubnet::kGatewayIp
        || packet.payload.size() < P::kIcmpHeader
        || packet.payload[0] != P::kIcmpEchoRequest
        || P::Checksum (packet.payload) != 0)
    {
        return;
    }

    reply = P::BuildIcmpEchoReply (packet.payload);
    QueueIpv4 (VirtualSubnet::kGatewayIp, packet.source, P::kProtocolIcmp, reply);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::OnUdp
//
//  DHCP goes to the server here, whatever its destination address; the
//  reply is broadcast, since the guest has no address to unicast it to yet.
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::OnUdp (
    const Ipv4Packet  & packet,
    uint64_t            now)
{
    UdpDatagram        datagram;
    std::vector<Byte>  reply;
    std::vector<Byte>  udp;



    if (!P::TryParseUdp (packet, datagram))
    {
        return;
    }

    if (datagram.destinationPort != DhcpServer::kServerPort)
    {
        m_udp.SendFromGuest (datagram.sourcePort, packet.destination, datagram.destinationPort, datagram.payload, now);
        return;
    }

    reply = m_dhcp.Process (datagram.payload);

    if (reply.empty())
    {
        return;
    }

    udp = P::BuildUdp (VirtualSubnet::kGatewayIp, VirtualSubnet::kBroadcastIp, DhcpServer::kServerPort, DhcpServer::kClientPort, reply);
    QueueFrame (P::BuildEthernet (P::kBroadcastMac,
                                  VirtualSubnet::kGatewayMac,
                                  P::kEtherTypeIpv4,
                                  P::BuildIpv4 (VirtualSubnet::kGatewayIp, VirtualSubnet::kBroadcastIp, P::kProtocolUdp, m_nextIpId++, udp)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::SendUdpToGuest
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::SendUdpToGuest (
    uint32_t                sourceIp,
    uint16_t                sourcePort,
    uint16_t                guestPort,
    std::span<const Byte>   payload)
{
    std::vector<Byte>  udp = P::BuildUdp (sourceIp, m_guestIp, sourcePort, guestPort, payload);



    QueueIpv4 (sourceIp, m_guestIp, P::kProtocolUdp, udp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::SendTcpToGuest
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::SendTcpToGuest (const TcpSegment & segment)
{
    std::vector<Byte>  tcp = P::BuildTcp (segment);



    QueueIpv4 (segment.sourceIp, segment.destinationIp, P::kProtocolTcp, tcp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::QueueIpv4
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::QueueIpv4 (
    uint32_t                source,
    uint32_t                destination,
    Byte                    protocol,
    std::span<const Byte>   payload)
{
    std::vector<Byte>  packet = P::BuildIpv4 (source, destination, protocol, m_nextIpId++, payload);



    QueueFrame (P::BuildEthernet (m_guestMac, VirtualSubnet::kGatewayMac, P::kEtherTypeIpv4, packet));
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::QueueFrame
//
//  A full queue drops the frame: the guest is not reading, and TCP resends
//  while UDP tolerates the loss.
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::QueueFrame (std::vector<Byte> frame)
{
    if (m_outbound.size() < kMaxQueuedFrames)
    {
        m_outbound.push_back (std::move (frame));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::Pump
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::Pump (
    uint64_t  now,
    bool      guestReady)
{
    m_forwarder.Pump (now, guestReady);
    m_udp.Pump (now);
    m_tcp.Pump (now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::Reset
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::Reset()
{
    m_tcp.CloseAll();
    m_udp.CloseFlows();
    m_outbound.clear();

    m_guestMac = P::kBroadcastMac;
    m_guestIp  = VirtualSubnet::kGuestIp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::Rewind
//
//  The frames queued now are dropped where they are TCP, which the relays
//  send again from the point.
//
////////////////////////////////////////////////////////////////////////////////

void VirtualNetwork::Rewind (
    std::span<const std::span<const Byte>>   toGuest,
    size_t                                   firstToGuestAfter,
    std::span<const std::span<const Byte>>   fromGuest,
    size_t                                   firstFromGuestAfter,
    uint64_t                                 now)
{
    std::deque<std::vector<Byte>>  queued;
    TcpSegment                     segment;
    size_t                         i       = 0;



    for (i = 0; i < fromGuest.size(); i++)
    {
        if (TryParseTcpFrame (fromGuest[i], segment))
        {
            m_tcp.NoteRewindSegment (segment, false, i >= firstFromGuestAfter);
        }
    }

    for (i = 0; i < toGuest.size(); i++)
    {
        if (TryParseTcpFrame (toGuest[i], segment))
        {
            m_tcp.NoteRewindSegment (segment, true, i >= firstToGuestAfter);
        }
        else if (i >= firstToGuestAfter && IsRelayedUdp (toGuest[i]))
        {
            queued.emplace_back (toGuest[i].begin(), toGuest[i].end());
        }
    }

    m_tcp.Rewind (now);
    m_udp.Rewind (now);

    for (std::vector<Byte> & frame : m_outbound)
    {
        if (!TryParseTcpFrame (frame, segment))
        {
            queued.push_back (std::move (frame));
        }
    }

    while (queued.size() > kMaxQueuedFrames)
    {
        queued.pop_back();
    }

    m_outbound = std::move (queued);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::TryParseTcpFrame
//
////////////////////////////////////////////////////////////////////////////////

bool VirtualNetwork::TryParseTcpFrame (
    std::span<const Byte>   frame,
    TcpSegment            & out)
{
    EthernetFrame  ethernet;
    Ipv4Packet     packet;



    return P::TryParseEthernet (frame, ethernet)
        && ethernet.etherType == P::kEtherTypeIpv4
        && P::TryParseIpv4 (ethernet.payload, packet)
        && packet.protocol == P::kProtocolTcp
        && P::TryParseTcp (packet, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::IsRelayedUdp
//
//  A datagram from the network, not a reply from the gateway's DHCP
//  server or DNS relay, which answer requests the guest makes again.
//
////////////////////////////////////////////////////////////////////////////////

bool VirtualNetwork::IsRelayedUdp (std::span<const Byte> frame)
{
    EthernetFrame  ethernet;
    Ipv4Packet     packet;
    UdpDatagram    datagram;



    if (!P::TryParseEthernet (frame, ethernet)
        || ethernet.etherType != P::kEtherTypeIpv4
        || !P::TryParseIpv4 (ethernet.payload, packet)
        || packet.protocol != P::kProtocolUdp
        || !P::TryParseUdp (packet, datagram))
    {
        return false;
    }

    return !(packet.source == VirtualSubnet::kGatewayIp && datagram.sourcePort == DhcpServer::kServerPort)
        && packet.source != VirtualSubnet::kDnsIp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VirtualNetwork::IsActive
//
////////////////////////////////////////////////////////////////////////////////

bool VirtualNetwork::IsActive() const
{
    return m_udp.IsActive() || m_tcp.IsActive() || m_forwarder.IsActive() || !m_outbound.empty();
}
