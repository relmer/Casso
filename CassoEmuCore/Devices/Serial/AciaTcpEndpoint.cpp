#include "Pch.h"

#include "Devices/Serial/AciaTcpEndpoint.h"
#include "Devices/Acia6551.h"
#include "Devices/Serial/SerialLinkReports.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::AciaTcpEndpoint
//
////////////////////////////////////////////////////////////////////////////////

AciaTcpEndpoint::AciaTcpEndpoint (
    Acia6551           & acia,
    IHostNetwork       * network,
    SerialLinkReports  & reports,
    std::string          portTitle,
    Settings             settings)
    : m_acia      (acia),
      m_network   (network),
      m_reports   (reports),
      m_portTitle (std::move (portTitle)),
      m_settings  (std::move (settings)),
      m_link      (network)
{
    m_link.SetTelnet (m_settings.isTelnet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::OnAttached
//
//  The lines start as an unconnected cable has them. The port listens, or
//  the connect out starts, unless the settings or the PC prevent it, which
//  is reported.
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::OnAttached()
{
    HRESULT  hr          = S_OK;
    bool     isPortValid = m_settings.port >= 1 && m_settings.port <= kMaxPort;
    bool     hasTarget   = m_settings.isListen || !m_settings.host.empty();



    m_acia.ApplyHostLinesNow (true, false, false);

    CBRF (m_network != nullptr, Report ("The PC's network is not available."));
    CBRF (isPortValid,          Report ("The TCP port must be from 1 to 65535."));
    CBRF (hasTarget,            Report ("The TCP attachment has no host to connect to."));

    m_isUsable = true;

    if (m_settings.isListen)
    {
        StartListening();
    }
    else
    {
        StartConnect();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::StartListening
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::StartListening()
{
    HRESULT  hr      = S_OK;
    bool     isInUse = false;



    hr      = m_link.Listen ((uint16_t) m_settings.port);
    isInUse = hr == HRESULT_FROM_WIN32 (WSAEADDRINUSE);

    CBRF (!isInUse, Report (std::format ("Port {} is in use by another program.", m_settings.port)));
    CHRF (hr,       Report (std::format ("Cannot listen on port {}.", m_settings.port)));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::OnByteTransmitted
//
//  Dropped while no connection is open, as on an unplugged cable.
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::OnByteTransmitted (Byte value)
{
    m_link.Send (std::span<const Byte> (&value, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::Poll
//
//  Reads what fits in the chip's receive queue, passes each byte to the
//  chip, reports the connection opening or closing, and sets DCD and DSR to
//  match it. A connect out due again is started.
//
////////////////////////////////////////////////////////////////////////////////

bool AciaTcpEndpoint::Poll (uint64_t cycles)
{
    size_t                     queued      = m_acia.GetQueuedReceiveCount();
    size_t                     room        = (queued < kReceiveQueueLimit) ? kReceiveQueueLimit - queued : 0;
    TcpSerialLink::PollResult  result;
    bool                       isConnected = false;
    bool                       isRetryDue  = false;



    m_received.clear();
    m_link.Poll (room, m_received, result);

    for (Byte value : m_received)
    {
        m_acia.ReceiveFromHost (value, 0);
    }

    OnLinkEvents (result, cycles);

    isConnected = m_link.IsConnected();
    m_acia.SetHostLines (true, isConnected, isConnected);

    isRetryDue = m_retryAt != 0 && cycles >= m_retryAt && m_link.GetState() == TcpSerialLink::State::Idle;

    if (isRetryDue)
    {
        m_retryAt = 0;
        StartConnect();
    }

    return result.isBusy || result.isConnected || result.isAccepted || result.isClosed || result.isFailed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::DropConnection
//
//  The connection closes and the chip's lines drop at once, outside the
//  journal; the boundary keyframe taken next has them. A connect out starts
//  again later.
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::DropConnection()
{
    bool  wasOpen = m_link.GetState() != TcpSerialLink::State::Idle;



    m_link.Close();
    m_acia.ApplyHostLinesNow (true, false, false);

    if (wasOpen && m_isUsable && !m_settings.isListen)
    {
        m_retryAt = m_acia.GetElapsedCycles() + (uint64_t) kRetrySeconds * m_acia.GetCpuClockHz();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::StartConnect
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::StartConnect()
{
    HRESULT  hr = S_OK;



    hr = m_link.Connect (m_settings.host, (uint16_t) m_settings.port);
    CHRF (hr, Report (std::format ("Cannot connect to {}:{}.", m_settings.host, m_settings.port)));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::OnLinkEvents
//
//  A connect out that failed or a connection that closed starts again
//  kRetrySeconds later.
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::OnLinkEvents (
    const TcpSerialLink::PollResult  & result,
    uint64_t                           cycles)
{
    uint64_t  retry = cycles + (uint64_t) kRetrySeconds * m_acia.GetCpuClockHz();



    if (result.isConnected || result.isAccepted)
    {
        m_acia.NoteHostConnection (true, m_link.GetPeer());
        Report ("");
    }

    if (result.isClosed)
    {
        m_acia.NoteHostConnection (false, m_link.GetPeer());
    }

    if (result.isFailed)
    {
        ReportFailure (result);
    }

    if ((result.isClosed || result.isFailed) && !m_settings.isListen)
    {
        m_retryAt = retry;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::Report
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::Report (const std::string & text)
{
    m_reports.Post (m_portTitle, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaTcpEndpoint::ReportFailure
//
////////////////////////////////////////////////////////////////////////////////

void AciaTcpEndpoint::ReportFailure (const TcpSerialLink::PollResult & result)
{
    if (result.isLookupFailure)
    {
        Report (std::format ("Cannot find the host {}.", m_settings.host));
    }
    else
    {
        Report (std::format ("Cannot connect to {}:{}. The host is offline, or nothing is listening on that port.", m_settings.host, m_settings.port));
    }
}
