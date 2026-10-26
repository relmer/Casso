#pragma once

#include "Pch.h"

#include "Devices/Network/IW5100Backend.h"
#include "Seams/IHostFirewall.h"
#include "Seams/IHostNetwork.h"

class VirtualNetwork;
class W5100;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge
//
//  The W5100's backend. Hardware TCP and UDP sockets map onto non-blocking
//  host sockets; socket 0 in MACRAW exchanges frames with the virtual
//  network; UDP sent to port 67 from a hardware socket goes to the virtual
//  network's DHCP server. Commands take effect at once where possible, and
//  Pump completes the rest: finishing connects and accepts, sending what the
//  host could not take yet, and moving received data into the RX buffers.
//
////////////////////////////////////////////////////////////////////////////////

class W5100SocketBridge : public IW5100Backend
{
public:

    //  A listen the PC could not open: the guest's port, and why.
    struct ListenFailure
    {
        uint16_t  port = 0;
        HRESULT   hr   = S_OK;
    };

    W5100SocketBridge (W5100 & chip, VirtualNetwork & network) : m_chip (chip), m_network (network) {}
    ~W5100SocketBridge () override;

    void  SetHostNetwork (IHostNetwork * network) { m_hostNetwork = network; }
    void  SetLanAccess   (bool lanAccess)         { m_lanAccess = lanAccess; }

    void  OnCommand      (int socket, Byte command) override;
    void  OnReset        ()                         override;

    bool  IsActive       () const;
    void  Pump           (uint64_t now);
    void  CloseAll       ();

    //  The PC ports the guest's sockets have, which other computers send to
    //  while LAN access is on: each listening TCP socket's, and each UDP
    //  socket's bound to its guest port.
    void  GetInboundPorts (std::vector<FirewallPort> & outPorts) const;

    //  The ports whose last listen failed, in port order. A port is removed
    //  from the list when a listen on it succeeds or the card is reset, so a
    //  guest that listens again every few seconds is one entry, not many.
    std::vector<ListenFailure>  GetFailedListens () const;

    //  A failed listen as a notice's sentence. `serialPort` is the serial
    //  port whose connector has the same TCP port, 0 for none, and `onCard`
    //  is whether that port is a Super Serial Card's.
    static std::string  DescribeListenFailure (const ListenFailure & failure, int serialPort, bool onCard);

private:

    static constexpr int     kSocketCount   = 4;
    static constexpr size_t  kReceiveBuffer = 65535;

    enum class Phase
    {
        Idle,
        Connecting,
        Listening,
        Established,
        PeerClosed,
        Udp,
        MacRaw,
    };

    struct Datagram
    {
        HostEndpoint       from;
        std::vector<Byte>  payload;
    };

    struct Slot
    {
        Phase                         phase       = Phase::Idle;
        std::unique_ptr<IHostSocket>  host;
        std::vector<Byte>             txQueue;      // copied from TX, not yet taken by the host
        bool                          sendPending = false;
        std::set<uint64_t>            udpPeers;     // packed endpoints sent to
        std::deque<Datagram>          udpPending;   // received, waiting for RX space
        uint16_t                      hostPort    = 0;   // the PC port a listener or UDP socket has; 0 for an ephemeral one
    };

    void  CloseSlot        (int socket);
    void  OnOpen           (int socket);
    void  OnConnect        (int socket);
    void  OnListen         (int socket);
    void  OnDisconnect     (int socket);
    void  OnSend           (int socket);
    void  SendTcp          (int socket);
    void  SendUdp          (int socket);
    void  SendMacRaw       (int socket, uint64_t now);
    void  Fail             (int socket, Byte interrupt);
    void  NoteListenFailure (uint16_t port, HRESULT hr);

    void  PumpSocket       (int socket, uint64_t now);
    void  PumpConnecting   (int socket);
    void  PumpListening    (int socket);
    void  PumpTcpTransfer  (int socket);
    void  PumpTcpReceive   (int socket);
    void  PumpUdpReceive   (int socket);
    void  PumpMacRaw       (int socket);
    bool  TryDeliverDatagram (int socket, const Datagram & datagram);

    static uint64_t  PackEndpoint (const HostEndpoint & endpoint);

    W5100                         & m_chip;
    VirtualNetwork                & m_network;
    IHostNetwork                  * m_hostNetwork = nullptr;
    bool                            m_lanAccess   = false;
    uint64_t                        m_now         = 0;
    std::array<Slot, kSocketCount>  m_slots;
    std::vector<Byte>               m_buffer;
    std::map<uint16_t, HRESULT>     m_failedListens;   // each port's last failed listen
};
