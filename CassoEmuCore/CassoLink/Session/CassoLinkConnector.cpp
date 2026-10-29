#include "Pch.h"

#include "CassoLink/Session/CassoLinkConnector.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSearch.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::CassoLinkConnector
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnector::CassoLinkConnector (CassoLinkSession & session)
    : m_session (session)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::StartWithBootstrap
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnector::StartWithBootstrap (const CassoLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation)
{
    HRESULT  hr   = S_OK;
    bool     idle = !m_thread.joinable();



    CBRAEx (idle, E_UNEXPECTED);

    m_bootstrap        = std::make_shared<LinkOperation>();
    m_bootstrap->id    = m_nextId++;
    m_bootstrap->kind  = LinkOperationKind::Bootstrap;
    outOperation       = m_bootstrap;

    hr = m_session.Start (settings);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::RunBootstrap
//
//  The loader is typed at the bootstrap speed, and the session then waits on
//  the same line for the shim's Ready. No Apple on any port replies before
//  the loader runs, so a serial link with no port set cannot search: the
//  loader goes to the PC's only COM port, and with none or several the user
//  enters the port.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnector::RunBootstrap (std::stop_token stop, LinkOperation & operation)
{
    HRESULT                    hr      = S_OK;
    std::vector<std::string>   lines;
    std::vector<std::wstring>  ports;
    bool                       hasPort = m_session.m_settings.transport != LinkTransport::Serial || !m_port.empty();
    bool                       typable = m_session.m_settings.transport != LinkTransport::Uthernet;



    operation.state = LinkOperationState::Running;

    //  The loader is typed into a serial port; an Uthernet II has none.
    if (!typable)
    {
        operation.message = "An Uthernet II link cannot be bootstrapped. Boot the link disk on the Apple instead.";
    }

    CBREx (typable, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (!hasPort)
    {
        hr = m_session.m_factory.ListComPorts (ports);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = S_OK;

        if (ports.size() == 1)
        {
            m_port  = ports[0];
            hasPort = true;
        }
        else if (ports.empty())
        {
            operation.message = "No COM port was found to bootstrap the Apple on. Connect the serial adapter, then choose Bootstrap again.";
        }
        else
        {
            operation.message = "Enter the COM port the cable is on: choose Edit connection and type it in COM port, such as "
                              + TextEncoding::WideToNarrow (ports[0]) + ".";
        }
    }

    CBREx (hasPort, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = LinkBootstrap::BuildLines (m_session.m_settings.bootstrapBaud, lines);
    CHR (hr);

    m_session.SetState (LinkState::Bootstrapping);

    hr = OpenStream (m_session.m_settings.bootstrapBaud, stop);
    CHR (hr);

    hr = LinkBootstrap::Send (*m_stream, lines, m_bootstrapPauseMs, [&stop]() { return stop.stop_requested(); }, m_charPauseUs);
    CHR (hr);

    //  The shim starts on the open line and sends Ready unprompted. With an
    //  Idle as well, the shim sends a second Ready in reply to the Idle while
    //  the host's reply to the first arrives, and the 6551's one-byte buffer
    //  overruns.
    m_prompted = true;

    hr = Connect (stop, m_session.m_settings.bootstrapBaud);
    CHR (hr);

Error:
    operation.result = hr;
    operation.state  = SUCCEEDED (hr) ? LinkOperationState::Done : LinkOperationState::Failed;

    if (FAILED (hr) && operation.message.empty())
    {
        operation.message = "The Apple did not answer after the bootstrap. Check that IN# with the port or the card's slot, and Ctrl+A 14B, were typed on it.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::Connect
//
//  A serial line is opened at the bootstrap speed, where a typed-in shim
//  starts, and then at the link's speed, where a shim from a boot disk or a
//  program already loaded runs. A TCP line has no speed of its own; it is
//  opened once, as the Apple end starts at the link's speed. The
//  program is then set to the link's speed, and a serial port opened again
//  at it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnector::Connect (std::stop_token stop, DWORD openAt)
{
    HRESULT             hr       = S_OK;
    std::vector<DWORD>  speeds   = { m_session.m_settings.baud };
    ComSettings         search;
    DWORD               opened   = 0;
    Byte                control  = 0;
    Byte                driveMap = 0;
    bool                ready    = false;
    std::wstring        model;
    HRESULT             logged   = S_OK;



    //  After a bootstrap the line is open already, at the bootstrap speed.
    //  A line that read the Apple's Ready between attempts stays at its
    //  speed. The bootstrap speed is added only on a line just opened, for
    //  a shim typed in that is waiting at it; on a line open already, and
    //  once ADTPro's client has made a request, an attempt at the bootstrap
    //  speed would garble ADTPro's requests at the link's speed. The port opens
    //  at it after the link's speed, since an Apple that ran the link before
    //  waits at that speed, and a shim typed in at the keyboard is rarer.
    if (openAt != 0)
    {
        speeds = { openAt };
    }
    else if (m_session.m_host && (m_session.m_host->HasClient() || m_session.m_host->HasAdtProClient()))
    {
        speeds = { m_openBaud };
    }
    else if (m_session.m_settings.transport == LinkTransport::Serial && m_session.m_settings.baud != m_session.m_settings.bootstrapBaud && !m_adtPro && !IsLineOpen())
    {
        speeds.push_back (m_session.m_settings.bootstrapBaud);
    }

    //  A line kept open between attempts is prompted again on each one. A
    //  boot disk that started after the last prompt -- a universal disk asks
    //  for an address first, and an Apple can be turned off and on with the
    //  line open -- sends no Ready of its own, and was never asked again.
    if (openAt == 0 && IsLineOpen() && m_session.m_host && !m_session.m_host->HasClient() && !m_session.m_host->HasAdtProClient())
    {
        m_prompted = false;
    }

    //  A serial link with no port set finds one, once; the port found is
    //  used again when the line drops.
    if (openAt == 0 && m_session.m_settings.transport == LinkTransport::Serial && m_port.empty())
    {
        m_session.SetState (LinkState::Searching);

        search.hardwareHandshake = m_session.m_settings.hardwareHandshake;

        hr = CassoLinkSearch::Find (m_session.m_factory, search, speeds, CassoLinkSearch::kReplyTimeoutMs, m_port, stop);
        CHR (hr);
    }

    for (DWORD speed : speeds)
    {
        if (ready || stop.stop_requested())
        {
            break;
        }

        if (openAt == 0)
        {
            m_session.SetState (LinkState::Opening);

            hr = OpenStream (speed, stop);
            CHR (hr);
        }

        opened = speed;

        hr = AwaitProgram (stop, m_connectTimeoutMs / (DWORD) speeds.size(), ready);
        CHR (hr);
    }

    CBREx (ready, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    //  An Uthernet II has no speed to set.
    if (m_session.m_settings.transport != LinkTransport::Uthernet)
    {
        LinkProgram::GetSpeedControl (m_session.m_settings.baud, control);

        hr = m_session.m_host->SetSpeed (control);
        CHR (hr);
    }

    //  A port opened again at the link's speed has a new host, and the link
    //  is ready once the program's Ready arrives on it. The program sends
    //  Ready again after each second with no reply, so no Idle is sent.
    if (m_session.m_settings.transport == LinkTransport::Serial && opened != m_session.m_settings.baud)
    {
        hr = OpenStream (m_session.m_settings.baud, stop);
        CHR (hr);

        m_prompted = true;

        hr = AwaitProgram (stop, m_connectTimeoutMs, ready);
        CHR (hr);

        CBREx (ready, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    }

    driveMap = m_session.m_host->GetClient().driveMap;
    model    = m_session.m_host->GetClient().GetModelName();

    //  What was read before is read again: the line may have dropped in the
    //  middle of a write, or the disks may have been changed while it was
    //  down.
    for (int i = 0; i < CassoLinkSessionTypes::kDriveCount; i++)
    {
        m_session.m_disks[(size_t) i]->Clear();
        m_session.m_listings[(size_t) i].reset();
    }

    m_driveMap = driveMap;

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_modelName = model;
    }

    m_session.SetState (LinkState::Ready);

    logged = LogOnApple ("PC CONNECTED");
    IGNORE_RETURN_VALUE (logged, S_OK);

Error:
    //  With no Apple on it, the line stays open, and the wait before the
    //  next attempt serves it. After a load that stopped partway, the
    //  Apple's shim can be at another speed, so the line is closed, and the
    //  next attempt opens it at each speed again.
    if (hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT) && IsLineOpen() && m_state != LinkState::LoadingProgram)
    {
        m_session.SetState (LinkState::Disconnected);
    }
    else if (FAILED (hr))
    {
        m_session.Disconnect();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::LogOnApple
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnector::LogOnApple (const std::string & text)
{
    HRESULT     hr  = S_OK;
    SYSTEMTIME  now = {};



    GetLocalTime (&now);

    hr = m_session.m_host->SendLog (now.wHour, now.wMinute, text);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::IsLineOpen
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnector::IsLineOpen() const
{
    return m_session.m_host && m_stream && !m_stream->IsClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::ServeUntilRetry
//
//  The line, while it is open, is served until the next attempt is due, a
//  request is queued or the Apple's Ready arrives; with no line open, the
//  wait is only for the next attempt or a request.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnector::ServeUntilRetry (std::stop_token stop)
{
    auto     deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (m_retryMs);
    bool     queued   = false;
    bool     ready    = false;
    HRESULT  hr       = S_OK;



    while (!stop.stop_requested() && !queued && !ready && std::chrono::steady_clock::now() < deadline)
    {
        if (!m_session.m_host)
        {
            std::unique_lock<std::mutex>  lock (m_lock);

            m_wake.wait_until (lock, stop, deadline, [this] () { return !m_queue.empty(); });
            break;
        }

        hr = m_session.m_host->ServeOne (CassoLinkSession::kIdlePollMs);

        if (FAILED (hr))
        {
            m_session.Disconnect();
            continue;
        }

        ready = m_session.m_host->HasClient();

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            queued = !m_queue.empty();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::ServeAdtPro
//
//  Casso's operations have no client here, so any queued fails at once.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnector::ServeAdtPro (std::stop_token stop)
{
    HRESULT  hr = S_OK;



    while (!stop.stop_requested())
    {
        {
            std::lock_guard<std::mutex>  lock (m_lock);

            for (const std::shared_ptr<LinkOperation> & queued : m_queue)
            {
                queued->result  = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
                queued->message = "An ADTPro-compatible server serves only ADTPro's client.";
                queued->state   = LinkOperationState::Failed;
                m_session.m_sink.OnDone (queued);
            }

            m_queue.clear();
        }

        if (!IsLineOpen())
        {
            m_session.SetState (LinkState::Opening);

            hr = OpenStream (m_session.m_settings.baud, stop);

            if (FAILED (hr))
            {
                m_lostReason = CassoLinkSession::DescribeLineFailure (hr);
                m_session.SetState (LinkState::Disconnected);

                {
                    std::unique_lock<std::mutex>  lock (m_lock);

                    m_wake.wait_for (lock, stop, std::chrono::milliseconds (m_retryMs), [] () { return false; });
                }

                continue;
            }

            m_session.SetState (LinkState::WaitingForReady);
        }

        hr = m_session.m_host->ServeOne (CassoLinkSession::kIdlePollMs);

        if (FAILED (hr))
        {
            m_lostReason = CassoLinkSession::DescribeLineFailure (hr);
            m_session.Disconnect();
            continue;
        }

        if (m_session.m_host->HasAdtProClient() && m_state != LinkState::Ready)
        {
            m_session.SetState (LinkState::Ready);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::OpenStream
//
//  A serial port at the given speed; the TCP transports connect, or, for a
//  Wi-Fi modem, wait for it to connect. A host engine is made for each new
//  stream.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnector::OpenStream (DWORD baud, std::stop_token stop)
{
    HRESULT                       hr        = S_OK;
    std::unique_ptr<IByteStream>  stream;
    ComSettings                   com;
    std::string                   host;
    uint16_t                      port      = 0;
    size_t                        colon     = m_session.m_settings.address.rfind (':');
    unsigned                      parsed    = 0;
    std::from_chars_result        digits    = {};
    bool                          addressed = false;
    bool                          reusable  = IsLineOpen() && (!m_session.m_settings.IsComPort() || m_openBaud == baud);



    //  A line open already at the speed is used as it is, so no byte from
    //  the Apple is dropped while the line is closed and opened again.
    BAIL_OUT_IF (reusable, S_OK);

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        if (m_stream)
        {
            m_stream->Close();
        }

        m_session.m_host.reset();
        m_stream.reset();
    }

    if (m_session.m_settings.IsComPort())
    {
        com.baud              = baud;
        com.hardwareHandshake = m_session.m_settings.hardwareHandshake;

        hr = m_session.m_factory.OpenComPort (m_port, com, stream);
        CHR (hr);
    }
    else
    {
        //  The address comes from the saved settings, so a damaged one fails
        //  the connection rather than throwing.
        if (colon != std::string::npos)
        {
            digits = std::from_chars (m_session.m_settings.address.data() + colon + 1, m_session.m_settings.address.data() + m_session.m_settings.address.size(), parsed);
        }

        addressed = colon != std::string::npos && digits.ec == std::errc() && digits.ptr == m_session.m_settings.address.data() + m_session.m_settings.address.size()
                 && parsed >= 1 && parsed <= 65535;
        CBREx (addressed, E_INVALIDARG);

        host = m_session.m_settings.address.substr (0, colon);
        port = (uint16_t) parsed;

        if (m_session.m_settings.transport == LinkTransport::WifiModem)
        {
            hr = m_session.m_factory.ListenTcp (port, m_session.m_settings.transport, stream);
        }
        else
        {
            hr = m_session.m_factory.ConnectTcp (host, port, stop, stream);
        }

        CHR (hr);
    }

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_stream         = std::shared_ptr<IByteStream> (std::move (stream));
        m_session.m_host = std::make_unique<CassoLinkHost> (m_stream, m_session.m_folder, [] ()
        {
            SYSTEMTIME  now = {};

            GetLocalTime (&now);
            return CassoLinkAdtProServer::GetProDosDateTime (now);
        });

        m_session.m_host->SetActivitySink ([this] (const std::string & text) { m_session.m_sink.OnActivity (text); });
        m_session.m_host->SetProgramChip (CassoLinkSession::GetLinkChip (m_session.m_settings.transport));
        m_session.m_host->SetClients (m_session.m_settings.transport == LinkTransport::AdtProServer ? CassoLinkHost::Clients::AdtPro : CassoLinkHost::Clients::Casso);
        m_openBaud = baud;
        m_prompted = false;

        //  DSR is judged on each line from its own first on: a USB serial
        //  adapter just opened reports DSR off until its driver polls it,
        //  which on a line opened again would read as the Apple gone.
        m_peerSeen       = false;
        m_peerOffSinceMs = 0;
    }

Error:
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_openFailure = FAILED (hr) ? DescribeOpenFailure (hr, port) : std::string();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::DescribeOpenFailure
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkConnector::DescribeOpenFailure (HRESULT hr, uint16_t port)
{
    std::string  text;



    if (hr == HRESULT_FROM_WIN32 (WSAEADDRINUSE))
    {
        text = std::format ("TCP port {} is in use by another program.", port);
    }
    else
    {
        text = "The connection could not be opened: " + CassoLinkSession::DescribeLineFailure (hr) + ".";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::AwaitProgram
//
//  Serves the line until the current program sends Ready, the shim's Ready
//  having the program loaded on the way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnector::AwaitProgram (std::stop_token stop, DWORD timeoutMs, bool & outReady)
{
    HRESULT  hr       = S_OK;
    auto     deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);



    outReady = false;
    m_session.SetState (LinkState::WaitingForReady);

    while (!outReady && !stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
    {
        //  An Apple that sent its Ready before the line was open is waiting
        //  for a reply, and is prompted for another, once for each line
        //  opened. A prompt is an envelope, which ADTPro's client, idle on
        //  the same line, would read as the start of its next reply.
        if (!m_prompted && !m_session.m_host->HasClient() && !m_session.m_host->HasAdtProClient())
        {
            hr = m_session.m_host->PromptReady();
            CHR (hr);

            m_prompted = true;
        }

        hr = m_session.m_host->ServeOne (CassoLinkSession::kIdlePollMs);
        CHR (hr);

        //  ADTPro's client is served for as long as it is there, rather
        //  than dropped when Casso's program does not answer.
        if (m_session.m_host->HasAdtProClient())
        {
            m_adtPro = true;
            deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        }

        if (m_session.m_host->HasClient())
        {
            m_adtPro = false;
            outReady = m_session.m_host->GetClient().programVersion == LinkProgram::kProgramVersion;

            if (!outReady)
            {
                m_session.SetState (LinkState::LoadingProgram);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnector::IsPeerGone
//
//  A serial line whose DSR was on, as the Apple's DTR is while its 6551 is
//  set up, and has been off for kPeerOffMs: the Apple was reset or turned
//  off. A USB serial adapter can report DSR off for a millisecond at a time
//  on a line that is up, many times a second, so one reading off does not
//  count. A cable that does not wire the line has it off throughout, and
//  only silence counts.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnector::IsPeerGone()
{
    bool      ready = false;
    bool      known = m_stream && m_stream->TryGetPeerReady (ready);
    uint64_t  now   = GetTickCount64();



    m_peerSeen = m_peerSeen || (known && ready);

    if (!known || ready)
    {
        m_peerOffSinceMs = 0;
    }
    else if (m_peerOffSinceMs == 0)
    {
        m_peerOffSinceMs = now;
    }

    return m_peerSeen && m_peerOffSinceMs != 0 && now - m_peerOffSinceMs >= kPeerOffMs;
}
