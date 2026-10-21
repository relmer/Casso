#pragma once

#include "Pch.h"

#include "Seams/IHostNetwork.h"

class IGuestPacketSink;





////////////////////////////////////////////////////////////////////////////////
//
//  UdpNat
//
//  Guest UDP over host UDP sockets, one socket per guest source port. The
//  host socket binds the same port number when it is free, so a peer that
//  replies to the source port it received from (ADTPro's server does) gets
//  through. Datagrams to the virtual DNS address go to the PC's DNS server.
//
//  Inbound datagrams are delivered when they come from this PC, from an
//  address and port the flow has sent to, or from anywhere when LAN access is
//  on. A flow ends after kIdleCycles of emulated time with no traffic; a flow
//  created for a port forward does not.
//
////////////////////////////////////////////////////////////////////////////////

class UdpNat
{
public:

    static constexpr uint64_t  kIdleCycles     = 120ULL * 1022727ULL;   // 120 s at 1.023 MHz
    static constexpr size_t    kMaxDatagram    = 65535;

    explicit UdpNat (IGuestPacketSink & sink) : m_sink (sink) {}

    void  SetHostNetwork  (IHostNetwork * network) { m_network = network; }
    void  SetLanAccess    (bool lanAccess)         { m_lanAccess = lanAccess; }

    void  SendFromGuest   (uint16_t                guestPort,
                           uint32_t                destinationIp,
                           uint16_t                destinationPort,
                           std::span<const Byte>   payload,
                           uint64_t                now);

    // A port forward's bound socket, whose datagrams go to `guestPort`.
    void  AddForward      (uint16_t guestPort, std::unique_ptr<IHostSocket> socket);

    void  Pump            (uint64_t now);
    void  CloseFlows      ();    // all but the port forwards
    void  CloseAll        ();
    bool  IsActive        () const { return !m_flows.empty(); }
    size_t  GetFlowCount  () const { return m_flows.size(); }

private:

    struct Flow
    {
        std::unique_ptr<IHostSocket>  socket;
        std::set<uint64_t>            peers;          // HostEndpoints sent to, packed
        uint64_t                      lastActivity  = 0;
        bool                          isForward     = false;
    };

    static uint64_t  PackEndpoint (const HostEndpoint & endpoint);

    Flow *  FindOrCreateFlow (uint16_t guestPort, uint64_t now);
    bool    TryMapDestination (uint32_t destinationIp, uint16_t destinationPort, HostEndpoint & outHost);
    bool    IsAllowed         (const Flow & flow, const HostEndpoint & from) const;
    void    DrainFlow         (uint16_t guestPort, Flow & flow, uint64_t now);

    IGuestPacketSink                  & m_sink;
    IHostNetwork                      * m_network    = nullptr;
    bool                                m_lanAccess  = false;
    uint32_t                            m_dnsServer  = 0;
    std::map<uint16_t, Flow>            m_flows;
    std::vector<Byte>                   m_buffer;
};
