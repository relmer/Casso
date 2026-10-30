#include "Pch.h"

#include "CassoLink/CassoLinkSession.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
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
    bool     idle    = !m_connector.m_thread.joinable();



    CBRAEx (idle, E_UNEXPECTED);

    known = LinkProgram::GetSpeedControl (settings.baud, control) && LinkProgram::GetSpeedControl (settings.bootstrapBaud, control);
    CBREx (known, E_INVALIDARG);

    m_settings           = settings;
    m_connector.m_port   = TextEncoding::NarrowToWide (settings.port);
    m_stopping           = false;
    m_connector.m_thread = std::jthread ([this] (std::stop_token stop) { Run (stop); });

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
    if (m_connector.m_thread.joinable())
    {
        m_stopping = true;
        m_connector.m_thread.request_stop();
        m_connector.m_wake.notify_all();

        {
            std::lock_guard<std::mutex>  lock (m_connector.m_lock);

            if (m_connector.m_stream)
            {
                m_connector.m_stream->Close();
            }
        }

        m_connector.m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Cancel()
{
    std::lock_guard<std::mutex>  lock (m_connector.m_lock);



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
//  CassoLinkSession::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LinkOperation> CassoLinkSession::SetAttributes (int drive, const FilePath & path, const FileAttributes & attributes)
{
    std::shared_ptr<LinkOperation>  operation = std::make_shared<LinkOperation>();



    operation->attributes = attributes;

    return Queue (LinkOperationKind::SetAttributes, drive, path, operation);
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
//  CassoLinkSessionTypes::DriveSource::ReadSectors
//
//  The sectors are read a track at a time, by their physical numbers, and
//  returned in the order of the list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSessionTypes::DriveSource::ReadSectors (std::span<const size_t> sectors, std::vector<Byte> & outData)
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
                physicals.push_back ((Byte) CassoLinkSession::GetPhysicalSector ((int) (sectors[i] % NibblizationLayer::kSectorsPerTrack)));
                places.push_back (i);
            }
        }

        if (physicals.empty())
        {
            continue;
        }

        hr = m_session.m_host->ReadSectors ((Byte) m_drive, (Byte) track, physicals, data, status);
        CHR (hr);

        hr = CassoLinkSession::FromStatusError (status.error);
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
//  CassoLinkSessionTypes::DriveSource::WriteSectors
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkSessionTypes::DriveSource::WriteSectors (std::span<const size_t> sectors, std::span<const Byte> data)
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
                physicals.push_back ((Byte) CassoLinkSession::GetPhysicalSector ((int) (sectors[i] % NibblizationLayer::kSectorsPerTrack)));
                trackData.insert (trackData.end(), data.begin() + (ptrdiff_t) (i * LinkedDisk::kSectorSize), data.begin() + (ptrdiff_t) ((i + 1) * LinkedDisk::kSectorSize));
            }
        }

        if (physicals.empty())
        {
            continue;
        }

        hr = m_session.m_host->WriteSectors ((Byte) m_drive, (Byte) track, physicals, trackData, status);
        CHR (hr);

        hr = CassoLinkSession::FromStatusError (status.error);
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
        std::lock_guard<std::mutex>  lock (m_connector.m_lock);

        operation->id = m_connector.m_nextId++;
        m_connector.m_queue.push_back (operation);
    }

    m_connector.m_wake.notify_all();

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
    bool                            lost   = false;
    bool                            server = m_settings.transport == LinkTransport::AdtProServer;
    std::shared_ptr<LinkOperation>  operation;



    if (server)
    {
        m_connector.ServeAdtPro (stop);
    }
    else if (m_connector.m_bootstrap)
    {
        hr = m_connector.RunBootstrap (stop, *m_connector.m_bootstrap);
        m_sink.OnDone (m_connector.m_bootstrap);
        m_connector.m_bootstrap.reset();
    }
    else
    {
        hr = m_connector.ConnectOrBootstrap (stop);
    }

    while (!server && !stop.stop_requested())
    {
        if (FAILED (hr) || m_connector.m_state != LinkState::Ready)
        {
            {
                std::unique_lock<std::mutex>  lock (m_connector.m_lock);

                for (const std::shared_ptr<LinkOperation> & queued : m_connector.m_queue)
                {
                    queued->result  = FAILED (hr) ? hr : HRESULT_FROM_WIN32 (ERROR_NOT_CONNECTED);
                    queued->message = "The Apple did not answer.";
                    queued->state   = LinkOperationState::Failed;
                    m_sink.OnDone (queued);
                }

                m_connector.m_queue.clear();
            }

            //  Connecting again after a while, or at once when a request
            //  arrives, so a link started before its Apple is ready connects
            //  when it is.
            m_connector.ServeUntilRetry (stop);

            hr = stop.stop_requested() ? S_OK : m_connector.ConnectOrBootstrap (stop);
            continue;
        }

        {
            std::lock_guard<std::mutex>  lock (m_connector.m_lock);

            operation = nullptr;

            if (!m_connector.m_queue.empty())
            {
                operation = m_connector.m_queue.front();
                m_connector.m_queue.pop_front();
                m_running = operation;
            }
        }

        if (!operation)
        {
            hr = m_host->ServeOne (kIdlePollMs);

            m_connector.m_lostReason = FAILED (hr)                          ? DescribeLineFailure (hr)
                         : (m_host->GetSilenceMs() > kSilenceMs) ? std::format ("no packet in {} seconds", kSilenceMs / 1000)
                         : m_connector.IsPeerGone()                          ? std::string ("DSR went off, the Apple's DTR on most cables")
                                                                 : std::string();

            if (!m_connector.m_lostReason.empty())
            {
                Disconnect();
                hr = S_OK;
            }

            continue;
        }

        SetState (LinkState::Busy);
        GetLocalTime (&operation->started);
        operation->state = LinkOperationState::Running;
        m_sink.OnProgress (operation);

        hr = Execute (*operation);

        {
            std::lock_guard<std::mutex>  lock (m_connector.m_lock);

            m_running = nullptr;
        }

        operation->result   = hr;
        operation->progress = 100;

        if (operation->kind != LinkOperationKind::Enumerate && operation->kind != LinkOperationKind::CheckLinkDisk)
        {
            m_sink.OnFinished (*operation, std::format ("{}: {}", DescribeOperation (*operation), SUCCEEDED (hr) ? "done" : "failed"));
        }

        operation->state    = SUCCEEDED (hr)                              ? LinkOperationState::Done
                            : hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED)  ? LinkOperationState::Canceled
                                                                          : LinkOperationState::Failed;
        m_sink.OnDone (operation);

        //  A failure after which the stream is closed, such as an unplugged
        //  USB adapter, is a lost line whatever its code.
        lost = IsLineLost (hr) || (FAILED (hr) && !m_connector.IsLineOpen());

        if (!lost && operation->kind != LinkOperationKind::Enumerate && operation->kind != LinkOperationKind::CheckLinkDisk)
        {
            logged = m_connector.LogOnApple (std::format ("{} {}", SUCCEEDED (hr) ? "OK " : "ERR", DescribeOperation (*operation, std::nullopt, SUCCEEDED (hr))));
            IGNORE_RETURN_VALUE (logged, S_OK);
        }

        if (lost)
        {
            m_connector.m_lostReason = std::format ("{}: {}", DescribeOperation (*operation), DescribeLineFailure (hr));
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
        std::lock_guard<std::mutex>  lock (m_connector.m_lock);

        for (const std::shared_ptr<LinkOperation> & queued : m_connector.m_queue)
        {
            queued->result = HRESULT_FROM_WIN32 (ERROR_CANCELLED);
            queued->state  = LinkOperationState::Canceled;
            m_sink.OnDone (queued);
        }

        m_connector.m_queue.clear();
    }

    Disconnect();
    SetState (LinkState::Closed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Disconnect()
{
    m_connector.m_peerSeen       = false;
    m_connector.m_peerOffSinceMs = 0;

    {
        std::lock_guard<std::mutex>  lock (m_connector.m_lock);

        if (m_connector.m_stream)
        {
            m_connector.m_stream->Close();
        }

        m_host.reset();
        m_connector.m_stream.reset();
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
    LinkState  before = m_connector.m_state.exchange (state);



    if (before == state)
    {
        return;
    }

    if (state == LinkState::Ready && before != LinkState::Busy)
    {
        //  A new connection may be to another Apple, with other drives.
        m_diskTransfer.m_revolutionMs = {};

        m_sink.OnActivity (std::format ("Connected to {}", TextEncoding::WideToNarrow (GetModelName().empty() ? std::wstring (L"the Apple") : GetModelName())));
    }
    else if (state == LinkState::Disconnected && (before == LinkState::Ready || before == LinkState::Busy))
    {
        m_sink.OnActivity (m_stopping           ? std::string ("Disconnected")
                         : m_connector.m_lostReason.empty() ? std::string ("The Apple stopped answering")
                                                : "The Apple stopped answering: " + m_connector.m_lostReason);
    }

    m_connector.m_lostReason.clear();

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
        case LinkOperationKind::SetAttributes:   text = std::format ("{} {}", finished ? "CHANGED" : "CHANGING", path);                     break;
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



    present = operation.drive >= 1 && operation.drive <= kDriveCount && (m_connector.m_driveMap & (1 << index)) != 0;
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

        case LinkOperationKind::SetAttributes:
            hr = RunWrite (operation, [&] (const IVolume & volume, std::vector<Byte> & outBuffer) { return volume.SetAttributes (operation.path, operation.attributes, outBuffer); });
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
            hr = m_diskTransfer.ReadWholeDisk (operation);
            break;

        case LinkOperationKind::WriteDisk:
            hr = m_diskTransfer.WriteWholeDisk (operation);
            break;

        case LinkOperationKind::CheckLinkDisk:
            hr = m_diskTransfer.ReadBootSector (operation);
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
//  CassoLinkSession::Report
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkSession::Report (LinkOperation & operation, int progress)
{
    std::shared_ptr<LinkOperation>  running;



    operation.progress = progress;

    {
        std::lock_guard<std::mutex>  lock (m_connector.m_lock);

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





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSession::DescribeLineFailure
//
//  The system's text for an error starts with a capital and ends with a
//  period, so it is fitted into the sentence it ends.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkSession::DescribeLineFailure (HRESULT hr)
{
    std::string  text;



    if (hr == HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE) || hr == HRESULT_FROM_WIN32 (WSAECONNRESET) || hr == HRESULT_FROM_WIN32 (ERROR_NETNAME_DELETED))
    {
        text = "the connection closed";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_TIMEOUT))
    {
        text = "no reply";
    }
    else if (hr == ByteStreamSettings::kCtsOff)
    {
        text = "the PC's CTS line is off, so the cable may not connect it; turn off hardware handshake in the connection's settings";
    }
    else
    {
        text = std::system_category().message ((int) hr);

        while (!text.empty() && (text.back() == '.' || text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
        {
            text.pop_back();
        }

        if (!text.empty())
        {
            text[0] = (char) tolower ((unsigned char) text[0]);
        }

        text = text.empty() ? std::format ("the line failed with 0x{:08X}", (uint32_t) hr) : "the line failed: " + text;
    }

    return text;
}
