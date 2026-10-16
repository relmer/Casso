#include "Pch.h"

#include "AppleLink/AppleLinkSession.h"
#include "AppleLink/AppleLinkSearch.h"
#include "AppleLink/LinkBootstrap.h"
#include "AppleLink/LinkProgram.h"
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
//  AppleLinkSession::AppleLinkSession
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkSession::AppleLinkSession (IByteStreamFactory & factory, IAppleLinkFolder & folder, IDiskFileIo & fileIo, IAppleLinkSessionSink & sink) :
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
//  AppleLinkSession::~AppleLinkSession
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkSession::~AppleLinkSession()
{
    Stop();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::Start (const AppleLinkSettings & settings)
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
    m_thread   = std::jthread ([this] (std::stop_token stop) { Run (stop); });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Stop
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkSession::Stop()
{
    if (m_thread.joinable())
    {
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
//  AppleLinkSession::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkSession::Cancel()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    if (m_running && m_host)
    {
        m_host->Cancel();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::Enumerate (int drive, const FilePath & directory)
{
    return Queue (LinkOperationKind::Enumerate, drive, directory);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::ReadFile
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::ReadFile (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::ReadFile, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::WriteFile
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::WriteFile (int drive, const FilePath & path, const FilePayload & payload)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->payload = payload;

    return Queue (LinkOperationKind::WriteFile, drive, path, operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Delete
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::Delete (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::Delete, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Rename
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::Rename (int drive, const FilePath & path, const std::string & newName)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->newName = newName;

    return Queue (LinkOperationKind::Rename, drive, path, operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::MakeDirectory
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::MakeDirectory (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::MakeDirectory, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::RemoveDirectory
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::RemoveDirectory (int drive, const FilePath & path)
{
    return Queue (LinkOperationKind::RemoveDirectory, drive, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::ReadDisk
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::ReadDisk (int drive, const std::string & imagePath, bool blankUnreadable)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->imagePath       = imagePath;
    operation->blankUnreadable = blankUnreadable;

    return Queue (LinkOperationKind::ReadDisk, drive, FilePath(), operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::WriteDisk
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::WriteDisk (int drive, const std::string & imagePath)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->imagePath = imagePath;

    return Queue (LinkOperationKind::WriteDisk, drive, FilePath(), operation);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::GetLogicalSector
//
////////////////////////////////////////////////////////////////////////////////

int AppleLinkSession::GetLogicalSector (int physical)
{
    return NibblizationLayer::GetDosFileIndexForPhysicalSector (physical);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::GetPhysicalSector
//
////////////////////////////////////////////////////////////////////////////////

int AppleLinkSession::GetPhysicalSector (int logical)
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
//  AppleLinkSession::FromStatusError
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::FromStatusError (Byte error)
{
    HRESULT  hr = S_OK;



    switch (error)
    {
        case AppleLinkHost::kErrorNone:           hr = S_OK;                                         break;
        case AppleLinkHost::kErrorWriteProtected: hr = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);      break;
        case AppleLinkHost::kErrorNotFound:       hr = HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND);   break;
        case AppleLinkHost::kErrorData:           hr = HRESULT_FROM_WIN32 (ERROR_CRC);                break;
        case AppleLinkHost::kErrorVerify:         hr = HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT);        break;
        case AppleLinkHost::kErrorLink:           hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);       break;
        case AppleLinkHost::kErrorDrive:          hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DRIVE);      break;
        default:                                  hr = HRESULT_FROM_WIN32 (ERROR_GEN_FAILURE);        break;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::DriveSource::ReadSectors
//
//  The sectors are read a track at a time, by their physical numbers, and
//  returned in the order of the list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::DriveSource::ReadSectors (std::span<const size_t> sectors, std::vector<Byte> & outData)
{
    HRESULT              hr     = S_OK;
    std::vector<Byte>    physicals;
    std::vector<size_t>  places;
    std::vector<Byte>    data;
    AppleLinkStatus      status;



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
//  AppleLinkSession::DriveSource::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::DriveSource::WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  trackData;
    AppleLinkStatus    status;



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
//  AppleLinkSession::Queue
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> AppleLinkSession::Queue (LinkOperationKind kind, int drive, const FilePath & path, std::shared_ptr<LinkOperation> operation)
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
//  AppleLinkSession::Run
//
//  The session's thread. A failed connection fails what is queued, so a
//  request made with no Apple on the line ends instead of waiting; the next
//  request tries again.
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkSession::Run (std::stop_token stop)
{
    HRESULT                         hr = S_OK;
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
                m_wake.wait (lock, stop, [this] () { return !m_queue.empty(); });
            }

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

            if (FAILED (hr))
            {
                Disconnect();
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
        operation->state    = SUCCEEDED (hr)                              ? LinkOperationState::Done
                            : hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED)  ? LinkOperationState::Canceled
                                                                          : LinkOperationState::Failed;
        m_sink.OnDone (operation);

        if (IsLineLost (hr))
        {
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
//  AppleLinkSession::StartWithBootstrap
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::StartWithBootstrap (const AppleLinkSettings & settings, std::shared_ptr<LinkOperation> & outOperation)
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
//  AppleLinkSession::RunBootstrap
//
//  The loader is typed at the bootstrap speed, and the session then waits on
//  the same line for the shim's Ready. A serial link must have its port set, as no
//  Apple on any port replies before the loader runs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::RunBootstrap (std::stop_token stop, LinkOperation & operation)
{
    HRESULT                   hr      = S_OK;
    std::vector<std::string>  lines;
    bool                      hasPort = m_settings.transport != LinkTransport::Serial || !m_port.empty();



    operation.state = LinkOperationState::Running;

    if (!hasPort)
    {
        operation.message = "Choose the link's COM port to bootstrap the Apple.";
    }

    CBREx (hasPort, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = LinkBootstrap::BuildLines (m_settings.bootstrapBaud, lines);
    CHR (hr);

    SetState (LinkState::Bootstrapping);

    hr = OpenStream (m_settings.bootstrapBaud);
    CHR (hr);

    hr = LinkBootstrap::Send (*m_stream, lines, m_bootstrapPauseMs, [&stop]() { return stop.stop_requested(); });
    CHR (hr);

    hr = Connect (stop, m_settings.bootstrapBaud);
    CHR (hr);

Error:
    operation.result = hr;
    operation.state  = SUCCEEDED (hr) ? LinkOperationState::Done : LinkOperationState::Failed;

    if (FAILED (hr) && operation.message.empty())
    {
        operation.message = "The Apple did not answer after the bootstrap. Check that IN#2 and Control-A 14B were typed on it.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Connect
//
//  A serial line is opened at the bootstrap speed, where a typed-in shim
//  starts, and then at the link's speed, where a shim from a boot disk or a
//  program already loaded runs. A TCP line has no speed of its own; it is
//  opened once, as the Apple end starts at the link's speed (FR-094). The
//  program is then set to the link's speed, and a serial port opened again
//  at it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::Connect (std::stop_token stop, DWORD openAt)
{
    HRESULT             hr       = S_OK;
    std::vector<DWORD>  speeds   = { m_settings.baud };
    ComSettings         search;
    DWORD               opened   = 0;
    Byte                control  = 0;
    Byte                driveMap = 0;
    bool                ready    = false;



    //  After a bootstrap the line is open already, at the bootstrap speed.
    if (openAt != 0)
    {
        speeds = { openAt };
    }
    else if (m_settings.transport == LinkTransport::Serial && m_settings.baud != m_settings.bootstrapBaud)
    {
        speeds.insert (speeds.begin(), m_settings.bootstrapBaud);
    }

    //  A serial link with no port set finds one, once; the port found is
    //  used again when the line drops.
    if (openAt == 0 && m_settings.transport == LinkTransport::Serial && m_port.empty())
    {
        SetState (LinkState::Searching);

        search.hardwareHandshake = m_settings.hardwareHandshake;

        hr = AppleLinkSearch::Find (m_factory, search, speeds, AppleLinkSearch::kReplyTimeoutMs, m_port);
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

    LinkProgram::GetSpeedControl (m_settings.baud, control);

    hr = m_host->SetSpeed (control);
    CHR (hr);

    if (m_settings.transport == LinkTransport::Serial && opened != m_settings.baud)
    {
        hr = OpenStream (m_settings.baud);
        CHR (hr);
    }

    driveMap = m_host->GetClient().driveMap;

    if (driveMap != m_driveMap)
    {
        for (int i = 0; i < kDriveCount; i++)
        {
            m_disks[(size_t) i]->Clear();
            m_listings[(size_t) i].reset();
        }

        m_driveMap = driveMap;
    }

    SetState (LinkState::Ready);

Error:
    if (FAILED (hr))
    {
        Disconnect();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::OpenStream
//
//  A serial port at the given speed; the TCP transports connect, or, for a
//  Wi-Fi modem, wait for it to connect. A host engine is made for each new
//  stream.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::OpenStream (DWORD baud)
{
    HRESULT                       hr        = S_OK;
    std::unique_ptr<IByteStream>  stream;
    ComSettings                   com;
    std::string                   host;
    uint16_t                      port      = 0;
    size_t                        colon     = m_settings.address.rfind (':');
    bool                          addressed = false;



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
        m_host   = std::make_unique<AppleLinkHost> (m_stream, m_folder, [] ()
        {
            SYSTEMTIME  now = {};

            GetLocalTime (&now);
            return AppleLinkHost::GetProDosDateTime (now);
        });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::AwaitProgram
//
//  Serves the line until the current program sends Ready, the shim's Ready
//  having the program loaded on the way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::AwaitProgram (std::stop_token stop, DWORD timeoutMs, bool & outReady)
{
    HRESULT  hr       = S_OK;
    auto     deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
    auto     prompt   = std::chrono::steady_clock::now();



    outReady = false;
    SetState (LinkState::WaitingForReady);

    while (!outReady && !stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
    {
        //  An Apple that sent its Ready before the line was open is waiting
        //  for a reply, and is prompted for another.
        if (!m_host->HasClient() && !m_host->HasAdtProClient() && std::chrono::steady_clock::now() >= prompt)
        {
            hr = m_host->PromptReady();
            CHR (hr);

            prompt = std::chrono::steady_clock::now() + std::chrono::milliseconds (kPromptMs);
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
//  AppleLinkSession::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkSession::Disconnect()
{
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
//  AppleLinkSession::SetState
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkSession::SetState (LinkState state)
{
    if (m_state.exchange (state) != state)
    {
        m_sink.OnStateChanged (state);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::DescribeOperation
//
//  The Label text for an operation, as the program's screen draws it.
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkSession::DescribeOperation (const LinkOperation & operation)
{
    std::string  path  = operation.path.ToString();
    std::string  text;



    switch (operation.kind)
    {
        case LinkOperationKind::Enumerate:       text = path.empty() ? std::format ("READING THE CATALOG OF DRIVE {}", operation.drive) : "LISTING " + path; break;
        case LinkOperationKind::ReadFile:        text = std::format ("COPYING {} FROM DRIVE {}", path, operation.drive);  break;
        case LinkOperationKind::WriteFile:       text = std::format ("COPYING {} TO DRIVE {}", path, operation.drive);    break;
        case LinkOperationKind::Delete:          text = "DELETING " + path;                                                break;
        case LinkOperationKind::Rename:          text = std::format ("RENAMING {} TO {}", path, operation.newName);        break;
        case LinkOperationKind::MakeDirectory:   text = "MAKING FOLDER " + path;                                           break;
        case LinkOperationKind::RemoveDirectory: text = "DELETING FOLDER " + path;                                         break;
        case LinkOperationKind::ReadDisk:        text = std::format ("READING DRIVE {} TO AN IMAGE", operation.drive);     break;
        case LinkOperationKind::WriteDisk:       text = std::format ("WRITING AN IMAGE TO DRIVE {}", operation.drive);     break;
        default:                                 text = "WORKING";                                                         break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Execute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::Execute (LinkOperation & operation)
{
    HRESULT       hr      = S_OK;
    size_t        index   = (size_t) operation.drive - 1;
    bool          present = false;
    LinkedDisk  * disk    = nullptr;



    present = operation.drive >= 1 && operation.drive <= kDriveCount && (m_driveMap & (1 << index)) != 0;
    CBREx (present, HRESULT_FROM_WIN32 (ERROR_INVALID_DRIVE));

    disk = m_disks[index].get();

    hr = m_host->SendLabel (DescribeOperation (operation));
    CHR (hr);

    if (operation.kind != LinkOperationKind::ReadDisk && operation.kind != LinkOperationKind::WriteDisk)
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

        default:
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
            break;
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Revalidate
//
//  The sectors that identify the volume -- the VTOC, or the volume
//  directory's key block -- are read again; when they differ from what is
//  in memory, another disk is in the drive, or this one was written
//  elsewhere, and its sectors are dropped. The drive's listing is not: a
//  write checks the disk against the drive's last listing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::Revalidate (int drive)
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
        marks.push_back (ProDosSkeleton::GetBlockByteOffset (ProDosSkeleton::kDirKeyBlock, 0) / LinkedDisk::kSectorSize);
        marks.push_back (ProDosSkeleton::GetBlockByteOffset (ProDosSkeleton::kDirKeyBlock, LinkedDisk::kSectorSize) / LinkedDisk::kSectorSize);
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
//  AppleLinkSession::CheckCatalog
//
//  The catalog read from the disk, everything else dropped, against the
//  drive's last listing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::CheckCatalog (int drive)
{
    HRESULT        hr    = S_OK;
    size_t         index = (size_t) drive - 1;
    VolumeListing  listing;
    bool           same  = false;



    BAIL_OUT_IF (!m_listings[index].has_value(), S_OK);

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
//  AppleLinkSession::RunWrite
//
//  After the write, the drive's listing is the volume's new catalog, so the
//  next write compares against what this one left.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::RunWrite (LinkOperation & operation, const LinkedDisk::WriteOperation & write)
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
//  AppleLinkSession::ReadWholeDisk
//
//  The image is written only once every track has been read, so a failed or
//  canceled read writes nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::ReadWholeDisk (LinkOperation & operation)
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
//  AppleLinkSession::ReadTrack
//
//  A track's sixteen sectors in one request. When the drive cannot read one
//  of them, each is read on its own, and those that fail again are listed
//  and written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::ReadTrack (int drive, int track, std::vector<Byte> & sectors, std::vector<std::pair<int, int>> & unreadable)
{
    HRESULT            hr        = S_OK;
    HRESULT            disk      = S_OK;
    std::vector<Byte>  physicals;
    std::vector<Byte>  data;
    AppleLinkStatus    status;
    bool               damaged   = false;
    int                missing   = 0;



    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    hr = m_host->ReadSectors ((Byte) drive, (Byte) track, physicals, data, status);
    CHR (hr);

    disk    = FromStatusError (status.error);
    damaged = status.error == AppleLinkHost::kErrorNotFound || status.error == AppleLinkHost::kErrorData;
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

            if (status.error != AppleLinkHost::kErrorNone)
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
//  AppleLinkSession::WriteWholeDisk
//
//  The image is decoded first, and nothing is written if any track is not
//  standard sectors. The volume number formatted into each address field
//  is the image's own for DOS 3.3, and 254 otherwise. The drive's sectors
//  and listing are then dropped, as the disk is another one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkSession::WriteWholeDisk (LinkOperation & operation)
{
    HRESULT             hr       = S_OK;
    size_t              index    = (size_t) operation.drive - 1;
    std::vector<Byte>   fileBytes;
    std::vector<Byte>   sectors;
    std::vector<Byte>   physicals;
    std::vector<Byte>   data;
    std::vector<int>    badTracks;
    std::string         list;
    SectorDecodeReport  report;
    VolumeListing       listing;
    AppleLinkStatus     status;
    Byte                volume   = 254;
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

    for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
    {
        physicals.push_back ((Byte) physical);
    }

    m_disks[index]->Clear();
    m_listings[index].reset();

    for (int track = 0; track < NibblizationLayer::kTrackCount; track++)
    {
        Report (operation, track * 100 / NibblizationLayer::kTrackCount);

        hr = m_host->FormatTrack ((Byte) operation.drive, (Byte) track, volume, status);
        CHR (hr);

        hr = FromStatusError (status.error);
        CHR (hr);

        data.clear();

        for (int physical = 0; physical < NibblizationLayer::kSectorsPerTrack; physical++)
        {
            auto  from = sectors.begin() + (ptrdiff_t) ((size_t) (track * NibblizationLayer::kSectorsPerTrack + GetLogicalSector (physical)) * LinkedDisk::kSectorSize);

            data.insert (data.end(), from, from + (ptrdiff_t) LinkedDisk::kSectorSize);
        }

        hr = m_host->WriteSectors ((Byte) operation.drive, (Byte) track, physicals, data, status);
        CHR (hr);

        hr = FromStatusError (status.error);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSession::Report
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkSession::Report (LinkOperation & operation, int progress)
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
//  AppleLinkSession::IsSameCatalog
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkSession::IsSameCatalog (const VolumeListing & a, const VolumeListing & b)
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
//  AppleLinkSession::IsLineLost
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkSession::IsLineLost (HRESULT hr)
{
    return hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT) || hr == HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE);
}
