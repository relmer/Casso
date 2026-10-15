#include "Pch.h"

#include "Devices/Network/PortForwarder.h"
#include "Devices/Network/TcpNat.h"
#include "Devices/Network/UdpNat.h"
#include "Devices/Network/VirtualSubnet.h"





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

    if (network == nullptr)
    {
        return;
    }

    for (const PortForward & forward : forwards)
    {
        hr = OpenForward (network, forward, lanAccess);

        if (FAILED (hr))
        {
            m_errors.push_back (std::format ("The {} port forward from host port {} to guest port {} could not be opened.",
                                             forward.protocol == PortForwardProtocol::Tcp ? "TCP" : "UDP",
                                             forward.hostPort,
                                             forward.guestPort));
        }
    }
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
