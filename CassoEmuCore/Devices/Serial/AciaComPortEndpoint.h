#pragma once

#include "Pch.h"

#include "Devices/IAciaEndpoint.h"
#include "Devices/Serial/SerialLineFormat.h"
#include "Seams/IHostSerialPort.h"

class Acia6551;
class SerialLinkReports;





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint
//
//  A 6551 attached to a PC serial port. Each poll sets the port to the
//  chip's format, DTR, RTS and break when they changed, as the chip has
//  them at that moment, so the port matches the chip after a seek through
//  history as well as after a register write. It then writes what the
//  guest sent, reads what arrived with the errors the driver latched, which
//  go with the last byte read, and passes the status lines to the chip.
//
//  A rate the port cannot run at is replaced by its nearest, reported once
//  per rate when they differ by more than kRateTolerance. A port that
//  fails, as an unplugged USB adapter does, is closed with DCD off, and
//  opened again every kReopenSeconds of emulated time until it opens.
//
////////////////////////////////////////////////////////////////////////////////

class AciaComPortEndpoint : public IAciaEndpoint
{
public:
    static constexpr uint32_t  kReopenSeconds     = 2;
    static constexpr size_t    kReceiveQueueLimit = 64;
    static constexpr double    kRateTolerance     = 0.02;

    AciaComPortEndpoint (Acia6551           & acia,
                         IHostSerialPorts   * ports,
                         SerialLinkReports  & reports,
                         std::string          portTitle,
                         std::string          portName);

    void  OnByteTransmitted (Byte value) override;
    void  OnAttached        () override;
    bool  IsHostLink        () const override { return true; }
    bool  Poll              (uint64_t cycles) override;

    // The nearest rate a port can run at to `baud`.
    static uint32_t  ChooseRate (double baud, const std::vector<uint32_t> & rates, bool isAnyRate);

private:
    void     Open          ();
    void     Close         (const std::string & problem);
    HRESULT  ApplyControls ();
    HRESULT  ApplyFormat   ();
    HRESULT  Transfer      (bool & outIsBusy);
    void     PassReceived  (const Byte * bytes, size_t count, const SerialPortErrors & errors);
    void     Report        (const std::string & text);

    Acia6551                          & m_acia;
    IHostSerialPorts                  * m_ports          = nullptr;
    SerialLinkReports                 & m_reports;
    std::string                         m_portTitle;
    std::string                         m_portName;
    std::unique_ptr<IHostSerialPort>    m_port;
    std::vector<uint32_t>               m_rates;
    bool                                m_isAnyRate      = false;
    std::vector<Byte>                   m_outgoing;
    uint64_t                            m_reopenAt       = 0;      // the elapsed cycle count to open the port again; 0 for none
    bool                                m_isSet          = false;  // the port has the format and controls below
    SerialLineFormat                    m_format;
    bool                                m_dtr            = false;
    bool                                m_rts            = false;
    bool                                m_isBreak        = false;
    Byte                                m_pendingErrors  = 0;      // errors latched with no byte, for the next one
    std::set<uint32_t>                  m_reportedRates;           // 6551 rates already reported as replaced
};
