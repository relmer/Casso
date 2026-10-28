#pragma once

#include "Pch.h"

#include "Devices/Network/TcpNatConnection.h"
#include "Seams/IHostNetwork.h"

class IGuestPacketSink;

struct TcpSegment;





////////////////////////////////////////////////////////////////////////////////
//
//  TcpNat
//
//  The guest's TCP connections, keyed by guest port and peer. A guest SYN
//  for a new peer starts a host connect; a segment for no known connection
//  gets a reset, as a real host's stack would send.
//
////////////////////////////////////////////////////////////////////////////////

class TcpNat
{
public:

    explicit TcpNat (IGuestPacketSink & sink) : m_sink (sink) {}

    void    SetHostNetwork  (IHostNetwork * network) { m_network = network; }

    void    OnGuestSegment  (const TcpSegment & segment, uint64_t now);

    // A connection a port forward accepted, for `guestPort` on the guest.
    void    AcceptInbound   (std::unique_ptr<IHostSocket>   host,
                             const HostEndpoint           & peer,
                             uint16_t                       guestPort,
                             uint64_t                       now);

    void    Pump            (uint64_t now);
    void    CloseAll        ();

    //  A resume from the past: each segment in history between the guest
    //  and a connection is noted, oldest first, then Rewind returns each
    //  connection to the point and closes the ones opened after it.
    void    NoteRewindSegment (const TcpSegment & segment, bool isToGuest, bool isAfter);
    void    Rewind          (uint64_t now);
    bool    IsActive        () const { return !m_connections.empty(); }
    size_t  GetConnectionCount () const { return m_connections.size(); }

private:

    static constexpr uint32_t  kIsnStep = 0x00100000;

    void      StartOutbound (const TcpSegment & syn);
    void      SendReset     (const TcpSegment & cause);
    uint32_t  TakeIsn       ();

    IGuestPacketSink                                                      & m_sink;
    IHostNetwork                                                          * m_network = nullptr;
    std::map<TcpNatConnection::Key, std::unique_ptr<TcpNatConnection>>     m_connections;
    std::map<TcpNatConnection::Key, TcpNatConnection::RewindPoint>          m_rewind;
    uint32_t                                                                m_nextIsn = 0x01000000;
};
