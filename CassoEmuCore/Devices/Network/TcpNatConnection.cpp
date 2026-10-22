#include "Pch.h"

#include "Devices/Network/TcpNatConnection.h"
#include "Devices/Network/IGuestPacketSink.h"

using P = PacketCodec;





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::TcpNatConnection
//
////////////////////////////////////////////////////////////////////////////////

TcpNatConnection::TcpNatConnection (
    IGuestPacketSink              & sink,
    const Key                     & key,
    std::unique_ptr<IHostSocket>    host,
    uint32_t                        isn)
    : m_sink   (sink),
      m_key    (key),
      m_host   (std::move (host)),
      m_ourIsn (isn)
{
    m_ourUnacked = isn;
    m_ourNext    = isn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::MakeOutbound
//
//  The guest's SYN has arrived and the host connect has been started; the
//  SYN-ACK waits until the connect completes.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<TcpNatConnection> TcpNatConnection::MakeOutbound (
    IGuestPacketSink              & sink,
    const Key                     & key,
    std::unique_ptr<IHostSocket>    host,
    const TcpSegment              & syn,
    uint32_t                        isn)
{
    auto  connection = std::make_unique<TcpNatConnection> (sink, key, std::move (host), isn);



    connection->m_state       = State::Connecting;
    connection->m_guestNext   = syn.sequence + 1;
    connection->m_guestWindow = syn.window;
    connection->m_mss         = (syn.mss != 0) ? std::min (syn.mss, kMaxMss) : kDefaultMss;

    return connection;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::MakeInbound
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<TcpNatConnection> TcpNatConnection::MakeInbound (
    IGuestPacketSink              & sink,
    const Key                     & key,
    std::unique_ptr<IHostSocket>    host,
    uint32_t                        isn,
    uint64_t                        now)
{
    auto  connection = std::make_unique<TcpNatConnection> (sink, key, std::move (host), isn);



    connection->m_state         = State::SynSentToGuest;
    connection->m_lastSendCycle = now;
    connection->SendToGuest (P::kTcpSyn, isn, {}, kMaxMss);

    return connection;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::OnGuestSegment
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::OnGuestSegment (
    const TcpSegment  & segment,
    uint64_t            now)
{
    bool  isSyn = (segment.flags & P::kTcpSyn) != 0;



    (void) now;

    if ((segment.flags & P::kTcpRst) != 0)
    {
        Close();
        return;
    }

    if (m_state == State::SynSentToGuest)
    {
        OnSynAck (segment);
        return;
    }

    if (m_state != State::Established)
    {
        return;
    }

    // A repeated SYN means the guest missed our SYN-ACK.
    if (isSyn && segment.sequence + 1 == m_guestNext && m_ourUnacked == m_ourIsn + 1)
    {
        SendToGuest (P::kTcpSyn | P::kTcpAck, m_ourIsn, {}, kMaxMss);
        return;
    }

    if ((segment.flags & P::kTcpAck) != 0)
    {
        OnAck (segment);
    }

    OnData (segment);
    OnFin (segment);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::OnSynAck
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::OnSynAck (const TcpSegment & segment)
{
    constexpr Byte  kSynAck = P::kTcpSyn | P::kTcpAck;



    if ((segment.flags & kSynAck) != kSynAck || segment.acknowledgment != m_ourIsn + 1)
    {
        return;
    }

    m_guestNext   = segment.sequence + 1;
    m_guestWindow = segment.window;
    m_mss         = (segment.mss != 0) ? std::min (segment.mss, kMaxMss) : kDefaultMss;
    m_ourUnacked  = m_ourIsn + 1;
    m_ourNext     = m_ourIsn + 1;
    m_state       = State::Established;
    m_retransmits = 0;

    SendAck();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::OnAck
//
//  An acknowledgment of nothing new, or of more than was sent, changes
//  nothing but the window.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::OnAck (const TcpSegment & segment)
{
    uint32_t  delta     = segment.acknowledgment - m_ourUnacked;
    size_t    dataAcked = 0;



    m_guestWindow = segment.window;

    if (delta == 0 || delta > GetInFlight())
    {
        return;
    }

    dataAcked = std::min ((size_t) delta, m_toGuest.size());
    m_toGuest.erase (m_toGuest.begin(), m_toGuest.begin() + dataAcked);

    m_ourUnacked  = segment.acknowledgment;
    m_retransmits = 0;

    if (m_finSent && m_ourUnacked == m_ourNext)
    {
        m_finAcked = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::OnData
//
//  Only the next in-sequence bytes are taken, as many as the queue for the
//  host has room for; anything else produces a duplicate ACK so the guest
//  resends from this side's next sequence number.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::OnData (const TcpSegment & segment)
{
    uint32_t  offset = m_guestNext - segment.sequence;
    size_t    room   = kQueueLimit - m_toHost.size();
    size_t    take   = 0;



    if (segment.payload.empty())
    {
        return;
    }

    if (offset < segment.payload.size())
    {
        take = std::min (segment.payload.size() - offset, room);
        m_toHost.insert (m_toHost.end(), segment.payload.begin() + offset, segment.payload.begin() + offset + take);
        m_guestNext += (uint32_t) take;
    }

    SendAck();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::OnFin
//
//  A FIN counts only once every byte before it has been taken.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::OnFin (const TcpSegment & segment)
{
    uint32_t  finSequence = segment.sequence + (uint32_t) segment.payload.size();



    if ((segment.flags & P::kTcpFin) == 0)
    {
        return;
    }

    if (!m_guestFin && finSequence == m_guestNext)
    {
        m_guestFin = true;
        m_guestNext++;
    }

    SendAck();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::Pump
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::Pump (uint64_t now)
{
    if (m_state == State::Connecting)
    {
        PumpConnect (now);
        return;
    }

    if (m_state == State::SynSentToGuest)
    {
        PumpRetransmit (now);
        return;
    }

    if (m_state != State::Established)
    {
        return;
    }

    //  A failed host send or receive resets and closes the connection, which
    //  releases the host socket, so each step after one runs only while the
    //  connection is established.
    PumpHostWrite();

    if (m_state == State::Established)
    {
        PumpHostRead();
    }

    if (m_state == State::Established)
    {
        PumpGuestSend (now);
        PumpRetransmit (now);
    }

    if (m_state == State::Established && m_guestFin && m_finAcked && m_toHost.empty())
    {
        Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::PumpConnect
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::PumpConnect (uint64_t now)
{
    HRESULT              hr        = S_OK;
    HostSocketReadiness  readiness;



    hr = m_host->Poll (readiness);

    if (FAILED (hr) || readiness.failed)
    {
        SendResetAndClose();
        return;
    }

    if (!readiness.writable)
    {
        return;
    }

    m_state         = State::Established;
    m_ourUnacked    = m_ourIsn + 1;
    m_ourNext       = m_ourIsn + 1;
    m_lastSendCycle = now;

    SendToGuest (P::kTcpSyn | P::kTcpAck, m_ourIsn, {}, kMaxMss);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::PumpHostWrite
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::PumpHostWrite()
{
    HRESULT            hr      = S_OK;
    std::vector<Byte>  pending (m_toHost.begin(), m_toHost.end());
    size_t             sent    = 0;
    bool               wasFull = m_toHost.size() == kQueueLimit;



    if (!pending.empty())
    {
        hr = m_host->Send (pending, sent);

        if (FAILED (hr))
        {
            SendResetAndClose();
            return;
        }

        m_toHost.erase (m_toHost.begin(), m_toHost.begin() + sent);

        // The window this side advertised reopens.
        if (wasFull && sent != 0)
        {
            SendAck();
        }
    }

    if (m_guestFin && m_toHost.empty() && !m_hostShutdown)
    {
        hr = m_host->ShutdownSend();
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_hostShutdown = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::PumpHostRead
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::PumpHostRead()
{
    HRESULT            hr       = S_OK;
    std::vector<Byte>  buffer;
    size_t             received = 0;
    bool               eof      = false;
    size_t             room     = kQueueLimit - std::min (m_toGuest.size(), kQueueLimit);



    if (m_hostEof || room == 0)
    {
        return;
    }

    buffer.resize (room);

    hr = m_host->Receive (buffer, received, eof);

    if (FAILED (hr))
    {
        SendResetAndClose();
        return;
    }

    m_toGuest.insert (m_toGuest.end(), buffer.begin(), buffer.begin() + received);
    m_hostEof = eof;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::PumpGuestSend
//
//  New data within the guest's window, MSS-sized; then FIN once the host has
//  closed and everything before it has gone out.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::PumpGuestSend (uint64_t now)
{
    size_t             sentData = 0;
    size_t             unsent   = 0;
    size_t             window   = 0;
    size_t             count    = 0;
    std::vector<Byte>  payload;



    while (!m_finSent)
    {
        sentData = GetInFlight();
        unsent   = m_toGuest.size() - sentData;
        window   = (m_guestWindow > sentData) ? m_guestWindow - sentData : 0;
        count    = std::min ({ (size_t) m_mss, unsent, window });

        if (count == 0)
        {
            break;
        }

        payload.assign (m_toGuest.begin() + sentData, m_toGuest.begin() + sentData + count);
        SendToGuest (P::kTcpAck | P::kTcpPsh, m_ourNext, payload, 0);

        m_ourNext       += (uint32_t) count;
        m_lastSendCycle  = now;
    }

    if (m_hostEof && !m_finSent && GetInFlight() == m_toGuest.size())
    {
        SendToGuest (P::kTcpFin | P::kTcpAck, m_ourNext, {}, 0);

        m_ourNext++;
        m_finSent       = true;
        m_lastSendCycle = now;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::PumpRetransmit
//
//  The oldest unacknowledged segment -- the SYN, data, or the FIN -- once a
//  second without progress, up to kMaxRetransmits times.
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::PumpRetransmit (uint64_t now)
{
    std::vector<Byte>  payload;
    size_t             dataInFlight = 0;
    size_t             count        = 0;



    if (now - m_lastSendCycle < kRetransmitCycles)
    {
        return;
    }

    if (m_state == State::SynSentToGuest || GetInFlight() != 0)
    {
        if (++m_retransmits > kMaxRetransmits)
        {
            SendResetAndClose();
            return;
        }

        m_lastSendCycle = now;
    }

    if (m_state == State::SynSentToGuest)
    {
        SendToGuest (P::kTcpSyn, m_ourIsn, {}, kMaxMss);
        return;
    }

    dataInFlight = GetInFlight() - ((m_finSent && !m_finAcked) ? 1 : 0);

    if (dataInFlight != 0)
    {
        count = std::min ((size_t) m_mss, dataInFlight);
        payload.assign (m_toGuest.begin(), m_toGuest.begin() + count);
        SendToGuest (P::kTcpAck | P::kTcpPsh, m_ourUnacked, payload, 0);
    }
    else if (m_finSent && !m_finAcked)
    {
        SendToGuest (P::kTcpFin | P::kTcpAck, m_ourUnacked, {}, 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::SendToGuest
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::SendToGuest (
    Byte                    flags,
    uint32_t                sequence,
    std::span<const Byte>   payload,
    uint16_t                mss)
{
    TcpSegment  segment;



    segment.sourceIp        = m_key.peerIp;
    segment.destinationIp   = m_sink.GetGuestIp();
    segment.sourcePort      = m_key.peerPort;
    segment.destinationPort = m_key.guestPort;
    segment.sequence        = sequence;
    segment.acknowledgment  = ((flags & P::kTcpAck) != 0) ? m_guestNext : 0;
    segment.flags           = flags;
    segment.window          = GetOurWindow();
    segment.mss             = mss;
    segment.payload.assign (payload.begin(), payload.end());

    m_sink.SendTcpToGuest (segment);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::SendAck
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::SendAck()
{
    SendToGuest (P::kTcpAck, m_ourNext, {}, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::SendResetAndClose
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::SendResetAndClose()
{
    SendToGuest (P::kTcpRst | P::kTcpAck, m_ourNext, {}, 0);
    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::Close
//
////////////////////////////////////////////////////////////////////////////////

void TcpNatConnection::Close()
{
    m_host.reset();
    m_state = State::Closed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection::GetOurWindow
//
////////////////////////////////////////////////////////////////////////////////

uint16_t TcpNatConnection::GetOurWindow() const
{
    return (uint16_t) (kQueueLimit - m_toHost.size());
}
