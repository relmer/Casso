#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"
#include "FakeHostNameLookup.h"
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
    HRESULT  LookUpHost   (const std::string & host, std::unique_ptr<IHostNameLookup> & outLookup) override;

    // Host names a lookup finds; any other fails as the PC's resolver fails
    // a name it cannot find. Held lookups stay pending until completed.
    void      SetHostAddress  (const std::string & host, uint32_t ipv4) { m_addresses[host] = ipv4; }
    void      HoldLookups     (bool hold)                               { m_holdLookups = hold; }
    void      CompleteLookups ();
    size_t    GetLookupCount  () const                                  { return m_lookups.size(); }

    // Calls made on the sockets' Accept, Send, Receive and Poll, the ones a
    // polled idle connection makes.
    void      NoteCall        ()                                        { m_calls++; }
    size_t    GetCallCount    () const                                  { return m_calls; }

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
    void  SetDnsServer    (uint32_t ipv4)                     { m_dnsServer = ipv4; }
    void  FailCreate      (bool fail)                         { m_failCreate = fail; }

    bool  IsPortInUse     (HostSocketKind kind, uint16_t port) const;
    uint16_t  AllocateEphemeralPort();

private:

    void  Complete (FakeLookupState & lookup) const;

    std::vector<std::shared_ptr<FakeSocketState>>   m_sockets;
    std::vector<std::shared_ptr<FakeLookupState>>   m_lookups;
    std::map<std::string, uint32_t>                 m_addresses;
    bool                                            m_holdLookups   = false;
    size_t                                          m_calls         = 0;
    std::set<std::pair<HostSocketKind, uint16_t>>   m_occupied;
    uint32_t                                        m_dnsServer     = 0;
    bool                                            m_failCreate    = false;
    uint16_t                                        m_nextEphemeral = kFirstEphemeralPort;
};
