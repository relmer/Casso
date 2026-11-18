#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"
#include "FakeHostSocket.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork
//
//  IHostNetwork for unit tests. No Winsock: every socket is a FakeSocketState,
//  and the test acts as the far side of each -- completing or failing
//  connects, offering inbound connections, pushing bytes and datagrams, and
//  reading what the code under test sent.
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostNetwork : public IHostNetwork
{
public:

    static constexpr uint16_t  kFirstEphemeralPort = 49152;
    static constexpr uint32_t  kLoopback           = 0x7F000001;

    HRESULT  CreateSocket (HostSocketKind kind, std::unique_ptr<IHostSocket> & outSocket) override;
    HRESULT  GetDnsServer (uint32_t & outIpv4)                                            override;

    // Every socket created so far, live or destroyed, in creation order.
    const std::vector<std::shared_ptr<FakeSocketState>> & GetSockets() const { return m_sockets; }

    // The most recent live socket of `kind` bound to `localPort`, or the most
    // recent one connected to `remote`; null when none matches.
    std::shared_ptr<FakeSocketState>  FindBound     (HostSocketKind kind, uint16_t localPort) const;
    std::shared_ptr<FakeSocketState>  FindConnected (const HostEndpoint & remote) const;
    size_t                            CountLive     () const;

    // A connection arriving at a listening socket.
    std::shared_ptr<FakeSocketState>  OfferConnection (const std::shared_ptr<FakeSocketState> & listener,
                                                       const HostEndpoint                     & peer);

    // A port some other program on the PC has bound.
    void  OccupyPort      (HostSocketKind kind, uint16_t port) { m_occupied.insert ({ kind, port }); }
    void  ReleasePort     (HostSocketKind kind, uint16_t port) { m_occupied.erase ({ kind, port }); }
    void  SetDnsServer    (uint32_t ipv4)                     { m_dnsServer = ipv4; }
    void  FailCreate      (bool fail)                         { m_failCreate = fail; }

    bool      IsPortInUse           (HostSocketKind kind, uint16_t port) const;
    uint16_t  AllocateEphemeralPort ();

private:

    std::vector<std::shared_ptr<FakeSocketState>>   m_sockets;
    std::set<std::pair<HostSocketKind, uint16_t>>   m_occupied;
    uint32_t                                        m_dnsServer     = 0;
    bool                                            m_failCreate    = false;
    uint16_t                                        m_nextEphemeral = kFirstEphemeralPort;
};
