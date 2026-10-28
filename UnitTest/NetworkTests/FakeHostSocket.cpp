#include "Pch.h"

#include "FakeHostSocket.h"
#include "FakeHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::FakeHostSocket
//
////////////////////////////////////////////////////////////////////////////////

FakeHostSocket::FakeHostSocket (
    FakeHostNetwork                   & network,
    std::shared_ptr<FakeSocketState>    state)
    : m_network (network),
      m_state   (std::move (state))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::~FakeHostSocket
//
////////////////////////////////////////////////////////////////////////////////

FakeHostSocket::~FakeHostSocket()
{
    m_state->destroyed = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Bind
//
//  Port 0 binds the next ephemeral port. Binding a port another live socket
//  of the same kind has bound, or one the test marked as taken, fails the way
//  Winsock does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Bind (
    const HostEndpoint  & local,
    bool                  allowBroadcast)
{
    HRESULT  hr    = S_OK;
    bool     inUse = false;



    m_state->local          = local;
    m_state->allowBroadcast = allowBroadcast;

    if (local.port == 0)
    {
        m_state->local.port = m_network.AllocateEphemeralPort();
    }
    else
    {
        inUse = m_network.IsPortInUse (m_state->kind, local.port);
        CBREx (!inUse, HRESULT_FROM_WIN32 (WSAEADDRINUSE));
    }

    m_state->bound = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::GetLocal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::GetLocal (HostEndpoint & outLocal) const
{
    outLocal = m_state->local;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Listen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Listen()
{
    HRESULT  hr = S_OK;



    CBREx (m_state->bound, HRESULT_FROM_WIN32 (WSAEINVAL));

    m_state->listening = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Accept
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Accept (
    std::unique_ptr<IHostSocket>  & outAccepted,
    HostEndpoint                  & outPeer,
    bool                          & outWouldBlock)
{
    HRESULT                         hr = S_OK;
    FakeSocketState::PendingAccept  next;



    m_network.NoteCall();

    outWouldBlock = m_state->pendingAccepts.empty();
    BAIL_OUT_IF (outWouldBlock, S_OK);

    next = m_state->pendingAccepts.front();
    m_state->pendingAccepts.pop_front();

    outAccepted = std::make_unique<FakeHostSocket> (m_network, next.state);
    outPeer     = next.peer;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Connect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Connect (const HostEndpoint & remote)
{
    m_state->remote  = remote;
    m_state->connect = FakeSocketState::ConnectOutcome::Pending;

    if (!m_state->bound)
    {
        m_state->local.port = m_network.AllocateEphemeralPort();
        m_state->bound      = true;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Send
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Send (
    std::span<const Byte>   bytes,
    size_t                & outSent)
{
    HRESULT  hr = S_OK;



    m_network.NoteCall();

    outSent = 0;

    CBREx (!m_state->resetByPeer, HRESULT_FROM_WIN32 (WSAECONNRESET));

    outSent = std::min (bytes.size(), m_state->sendLimit);

    m_state->sent.insert (m_state->sent.end(), bytes.begin(), bytes.begin() + outSent);

    if (m_state->sendLimit != SIZE_MAX)
    {
        m_state->sendLimit -= outSent;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Receive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Receive (
    std::span<Byte>   buffer,
    size_t          & outReceived,
    bool            & outEof)
{
    HRESULT  hr    = S_OK;
    size_t   count = 0;
    size_t   i     = 0;



    m_network.NoteCall();

    outReceived = 0;
    outEof      = false;

    CBREx (!m_state->resetByPeer, HRESULT_FROM_WIN32 (WSAECONNRESET));

    count = std::min (buffer.size(), m_state->inbound.size());

    for (i = 0; i < count; i++)
    {
        buffer[i] = m_state->inbound.front();
        m_state->inbound.pop_front();
    }

    outReceived = count;
    outEof      = (count == 0 && m_state->inboundEof);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::SendTo
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::SendTo (
    const HostEndpoint     & remote,
    std::span<const Byte>    bytes)
{
    FakeSocketState::Datagram  datagram;



    if (!m_state->bound)
    {
        m_state->local.port = m_network.AllocateEphemeralPort();
        m_state->bound      = true;
    }

    datagram.peer  = remote;
    datagram.bytes.assign (bytes.begin(), bytes.end());

    m_state->sentDatagrams.push_back (std::move (datagram));

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::ReceiveFrom
//
//  A datagram longer than the buffer is truncated, as Winsock does.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::ReceiveFrom (
    std::span<Byte>   buffer,
    size_t          & outReceived,
    HostEndpoint    & outFrom)
{
    HRESULT                    hr      = S_OK;
    bool                       isEmpty = false;
    FakeSocketState::Datagram  datagram;



    outReceived = 0;

    isEmpty = m_state->inboundDatagrams.empty();
    BAIL_OUT_IF (isEmpty, S_OK);

    datagram = std::move (m_state->inboundDatagrams.front());
    m_state->inboundDatagrams.pop_front();

    outReceived = std::min (buffer.size(), datagram.bytes.size());
    std::copy_n (datagram.bytes.begin(), outReceived, buffer.begin());
    outFrom = datagram.peer;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::ShutdownSend
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::ShutdownSend()
{
    m_state->shutdownSent = true;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket::Poll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FakeHostSocket::Poll (HostSocketReadiness & outReadiness)
{
    using Outcome = FakeSocketState::ConnectOutcome;

    const FakeSocketState  & state = *m_state;



    m_network.NoteCall();

    outReadiness = {};

    outReadiness.readable = !state.inbound.empty()
                         || state.inboundEof
                         || !state.inboundDatagrams.empty()
                         || !state.pendingAccepts.empty();

    outReadiness.writable = state.connect != Outcome::Pending
                         && state.connect != Outcome::Refused
                         && state.sendLimit != 0;

    outReadiness.failed   = state.connect == Outcome::Refused || state.resetByPeer;

    return S_OK;
}
