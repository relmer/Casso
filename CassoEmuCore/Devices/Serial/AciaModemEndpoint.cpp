#include "Pch.h"

#include "Devices/Serial/AciaModemEndpoint.h"
#include "Devices/Acia6551.h"
#include "Devices/Serial/SerialLinkReports.h"
#include "Devices/Serial/SerialLineFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::AciaModemEndpoint
//
////////////////////////////////////////////////////////////////////////////////

AciaModemEndpoint::AciaModemEndpoint (
    Acia6551           & acia,
    IHostNetwork       * network,
    SerialLinkReports  & reports,
    std::string          portTitle,
    Settings             settings)
    : m_acia      (acia),
      m_network   (network),
      m_reports   (reports),
      m_portTitle (std::move (portTitle)),
      m_settings  (settings),
      m_modem     (acia.GetCpuClockHz()),
      m_link      (network)
{
    m_link.SetTelnet (m_settings.isTelnet);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::OnAttached
//
//  The modem is on: DSR and CTS on, and DCD as the modem sets it. With a
//  listen port it waits for calls. Without the PC's network the modem takes
//  commands, and every dial fails.
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::OnAttached()
{
    HRESULT  hr          = S_OK;
    bool     isPortValid = m_settings.listenPort >= 0 && m_settings.listenPort <= kMaxPort;



    m_notedDtr = m_acia.IsDtrOn();
    m_acia.ApplyHostLinesNow (true, true, m_modem.GetDcd());

    CBRF (m_network != nullptr, Report ("The PC's network is not available."));
    CBRF (isPortValid,          Report ("The modem's listen port must be from 1 to 65535."));

    if (m_settings.listenPort != 0)
    {
        StartListening();
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::StartListening
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::StartListening()
{
    HRESULT  hr      = S_OK;
    bool     isInUse = false;



    hr      = m_link.Listen ((uint16_t) m_settings.listenPort);
    isInUse = hr == HRESULT_FROM_WIN32 (WSAEADDRINUSE);

    CBRF (!isInUse, Report (std::format ("Port {} is in use by another program.", m_settings.listenPort)));
    CHRF (hr,       Report (std::format ("Cannot listen on port {}.", m_settings.listenPort)));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::OnByteTransmitted
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::OnByteTransmitted (Byte value)
{
    m_modem.ReceiveFromGuest (value, m_acia.GetElapsedCycles());
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::Poll
//
//  The modem's timers and DTR, its requests to the link, the link's events
//  to the modem, then what is waiting for each side: the remote end's data,
//  read only in data mode and only as fast as the chip takes it, the
//  modem's output to the guest, and the guest's data to the remote end.
//
////////////////////////////////////////////////////////////////////////////////

bool AciaModemEndpoint::Poll (uint64_t cycles)
{
    size_t                     queued  = m_acia.GetQueuedReceiveCount();
    size_t                     room    = (queued < kReceiveQueueLimit) ? kReceiveQueueLimit - queued : 0;
    TcpSerialLink::PollResult  result;
    std::vector<Byte>          toGuest;
    std::vector<Byte>          toRemote;
    bool                       dtr     = m_acia.IsDtrOn();



    m_modem.SetBaudRate (m_acia.GetLineFormat().baud);

    if (dtr != m_notedDtr)
    {
        m_notedDtr = dtr;
        m_modem.OnDtrChanged (dtr, cycles);
    }

    m_modem.Tick (cycles);
    CarryOutRequests();

    m_received.clear();
    m_link.Poll (m_modem.IsDataMode() ? room : 0, m_received, result);
    OnLinkEvents (result, cycles);
    CarryOutRequests();

    toGuest  = m_modem.TakeToGuest();
    toRemote = m_modem.TakeToRemote();

    for (Byte value : toGuest)
    {
        m_acia.ReceiveFromHost (value, 0);
    }

    for (Byte value : m_received)
    {
        m_acia.ReceiveFromHost (value, 0);
    }

    m_link.Send (toRemote);
    m_acia.SetHostLines (true, true, m_modem.GetDcd());

    return result.isBusy || !toGuest.empty() || !toRemote.empty() || !m_received.empty() || m_modem.GetMode() == HayesModem::Mode::Dialing;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::CarryOutRequests
//
//  A dial without the PC's network fails at once. An answer requires nothing
//  more: the caller's connection is open already.
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::CarryOutRequests()
{
    HRESULT  hr = S_OK;



    for (const HayesModem::LinkRequest & request : m_modem.TakeLinkRequests())
    {
        switch (request.kind)
        {
        case HayesModem::LinkRequestKind::Dial:
            hr = m_link.Connect (request.host, request.port);

            if (FAILED (hr))
            {
                m_modem.OnDialFailed();
            }

            break;

        case HayesModem::LinkRequestKind::Answer:
            break;

        case HayesModem::LinkRequestKind::HangUp:
            m_link.Close();
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::OnLinkEvents
//
//  A call arriving while the modem is idle rings; one arriving at any other
//  time is closed. A failed lookup or connect is NO CARRIER.
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::OnLinkEvents (
    const TcpSerialLink::PollResult  & result,
    uint64_t                           cycles)
{
    bool  canRing = m_modem.GetMode() == HayesModem::Mode::Command && !m_modem.IsRinging();



    if (result.isConnected)
    {
        m_acia.NoteHostConnection (true, m_link.GetPeer());
        m_modem.OnConnected (cycles);
    }

    if (result.isAccepted && canRing)
    {
        m_acia.NoteHostConnection (true, m_link.GetPeer());
        m_modem.OnIncomingCall (cycles);
    }
    else if (result.isAccepted)
    {
        m_link.Close();
    }

    if (result.isFailed)
    {
        m_modem.OnDialFailed();
    }

    if (result.isClosed)
    {
        m_acia.NoteHostConnection (false, m_link.GetPeer());
        m_modem.OnRemoteClosed();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::DropConnection
//
//  The call ends, and NO CARRIER and the line drop go straight into the
//  chip, outside the journal; the boundary keyframe taken next has them.
//  NO CARRIER is sent when the guest had carrier at this point, whatever the
//  modem has done since.
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::DropConnection()
{
    bool  hadCarrier = m_acia.IsDcdTrue() && m_modem.DoesDcdFollowCarrier();



    m_link.Close();
    m_modem.DropCarrier (hadCarrier);
    (void) m_modem.TakeLinkRequests();

    for (Byte value : m_modem.TakeToGuest())
    {
        m_acia.ReceiveFromHostNow (value, 0);
    }

    m_acia.ApplyHostLinesNow (true, true, m_modem.GetDcd());
}





////////////////////////////////////////////////////////////////////////////////
//
//  AciaModemEndpoint::Report
//
////////////////////////////////////////////////////////////////////////////////

void AciaModemEndpoint::Report (const std::string & text)
{
    m_reports.Post (m_portTitle, text);
}
