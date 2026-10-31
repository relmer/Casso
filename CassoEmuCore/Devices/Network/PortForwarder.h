#pragma once

#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Seams/IHostNetwork.h"

class TcpNat;
class UdpNat;





////////////////////////////////////////////////////////////////////////////////
//
//  PortForwarder
//
//  The card's port forwards. A TCP forward is a host listener whose
//  accepted connections go to TcpNat; a UDP forward is a bound host socket
//  that becomes a permanent UdpNat flow. Both bind the loopback address
//  unless LAN access is on. A host port that cannot be bound becomes an
//  entry in the error list, which the shell reports when the machine starts.
//
////////////////////////////////////////////////////////////////////////////////

class PortForwarder
{
public:

    PortForwarder (TcpNat & tcp, UdpNat & udp) : m_tcp (tcp), m_udp (udp) {}

    void  Start     (IHostNetwork                    * network,
                     const std::vector<PortForward>  & forwards,
                     bool                              lanAccess);
    void  Stop      ();

    // Accepts pending connections. With `guestReady` false (socket 0 is not
    // in MACRAW) they are closed, since nothing would answer the SYN.
    void  Pump      (uint64_t now, bool guestReady);

    bool  IsActive  () const { return !m_listeners.empty(); }
    const std::vector<std::string> & GetErrors () const { return m_errors; }

private:

    struct Listener
    {
        std::unique_ptr<IHostSocket>  socket;
        uint16_t                      guestPort = 0;
    };

    HRESULT  OpenForward (IHostNetwork * network, const PortForward & forward, bool lanAccess);

    TcpNat                     & m_tcp;
    UdpNat                     & m_udp;
    std::vector<Listener>        m_listeners;
    std::vector<std::string>     m_errors;
};
