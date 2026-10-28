#include "Pch.h"

#include "Devices/Serial/TcpSerialLink.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::Connect
//
//  Closes any connection and starts looking up the host; Poll connects once
//  the lookup is done.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TcpSerialLink::Connect (
    const std::string  & host,
    uint16_t             port)
{
    HRESULT  hr = S_OK;



    Close();

    CBREx (m_network != nullptr, E_NOT_VALID_STATE);

    hr = m_network->LookUpHost (host, m_lookup);
    CHR (hr);

    m_host  = host;
    m_port  = port;
    m_peer  = std::format ("{}:{}", host, port);
    m_state = State::LookingUp;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::Listen
//
//  A port another program has bound fails with WSAEADDRINUSE.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TcpSerialLink::Listen (uint16_t port)
{
    HRESULT                       hr       = S_OK;
    std::unique_ptr<IHostSocket>  listener;



    StopListening();

    CBREx (m_network != nullptr, E_NOT_VALID_STATE);

    hr = m_network->CreateSocket (HostSocketKind::Tcp, listener);
    CHR (hr);

    hr = listener->Bind ({ 0, port }, false);
    CHR (hr);

    hr = listener->Listen();
    CHR (hr);

    m_listener = std::move (listener);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::StopListening
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::StopListening()
{
    m_listener.reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::Close
//
//  Ends the connection or the connect under way; a listening port goes on
//  listening.
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::Close()
{
    m_lookup.reset();
    m_socket.reset();
    m_outgoing.clear();
    m_telnet.Reset();
    m_state = State::Idle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::Send
//
//  Queued for the next poll; dropped when no connection is open.
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::Send (std::span<const Byte> data)
{
    if (m_state != State::Connected)
    {
        return;
    }

    if (m_isTelnet)
    {
        m_telnet.Encode (data, m_outgoing);
    }
    else
    {
        m_outgoing.insert (m_outgoing.end(), data.begin(), data.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::Poll
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::Poll (
    size_t                receiveLimit,
    std::vector<Byte>   & outData,
    PollResult          & outResult)
{
    outResult = PollResult();

    switch (m_state)
    {
    case State::LookingUp:
        PollLookup (outResult);
        break;

    case State::Connecting:
        PollConnect (outResult);
        break;

    case State::Connected:
        PollReceive (receiveLimit, outData, outResult);
        break;

    case State::Idle:
        break;
    }

    PollListener (outResult);
    PollSend (outResult);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::PollLookup
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::PollLookup (PollResult & outResult)
{
    HRESULT   hr     = S_OK;
    HRESULT   result = S_OK;
    uint32_t  ipv4   = 0;
    bool      isDone = m_lookup->TryGetResult (result, ipv4);



    BAIL_OUT_IF (!isDone, S_OK);

    m_lookup.reset();
    CHRF (result, Fail (result, true, outResult));

    hr = m_network->CreateSocket (HostSocketKind::Tcp, m_socket);
    CHRF (hr, Fail (hr, false, outResult));

    hr = m_socket->Connect ({ ipv4, m_port });
    CHRF (hr, Fail (hr, false, outResult));

    m_state = State::Connecting;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::PollConnect
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::PollConnect (PollResult & outResult)
{
    HRESULT              hr        = S_OK;
    HostSocketReadiness  readiness = {};



    hr = m_socket->Poll (readiness);
    CHRF (hr, Fail (hr, false, outResult));

    CBRF (!readiness.failed, Fail (HRESULT_FROM_WIN32 (WSAECONNREFUSED), false, outResult));
    BAIL_OUT_IF (!readiness.writable, S_OK);

    m_state               = State::Connected;
    outResult.isConnected = true;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::PollListener
//
//  A connection arriving while none is open is taken; any other is closed at
//  once, including one arriving while this end is connecting out.
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::PollListener (PollResult & outResult)
{
    HRESULT                       hr         = S_OK;
    std::unique_ptr<IHostSocket>  accepted;
    HostEndpoint                  peer;
    bool                          wouldBlock = false;



    BAIL_OUT_IF (m_listener == nullptr, S_OK);

    for (;;)
    {
        accepted.reset();

        hr = m_listener->Accept (accepted, peer, wouldBlock);
        CHR (hr);

        if (wouldBlock || accepted == nullptr)
        {
            break;
        }

        if (m_state == State::Idle && !outResult.isClosed)
        {
            OpenConnection (std::move (accepted), FormatEndpoint (peer));
            outResult.isAccepted = true;
        }
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::PollReceive
//
//  Reads up to the limit. An orderly close or an error ends the connection
//  once the bytes before it are read.
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::PollReceive (
    size_t                receiveLimit,
    std::vector<Byte>   & outData,
    PollResult          & outResult)
{
    HRESULT                       hr       = S_OK;
    std::array<Byte, kReadChunk>  buffer   = {};
    size_t                        received = 0;
    bool                          isEof    = false;
    size_t                        wanted   = std::min (receiveLimit, buffer.size());
    std::vector<Byte>             replies;



    BAIL_OUT_IF (wanted == 0, S_OK);

    hr = m_socket->Receive (std::span<Byte> (buffer.data(), wanted), received, isEof);
    CHRF (hr, EndConnection (outResult));
    CBRF (!isEof, EndConnection (outResult));

    BAIL_OUT_IF (received == 0, S_OK);

    outResult.isBusy = true;

    if (m_isTelnet)
    {
        m_telnet.Decode (std::span<const Byte> (buffer.data(), received), outData, replies);
        m_outgoing.insert (m_outgoing.end(), replies.begin(), replies.end());
    }
    else
    {
        outData.insert (outData.end(), buffer.begin(), buffer.begin() + received);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::PollSend
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::PollSend (PollResult & outResult)
{
    HRESULT  hr        = S_OK;
    size_t   sent      = 0;
    bool     isWaiting = m_state == State::Connected && !m_outgoing.empty();



    BAIL_OUT_IF (!isWaiting, S_OK);

    hr = m_socket->Send (m_outgoing, sent);
    CHRF (hr, EndConnection (outResult));

    m_outgoing.erase (m_outgoing.begin(), m_outgoing.begin() + sent);
    outResult.isBusy = outResult.isBusy || sent != 0;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::OpenConnection
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::OpenConnection (
    std::unique_ptr<IHostSocket>    socket,
    const std::string             & peer)
{
    m_socket = std::move (socket);
    m_peer   = peer;
    m_state  = State::Connected;
    m_outgoing.clear();
    m_telnet.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::Fail
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::Fail (
    HRESULT        hr,
    bool           isLookup,
    PollResult   & outResult)
{
    Close();

    outResult.isFailed        = true;
    outResult.isLookupFailure = isLookup;
    outResult.failure         = hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::EndConnection
//
////////////////////////////////////////////////////////////////////////////////

void TcpSerialLink::EndConnection (PollResult & outResult)
{
    Close();

    outResult.isClosed = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink::FormatEndpoint
//
////////////////////////////////////////////////////////////////////////////////

std::string TcpSerialLink::FormatEndpoint (const HostEndpoint & endpoint)
{
    constexpr int  kOctetBits = 8;
    constexpr int  kOctets    = 4;
    std::string    address;
    int            i          = 0;



    for (i = kOctets - 1; i >= 0; i--)
    {
        address += std::format ("{}{}", (Byte) (endpoint.ipv4 >> (i * kOctetBits)), (i > 0) ? "." : "");
    }

    return std::format ("{}:{}", address, endpoint.port);
}