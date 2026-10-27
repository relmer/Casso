#include "Pch.h"

#include "Devices/Network/W5100SocketBridge.h"
#include "Devices/Network/DhcpServer.h"
#include "Devices/Network/PacketCodec.h"
#include "Devices/Network/VirtualNetwork.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Devices/Network/W5100.h"

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::~W5100SocketBridge
//
////////////////////////////////////////////////////////////////////////////////

W5100SocketBridge::~W5100SocketBridge() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnCommand
//
//  The chip has already applied its own part: OPEN set Sn_SR from the
//  protocol, CLOSE set SOCK_CLOSED, RECV moved Sn_RX_RD.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnCommand (
    int   socket,
    Byte  command)
{
    switch (command)
    {
        case R::kCmdOpen:
            OnOpen (socket);
            break;

        case R::kCmdListen:
            OnListen (socket);
            break;

        case R::kCmdConnect:
            OnConnect (socket);
            break;

        case R::kCmdDisconnect:
            OnDisconnect (socket);
            break;

        case R::kCmdClose:
            CloseSlot (socket);
            break;

        case R::kCmdSend:
        case R::kCmdSendMac:
            OnSend (socket);
            break;

        case R::kCmdRecv:
            PumpSocket (socket, m_now);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnReset
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnReset()
{
    CloseAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::CloseAll
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::CloseAll()
{
    int  socket = 0;



    for (socket = 0; socket < kSocketCount; socket++)
    {
        CloseSlot (socket);
    }

    m_network.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::CloseSlot
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::CloseSlot (int socket)
{
    if (m_slots[socket].phase == Phase::MacRaw)
    {
        m_network.Reset();
    }

    m_slots[socket] = {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::Fail
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::Fail (
    int   socket,
    Byte  interrupt)
{
    CloseSlot (socket);
    m_chip.SetSocketStatus (socket, R::kSockClosed);
    m_chip.RaiseSocketInterrupt (socket, interrupt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnOpen
//
//  A UDP socket binds its source port on the PC, or an ephemeral one when
//  the PC has that port taken. Without a host network a UDP socket opens with
//  no host socket, so DHCP to the virtual server works.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnOpen (int socket)
{
    HRESULT   hr     = S_OK;
    Slot    & slot   = m_slots[socket];
    Byte      status = m_chip.GetSocketStatus (socket);
    uint16_t  port   = m_chip.GetSourcePort (socket);



    CloseSlot (socket);

    if (status == R::kSockMacRaw)
    {
        slot.phase = Phase::MacRaw;
    }

    BAIL_OUT_IF (status != R::kSockUdp, S_OK);

    slot.phase = Phase::Udp;
    BAIL_OUT_IF (m_hostNetwork == nullptr, S_OK);

    hr = m_hostNetwork->CreateSocket (HostSocketKind::Udp, slot.host);
    CHR (hr);

    hr = slot.host->Bind ({ VirtualSubnet::kAnyIp, port }, true);

    if (FAILED (hr))
    {
        hr = slot.host->Bind ({ VirtualSubnet::kAnyIp, 0 }, true);
        CHR (hr);
    }

Error:
    if (FAILED (hr))
    {
        Fail (socket, 0);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnConnect
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnConnect (int socket)
{
    HRESULT        hr     = S_OK;
    Slot         & slot   = m_slots[socket];
    HostEndpoint   remote = { VirtualSubnet::GuestToHost (m_chip.GetDestinationIp (socket)), m_chip.GetDestinationPort (socket) };
    Byte           status = m_chip.GetSocketStatus (socket);



    BAIL_OUT_IF (status != R::kSockInit, S_OK);
    CBR (m_hostNetwork != nullptr);

    hr = m_hostNetwork->CreateSocket (HostSocketKind::Tcp, slot.host);
    CHR (hr);

    hr = slot.host->Connect (remote);
    CHR (hr);

    slot.phase = Phase::Connecting;
    m_chip.SetSocketStatus (socket, R::kSockSynSent);

Error:
    if (FAILED (hr))
    {
        Fail (socket, R::kSnIrTimeout);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnListen
//
//  Loopback only unless LAN access is on. A port the PC already uses closes
//  the socket with TIMEOUT rather than letting it appear to listen.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnListen (int socket)
{
    HRESULT   hr      = S_OK;
    Slot    & slot    = m_slots[socket];
    uint32_t  address = m_lanAccess ? VirtualSubnet::kAnyIp : VirtualSubnet::kLoopbackIp;
    Byte      status  = m_chip.GetSocketStatus (socket);



    BAIL_OUT_IF (status != R::kSockInit, S_OK);
    CBR (m_hostNetwork != nullptr);

    hr = m_hostNetwork->CreateSocket (HostSocketKind::Tcp, slot.host);
    CHR (hr);

    hr = slot.host->Bind ({ address, m_chip.GetSourcePort (socket) }, false);
    CHR (hr);

    hr = slot.host->Listen();
    CHR (hr);

    slot.phase = Phase::Listening;
    m_chip.SetSocketStatus (socket, R::kSockListen);

Error:
    if (FAILED (hr))
    {
        Fail (socket, R::kSnIrTimeout);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnDisconnect
//
//  Queued bytes the host accepts now are sent before the shutdown.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnDisconnect (int socket)
{
    HRESULT   hr        = S_OK;
    Slot    & slot      = m_slots[socket];
    bool      connected = slot.phase == Phase::Established || slot.phase == Phase::PeerClosed;



    if (connected)
    {
        PumpTcpTransfer (socket);

        hr = slot.host->ShutdownSend();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    CloseSlot (socket);
    m_chip.SetSocketStatus (socket, R::kSockClosed);

    if (connected)
    {
        m_chip.RaiseSocketInterrupt (socket, R::kSnIrDisconnect);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::OnSend
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::OnSend (int socket)
{
    switch (m_slots[socket].phase)
    {
        case Phase::Established:
        case Phase::PeerClosed:
            SendTcp (socket);
            break;

        case Phase::Udp:
            SendUdp (socket);
            break;

        case Phase::MacRaw:
            SendMacRaw (socket, m_now);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::SendTcp
//
//  Copies the bytes written since the last SEND onto the queue for the host
//  and sends what the host takes now. Sn_TX_RD advances as the host takes
//  bytes, which is what raises Sn_TX_FSR; SEND_OK follows once the queue is
//  empty.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::SendTcp (int socket)
{
    Slot               & slot    = m_slots[socket];
    size_t               pending = m_chip.GetTxPending (socket);
    size_t               queued  = slot.txQueue.size();
    std::vector<Byte>    fresh;



    if (pending > queued)
    {
        m_chip.CopyTx (socket, queued, pending - queued, fresh);
        slot.txQueue.insert (slot.txQueue.end(), fresh.begin(), fresh.end());
    }

    slot.sendPending = true;
    PumpTcpTransfer (socket);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::SendUdp
//
//  One SEND is one datagram. Port 67 is the virtual DHCP server; its reply
//  arrives from the gateway, port 67.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::SendUdp (int socket)
{
    HRESULT              hr       = S_OK;
    Slot               & slot     = m_slots[socket];
    size_t               pending  = m_chip.GetTxPending (socket);
    uint16_t             port     = m_chip.GetDestinationPort (socket);
    HostEndpoint         remote   = { VirtualSubnet::GuestToHost (m_chip.GetDestinationIp (socket)), port };
    std::vector<Byte>    payload;
    std::vector<Byte>    reply;



    m_chip.CopyTx (socket, 0, pending, payload);
    m_chip.CommitTx (socket, pending);

    if (port == DhcpServer::kServerPort)
    {
        reply = m_network.ProcessDhcp (payload);

        if (!reply.empty())
        {
            slot.udpPending.push_back ({ { VirtualSubnet::kLoopbackIp, DhcpServer::kServerPort }, std::move (reply) });
        }
    }
    else if (slot.host != nullptr)
    {
        hr = slot.host->SendTo (remote, payload);
        IGNORE_RETURN_VALUE (hr, S_OK);

        slot.udpPeers.insert (PackEndpoint (remote));
    }

    m_chip.RaiseSocketInterrupt (socket, R::kSnIrSendOk);
    PumpUdpReceive (socket);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::SendMacRaw
//
//  One SEND is one frame. Frames the virtual network produces at once, such
//  as an ARP reply, are in the RX buffer before this returns.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::SendMacRaw (
    int       socket,
    uint64_t  now)
{
    size_t             pending = m_chip.GetTxPending (socket);
    std::vector<Byte>  frame;



    m_chip.CopyTx (socket, 0, pending, frame);
    m_chip.CommitTx (socket, pending);

    m_network.OnGuestFrame (frame, now);

    m_chip.RaiseSocketInterrupt (socket, R::kSnIrSendOk);
    PumpMacRaw (socket);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::IsActive
//
////////////////////////////////////////////////////////////////////////////////

bool W5100SocketBridge::IsActive() const
{
    for (const Slot & slot : m_slots)
    {
        if (slot.phase != Phase::Idle)
        {
            return true;
        }
    }

    return m_network.IsActive();
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::Pump
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::Pump (uint64_t now)
{
    int  socket = 0;



    m_now = now;

    m_network.Pump (now, m_slots[0].phase == Phase::MacRaw);

    for (socket = 0; socket < kSocketCount; socket++)
    {
        PumpSocket (socket, now);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpSocket
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpSocket (
    int       socket,
    uint64_t  now)
{
    (void) now;

    switch (m_slots[socket].phase)
    {
        case Phase::Connecting:
            PumpConnecting (socket);
            break;

        case Phase::Listening:
            PumpListening (socket);
            break;

        case Phase::Established:
            PumpTcpTransfer (socket);
            PumpTcpReceive (socket);
            break;

        case Phase::PeerClosed:
            PumpTcpTransfer (socket);
            break;

        case Phase::Udp:
            PumpUdpReceive (socket);
            break;

        case Phase::MacRaw:
            PumpMacRaw (socket);
            break;

        default:
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpConnecting
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpConnecting (int socket)
{
    HRESULT              hr        = S_OK;
    Slot               & slot      = m_slots[socket];
    HostSocketReadiness  readiness;



    hr = slot.host->Poll (readiness);

    if (FAILED (hr) || readiness.failed)
    {
        Fail (socket, R::kSnIrTimeout);
        return;
    }

    if (readiness.writable)
    {
        slot.phase = Phase::Established;
        m_chip.SetSocketStatus (socket, R::kSockEstablished);
        m_chip.RaiseSocketInterrupt (socket, R::kSnIrConnect);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpListening
//
//  The first connection takes the socket, as on the chip; the listener
//  closes with it.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpListening (int socket)
{
    HRESULT                        hr         = S_OK;
    Slot                         & slot       = m_slots[socket];
    std::unique_ptr<IHostSocket>   accepted;
    HostEndpoint                   peer;
    bool                           wouldBlock = false;



    hr = slot.host->Accept (accepted, peer, wouldBlock);

    if (FAILED (hr) || wouldBlock)
    {
        return;
    }

    slot.host  = std::move (accepted);
    slot.phase = Phase::Established;

    m_chip.SetDestination (socket, VirtualSubnet::HostToGuest (peer.ipv4), peer.port);
    m_chip.SetSocketStatus (socket, R::kSockEstablished);
    m_chip.RaiseSocketInterrupt (socket, R::kSnIrConnect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpTcpTransfer
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpTcpTransfer (int socket)
{
    HRESULT   hr   = S_OK;
    Slot    & slot = m_slots[socket];
    size_t    sent = 0;



    if (!slot.txQueue.empty())
    {
        hr = slot.host->Send (slot.txQueue, sent);

        if (FAILED (hr))
        {
            Fail (socket, R::kSnIrTimeout);
            return;
        }

        slot.txQueue.erase (slot.txQueue.begin(), slot.txQueue.begin() + sent);
        m_chip.CommitTx (socket, sent);
    }

    if (slot.sendPending && slot.txQueue.empty())
    {
        slot.sendPending = false;
        m_chip.RaiseSocketInterrupt (socket, R::kSnIrSendOk);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpTcpReceive
//
//  No more than the RX buffer has room for; the rest stays in the PC's
//  socket buffer, under TCP flow control, until the guest frees space with
//  RECV.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpTcpReceive (int socket)
{
    HRESULT   hr       = S_OK;
    Slot    & slot     = m_slots[socket];
    size_t    room     = m_chip.GetRxFree (socket);
    size_t    received = 0;
    bool      eof      = false;
    bool      appended = false;



    if (room == 0)
    {
        return;
    }

    m_buffer.resize (room);

    hr = slot.host->Receive (m_buffer, received, eof);

    if (FAILED (hr))
    {
        Fail (socket, R::kSnIrDisconnect);
        return;
    }

    if (received != 0)
    {
        appended = m_chip.TryAppendRx (socket, std::span<const Byte> (m_buffer).first (received));
        ASSERT (appended);
        m_chip.RaiseSocketInterrupt (socket, R::kSnIrReceive);
    }

    if (eof)
    {
        slot.phase = Phase::PeerClosed;
        m_chip.SetSocketStatus (socket, R::kSockCloseWait);
        m_chip.RaiseSocketInterrupt (socket, R::kSnIrDisconnect);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpUdpReceive
//
//  Datagrams wait, in order, for room in the RX buffer. Unless LAN access is
//  on, only datagrams from this PC or from a peer this socket has sent to
//  are delivered.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpUdpReceive (int socket)
{
    constexpr int   kMaxPerPump = 16;
    HRESULT         hr          = S_OK;
    Slot          & slot        = m_slots[socket];
    size_t          received    = 0;
    HostEndpoint    from;
    int             count       = 0;
    bool            allowed     = false;



    while (!slot.udpPending.empty() && TryDeliverDatagram (socket, slot.udpPending.front()))
    {
        slot.udpPending.pop_front();
    }

    if (!slot.udpPending.empty() || slot.host == nullptr)
    {
        return;
    }

    m_buffer.resize (kReceiveBuffer);

    for (count = 0; count < kMaxPerPump; count++)
    {
        hr = slot.host->ReceiveFrom (m_buffer, received, from);

        if (FAILED (hr) || received == 0)
        {
            break;
        }

        allowed = m_lanAccess || VirtualSubnet::IsLoopback (from.ipv4) || slot.udpPeers.contains (PackEndpoint (from));

        if (!allowed)
        {
            continue;
        }

        slot.udpPending.push_back ({ from, std::vector<Byte> (m_buffer.begin(), m_buffer.begin() + received) });

        if (!TryDeliverDatagram (socket, slot.udpPending.front()))
        {
            break;
        }

        slot.udpPending.pop_front();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::TryDeliverDatagram
//
//  The 8-byte header is the peer's address, port and payload length (the
//  datasheet's receive procedure reads that many bytes after the header).
//  A datagram larger than the whole buffer can never fit and is discarded,
//  which counts as delivered.
//
////////////////////////////////////////////////////////////////////////////////

bool W5100SocketBridge::TryDeliverDatagram (
    int               socket,
    const Datagram  & datagram)
{
    std::vector<Byte>  block (R::kUdpHeaderLength);
    size_t             total = R::kUdpHeaderLength + datagram.payload.size();
    bool               fits  = false;



    if (total > m_chip.GetRxBufferSize (socket))
    {
        return true;
    }

    PacketCodec::WriteU32 (block, 0, VirtualSubnet::HostToGuest (datagram.from.ipv4));
    PacketCodec::WriteU16 (block, 4, datagram.from.port);
    PacketCodec::WriteU16 (block, 6, (uint16_t) datagram.payload.size());
    block.insert (block.end(), datagram.payload.begin(), datagram.payload.end());

    fits = m_chip.TryAppendRx (socket, block);

    if (fits)
    {
        m_chip.RaiseSocketInterrupt (socket, R::kSnIrReceive);
    }

    return fits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PumpMacRaw
//
//  Frames from the virtual network, each behind a 2-byte length that counts
//  itself. With the MAC filter on, only frames for SHAR or broadcast pass.
//
////////////////////////////////////////////////////////////////////////////////

void W5100SocketBridge::PumpMacRaw (int socket)
{
    std::array<Byte, R::kMacLength>  shar    = {};
    bool                             filter  = (m_chip.GetSocketMode (socket) & R::kSnMrMacFilter) != 0;
    std::vector<Byte>                block;
    size_t                           total   = 0;
    bool                             match   = false;



    m_chip.GetSourceMac (shar);

    while (m_network.HasFrame())
    {
        const std::vector<Byte>  & frame = m_network.PeekFrame();

        total = R::kMacRawHeaderLength + frame.size();
        match = !filter
             || std::equal (shar.begin(), shar.end(), frame.begin())
             || std::equal (PacketCodec::kBroadcastMac.begin(), PacketCodec::kBroadcastMac.end(), frame.begin());

        if (match && total <= m_chip.GetRxBufferSize (socket))
        {
            if (total > m_chip.GetRxFree (socket))
            {
                break;
            }

            block.assign (R::kMacRawHeaderLength, 0);
            PacketCodec::WriteU16 (block, 0, (uint16_t) total);
            block.insert (block.end(), frame.begin(), frame.end());

            m_chip.TryAppendRx (socket, block);
            m_chip.RaiseSocketInterrupt (socket, R::kSnIrReceive);
        }

        m_network.PopFrame();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  W5100SocketBridge::PackEndpoint
//
////////////////////////////////////////////////////////////////////////////////

uint64_t W5100SocketBridge::PackEndpoint (const HostEndpoint & endpoint)
{
    return ((uint64_t) endpoint.ipv4 << 16) | endpoint.port;
}
