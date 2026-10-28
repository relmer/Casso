#include "Pch.h"

#include "Devices/Serial/AciaComPortEndpoint.h"
#include "Devices/Acia6551.h"
#include "Devices/Serial/SerialLinkReports.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::AciaComPortEndpoint
//
////////////////////////////////////////////////////////////////////////////////

AciaComPortEndpoint::AciaComPortEndpoint (
    Acia6551           & acia,
    IHostSerialPorts   * ports,
    SerialLinkReports  & reports,
    std::string          portTitle,
    std::string          portName)
    : m_acia      (acia),
      m_ports     (ports),
      m_reports   (reports),
      m_portTitle (std::move (portTitle)),
      m_portName  (std::move (portName))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::OnAttached
//
//  DCD and DSR are off until the port's own lines are read on the first
//  poll.
//
////////////////////////////////////////////////////////////////////////////////

void AciaComPortEndpoint::OnAttached()
{
    HRESULT  hr      = S_OK;
    bool     hasName = !m_portName.empty();



    m_port.reset();
    m_outgoing.clear();
    m_isSet         = false;
    m_reopenAt      = 0;
    m_pendingErrors = 0;

    m_acia.ApplyHostLinesNow (true, false, false);

    CBRF (m_ports != nullptr, Report ("The PC's serial ports are not available."));
    CBRF (hasName,            Report ("No COM port is selected."));

    Open();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::OnByteTransmitted
//
//  Written on the next poll; dropped while the port is closed.
//
////////////////////////////////////////////////////////////////////////////////

void AciaComPortEndpoint::OnByteTransmitted (Byte value)
{
    if (m_port != nullptr)
    {
        m_outgoing.push_back (value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::Poll
//
////////////////////////////////////////////////////////////////////////////////

bool AciaComPortEndpoint::Poll (uint64_t cycles)
{
    HRESULT  hr       = S_OK;
    bool     isBusy   = false;
    bool     isReopen = m_port == nullptr && m_reopenAt != 0 && cycles >= m_reopenAt;



    if (isReopen)
    {
        m_reopenAt = 0;
        Open();
    }

    BAIL_OUT_IF (m_port == nullptr, S_OK);

    hr = ApplyControls();
    CHRF (hr, Close (std::format ("{} was disconnected.", m_portName)));

    hr = Transfer (isBusy);
    CHRF (hr, Close (std::format ("{} was disconnected.", m_portName)));

Error:
    return isBusy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::Open
//
//  A port that cannot be opened is tried again kReopenSeconds later, so one
//  plugged in or let go of by another program is taken up.
//
////////////////////////////////////////////////////////////////////////////////

void AciaComPortEndpoint::Open()
{
    HRESULT                           hr     = S_OK;
    std::unique_ptr<IHostSerialPort>  port;
    bool                              isGone = false;
    bool                              isBusy = false;



    hr     = m_ports->Open (m_portName, port);
    isGone = hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
    isBusy = hr == HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

    CBRF (!isGone, Report (std::format ("{} is not on this PC.", m_portName)));
    CBRF (!isBusy, Report (std::format ("{} is in use by another program.", m_portName)));
    CHRF (hr,      Report (std::format ("Cannot open {}.", m_portName)));

    hr = port->GetBaudRates (m_rates, m_isAnyRate);
    CHRF (hr, Report (std::format ("Cannot open {}.", m_portName)));

    m_port  = std::move (port);
    m_isSet = false;
    Report ("");

Error:
    if (FAILED (hr))
    {
        m_reopenAt = m_acia.GetElapsedCycles() + (uint64_t) kReopenSeconds * m_acia.GetCpuClockHz();
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::Close
//
//  The port failed: it is closed, DCD and DSR drop, and it is opened again
//  later.
//
////////////////////////////////////////////////////////////////////////////////

void AciaComPortEndpoint::Close (const std::string & problem)
{
    m_port.reset();
    m_outgoing.clear();
    m_isSet    = false;
    m_reopenAt = m_acia.GetElapsedCycles() + (uint64_t) kReopenSeconds * m_acia.GetCpuClockHz();

    Report (problem);
    m_acia.SetHostLines (true, false, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::ApplyControls
//
//  The chip's format, DTR, RTS and break, each set on the port when it
//  differs from what the port was last given.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AciaComPortEndpoint::ApplyControls()
{
    HRESULT           hr      = S_OK;
    SerialLineFormat  format  = m_acia.GetLineFormat();
    bool              dtr     = m_acia.IsDtrOn();
    bool              rts     = m_acia.IsRtsOn();
    bool              isBreak = (m_acia.GetCommand() & Acia6551::kCommandTicMask) == Acia6551::kCommandTicBreak;



    if (!m_isSet || !(format == m_format))
    {
        m_format = format;
        hr       = ApplyFormat();
        CHR (hr);
    }

    if (!m_isSet || dtr != m_dtr || rts != m_rts)
    {
        m_dtr = dtr;
        m_rts = rts;
        hr    = m_port->SetControl (dtr, rts);
        CHR (hr);
    }

    if (!m_isSet || isBreak != m_isBreak)
    {
        m_isBreak = isBreak;
        hr        = m_port->SetBreak (isBreak);
        CHR (hr);
    }

    m_isSet = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::ApplyFormat
//
//  The port runs at the nearest rate it has; a replacement off by more than
//  kRateTolerance is reported the first time the guest asks for that rate.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AciaComPortEndpoint::ApplyFormat()
{
    constexpr double  kHundredths = 100.0;
    HRESULT           hr          = S_OK;
    SerialLineFormat  applied     = m_format;
    uint32_t          rate        = ChooseRate (m_format.baud, m_rates, m_isAnyRate);
    uint32_t          key         = (uint32_t) llround (m_format.baud * kHundredths);
    bool              isFar       = std::abs (rate - m_format.baud) > m_format.baud * kRateTolerance;



    if (isFar && !m_reportedRates.contains (key))
    {
        m_reportedRates.insert (key);
        Report (std::format ("{:g} baud is not available on {}. It runs at {} baud.", m_format.baud, m_portName, rate));
    }

    applied.baud = rate;

    hr = m_port->Configure (applied);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::Transfer
//
//  Writes what the guest sent, reads what fits in the chip's receive queue,
//  and passes the port's status lines to the chip.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AciaComPortEndpoint::Transfer (bool & outIsBusy)
{
    HRESULT                                 hr      = S_OK;
    std::array<Byte, kReceiveQueueLimit>    buffer  = {};
    size_t                                  queued  = m_acia.GetQueuedReceiveCount();
    size_t                                  room    = (queued < kReceiveQueueLimit) ? kReceiveQueueLimit - queued : 0;
    size_t                                  written = 0;
    size_t                                  read    = 0;
    SerialPortErrors                        errors;
    SerialPortLines                         lines;



    if (!m_outgoing.empty())
    {
        hr = m_port->Write (m_outgoing, written);
        CHR (hr);

        m_outgoing.erase (m_outgoing.begin(), m_outgoing.begin() + written);
    }

    hr = m_port->Read (std::span<Byte> (buffer.data(), room), read, errors);
    CHR (hr);

    PassReceived (buffer.data(), read, errors);

    hr = m_port->GetLines (lines);
    CHR (hr);

    m_acia.SetHostLines (lines.cts, lines.dsr, lines.dcd);

    outIsBusy = written != 0 || read != 0;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::PassReceived
//
//  The driver does not give the position of a byte in error, so its errors
//  go with the last byte read, the one arriving when they were latched. A
//  break arrives as $00 with a framing error, as the 6551 receives one.
//  Errors latched with no byte wait for the next.
//
////////////////////////////////////////////////////////////////////////////////

void AciaComPortEndpoint::PassReceived (
    const Byte               * bytes,
    size_t                     count,
    const SerialPortErrors   & errors)
{
    Byte    errorBits = m_pendingErrors;
    size_t  i         = 0;



    errorBits |= (errors.isFraming || errors.isBreak) ? Acia6551::kRxErrorFraming : 0;
    errorBits |= errors.isParity                      ? Acia6551::kRxErrorParity  : 0;
    errorBits |= errors.isOverrun                     ? Acia6551::kRxErrorOverrun : 0;

    m_pendingErrors = 0;

    if (count == 0 && errors.isBreak)
    {
        m_acia.ReceiveFromHost (0, errorBits);
        return;
    }

    if (count == 0)
    {
        m_pendingErrors = errorBits;
        return;
    }

    for (i = 0; i < count; i++)
    {
        m_acia.ReceiveFromHost (bytes[i], (i + 1 == count) ? errorBits : 0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::ChooseRate
//
////////////////////////////////////////////////////////////////////////////////

uint32_t AciaComPortEndpoint::ChooseRate (
    double                          baud,
    const std::vector<uint32_t>   & rates,
    bool                            isAnyRate)
{
    uint32_t  chosen = (uint32_t) std::max (1LL, llround (baud));



    if (isAnyRate || rates.empty())
    {
        return chosen;
    }

    chosen = rates.front();

    for (uint32_t rate : rates)
    {
        if (std::abs (rate - baud) < std::abs (chosen - baud))
        {
            chosen = rate;
        }
    }

    return chosen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaComPortEndpoint::Report
//
////////////////////////////////////////////////////////////////////////////////

void AciaComPortEndpoint::Report (const std::string & text)
{
    m_reports.Post (m_portTitle, text);
}
