#pragma once

#include "Pch.h"

#include "Devices/Network/IW5100Backend.h"
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

    W5100SocketBridge (W5100 & chip, VirtualNetwork & network) : m_chip (chip), m_network (network) {}
    ~W5100SocketBridge () override;

    void  SetHostNetwork (IHostNetwork * network) { m_hostNetwork = network; }
    void  SetLanAccess   (bool lanAccess)         { m_lanAccess = lanAccess; }

    void  OnCommand      (int socket, Byte command) override;
    void  OnReset        ()                         override;

    bool  IsActive       () const;
    void  Pump           (uint64_t now);
    void  CloseAll       ();

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
};
