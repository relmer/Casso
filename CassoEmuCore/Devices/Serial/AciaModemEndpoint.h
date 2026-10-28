#pragma once

#include "Pch.h"

#include "Devices/IAciaEndpoint.h"
#include "Devices/Serial/HayesModem.h"
#include "Devices/Serial/TcpSerialLink.h"

class Acia6551;
class IHostNetwork;
class SerialLinkReports;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint
//
//  A 6551 attached to a Hayes-compatible modem that dials over TCP: a
//  HayesModem for the commands and results, and a TcpSerialLink for the
//  call, dialed out or, with a listen port, taken in as a ring.
//
//  The modem is equipment outside the machine. Its echo, its results and
//  the remote end's data are passed to the chip only from a poll, through
//  the chip's journaled host input, so a replay delivers the same bytes to
//  the guest at the same instructions without a modem. Bytes the guest sends are
//  passed to the modem as they finish shifting, with the emulated cycle,
//  which times the escape. The chip's DTR is passed to the modem on each
//  poll.
//
//  DSR and CTS are on while the modem is attached; DCD follows the call
//  unless &C0.
//
////////////////////////////////////////////////////////////////////////////////

class AciaModemEndpoint : public IAciaEndpoint
{
public:
    struct Settings
    {
        bool  isTelnet   = true;
        int   listenPort = 0;
    };

    AciaModemEndpoint (Acia6551           & acia,
                       IHostNetwork       * network,
                       SerialLinkReports  & reports,
                       std::string          portTitle,
                       Settings             settings);

    void  OnByteTransmitted (Byte value) override;
    void  OnAttached        () override;
    bool  IsHostLink        () const override { return true; }
    bool  Poll              (uint64_t cycles) override;
    void  DropConnection    () override;

    const HayesModem &  GetModem () const { return m_modem; }

private:
    static constexpr size_t  kReceiveQueueLimit = 64;
    static constexpr int     kMaxPort           = 65535;

    void  StartListening   ();
    void  CarryOutRequests ();
    void  OnLinkEvents     (const TcpSerialLink::PollResult & result, uint64_t cycles);
    void  Report           (const std::string & text);

    Acia6551           & m_acia;
    IHostNetwork       * m_network    = nullptr;
    SerialLinkReports  & m_reports;
    std::string          m_portTitle;
    Settings             m_settings;
    HayesModem           m_modem;
    TcpSerialLink        m_link;
    bool                 m_notedDtr   = false;   // the DTR last passed to the modem
    std::vector<Byte>    m_received;
};
