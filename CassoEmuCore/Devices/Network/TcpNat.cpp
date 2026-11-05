#include "Pch.h"

#include "Devices/Network/TcpNat.h"
#include "Devices/Network/IGuestPacketSink.h"
#include "Devices/Network/PacketCodec.h"
#include "Devices/Network/VirtualSubnet.h"

using P = PacketCodec;





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::OnGuestSegment
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::OnGuestSegment (
    const TcpSegment  & segment,
    uint64_t            now)
{
    TcpNatConnection::Key  key   = { segment.sourcePort, segment.destinationIp, segment.destinationPort };
    auto                   found = m_connections.find (key);
    bool                   isSyn = (segment.flags & (P::kTcpSyn | P::kTcpAck)) == P::kTcpSyn;



    if (found != m_connections.end())
    {
        found->second->OnGuestSegment (segment, now);
    }
    else if (isSyn)
    {
        StartOutbound (segment);
    }
    else if ((segment.flags & P::kTcpRst) == 0)
    {
        SendReset (segment);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::StartOutbound
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::StartOutbound (const TcpSegment & syn)
{
    HRESULT                        hr     = S_OK;
    std::unique_ptr<IHostSocket>   socket;
    TcpNatConnection::Key          key    = { syn.sourcePort, syn.destinationIp, syn.destinationPort };
    HostEndpoint                   remote = { VirtualSubnet::GuestToHost (syn.destinationIp), syn.destinationPort };



    CBR (m_network != nullptr);

    hr = m_network->CreateSocket (HostSocketKind::Tcp, socket);
    CHR (hr);

    hr = socket->Connect (remote);
    CHR (hr);

    m_connections[key] = TcpNatConnection::MakeOutbound (m_sink, key, std::move (socket), syn, TakeIsn());

Error:
    if (FAILED (hr))
    {
        SendReset (syn);
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::AcceptInbound
//
//  The peer appears to the guest at its own address, except that a peer on
//  this PC appears at the gateway's.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::AcceptInbound (
    std::unique_ptr<IHostSocket>   host,
    const HostEndpoint           & peer,
    uint16_t                       guestPort,
    uint64_t                       now)
{
    TcpNatConnection::Key  key = { guestPort, VirtualSubnet::HostToGuest (peer.ipv4), peer.port };



    m_connections[key] = TcpNatConnection::MakeInbound (m_sink, key, std::move (host), TakeIsn(), now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::Pump
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::Pump (uint64_t now)
{
    auto  it = m_connections.begin();



    while (it != m_connections.end())
    {
        it->second->Pump (now);

        if (it->second->IsClosed())
        {
            it = m_connections.erase (it);
        }
        else
        {
            ++it;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::CloseAll
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::CloseAll()
{
    m_connections.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::NoteRewindSegment
//
//  A guest SYN after the point opens a new connection unless it repeats the
//  SYN from before it.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::NoteRewindSegment (
    const TcpSegment  & segment,
    bool                isToGuest,
    bool                isAfter)
{
    TcpNatConnection::Key            key    = isToGuest ? TcpNatConnection::Key { segment.destinationPort, segment.sourceIp, segment.sourcePort }
                                                        : TcpNatConnection::Key { segment.sourcePort, segment.destinationIp, segment.destinationPort };
    TcpNatConnection::RewindPoint  & point  = m_rewind[key];
    bool                             isSyn  = (segment.flags & P::kTcpSyn) != 0;
    bool                             isAck  = (segment.flags & P::kTcpAck) != 0;
    bool                             isFin  = (segment.flags & P::kTcpFin) != 0;
    uint32_t                         end    = segment.sequence + (uint32_t) segment.payload.size() + (isSyn ? 1 : 0) + (isFin ? 1 : 0);



    if (!isToGuest)
    {
        if (isSyn && !isAck && !isAfter)
        {
            point.hasGuestSyn     = true;
            point.guestSyn        = segment.sequence;
            point.isGuestSynAfter = false;
        }
        else if (isSyn && !isAck)
        {
            point.isGuestSynAfter = point.isGuestSynAfter || !point.hasGuestSyn || segment.sequence != point.guestSyn;
        }
        else if (isSyn && isAfter)
        {
            point.isSynAckAfter = true;
        }

        return;
    }

    if ((segment.flags & P::kTcpRst) != 0)
    {
        return;
    }

    if (isAfter)
    {
        point.hasSentAfter = true;

        if (!segment.payload.empty())
        {
            point.dataAfter.push_back ({ segment.sequence, segment.payload });
        }

        return;
    }

    if (!point.hasSentBefore || TcpNatConnection::IsAfter (end, point.sentEnd))
    {
        point.sentEnd = end;
    }

    point.hasSentBefore   = true;
    point.isFinSentBefore = point.isFinSentBefore || isFin;

    if (isAck)
    {
        point.hasAckBefore = true;
        point.ackBefore    = segment.acknowledgment;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::Rewind
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::Rewind (uint64_t now)
{
    static const TcpNatConnection::RewindPoint  s_kNone;
    auto                                        it      = m_connections.begin();



    while (it != m_connections.end())
    {
        auto  found = m_rewind.find (it->first);

        if (it->second->Rewind ((found != m_rewind.end()) ? found->second : s_kNone, now))
        {
            ++it;
        }
        else
        {
            it = m_connections.erase (it);
        }
    }

    m_rewind.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::SendReset
//
//  RFC 793's reset for a segment with no connection: for a segment with the
//  ACK flag, the reset's sequence number is that ACK value; otherwise the
//  reset acknowledges every sequence number the segment occupied.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNat::SendReset (const TcpSegment & cause)
{
    TcpSegment  reset;
    uint32_t    length = (uint32_t) cause.payload.size();



    if ((cause.flags & P::kTcpSyn) != 0)
    {
        length++;
    }

    if ((cause.flags & P::kTcpFin) != 0)
    {
        length++;
    }

    reset.sourceIp        = cause.destinationIp;
    reset.destinationIp   = cause.sourceIp;
    reset.sourcePort      = cause.destinationPort;
    reset.destinationPort = cause.sourcePort;

    if ((cause.flags & P::kTcpAck) != 0)
    {
        reset.sequence = cause.acknowledgment;
        reset.flags    = P::kTcpRst;
    }
    else
    {
        reset.acknowledgment = cause.sequence + length;
        reset.flags          = P::kTcpRst | P::kTcpAck;
    }

    m_sink.SendTcpToGuest (reset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat::TakeIsn
//
//  Deterministic initial sequence numbers: only the guest receives them, and
//  tests depend on repeatable runs.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t TcpNat::TakeIsn()
{
    uint32_t  isn = m_nextIsn;



    m_nextIsn += kIsnStep;

    return isn;
}
