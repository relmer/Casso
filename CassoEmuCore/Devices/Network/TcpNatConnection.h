#pragma once

#include "Pch.h"

#include "Devices/Network/PacketCodec.h"
#include "Seams/IHostNetwork.h"

class IGuestPacketSink;





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNatConnection
//
//  One guest TCP connection relayed through one host TCP connection. The
//  guest side is a minimal TCP endpoint with no congestion control, because
//  the virtual wire between it and the guest never drops a frame: segments go
//  out within the guest's advertised window and MSS, and the oldest
//  unacknowledged one is resent after kRetransmitCycles for guest stacks that
//  discard segments themselves.
//
//  Outbound connections start from the guest's SYN with a host connect in
//  progress; inbound ones (port forwards) start from an accepted host
//  connection with a SYN sent to the guest.
//
////////////////////////////////////////////////////////////////////////////////

class TcpNatConnection
{
public:

    enum class State
    {
        Connecting,        // guest SYN received, host connect in progress
        SynSentToGuest,    // inbound: SYN sent to the guest, SYN-ACK pending
        Established,
        Closed,
    };

    struct Key
    {
        uint16_t  guestPort = 0;
        uint32_t  peerIp    = 0;    // the guest-side address
        uint16_t  peerPort  = 0;

        auto operator<=> (const Key & other) const = default;
    };

    static constexpr size_t    kQueueLimit       = 4096;
    static constexpr uint16_t  kMaxMss           = 1460;
    static constexpr uint16_t  kDefaultMss       = 536;
    static constexpr uint64_t  kRetransmitCycles = 1022727;    // 1 s at 1.023 MHz
    static constexpr int       kMaxRetransmits   = 8;

    static std::unique_ptr<TcpNatConnection>  MakeOutbound (IGuestPacketSink              & sink,
                                                            const Key                     & key,
                                                            std::unique_ptr<IHostSocket>    host,
                                                            const TcpSegment              & syn,
                                                            uint32_t                        isn);
    static std::unique_ptr<TcpNatConnection>  MakeInbound  (IGuestPacketSink              & sink,
                                                            const Key                     & key,
                                                            std::unique_ptr<IHostSocket>    host,
                                                            uint32_t                        isn,
                                                            uint64_t                        now);

    TcpNatConnection (IGuestPacketSink & sink, const Key & key, std::unique_ptr<IHostSocket> host, uint32_t isn);

    void   OnGuestSegment (const TcpSegment & segment, uint64_t now);
    void   Pump           (uint64_t now);

    State  GetState       () const { return m_state; }
    bool   IsClosed       () const { return m_state == State::Closed; }

private:

    void      SendToGuest      (Byte flags, uint32_t sequence, std::span<const Byte> payload, uint16_t mss);
    void      SendAck          ();
    void      SendResetAndClose();
    void      Close            ();
    uint16_t  GetOurWindow     () const;
    uint32_t  GetInFlight      () const { return m_ourNext - m_ourUnacked; }

    void      OnSynAck         (const TcpSegment & segment);
    void      OnAck            (const TcpSegment & segment);
    void      OnData           (const TcpSegment & segment);
    void      OnFin            (const TcpSegment & segment);

    void      PumpConnect      (uint64_t now);
    void      PumpHostWrite    ();
    void      PumpHostRead     ();
    void      PumpGuestSend    (uint64_t now);
    void      PumpRetransmit   (uint64_t now);

    IGuestPacketSink              & m_sink;
    Key                             m_key;
    std::unique_ptr<IHostSocket>    m_host;
    State                           m_state          = State::Connecting;
    uint32_t                        m_ourIsn         = 0;
    uint32_t                        m_ourUnacked     = 0;
    uint32_t                        m_ourNext        = 0;
    uint32_t                        m_guestNext      = 0;
    uint16_t                        m_guestWindow    = 0;
    uint16_t                        m_mss            = kDefaultMss;
    std::deque<Byte>                m_toHost;
    std::deque<Byte>                m_toGuest;        // front has sequence m_ourUnacked
    bool                            m_guestFin       = false;
    bool                            m_hostShutdown   = false;
    bool                            m_hostEof        = false;
    bool                            m_finSent        = false;
    bool                            m_finAcked       = false;
    uint64_t                        m_lastSendCycle  = 0;
    int                             m_retransmits    = 0;
};
