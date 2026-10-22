#include "Pch.h"

#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSearch.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/DurableCommit.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Seams/IByteStream.h"
#include "Seams/IByteStreamFactory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::CassoLinkSession
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkSession::CassoLinkSession (IByteStreamFactory & factory, ICassoLinkFolder & folder, IDiskFileIo & fileIo, ICassoLinkSessionSink & sink) :
    m_factory (factory),
    m_folder  (folder),
    m_fileIo  (fileIo),
    m_sink    (sink)
{
    for (int i = 0; i < kDriveCount; i++)
    {
        m_sources[(size_t) i] = std::make_unique<DriveSource> (*this, i + 1);
        m_disks[(size_t) i]   = std::make_unique<LinkedDisk> (*m_sources[(size_t) i]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::~CassoLinkSession
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkSession::~CassoLinkSession()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::Start (const CassoLinkSettings & settings)
{
    HRESULT  hr      = S_OK;
    Byte     control = 0;
    bool     known   = false;
    bool     idle    = !m_thread.joinable();



    CBRAEx (idle, E_UNEXPECTED);

    known = LinkProgram::GetSpeedControl (settings.baud, control) && LinkProgram::GetSpeedControl (settings.bootstrapBaud, control);
    CBREx (known, E_INVALIDARG);

    m_settings = settings;
    m_port     = TextEncoding::NarrowToWide (settings.port);
    m_stopping = false;
    m_thread   = std::jthread ([this] (std::stop_token stop) { Run (stop); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Stop
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Stop()
{
    if (m_thread.joinable())
    {
        m_stopping = true;
        m_thread.request_stop();
        m_wake.notify_all();

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            if (m_stream)
            {
                m_stream->Close();
            }
        }

        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Cancel()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    if (m_running && m_host)
    {
        m_host->Cancel();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::Enumerate (int drive, const FilePath & directory)
{
    return Queue (LinkOperationKind::Enumerate, drive, directory);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::ReadFile
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::ReadFile (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::ReadFile, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::WriteFile
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::WriteFile (int drive, const FilePath & path, const FilePayload & payload)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->payload = payload;

    return Queue (LinkOperationKind::WriteFile, drive, path, operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Delete
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::Delete (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::Delete, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Rename
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::Rename (int drive, const FilePath & path, const std::string & newName)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->newName = newName;

    return Queue (LinkOperationKind::Rename, drive, path, operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::MakeDirectory
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::MakeDirectory (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::MakeDirectory, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::RemoveDirectory
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::RemoveDirectory (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::RemoveDirectory, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::CheckLinkDisk
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::CheckLinkDisk (int drive)
{
    return Queue (LinkOperationKind::CheckLinkDisk, drive, FilePath());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::ReadDisk
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::ReadDisk (int drive, const std::string & imagePath, bool blankUnreadable)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->imagePath       = imagePath;
    operation->blankUnreadable = blankUnreadable;

    return Queue (LinkOperationKind::ReadDisk, drive, FilePath(), operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::WriteDisk
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::WriteDisk (int drive, const std::string & imagePath, const std::string & formatted)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->imagePath = imagePath;
    operation->formatted = formatted;

    return Queue (LinkOperationKind::WriteDisk, drive, FilePath(), operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::GetLinkChip
//
////////////////////////////////////////////////////////////////////////////////

LinkChip CassoLinkSession::GetLinkChip (LinkTransport transport)
{
    return (transport == LinkTransport::Uthernet) ? LinkChip::W5100 : LinkChip::Acia;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::GetLogicalSector
//
////////////////////////////////////////////////////////////////////////////////

int CassoLinkSession::GetLogicalSector (int physical)
{
    return NibblizationLayer::GetDosFileIndexForPhysicalSector (physical);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::GetPhysicalSector
//
////////////////////////////////////////////////////////////////////////////////

int CassoLinkSession::GetPhysicalSector (int logical)
{
    int  found = 0;



    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        if (GetLogicalSector (physical) == logical)
        {
            found = physical;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::FromStatusError
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::FromStatusError (Byte error)
{
    HRESULT  hr = S_OK;



    switch (error)
    {
        case CassoLinkHost::kErrorNone:           hr = S_OK;                                         break;
        case CassoLinkHost::kErrorWriteProtected: hr = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);      break;
        case CassoLinkHost::kErrorNotFound:       hr = HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND);   break;
        case CassoLinkHost::kErrorData:           hr = HRESULT_FROM_WIN32 (ERROR_CRC);                break;
        case CassoLinkHost::kErrorVerify:         hr = HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT);        break;
        case CassoLinkHost::kErrorLink:           hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);       break;
        case CassoLinkHost::kErrorDrive:          hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DRIVE);      break;
        default:                                  hr = HRESULT_FROM_WIN32 (ERROR_GEN_FAILURE);        break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::DriveSource::ReadSectors
//
//  The sectors are read a track at a time, by their physical numbers, and
//  returned in the order of the list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::DriveSource::ReadSectors (std::span<const size_t> sectors, std::vector<Byte> & outData)
{
    HRESULT              hr     = S_OK;
    std::vector<Byte>    physicals;
    std::vector<size_t>  places;
    std::vector<Byte>    data;
    CassoLinkStatus      status;



    outData.assign (sectors.size() * LinkedDisk::kSectorSize, 0);

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        physicals.clear();
        places.clear();

        for (size_t i = 0; i < sectors.size(); i++)
        {
            if ((int) (sectors[i] / NibblizationLayer::kSectorsPerTrack) == track)
            {
                physicals.push_back ((Byte) GetPhysicalSector ((int) (sectors[i] % NibblizationLayer::kSectorsPerTrack)));
                places.push_back (i);
            }
        }

        if (physicals.empty())
        {
            continue;
        }

        hr = m_session.m_host->ReadSectors ((Byte) m_drive, (Byte) track, physicals, data, status);
        CHR (hr);

        hr = FromStatusError (status.error);
        CHR (hr);

        for (size_t i = 0; i < places.size(); i++)
        {
            std::copy_n (data.begin() + (ptrdiff_t) (i * LinkedDisk::kSectorSize), LinkedDisk::kSectorSize, outData.begin() + (ptrdiff_t) (places[i] * LinkedDisk::kSectorSize));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::DriveSource::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::DriveSource::WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  trackData;
    CassoLinkStatus    status;



    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        physicals.clear();
        trackData.clear();

        for (size_t i = 0; i < sectors.size(); i++)
        {
            if ((int) (sectors[i] / NibblizationLayer::kSectorsPerTrack) == track)
            {
                physicals.push_back ((Byte) GetPhysicalSector ((int) (sectors[i] % NibblizationLayer::kSectorsPerTrack)));
                trackData.insert (trackData.end(), data.begin() + (ptrdiff_t) (i * LinkedDisk::kSectorSize), data.begin() + (ptrdiff_t) ((i + 1) * LinkedDisk::kSectorSize));
            }
        }

        if (physicals.empty())
        {
            continue;
        }

        hr = m_session.m_host->WriteSectors ((Byte) m_drive, (Byte) track, physicals, trackData, status);
        CHR (hr);

        hr = FromStatusError (status.error);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Queue
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::Queue (LinkOperationKind kind, int drive, const FilePath & path, std::shared_ptr<LinkOperation> operation)
{
    if (!operation)
    {
        operation = std::make_shared<LinkOperation>();
    }

    operation->kind  = kind;
    operation->drive = drive;
    operation->path  = path;

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        operation->id = m_nextId++;
        m_queue.push_back (operation);
    }

    m_wake.notify_all();

    return operation;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Run
//
//  The session's thread. A failed connection fails what is queued, so a
//  request made with no Apple on the line ends instead of waiting; the next
//  request tries again.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Run (std::stop_token stop)
{
    HRESULT                         hr     = S_OK;
    HRESULT                         logged = S_OK;
    std::shared_ptr<LinkOperation>  operation;



    if (m_bootstrap)
    {
        hr = RunBootstrap (stop, *m_bootstrap);
        m_sink.OnDone (m_bootstrap);
        m_bootstrap.reset();
    }
    else
    {
        hr = Connect (stop);
    }

    while (!stop.stop_requested())
    {
        if (FAILED (hr) || m_state != LinkState::Ready)
        {
            {
                std::unique_lock<std::mutex>  lock (m_lock);

                for (const std::shared_ptr<LinkOperation> & queued : m_queue)
                {
                    queued->result  = FAILED (hr) ? hr : HRESULT_FROM_WIN32 (ERROR_NOT_CONNECTED);
                    queued->message = "The Apple did not answer.";
                    queued->state   = LinkOperationState::Failed;
                    m_sink.OnDone (queued);
                }

                m_queue.clear();
            }

            //  Connecting again after a while, or at once when a request
            //  arrives, so a link started before its Apple is ready connects
            //  when it is.
            ServeUntilRetry (stop);

            hr = stop.stop_requested() ? S_OK : Connect (stop);
            continue;
        }

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            operation = nullptr;

            if (!m_queue.empty())
            {
                operation = m_queue.front();
                m_queue.pop_front();
                m_running = operation;
            }
        }

        if (!operation)
        {
            hr = m_host->ServeOne (kIdlePollMs);

            m_lostReason = FAILED (hr)                          ? std::format ("the line failed with 0x{:08X}", (uint32_t) hr)
                         : (m_host->GetSilenceMs() > kSilenceMs) ? std::format ("no packet in {} seconds", kSilenceMs / 1000)
                         : IsPeerGone()                          ? std::string ("DSR went off, the Apple's DTR on most cables")
                                                                 : std::string();

            if (!m_lostReason.empty())
            {
                Disconnect();
                hr = S_OK;
            }

            continue;
        }

        SetState (LinkState::Busy);
        operation->state = LinkOperationState::Running;
        m_sink.OnProgress (operation);

        hr = Execute (*operation);

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            m_running = nullptr;
        }

        operation->result   = hr;
        operation->progress = 100;

        if (operation->kind != LinkOperationKind::Enumerate && operation->kind != LinkOperationKind::CheckLinkDisk)
        {
            m_sink.OnActivity (std::format ("{}: {}", DescribeOperation (*operation), SUCCEEDED (hr) ? "done" : "failed"));
        }

        operation->state    = SUCCEEDED (hr)                              ? LinkOperationState::Done
                            : hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED)  ? LinkOperationState::Canceled
                                                                          : LinkOperationState::Failed;
        m_sink.OnDone (operation);

        if (!IsLineLost (hr) && operation->kind != LinkOperationKind::Enumerate && operation->kind != LinkOperationKind::CheckLinkDisk)
        {
            logged = LogOnApple (std::format ("{} {}", SUCCEEDED (hr) ? "OK " : "ERR", DescribeOperation (*operation, std::nullopt, SUCCEEDED (hr))));
            IGNORE_RETURN_VALUE (logged, S_OK);
        }

        if (IsLineLost (hr))
        {
            m_lostReason = std::format ("{}: {}", DescribeOperation (*operation),
                                        (hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT)) ? "no reply" : "the line closed");
            Disconnect();
            hr = S_OK;
        }
        else
        {
            SetState (LinkState::Ready);
            hr = S_OK;
        }
    }

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        for (const std::shared_ptr<LinkOperation> & queued : m_queue)
        {
            queued->result = HRESULT_FROM_WIN32 (ERROR_CANCELLED);
            queued->state  = LinkOperationState::Canceled;
            m_sink.OnDone (queued);
        }

        m_queue.clear();
    }

    Disconnect();
    SetState (LinkState::Closed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::StartWithBootstrap
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::StartWithBootstrap (const CassoLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation)
{
    HRESULT  hr   = S_OK;
    bool     idle = !m_thread.joinable();



    CBRAEx (idle, E_UNEXPECTED);

    m_bootstrap        = std::make_shared<LinkOperation>();
    m_bootstrap->id    = m_nextId++;
    m_bootstrap->kind  = LinkOperationKind::Bootstrap;
    outOperation       = m_bootstrap;

    hr = Start (settings);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::RunBootstrap
//
//  The loader is typed at the bootstrap speed, and the session then waits on
//  the same line for the shim's Ready. No Apple on any port replies before
//  the loader runs, so a serial link with no port set cannot search: the
//  loader goes to the PC's only COM port, and with none or several the user
//  enters the port.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::RunBootstrap (std::stop_token stop, LinkOperation & operation)
{
    HRESULT                    hr      = S_OK;
    std::vector<std::string>   lines;
    std::vector<std::wstring>  ports;
    bool                       hasPort = m_settings.transport != LinkTransport::Serial || !m_port.empty();
    bool                       typable = m_settings.transport != LinkTransport::Uthernet;



    operation.state = LinkOperationState::Running;

    //  The loader is typed into a serial port; an Uthernet II has none.
    if (!typable)
    {
        operation.message = "An Uthernet II link cannot be bootstrapped. Boot the link disk on the Apple instead.";
    }

    CBREx (typable, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (!hasPort)
    {
        hr = m_factory.ListComPorts (ports);
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

    hr = LinkBootstrap::BuildLines (m_settings.bootstrapBaud, lines);
    CHR (hr);

    SetState (LinkState::Bootstrapping);

    hr = OpenStream (m_settings.bootstrapBaud);
    CHR (hr);

    hr = LinkBootstrap::Send (*m_stream, lines, m_bootstrapPauseMs, [&stop]() { return stop.stop_requested(); }, LinkBootstrap::kCharPauseUs);
    CHR (hr);

    //  The shim starts on the open line and sends Ready unprompted. With an
    //  Idle as well, the shim sends a second Ready in reply to the Idle while
    //  the host's reply to the first arrives, and the 6551's one-byte buffer
    //  overruns.
    m_prompted = true;

    hr = Connect (stop, m_settings.bootstrapBaud);
    CHR (hr);

Error:
    operation.result = hr;
    operation.state  = SUCCEEDED (hr) ? LinkOperationState::Done : LinkOperationState::Failed;

    if (FAILED (hr) && operation.message.empty())
    {
        operation.message = "The Apple did not answer after the bootstrap. Check that IN#2 and Ctrl+A 14B were typed on it.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Connect
//
//  A serial line is opened at the bootstrap speed, where a typed-in shim
//  starts, and then at the link's speed, where a shim from a boot disk or a
//  program already loaded runs. A TCP line has no speed of its own; it is
//  opened once, as the Apple end starts at the link's speed (FR-094). The
//  program is then set to the link's speed, and a serial port opened again
//  at it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::Connect (std::stop_token stop, DWORD openAt)
{
    HRESULT             hr       = S_OK;
    std::vector<DWORD>  speeds   = { m_settings.baud };
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
    //  speed would garble ADTPro's requests at the link's speed.
    if (openAt != 0)
    {
        speeds = { openAt };
    }
    else if (m_host && (m_host->HasClient() || m_host->HasAdtProClient()))
    {
        speeds = { m_openBaud };
    }
    else if (m_settings.transport == LinkTransport::Serial && m_settings.baud != m_settings.bootstrapBaud && !m_adtPro && !IsLineOpen())
    {
        speeds.insert (speeds.begin(), m_settings.bootstrapBaud);
    }

    //  A serial link with no port set finds one, once; the port found is
    //  used again when the line drops.
    if (openAt == 0 && m_settings.transport == LinkTransport::Serial && m_port.empty())
    {
        SetState (LinkState::Searching);

        search.hardwareHandshake = m_settings.hardwareHandshake;

        hr = CassoLinkSearch::Find (m_factory, search, speeds, CassoLinkSearch::kReplyTimeoutMs, m_port);
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
            SetState (LinkState::Opening);

            hr = OpenStream (speed);
            CHR (hr);
        }

        opened = speed;

        hr = AwaitProgram (stop, m_connectTimeoutMs / (DWORD) speeds.size(), ready);
        CHR (hr);
    }

    CBREx (ready, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    //  An Uthernet II has no speed to set.
    if (m_settings.transport != LinkTransport::Uthernet)
    {
        LinkProgram::GetSpeedControl (m_settings.baud, control);

        hr = m_host->SetSpeed (control);
        CHR (hr);
    }

    //  A port opened again at the link's speed has a new host, and the link
    //  is ready once the program's Ready arrives on it. The program sends
    //  Ready again after each second with no reply, so no Idle is sent.
    if (m_settings.transport == LinkTransport::Serial && opened != m_settings.baud)
    {
        hr = OpenStream (m_settings.baud);
        CHR (hr);

        m_prompted = true;

        hr = AwaitProgram (stop, m_connectTimeoutMs, ready);
        CHR (hr);

        CBREx (ready, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    }

    driveMap = m_host->GetClient().driveMap;
    model    = m_host->GetClient().GetModelName();

    if (driveMap != m_driveMap)
    {
        for (int i = 0; i < kDriveCount; i++)
        {
            m_disks[(size_t) i]->Clear();
            m_listings[(size_t) i].reset();
        }

        m_driveMap = driveMap;
    }

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_modelName = model;
    }

    SetState (LinkState::Ready);

    logged = LogOnApple ("PC CONNECTED");
    IGNORE_RETURN_VALUE (logged, S_OK);

Error:
    //  With no Apple on it, the line stays open, and the wait before the
    //  next attempt serves it. After a load that stopped partway, the
    //  Apple's shim can be at another speed, so the line is closed, and the
    //  next attempt opens it at each speed again.
    if (hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT) && IsLineOpen() && m_state != LinkState::LoadingProgram)
    {
        SetState (LinkState::Disconnected);
    }
    else if (FAILED (hr))
    {
        Disconnect();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::LogOnApple
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::LogOnApple (const std::string & text)
{
    HRESULT     hr  = S_OK;
    SYSTEMTIME  now = {};



    GetLocalTime (&now);

    hr = m_host->SendLog (now.wHour, now.wMinute, text);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::IsLineOpen
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSession::IsLineOpen() const
{
    return m_host && m_stream && !m_stream->IsClosed();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::ServeUntilRetry
//
//  The line, while it is open, is served until the next attempt is due, a
//  request is queued or the Apple's Ready arrives; with no line open, the
//  wait is only for the next attempt or a request.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::ServeUntilRetry (std::stop_token stop)
{
    auto     deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (m_retryMs);
    bool     queued   = false;
    bool     ready    = false;
    HRESULT  hr       = S_OK;



    while (!stop.stop_requested() && !queued && !ready && std::chrono::steady_clock::now() < deadline)
    {
        if (!m_host)
        {
            std::unique_lock<std::mutex>  lock (m_lock);

            m_wake.wait_until (lock, stop, deadline, [this] () { return !m_queue.empty(); });
            break;
        }

        hr = m_host->ServeOne (kIdlePollMs);

        if (FAILED (hr))
        {
            Disconnect();
            continue;
        }

        ready = m_host->HasClient();

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            queued = !m_queue.empty();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::OpenStream
//
//  A serial port at the given speed; the TCP transports connect, or, for a
//  Wi-Fi modem, wait for it to connect. A host engine is made for each new
//  stream.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::OpenStream (DWORD baud)
{
    HRESULT                       hr        = S_OK;
    std::unique_ptr<IByteStream>  stream;
    ComSettings                   com;
    std::string                   host;
    uint16_t                      port      = 0;
    size_t                        colon     = m_settings.address.rfind (':');
    bool                          addressed = false;
    bool                          reusable  = IsLineOpen() && (m_settings.transport != LinkTransport::Serial || m_openBaud == baud);



    //  A line open already at the speed is used as it is, so no byte from
    //  the Apple is dropped while the line is closed and opened again.
    BAIL_OUT_IF (reusable, S_OK);

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        if (m_stream)
        {
            m_stream->Close();
        }

        m_host.reset();
        m_stream.reset();
    }

    if (m_settings.transport == LinkTransport::Serial)
    {
        com.baud              = baud;
        com.hardwareHandshake = m_settings.hardwareHandshake;

        hr = m_factory.OpenComPort (m_port, com, stream);
        CHR (hr);
    }
    else
    {
        addressed = colon != std::string::npos;
        CBREx (addressed, E_INVALIDARG);

        host = m_settings.address.substr (0, colon);
        port = (uint16_t) std::stoul (m_settings.address.substr (colon + 1));

        if (m_settings.transport == LinkTransport::WifiModem)
        {
            hr = m_factory.ListenTcp (port, m_settings.transport, stream);
        }
        else
        {
            hr = m_factory.ConnectTcp (host, port, stream);
        }

        CHR (hr);
    }

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_stream = std::shared_ptr<IByteStream> (std::move (stream));
        m_host   = std::make_unique<CassoLinkHost> (m_stream, m_folder, [] ()
        {
            SYSTEMTIME  now = {};

            GetLocalTime (&now);
            return CassoLinkHost::GetProDosDateTime (now);
        });

        m_host->SetActivitySink ([this] (const std::string & text) { m_sink.OnActivity (text); });
        m_host->SetProgramChip (GetLinkChip (m_settings.transport));
        m_openBaud = baud;
        m_prompted = false;

        //  DSR is judged on each line from its own first on: a USB serial
        //  adapter just opened reports DSR off until its driver polls it,
        //  which on a line opened again would read as the Apple gone.
        m_peerSeen       = false;
        m_peerOffSinceMs = 0;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::AwaitProgram
//
//  Serves the line until the current program sends Ready, the shim's Ready
//  having the program loaded on the way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::AwaitProgram (std::stop_token stop, DWORD timeoutMs, bool & outReady)
{
    HRESULT  hr       = S_OK;
    auto     deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);



    outReady = false;
    SetState (LinkState::WaitingForReady);

    while (!outReady && !stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
    {
        //  An Apple that sent its Ready before the line was open is waiting
        //  for a reply, and is prompted for another, once for each line
        //  opened. A prompt is an envelope, which ADTPro's client, idle on
        //  the same line, would read as the start of its next reply.
        if (!m_prompted && !m_host->HasClient() && !m_host->HasAdtProClient())
        {
            hr = m_host->PromptReady();
            CHR (hr);

            m_prompted = true;
        }

        hr = m_host->ServeOne (kIdlePollMs);
        CHR (hr);

        //  ADTPro's client is served for as long as it is there, rather
        //  than dropped when Casso's program does not answer.
        if (m_host->HasAdtProClient())
        {
            m_adtPro = true;
            deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        }

        if (m_host->HasClient())
        {
            m_adtPro = false;
            outReady = m_host->GetClient().programVersion == LinkProgram::kProgramVersion;

            if (!outReady)
            {
                SetState (LinkState::LoadingProgram);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::IsPeerGone
//
//  A serial line whose DSR was on, as the Apple's DTR is while its 6551 is
//  set up, and has been off for kPeerOffMs: the Apple was reset or turned
//  off. A USB serial adapter can report DSR off for a millisecond at a time
//  on a line that is up, many times a second, so one reading off does not
//  count. A cable that does not wire the line has it off throughout, and
//  only silence counts.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSession::IsPeerGone()
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





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Disconnect()
{
    m_peerSeen       = false;
    m_peerOffSinceMs = 0;

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        if (m_stream)
        {
            m_stream->Close();
        }

        m_host.reset();
        m_stream.reset();
    }

    SetState (LinkState::Disconnected);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::SetState
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::SetState (LinkState state)
{
    LinkState  before = m_state.exchange (state);



    if (before == state)
    {
        return;
    }

    if (state == LinkState::Ready && before != LinkState::Busy)
    {
        //  A new connection may be to another Apple, with other drives.
        m_revolutionMs = {};

        m_sink.OnActivity (std::format ("Connected to {}", TextEncoding::WideToNarrow (GetModelName().empty() ? std::wstring (L"the Apple") : GetModelName())));
    }
    else if (state == LinkState::Disconnected && (before == LinkState::Ready || before == LinkState::Busy))
    {
        m_sink.OnActivity (m_stopping           ? std::string ("Disconnected")
                         : m_lostReason.empty() ? std::string ("The Apple stopped answering")
                                                : "The Apple stopped answering: " + m_lostReason);
    }

    m_lostReason.clear();

    m_sink.OnStateChanged (state);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::DescribeOperation
//
//  The Label text for an operation, as the program's screen draws it, or
//  with `finished`, in the past tense for the history. A Write disk's label
//  has its image's file name, or for a format, the new volume, and the drive
//  without the old disk's volume, which the write replaces.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkSession::DescribeOperation (const LinkOperation & operation, const std::optional<VolumeListing> & listing, bool finished)
{
    std::string  path  = operation.path.ToString();
    std::string  drive = DescribeDrive (operation.drive, listing);
    std::string  image = std::filesystem::path (operation.imagePath).filename().string();
    std::string  text;



    switch (operation.kind)
    {
        case LinkOperationKind::Enumerate:       text = path.empty() ? std::format ("{} THE CATALOG OF {}", finished ? "READ" : "READING", drive)
                                                                     : std::format ("{} {}", finished ? "LISTED" : "LISTING", path);       break;
        case LinkOperationKind::ReadFile:        text = std::format ("{} {} FROM {}", finished ? "COPIED" : "COPYING", path, drive);         break;
        case LinkOperationKind::WriteFile:       text = std::format ("{} {} TO {}", finished ? "COPIED" : "COPYING", path, drive);           break;
        case LinkOperationKind::Delete:          text = std::format ("{} {}", finished ? "DELETED" : "DELETING", path);                     break;
        case LinkOperationKind::Rename:          text = std::format ("{} {} TO {}", finished ? "RENAMED" : "RENAMING", path, operation.newName); break;
        case LinkOperationKind::MakeDirectory:   text = std::format ("{} FOLDER {}", finished ? "MADE" : "MAKING", path);                   break;
        case LinkOperationKind::RemoveDirectory: text = std::format ("{} FOLDER {}", finished ? "DELETED" : "DELETING", path);              break;
        case LinkOperationKind::ReadDisk:        text = std::format ("{} {} TO PC", finished ? "SENT" : "SENDING", drive);                  break;
        case LinkOperationKind::WriteDisk:       text = !operation.formatted.empty()
                                                          ? std::format ("{} {} IN D{}", finished ? "FORMATTED" : "FORMATTING", operation.formatted, operation.drive)
                                                          : std::format ("{} IMAGE {} TO D{}", finished ? "WROTE" : "WRITING", image, operation.drive); break;
        case LinkOperationKind::CheckLinkDisk:   text = std::format ("{} THE DISK IN {}", finished ? "CHECKED" : "CHECKING", drive);        break;
        default:                                 text = "WORKING";                                                                          break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::DescribeDrive
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkSession::DescribeDrive (int drive, const std::optional<VolumeListing> & listing)
{
    std::string  text = std::format ("D{}", drive);



    if (listing.has_value() && listing->hasVolumeName)
    {
        text += std::format (" (/{})", listing->volumeName);
    }
    else if (listing.has_value() && listing->hasVolumeNumber)
    {
        text += std::format (" (VOL {})", listing->volumeNumber);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Execute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::Execute (LinkOperation & operation)
{
    HRESULT       hr      = S_OK;
    size_t        index   = (size_t) operation.drive - 1;
    bool          present = false;
    LinkedDisk  * disk    = nullptr;



    present = operation.drive >= 1 && operation.drive <= kDriveCount && (m_driveMap & (1 << index)) != 0;
    CBREx (present, HRESULT_FROM_WIN32 (ERROR_INVALID_DRIVE));

    disk = m_disks[index].get();

    hr = m_host->SendLabel (DescribeOperation (operation, m_listings[index]));
    CHR (hr);

    if (operation.kind != LinkOperationKind::ReadDisk && operation.kind != LinkOperationKind::WriteDisk && operation.kind != LinkOperationKind::CheckLinkDisk)
    {
        hr = Revalidate (operation.drive);
        CHR (hr);
    }

    switch (operation.kind)
    {
        case LinkOperationKind::Enumerate:
            hr = disk->Run ([&] (const IVolume & volume) { return volume.EnumerateDirectory (operation.path, operation.listing); });
            operation.volumeKind = disk->GetKind();

            if (SUCCEEDED (hr) && operation.path.IsEmpty())
            {
                m_listings[index] = operation.listing;
            }

            break;

        case LinkOperationKind::ReadFile:
            hr = disk->Run ([&] (const IVolume & volume) { return volume.Read (operation.path, operation.file); });
            break;

        case LinkOperationKind::WriteFile:
            hr = RunWrite (operation, [&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Write (operation.path, operation.payload, outBuffer); });
            break;

        case LinkOperationKind::Delete:
            hr = RunWrite (operation, [&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Delete (operation.path, outBuffer); });
            break;

        case LinkOperationKind::Rename:
            hr = RunWrite (operation, [&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.Rename (operation.path, operation.newName, outBuffer); });
            break;

        case LinkOperationKind::MakeDirectory:
            hr = RunWrite (operation, [&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.CreateDirectory (operation.path, outBuffer); });
            break;

        case LinkOperationKind::RemoveDirectory:
            hr = RunWrite (operation, [&] (const IVolume & volume, std::vector<Byte> & outBuffer)
            {
                DeleteOutcome  outcome;

                return volume.RemoveDirectory (operation.path, true, outBuffer, outcome);
            });
            break;

        case LinkOperationKind::ReadDisk:
            hr = ReadWholeDisk (operation);
            break;

        case LinkOperationKind::WriteDisk:
            hr = WriteWholeDisk (operation);
            break;

        case LinkOperationKind::CheckLinkDisk:
            hr = ReadBootSector (operation);
            break;

        default:
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

    m_host->SetProgress (0, 100);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Revalidate
//
//  The sectors that identify the volume -- the VTOC, or the volume
//  directory's key block and the bitmap -- are read again; each has the
//  volume's free space, so another disk almost never matches them. When they
//  differ from what is
//  in memory, another disk is in the drive, or this one was written
//  elsewhere, and its sectors are dropped. The drive's listing is not: a
//  write checks the disk against the drive's last listing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::Revalidate (int drive)
{
    HRESULT              hr      = S_OK;
    size_t               index   = (size_t) drive - 1;
    LinkedDisk         & disk    = *m_disks[index];
    std::vector<size_t>  marks;
    std::vector<Byte>    fresh;
    bool                 same    = true;



    BAIL_OUT_IF (disk.GetKind() == VolumeKind::Unknown, S_OK);

    if (disk.GetKind() == VolumeKind::Dos33)
    {
        marks.push_back ((size_t) Dos33Skeleton::kVtocTrack * NibblizationLayer::kSectorsPerTrack);
    }
    else
    {
        for (int block : { ProDosSkeleton::kDirKeyBlock, ProDosSkeleton::kBitmapBlock })
        {
            marks.push_back (ProDosSkeleton::GetBlockByteOffset (block, 0) / LinkedDisk::kSectorSize);
            marks.push_back (ProDosSkeleton::GetBlockByteOffset (block, LinkedDisk::kSectorSize) / LinkedDisk::kSectorSize);
        }
    }

    hr = m_sources[index]->ReadSectors (marks, fresh);
    CHR (hr);

    for (size_t i = 0; i < marks.size(); i++)
    {
        same = same && std::equal (fresh.begin() + (ptrdiff_t) (i * LinkedDisk::kSectorSize),
                                   fresh.begin() + (ptrdiff_t) ((i + 1) * LinkedDisk::kSectorSize),
                                   disk.GetSectors().begin() + (ptrdiff_t) (marks[i] * LinkedDisk::kSectorSize));
    }

    if (!same)
    {
        disk.Clear();
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::CheckCatalog
//
//  The catalog read from the disk, everything else dropped, against the
//  drive's last listing. While CassoLink runs, the Apple runs nothing else,
//  so the disk changes only when another is put in the drive, and
//  Revalidate, at the start of the operation, drops the sectors in memory
//  when that happens; sectors still in memory are this disk's, and are used
//  as they are.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::CheckCatalog (int drive)
{
    HRESULT        hr    = S_OK;
    size_t         index = (size_t) drive - 1;
    VolumeListing  listing;
    bool           same  = false;



    BAIL_OUT_IF (!m_listings[index].has_value(), S_OK);
    BAIL_OUT_IF (m_disks[index]->GetKind() != VolumeKind::Unknown, S_OK);

    m_disks[index]->Clear();

    hr = m_disks[index]->Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); });
    CHR (hr);

    same = IsSameCatalog (listing, *m_listings[index]);
    CBREx (same, HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::RunWrite
//
//  After the write, the drive's listing is the volume's new catalog, so the
//  next write compares against what this one left.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::RunWrite (LinkOperation & operation, const LinkedDisk::WriteOperation & write)
{
    HRESULT              hr    = S_OK;
    size_t               index = (size_t) operation.drive - 1;
    std::vector<size_t>  sent;
    VolumeListing        listing;



    hr = CheckCatalog (operation.drive);

    if (hr == HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED))
    {
        operation.message = std::format ("The disk in drive {} changed since it was listed.", operation.drive);
    }

    CHR (hr);

    hr = m_disks[index]->Write (write, sent);
    CHR (hr);

    hr = m_disks[index]->Run ([&] (const IVolume & volume) { return volume.Enumerate (listing); });
    CHR (hr);

    m_listings[index] = listing;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::ReadWholeDisk
//
//  The image is written only once every track has been read, so a failed or
//  canceled read writes nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::ReadWholeDisk (LinkOperation & operation)
{
    HRESULT               hr       = S_OK;
    DiskFormat            format   = DiskFormat::Dsk;
    std::vector<Byte>     sectors (NibblizationLayer::kImageByteSize, 0);
    std::vector<Byte>     bytes;
    CommitPlan::Progress  progress;
    bool                  complete = false;



    hr = DiskImageStore::GetSourceFormatByExtension (operation.imagePath, format);
    CHR (hr);

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        Report (operation, track * 100 / NibblizationLayer::kTrackCount);
        m_host->SetProgress ((Byte) (track * 100 / NibblizationLayer::kTrackCount), (Byte) ((track + 1) * 100 / NibblizationLayer::kTrackCount));

        hr = ReadTrack (operation.drive, track, sectors, operation.unreadable);
        CHR (hr);
    }

    complete = operation.unreadable.empty() || operation.blankUnreadable;

    if (!complete)
    {
        operation.message = std::format ("{} sectors could not be read, the first on track {}, sector {}.",
                                         operation.unreadable.size(), operation.unreadable[0].first, operation.unreadable[0].second);
    }

    CBREx (complete, HRESULT_FROM_WIN32 (ERROR_CRC));

    hr = BlankDiskBuilder::WrapInContainer (format, NibbleImageCodec::kNibTrackSize, false, 0, sectors, bytes);
    CHR (hr);

    hr = DurableCommit::Commit (m_fileIo, operation.imagePath, bytes, m_commitTag++, CommitMode::Replace, progress);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::ReadBootSector
//
//  Track 0's first sector against the link boot disk's. That sector is the
//  loader, the same at every speed, but not for every chip: a W5100 shim
//  loads lower. A drive whose track 0 cannot be read has no link disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::ReadBootSector (LinkOperation & operation)
{
    HRESULT                           hr       = S_OK;
    std::vector<Byte>                 sectors (NibblizationLayer::kImageByteSize, 0);
    std::vector<Byte>                 boot;
    std::vector<std::pair<int, int>>  unreadable;
    LinkShimOptions                   options;
    bool                              loader   = false;



    options.chip = GetLinkChip (m_settings.transport);
    options.baud = m_settings.baud;

    hr = LinkProgram::BuildBootDisk (options, boot);
    CHR (hr);

    hr = ReadTrack (operation.drive, 0, sectors, unreadable);
    CHR (hr);

    loader = std::none_of (unreadable.begin(), unreadable.end(), [] (const std::pair<int, int> & sector) { return sector.second == 0; })
          && std::equal (boot.begin(), boot.begin() + NibblizationLayer::kSectorByteSize, sectors.begin());

    operation.linkDisk = loader;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::ReadTrack
//
//  A track's sixteen sectors in one request. When the drive cannot read one
//  of them, each is read on its own, and those that fail again are listed
//  and written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::ReadTrack (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable)
{
    HRESULT            hr        = S_OK;
    HRESULT            disk      = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  data;
    CassoLinkStatus    status;
    bool               damaged   = false;
    int                missing   = 0;



    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    hr = m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, status);
    CHR (hr);

    disk    = FromStatusError (status.error);
    damaged = status.error == CassoLinkHost::kErrorNotFound || status.error == CassoLinkHost::kErrorData;
    missing = status.done;

    BAIL_OUT_IF (FAILED (disk) && !damaged, disk);

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        int     logical = GetLogicalSector (physical);
        size_t  to      = (size_t) (track * NibblizationLayer::kSectorsPerTrack + logical) * LinkedDisk::kSectorSize;
        size_t  from    = (size_t) physical * LinkedDisk::kSectorSize;

        if (damaged && physical >= missing)
        {
            const Byte  one[] = { (Byte) physical };

            hr = m_host->ReadSectors ((Byte) drive, (Byte) track, one, data, status);
            CHR (hr);

            from = 0;

            if (status.error != CassoLinkHost::kErrorNone)
            {
                unreadable.push_back ({ track, logical });
                continue;
            }
        }

        std::copy_n (data.begin() + (ptrdiff_t) from, LinkedDisk::kSectorSize, sectors.begin() + (ptrdiff_t) to);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::WriteWholeDisk
//
//  The image is decoded first, and nothing is written if any track is not
//  standard sectors. The volume number formatted into each address field
//  is the image's own for DOS 3.3, and 254 otherwise. The drive's sectors
//  and listing are then dropped, as the disk is another one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::WriteWholeDisk (LinkOperation & operation)
{
    HRESULT             hr       = S_OK;
    size_t              index    = (size_t) operation.drive - 1;
    std::vector<Byte>   fileBytes;
    std::vector<Byte>   sectors;
    std::vector<Byte>   data;
    std::vector<int>    badTracks;
    std::string         list;
    SectorDecodeReport  report;
    VolumeListing       listing;
    CassoLinkStatus     status;
    Byte                volume   = 254;
    Byte                gap      = 0;
    bool                standard = false;
    HRESULT             listed   = S_OK;



    hr = m_fileIo.ReadAllBytes (operation.imagePath, fileBytes);
    CHR (hr);

    hr = VolumeImage::Load (fileBytes, operation.imagePath, sectors, report);
    CHR (hr);

    for (int track = 0; track < report.GetTrackCount(); track++)
    {
        if (report.GetOutcome (track) != TrackDecodeOutcome::Complete)
        {
            badTracks.push_back (track);
            list += std::format ("{}{}", list.empty() ? "" : ", ", track);
        }
    }

    if (!badTracks.empty())
    {
        operation.message = std::format ("Tracks {} of the image are not standard sectors, so nothing was written.", list);
    }

    standard = badTracks.empty();
    CBREx (standard, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33)
    {
        listed = Dos33Volume (sectors).Enumerate (listing);

        if (SUCCEEDED (listed) && listing.hasVolumeNumber)
        {
            volume = listing.volumeNumber;
        }
    }

    m_disks[index]->Clear();
    m_listings[index].reset();

    hr = TimeDriveForFormat (operation.drive, gap);
    CHR (hr);

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        Report (operation, track * 100 / NibblizationLayer::kTrackCount);
        m_host->SetProgress ((Byte) (track * 100 / NibblizationLayer::kTrackCount), (Byte) ((track + 1) * 100 / NibblizationLayer::kTrackCount));

        data.clear();

        for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
        {
            auto  from = sectors.begin() + (ptrdiff_t) ((size_t) (track * NibblizationLayer::kSectorsPerTrack + GetLogicalSector (physical)) * LinkedDisk::kSectorSize);

            data.insert (data.end(), from, from + (ptrdiff_t) LinkedDisk::kSectorSize);
        }

        hr = m_host->WriteTrack ((Byte) operation.drive, (Byte) track, volume, data, status, gap);
        CHR (hr);

        hr = FromStatusError (status.error);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::TimeDriveForFormat
//
//  The drive's revolution timed on track 0, and the gap after each sector
//  that fits 16 sectors in it. A track 0 with no sector 0, as on a blank
//  disk, is formatted first with the program's own gap, which fits a drive
//  up to about 4% fast. A drive that cannot be timed gets the program's gap.
//
//  Only a real Apple's drive is timed: the PC times the revolution by its
//  own clock, and an emulated Apple does not always run in real time, so its
//  milliseconds are not its cycles. An emulated drive turns at 300 rpm.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSession::TimeDriveForFormat (int drive, Byte & outGap)
{
    HRESULT          hr          = S_OK;
    CassoLinkStatus  status;
    double           revolution  = 0.0;



    outGap = 0;

    BAIL_OUT_IF (m_settings.transport != LinkTransport::Serial && m_settings.transport != LinkTransport::WifiModem, S_OK);

    //  A drive's speed is its own, not its disk's: timed once a connection.
    revolution = m_revolutionMs[(size_t) drive - 1];
    BAIL_OUT_IF (revolution > 0.0, S_OK);

    hr = m_host->TimeDrive ((Byte) drive, 0, kTimedRevolutions, revolution, status);
    CHR (hr);

    if (revolution == 0.0)
    {
        hr = m_host->FormatTrack ((Byte) drive, 0, 254, status);
        CHR (hr);

        hr = m_host->TimeDrive ((Byte) drive, 0, kTimedRevolutions, revolution, status);
        CHR (hr);
    }

    BAIL_OUT_IF (revolution == 0.0, S_OK);

    m_revolutionMs[(size_t) drive - 1] = revolution;
    m_sink.OnActivity (DescribeDriveSpeed (drive, revolution));

Error:
    outGap = (revolution > 0.0) ? CassoLinkHost::ComputeFormatGap (revolution) : 0;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::DescribeDriveSpeed
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkSession::DescribeDriveSpeed (int drive, double revolutionMs)
{
    double       rpm     = 60000.0 / revolutionMs;
    double       percent = (rpm - 300.0) / 3.0;
    std::string  text    = std::format ("D{} turns at {:.1f} rpm", drive, rpm);



    if (std::abs (percent) >= 0.05)
    {
        text += std::format (", {:.1f}% {}", std::abs (percent), percent < 0.0 ? "slow" : "fast");
    }

    if (std::abs (percent) > 1.5)
    {
        text += "; drives are usually set within 1.5% of 300 rpm";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Report
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Report (LinkOperation & operation, int progress)
{
    std::shared_ptr<LinkOperation>  running;



    operation.progress = progress;

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        running = m_running;
    }

    if (running)
    {
        m_sink.OnProgress (running);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::IsSameCatalog
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSession::IsSameCatalog (const VolumeListing & a, const VolumeListing & b)
{
    bool  same = a.volumeName == b.volumeName && a.volumeNumber == b.volumeNumber && a.entries.size() == b.entries.size();



    for (size_t i = 0; same && i < a.entries.size(); i++)
    {
        const FileEntry  & x = a.entries[i];
        const FileEntry  & y = b.entries[i];

        same = x.name == y.name && x.type == y.type && x.sizeUnits == y.sizeUnits && x.eofBytes == y.eofBytes && x.isLocked == y.isLocked;
    }

    return same;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::IsLineLost
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkSession::IsLineLost (HRESULT hr)
{
    return hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT) || hr == HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE);
}
