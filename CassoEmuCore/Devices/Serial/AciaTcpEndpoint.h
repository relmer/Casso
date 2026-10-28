#pragma once

#include "Pch.h"

#include "Devices/IAciaEndpoint.h"
#include "Devices/Serial/TcpSerialLink.h"

class Acia6551;
class IHostNetwork;
class SerialLinkReports;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint
//
//  A 6551 attached to TCP: connected out to a host and port, or listening on
//  a port for one connection at a time. DCD and DSR are on while a
//  connection is open and CTS is always on. A connect out that fails, or a
//  connection the far end closes, starts again every kRetrySeconds of
//  emulated time, as a cable plugged back in.
//
//  What is received is read from the PC no faster than the 6551 takes it:
//  at most kReceiveQueueLimit bytes wait in the chip, and the rest wait in
//  the PC's socket buffer. Problems go to the machine's SerialLinkReports
//  under the port's title.
//
////////////////////////////////////////////////////////////////////////////////

class AciaTcpEndpoint : public IAciaEndpoint
{
public:
    struct Settings
    {
        std::string  host;
        int          port     = 0;
        bool         isListen = false;
        bool         isTelnet = false;
    };

    static constexpr size_t    kReceiveQueueLimit = 64;
    static constexpr uint32_t  kRetrySeconds      = 5;

    AciaTcpEndpoint (Acia6551           & acia,
                     IHostNetwork       * network,
                     SerialLinkReports  & reports,
                     std::string          portTitle,
                     Settings             settings);

    void  OnByteTransmitted (Byte value) override;
    void  OnAttached        () override;
    bool  IsHostLink        () const override { return true; }
    bool  Poll              (uint64_t cycles) override;
    void  DropConnection    () override;

    const TcpSerialLink &  GetLink () const { return m_link; }

private:
    static constexpr int  kMaxPort = 65535;

    void  StartConnect   ();
    void  StartListening ();
    void  OnLinkEvents   (const TcpSerialLink::PollResult & result, uint64_t cycles);
    void  Report         (const std::string & text);
    void  ReportFailure  (const TcpSerialLink::PollResult & result);

    Acia6551           & m_acia;
    IHostNetwork       * m_network        = nullptr;
    SerialLinkReports  & m_reports;
    std::string          m_portTitle;
    Settings             m_settings;
    TcpSerialLink        m_link;
    bool                 m_isUsable       = false;  // the settings are valid and the network present
    uint64_t             m_retryAt        = 0;      // the elapsed cycle count at which to connect again; 0 for none
    std::vector<Byte>    m_received;
};
