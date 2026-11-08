#pragma once

#include "Pch.h"

#include "Devices/Network/IW5100Backend.h"
#include "Seams/IHostFirewall.h"
#include "Seams/IHostNetwork.h"

class ListenGate;
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

    //  A listener whose port the PC has in use moves to the next port, up to
    //  this many ports from the guest's, so a second Casso on the PC listens
    //  as well as the first.
    static constexpr int  kListenPorts = 8;

    //  A listen the PC could not open: the guest's port, the last port a
    //  listen was attempted on, and why.
    struct ListenFailure
    {
        uint16_t  port     = 0;
        uint16_t  lastPort = 0;
        HRESULT   hr       = S_OK;
    };

    //  A listener on a later PC port than the guest's.
    struct MovedListen
    {
        uint16_t  guestPort = 0;
        uint16_t  hostPort  = 0;
    };

    W5100SocketBridge (W5100 & chip, VirtualNetwork & network) : m_chip (chip), m_network (network) {}
    ~W5100SocketBridge () override;

    void  SetHostNetwork (IHostNetwork * network) { m_hostNetwork = network; }
    void  SetLanAccess   (bool lanAccess);

    //  While LAN access is on, a socket whose PC port the gate does not admit
    //  is deferred: the guest's socket opens with no PC socket behind it, and
    //  Pump opens the PC socket once the gate admits the port.
    void  SetListenGate  (const ListenGate * gate) { m_gate = gate; }

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

    //  The PC ports of the deferred sockets, in socket order.
    std::vector<FirewallPort>   GetDeferredPorts     () const;

    //  The listeners on a later PC port than the guest's, in socket order.
    std::vector<MovedListen>    GetMovedListens      () const;

    //  A failed listen as a notice's sentence. `serialPort` is the serial
    //  port whose connector has the same TCP port, 0 for none, and `onCard`
    //  is whether that port is a Super Serial Card's.
    static std::string  DescribeListenFailure (const ListenFailure & failure, int serialPort, bool onCard);

    //  A moved listener as a notice's sentence.
    static std::string  DescribeMovedListen   (const MovedListen & moved);

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
        std::vector<Byte>             txQueue;   // copied from TX, not yet taken by the host
        bool                          sendPending = false;
        std::set<uint64_t>            udpPeers;   // packed endpoints sent to
        std::deque<Datagram>          udpPending;   // received, waiting for RX space
        uint16_t                      hostPort    = 0;   // the PC port a listener or UDP socket has; 0 for an ephemeral one
        bool                          deferred    = false;   // waiting for the gate to admit hostPort
    };

    bool          IsDeferred          (HostSocketKind protocol, uint16_t port) const;
    FirewallPort  GetListenRange      (int socket) const;
    void          OpenDeferred        ();
    void          OpenUdp             (int socket);
    void          CloseSlot           (int socket);
    void          OnOpen              (int socket);
    void          OnConnect           (int socket);
    void          OnListen            (int socket);
    HRESULT       BindListener        (int socket);
    void          Relisten            (int socket);
    void          OnDisconnect        (int socket);
    void          OnSend              (int socket);
    void          SendTcp             (int socket);
    void          SendUdp             (int socket);
    void          RewriteAnnouncement (std::vector<Byte> & payload) const;
    void          SendMacRaw          (int socket, uint64_t now);
    void          Fail                (int socket, Byte interrupt);

    void  PumpSocket         (int socket, uint64_t now);
    void  PumpConnecting     (int socket);
    void  PumpListening      (int socket);
    void  PumpTcpTransfer    (int socket);
    void  PumpTcpReceive     (int socket);
    void  PumpUdpReceive     (int socket);
    void  PumpMacRaw         (int socket);
    bool  TryDeliverDatagram (int socket, const Datagram & datagram);

    static uint64_t  PackEndpoint (const HostEndpoint & endpoint);

    W5100                              & m_chip;
    VirtualNetwork                     & m_network;
    IHostNetwork                       * m_hostNetwork   = nullptr;
    const ListenGate                   * m_gate          = nullptr;
    uint32_t                             m_gateVersion   = 0;   // the gate's version when deferred sockets were last tried
    bool                                 m_lanAccess     = false;
    uint64_t                             m_now           = 0;
    std::array<Slot, kSocketCount>       m_slots;
    std::vector<Byte>                    m_buffer;
    std::map<uint16_t, ListenFailure>    m_failedListens;   // each guest port's last failed listen
};
