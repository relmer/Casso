#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"

class FakeHostNetwork;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeSocketState
//
//  Everything about one fake socket, shared between the IHostSocket the code
//  under test owns and the test. The test retains it after the code under test
//  destroys its socket, so `destroyed` can be asserted.
//
////////////////////////////////////////////////////////////////////////////////

struct FakeSocketState
{
    enum class ConnectOutcome
    {
        None,
        Pending,
        Accepted,
        Refused,
    };

    struct Datagram
    {
        HostEndpoint       peer;
        std::vector<Byte>  bytes;
    };

    struct PendingAccept
    {
        std::shared_ptr<FakeSocketState>  state;
        HostEndpoint                      peer;
    };

    HostSocketKind   kind            = HostSocketKind::Tcp;
    bool             bound           = false;
    bool             listening       = false;
    bool             allowBroadcast  = false;
    bool             shutdownSent    = false;
    bool             destroyed       = false;
    bool             accepted        = false;   // created by Accept
    HostEndpoint     local;
    HostEndpoint     remote;
    ConnectOutcome   connect         = ConnectOutcome::None;

    // TCP: what the peer sends, and what the code under test sent.
    std::deque<Byte>   inbound;
    bool               inboundEof    = false;
    bool               resetByPeer   = false;
    std::vector<Byte>  sent;
    size_t             sendLimit     = SIZE_MAX;   // bytes Send accepts before reporting a full buffer

    // UDP.
    std::deque<Datagram>   inboundDatagrams;
    std::vector<Datagram>  sentDatagrams;

    // Listener.
    std::deque<PendingAccept>  pendingAccepts;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostSocket
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostSocket : public IHostSocket
{
public:

    FakeHostSocket (FakeHostNetwork & network, std::shared_ptr<FakeSocketState> state);
    ~FakeHostSocket () override;

    HRESULT  Bind         (const HostEndpoint & local, bool allowBroadcast)          override;
    HRESULT  GetLocal     (HostEndpoint & outLocal) const                            override;
    HRESULT  Listen       ()                                                         override;
    HRESULT  Accept       (std::unique_ptr<IHostSocket>  & outAccepted,
                           HostEndpoint                  & outPeer,
                           bool                          & outWouldBlock)            override;
    HRESULT  Connect      (const HostEndpoint & remote)                              override;
    HRESULT  Send         (std::span<const Byte> bytes, size_t & outSent)            override;
    HRESULT  Receive      (std::span<Byte>  buffer,
                           size_t         & outReceived,
                           bool           & outEof)                                  override;
    HRESULT  SendTo       (const HostEndpoint & remote, std::span<const Byte> bytes) override;
    HRESULT  ReceiveFrom  (std::span<Byte>  buffer,
                           size_t         & outReceived,
                           HostEndpoint   & outFrom)                                 override;
    HRESULT  ShutdownSend ()                                                         override;
    HRESULT  Poll         (HostSocketReadiness & outReadiness)                       override;

private:

    FakeHostNetwork                   & m_network;
    std::shared_ptr<FakeSocketState>    m_state;
};
