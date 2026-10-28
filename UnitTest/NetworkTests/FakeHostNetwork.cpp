#include "Pch.h"

#include "FakeHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::CreateSocket
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostNetwork::CreateSocket (
    HostSocketKind                  kind,
    std::unique_ptr<IHostSocket>  & outSocket)
{
    HRESULT                           hr    = S_OK;
    std::shared_ptr<FakeSocketState>  state = std::make_shared<FakeSocketState>();



    CBREx (!m_failCreate, HRESULT_FROM_WIN32 (WSAENETDOWN));

    state->kind = kind;
    m_sockets.push_back (state);

    outSocket = std::make_unique<FakeHostSocket> (*this, state);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::GetDnsServer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostNetwork::GetDnsServer (uint32_t & outIpv4)
{
    HRESULT  hr = S_OK;



    outIpv4 = m_dnsServer;
    CBREx (m_dnsServer != 0, E_NOT_SET);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::LookUpHost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostNetwork::LookUpHost (
    const std::string                 & host,
    std::unique_ptr<IHostNameLookup>  & outLookup)
{
    std::shared_ptr<FakeLookupState>  state = std::make_shared<FakeLookupState>();



    state->host = host;
    m_lookups.push_back (state);

    if (!m_holdLookups)
    {
        Complete (*state);
    }

    outLookup = std::make_unique<FakeHostNameLookup> (state);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::CompleteLookups
//
////////////////////////////////////////////////////////////////////////////////

void FakeHostNetwork::CompleteLookups()
{
    for (const auto & state : m_lookups)
    {
        if (!state->isDone)
        {
            Complete (*state);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::Complete
//
////////////////////////////////////////////////////////////////////////////////

void FakeHostNetwork::Complete (FakeLookupState & lookup) const
{
    auto  found = m_addresses.find (lookup.host);



    lookup.isDone = true;
    lookup.hr     = (found != m_addresses.end()) ? S_OK : HRESULT_FROM_WIN32 (WSAHOST_NOT_FOUND);
    lookup.ipv4   = (found != m_addresses.end()) ? found->second : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::FindBound
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<FakeSocketState> FakeHostNetwork::FindBound (
    HostSocketKind  kind,
    uint16_t        localPort) const
{
    std::shared_ptr<FakeSocketState>  found;



    for (const auto & state : m_sockets)
    {
        if (!state->destroyed && state->kind == kind && state->bound && state->local.port == localPort)
        {
            found = state;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::FindConnected
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<FakeSocketState> FakeHostNetwork::FindConnected (const HostEndpoint & remote) const
{
    std::shared_ptr<FakeSocketState>  found;



    for (const auto & state : m_sockets)
    {
        if (!state->destroyed
            && state->connect != FakeSocketState::ConnectOutcome::None
            && state->remote == remote)
        {
            found = state;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::CountLive
//
////////////////////////////////////////////////////////////////////////////////

size_t FakeHostNetwork::CountLive() const
{
    size_t  live = 0;



    for (const auto & state : m_sockets)
    {
        if (!state->destroyed)
        {
            live++;
        }
    }

    return live;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::OfferConnection
//
//  The returned state is the PC side of the new connection; the test pushes
//  bytes into it and reads what the code under test sends back.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<FakeSocketState> FakeHostNetwork::OfferConnection (
    const std::shared_ptr<FakeSocketState>  & listener,
    const HostEndpoint                      & peer)
{
    std::shared_ptr<FakeSocketState>  state = std::make_shared<FakeSocketState>();



    state->kind     = HostSocketKind::Tcp;
    state->bound    = true;
    state->accepted = true;
    state->local    = listener->local;
    state->remote   = peer;
    state->connect  = FakeSocketState::ConnectOutcome::Accepted;

    m_sockets.push_back (state);
    listener->pendingAccepts.push_back ({ state, peer });

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::IsPortInUse
//
////////////////////////////////////////////////////////////////////////////////

bool FakeHostNetwork::IsPortInUse (
    HostSocketKind  kind,
    uint16_t        port) const
{
    bool  inUse = m_occupied.contains ({ kind, port });



    for (const auto & state : m_sockets)
    {
        if (!state->destroyed && !state->accepted && state->bound && state->kind == kind && state->local.port == port)
        {
            inUse = true;
        }
    }

    return inUse;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostNetwork::AllocateEphemeralPort
//
////////////////////////////////////////////////////////////////////////////////

uint16_t FakeHostNetwork::AllocateEphemeralPort()
{
    return m_nextEphemeral++;
}
