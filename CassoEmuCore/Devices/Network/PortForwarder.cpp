#include "Pch.h"

#include "Devices/Network/PortForwarder.h"
#include "Devices/Network/TcpNat.h"
#include "Devices/Network/UdpNat.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Net/ListenGate.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::Start
//
////////////////////////////////////////////////////////////////////////////////

void PortForwarder::Start (
    IHostNetwork                    * network,
    const std::vector<PortForward>  & forwards,
    bool                              lanAccess)
{
    HRESULT  hr = S_OK;



    Stop();

    m_network   = network;
    m_lanAccess = lanAccess;

    if (network == nullptr)
    {
        return;
    }

    for (const PortForward & forward : forwards)
    {
        if (IsDeferred (forward))
        {
            m_deferred.push_back (forward);
            continue;
        }

        hr = OpenForward (network, forward, lanAccess);

        if (FAILED (hr))
        {
            m_errors.push_back (DescribeOpenFailure (forward));

        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::DescribeOpenFailure
//
////////////////////////////////////////////////////////////////////////////////

std::string PortForwarder::DescribeOpenFailure (const PortForward & forward)
{
    return std::format ("The {} port forward from host port {} to guest port {} could not be opened.",
                        forward.protocol == PortForwardProtocol::Tcp ? "TCP" : "UDP",
                        forward.hostPort,
                        forward.guestPort);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::OpenForward
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PortForwarder::OpenForward (
    IHostNetwork        * network,
    const PortForward   & forward,
    bool                  lanAccess)
{
    HRESULT                        hr      = S_OK;
    std::unique_ptr<IHostSocket>   socket;
    uint32_t                       address = lanAccess ? VirtualSubnet::kAnyIp : VirtualSubnet::kLoopbackIp;
    bool                           isTcp   = forward.protocol == PortForwardProtocol::Tcp;



    hr = network->CreateSocket (isTcp ? HostSocketKind::Tcp : HostSocketKind::Udp, socket);
    CHR (hr);

    hr = socket->Bind ({ address, forward.hostPort }, false);
    CHR (hr);

    if (isTcp)
    {
        hr = socket->Listen();
        CHR (hr);

        m_listeners.push_back ({ std::move (socket), forward.guestPort });
    }
    else
    {
        m_udp.AddForward (forward.guestPort, std::move (socket));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::Stop
//
////////////////////////////////////////////////////////////////////////////////

void PortForwarder::Stop()
{
    m_listeners.clear();
    m_errors.clear();
    m_deferred.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::GetDeferredPorts
//
////////////////////////////////////////////////////////////////////////////////

void PortForwarder::GetDeferredPorts (std::vector<FirewallPort> & outPorts) const
{
    for (const PortForward & forward : m_deferred)
    {
        outPorts.push_back ({ forward.protocol == PortForwardProtocol::Tcp ? HostSocketKind::Tcp : HostSocketKind::Udp, forward.hostPort });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::IsDeferred
//
//  With LAN access off a forward binds 127.0.0.1, which the firewall does
//  not filter, so only a forward on the PC's addresses is deferred.
//
////////////////////////////////////////////////////////////////////////////////

bool PortForwarder::IsDeferred (const PortForward & forward) const
{
    HostSocketKind  protocol = forward.protocol == PortForwardProtocol::Tcp ? HostSocketKind::Tcp : HostSocketKind::Udp;



    return m_lanAccess && m_gate != nullptr && !m_gate->Admits (protocol, forward.hostPort);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::OpenDeferred
//
//  Each deferred forward whose port the gate now admits opens, and one that
//  cannot be bound becomes an error, as at Start.
//
////////////////////////////////////////////////////////////////////////////////

void PortForwarder::OpenDeferred()
{
    HRESULT                   hr    = S_OK;
    std::vector<PortForward>  waiting;



    for (const PortForward & forward : m_deferred)
    {
        if (IsDeferred (forward))
        {
            waiting.push_back (forward);
            continue;
        }

        hr = OpenForward (m_network, forward, m_lanAccess);

        if (FAILED (hr))
        {
            m_errors.push_back (DescribeOpenFailure (forward));

        }
    }

    m_deferred = std::move (waiting);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder::Pump
//
////////////////////////////////////////////////////////////////////////////////

void PortForwarder::Pump (
    uint64_t  now,
    bool      guestReady)
{
    HRESULT                        hr         = S_OK;
    std::unique_ptr<IHostSocket>   accepted;
    HostEndpoint                   peer;
    bool                           wouldBlock = false;



    //  The verdicts may have changed, so a deferred forward may open now.
    if (!m_deferred.empty() && m_gate != nullptr && m_gate->GetVersion() != m_gateVersion)
    {
        m_gateVersion = m_gate->GetVersion();
        OpenDeferred();
    }

    for (Listener & listener : m_listeners)
    {
        hr = listener.socket->Accept (accepted, peer, wouldBlock);

        if (FAILED (hr) || wouldBlock)
        {
            continue;
        }

        if (guestReady)
        {
            m_tcp.AcceptInbound (std::move (accepted), peer, listener.guestPort, now);
        }

        accepted.reset();
    }
}
