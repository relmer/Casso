#include "Pch.h"

#include "Devices/Network/UdpNat.h"
#include "Devices/Network/IGuestPacketSink.h"
#include "Devices/Network/VirtualSubnet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::SendFromGuest
//
//  A datagram that cannot be sent -- no host network, no DNS server for a
//  DNS query, no socket -- is dropped, as UDP permits.
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::SendFromGuest (
    uint16_t                guestPort,
    uint32_t                destinationIp,
    uint16_t                destinationPort,
    std::span<const Byte>   payload,
    uint64_t                now)
{
    HRESULT        hr     = S_OK;
    HostEndpoint   target;
    Flow         * flow   = nullptr;
    bool           mapped = false;



    CBR (m_network != nullptr);

    mapped = TryMapDestination (destinationIp, destinationPort, target);
    CBR (mapped);

    flow = FindOrCreateFlow (guestPort, now);
    CBR (flow != nullptr);

    hr = flow->socket->SendTo (target, payload);
    CHR (hr);

    flow->peers.insert (PackEndpoint (target));
    flow->lastActivity = now;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::TryMapDestination
//
////////////////////////////////////////////////////////////////////////////////

bool UdpNat::TryMapDestination (
    uint32_t        destinationIp,
    uint16_t        destinationPort,
    HostEndpoint  & outHost)
{
    HRESULT   hr        = S_OK;
    uint32_t  dnsServer = 0;
    bool      mapped    = false;



    outHost = { VirtualSubnet::GuestToHost (destinationIp), destinationPort };
    mapped  = destinationIp != VirtualSubnet::kDnsIp;
    BAIL_OUT_IF (mapped, S_OK);

    CBR (destinationPort == VirtualSubnet::kDnsPort);

    hr = m_network->GetDnsServer (dnsServer);
    CHR (hr);

    m_dnsServer = dnsServer;
    outHost     = { dnsServer, VirtualSubnet::kDnsPort };
    mapped      = true;

Error:
    return mapped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::FindOrCreateFlow
//
//  The same port number as the guest's when the PC has it free, otherwise
//  an ephemeral one.
//
//  The socket binds the any-address even with lanAccess off, unlike
//  PortForwarder's. A flow is the guest's outbound path to the network, so a
//  loopback bind cuts the Apple off from everything beyond the PC. A port
//  forward's socket is inbound only, so it binds loopback until lanAccess is
//  on. Here IsAllowed drops datagrams from senders the guest has not sent to.
//
////////////////////////////////////////////////////////////////////////////////

UdpNat::Flow * UdpNat::FindOrCreateFlow (
    uint16_t  guestPort,
    uint64_t  now)
{
    HRESULT                        hr     = S_OK;
    std::unique_ptr<IHostSocket>   socket;
    Flow                         * flow   = nullptr;
    auto                           found  = m_flows.find (guestPort);
    bool                           same   = false;



    if (found != m_flows.end())
    {
        flow = &found->second;
    }

    BAIL_OUT_IF (flow != nullptr, S_OK);

    hr = m_network->CreateSocket (HostSocketKind::Udp, socket);
    CHR (hr);

    hr = socket->Bind ({ VirtualSubnet::kAnyIp, guestPort }, true);
    same = SUCCEEDED (hr);

    if (FAILED (hr))
    {
        hr = socket->Bind ({ VirtualSubnet::kAnyIp, 0 }, true);
        CHR (hr);
    }

    flow               = &m_flows[guestPort];
    flow->socket       = std::move (socket);
    flow->lastActivity = now;
    flow->samePort     = same;

Error:
    return flow;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::AddForward
//
//  A forward replaces any flow the guest already has on that port, so the
//  guest's replies are sent from the forwarded host port.
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::AddForward (
    uint16_t                        guestPort,
    std::unique_ptr<IHostSocket>    socket)
{
    Flow  & flow = m_flows[guestPort];



    flow.socket    = std::move (socket);
    flow.isForward = true;
    flow.peers.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::Pump
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::Pump (uint64_t now)
{
    auto  it = m_flows.begin();



    while (it != m_flows.end())
    {
        DrainFlow (it->first, it->second, now);

        if (!it->second.isForward && now - it->second.lastActivity > kIdleCycles)
        {
            it = m_flows.erase (it);
        }
        else
        {
            ++it;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::DrainFlow
//
//  At most kMaxPerPump datagrams per pump, so one busy peer cannot bury the
//  guest's frame queue between two of its reads.
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::DrainFlow (
    uint16_t   guestPort,
    Flow     & flow,
    uint64_t   now)
{
    constexpr int   kMaxPerPump = 16;
    HRESULT         hr          = S_OK;
    size_t          received    = 0;
    HostEndpoint    from;
    uint32_t        sourceIp    = 0;
    int             count       = 0;



    m_buffer.resize (kMaxDatagram);

    for (count = 0; count < kMaxPerPump; count++)
    {
        hr = flow.socket->ReceiveFrom (m_buffer, received, from);

        if (FAILED (hr) || received == 0)
        {
            break;
        }

        if (!IsAllowed (flow, from))
        {
            continue;
        }

        sourceIp = (from.ipv4 == m_dnsServer && from.port == VirtualSubnet::kDnsPort)
                 ? VirtualSubnet::kDnsIp
                 : VirtualSubnet::HostToGuest (from.ipv4);

        m_sink.SendUdpToGuest (sourceIp, from.port, guestPort, std::span<const Byte> (m_buffer).first (received));
        flow.lastActivity = now;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::IsAllowed
//
////////////////////////////////////////////////////////////////////////////////

bool UdpNat::IsAllowed (
    const Flow          & flow,
    const HostEndpoint  & from) const
{
    return m_lanAccess
        || VirtualSubnet::IsLoopback (from.ipv4)
        || flow.peers.contains (PackEndpoint (from));
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::GetInboundPorts
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::GetInboundPorts (std::vector<FirewallPort> & outPorts) const
{
    for (const auto & [port, flow] : m_flows)
    {
        if (flow.samePort && !flow.isForward)
        {
            outPorts.push_back ({ HostSocketKind::Udp, port });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::CloseFlows
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::CloseFlows()
{
    std::erase_if (m_flows, [] (const auto & entry) { return !entry.second.isForward; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::CloseAll
//
////////////////////////////////////////////////////////////////////////////////

void UdpNat::CloseAll()
{
    m_flows.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat::PackEndpoint
//
////////////////////////////////////////////////////////////////////////////////

uint64_t UdpNat::PackEndpoint (const HostEndpoint & endpoint)
{
    return ((uint64_t) endpoint.ipv4 << 16) | endpoint.port;
}
