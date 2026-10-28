#pragma once

#include "Pch.h"

#include "Devices/Serial/TelnetCodec.h"
#include "Seams/IHostNetwork.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TcpSerialLink
//
//  One TCP connection for a serial attachment, over IHostNetwork's
//  non-blocking sockets, advanced only by Poll: a connect out, which looks
//  up the host first, or a listening port that takes one connection at a
//  time and closes any other at once. Data to send is queued and sent as the
//  socket takes it; data received is read up to the caller's limit, so the
//  rest waits in the PC's socket buffer. With Telnet on, both directions go
//  through a TelnetCodec, whose answers to the remote end's negotiation are
//  sent with the data.
//
//  Emulation thread only.
//
////////////////////////////////////////////////////////////////////////////////

class TcpSerialLink
{
public:
    enum class State
    {
        Idle,
        LookingUp,
        Connecting,
        Connected,
    };

    //  What one poll did. A connection opens as isConnected when this end
    //  connected out and as isAccepted when it came in; isFailed is a connect
    //  that did not open, the lookup's failure when isLookupFailure; isClosed
    //  is an open connection that ended. isBusy is any byte sent or received.
    struct PollResult
    {
        bool     isConnected     = false;
        bool     isAccepted      = false;
        bool     isFailed        = false;
        bool     isLookupFailure = false;
        bool     isClosed        = false;
        bool     isBusy          = false;
        HRESULT  failure         = S_OK;
    };

    explicit TcpSerialLink (IHostNetwork * network) : m_network (network) {}

    void     SetTelnet     (bool isOn) { m_isTelnet = isOn; }
    HRESULT  Connect       (const std::string & host, uint16_t port);
    HRESULT  Listen        (uint16_t port);
    void     StopListening ();
    void     Close         ();
    void     Send          (std::span<const Byte> data);
    void     Poll          (size_t receiveLimit, std::vector<Byte> & outData, PollResult & outResult);

    State                GetState    () const { return m_state; }
    bool                 IsConnected () const { return m_state == State::Connected; }
    bool                 IsListening () const { return m_listener != nullptr; }
    const std::string  & GetPeer     () const { return m_peer; }

private:
    static constexpr size_t  kReadChunk = 512;

    static std::string  FormatEndpoint (const HostEndpoint & endpoint);

    void  PollLookup     (PollResult & outResult);
    void  PollConnect    (PollResult & outResult);
    void  PollListener   (PollResult & outResult);
    void  PollReceive    (size_t receiveLimit, std::vector<Byte> & outData, PollResult & outResult);
    void  PollSend       (PollResult & outResult);
    void  OpenConnection (std::unique_ptr<IHostSocket> socket, const std::string & peer);
    void  Fail           (HRESULT hr, bool isLookup, PollResult & outResult);
    void  EndConnection  (PollResult & outResult);

    IHostNetwork                      * m_network  = nullptr;
    State                               m_state    = State::Idle;
    std::unique_ptr<IHostNameLookup>    m_lookup;
    std::unique_ptr<IHostSocket>        m_socket;
    std::unique_ptr<IHostSocket>        m_listener;
    std::string                         m_host;
    uint16_t                            m_port     = 0;
    std::string                         m_peer;
    bool                                m_isTelnet = false;
    TelnetCodec                         m_telnet;
    std::vector<Byte>                   m_outgoing;
};
