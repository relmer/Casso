#include "Pch.h"

#include "CassoExplorer/Model/AppleLinkConnections.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::AppleLinkConnections
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkConnections::AppleLinkConnections (IByteStreamFactory & factory, IAppleLinkFolder & folder, IDiskFileIo & fileIo) :
    m_factory (factory),
    m_folder  (folder),
    m_fileIo  (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::~AppleLinkConnections
//
//  Each session's thread is stopped before the state it posts to goes.
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkConnections::~AppleLinkConnections()
{
    for (std::unique_ptr<Link> & link : m_started)
    {
        link->session.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::SetLinks
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::SetLinks (const std::vector<AppleLinkSettings> & links)
{
    m_saved = links;
    Merge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::SetRunning
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::SetRunning (const std::vector<RunningCasso> & running)
{
    m_running = running;
    Merge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::IsDiscovered
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::IsDiscovered (const std::string & name) const
{
    bool  saved  = std::any_of (m_saved.begin(), m_saved.end(), [&name] (const AppleLinkSettings & settings) { return settings.name == name; });
    bool  listed = std::any_of (m_links.begin(), m_links.end(), [&name] (const AppleLinkSettings & settings) { return settings.name == name; });



    return listed && !saved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::MakeRunningName
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkConnections::MakeRunningName (const RunningCasso & running)
{
    size_t       colon   = running.address.rfind (':');
    std::string  port    = colon == std::string::npos ? running.address : running.address.substr (colon + 1);
    std::string  machine = running.machine.empty() ? std::string ("Casso") : running.machine;



    UNREFERENCED_PARAMETER (machine);

    return std::format ("Casso on port {}", port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::DescribeConnection
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkConnections::DescribeConnection (const AppleLinkSettings & settings)
{
    size_t       colon = settings.address.rfind (':');
    std::string  host  = (colon == std::string::npos) ? settings.address : settings.address.substr (0, colon);
    std::string  port  = (colon == std::string::npos) ? std::string() : settings.address.substr (colon + 1);



    switch (settings.transport)
    {
        case LinkTransport::Serial:    return settings.port.empty() ? std::string ("Serial") : settings.port;
        case LinkTransport::Emulated:  return port.empty() ? std::string ("Casso") : std::format ("Casso on port {}", port);
        case LinkTransport::WifiModem: return port.empty() ? std::string ("Wi-Fi modem") : std::format ("Wi-Fi modem on port {}", port);
        default:                       return port.empty() ? std::format ("TCP {}", host) : std::format ("TCP {} port {}", host, port);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Merge
//
//  The saved links and the running Cassos into the one list the tree and
//  the sessions use; a started link no longer in it, or changed, is stopped.
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Merge()
{
    std::vector<AppleLinkSettings>  links = m_saved;
    std::vector<std::string>        stale;



    m_unusable.clear();

    for (const RunningCasso & running : m_running)
    {
        AppleLinkSettings  settings;
        bool               covered  = false;

        settings.name      = MakeRunningName (running);
        settings.transport = LinkTransport::Emulated;
        settings.address   = running.address;

        covered = std::any_of (links.begin(), links.end(), [&settings] (const AppleLinkSettings & link)
        {
            bool  sameAddress = (link.transport == LinkTransport::Emulated || link.transport == LinkTransport::Tcp) && link.address == settings.address;

            return link.name == settings.name || sameAddress;
        });

        if (covered)
        {
            continue;
        }

        if (running.serialPort != 2)
        {
            m_unusable[settings.name] = std::format (L"Serial port {} of this Casso is on TCP, and the CassoLink program uses port 2. "
                                                     L"Connect port 2 in Casso's Hardware settings.", running.serialPort);
        }

        links.push_back (settings);
    }

    for (const std::unique_ptr<Link> & link : m_started)
    {
        bool  current = std::any_of (links.begin(), links.end(), [&link] (const AppleLinkSettings & settings) { return settings == link->settings; });

        if (!current)
        {
            stale.push_back (link->settings.name);
        }
    }

    for (const std::string & name : stale)
    {
        Stop (name);
    }

    m_links = links;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkConnections::Start (const std::string & name, bool search)
{
    return StartLink (name, search, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Bootstrap
//
//  The link is started again, typing the loader in first; the completion
//  runs when the program is running or the Apple does not answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkConnections::Bootstrap (const std::string & name, Completion completion)
{
    HRESULT                         hr        = S_OK;
    std::shared_ptr<LinkOperation>  operation;



    Stop (name);

    hr = StartLink (name, false, &operation);
    CHR (hr);

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_jobs.push_back (Job { name, operation, std::move (completion) });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::StartLink
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkConnections::StartLink (const std::string & name, bool search, std::shared_ptr<LinkOperation> * outBootstrap)
{
    HRESULT                hr       = S_OK;
    std::unique_ptr<Link>  link;
    AppleLinkSettings      opening;
    auto                   settings = std::find_if (m_links.begin(), m_links.end(), [&name] (const AppleLinkSettings & s) { return s.name == name; });
    bool                   known    = settings != m_links.end();
    bool                   usable   = !m_unusable.contains (name);



    BAIL_OUT_IF (IsStarted (name), S_OK);
    CBREx (known, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));
    CBREx (usable, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    link           = std::make_unique<Link>();
    link->settings = *settings;
    link->sink     = std::make_unique<Sink> (*this);
    link->session  = std::make_unique<AppleLinkSession> (m_factory, m_folder, m_fileIo, *link->sink);

    //  A search opens the ports in turn even when the link has one set.
    opening = link->settings;

    if (search)
    {
        opening.port.clear();
    }

    if (outBootstrap != nullptr)
    {
        hr = link->session->StartWithBootstrap (opening, *outBootstrap);
    }
    else
    {
        hr = link->session->Start (opening);
    }

    CHR (hr);

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_started.push_back (std::move (link));
    }

    Notify();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Stop
//
//  The session is stopped outside the lock, as its thread's last calls to the
//  sink take the lock too.
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Stop (const std::string & name)
{
    std::unique_ptr<Link>  stopped;



    {
        std::lock_guard<std::mutex>  lock (m_lock);
        auto                         found = std::find_if (m_started.begin(), m_started.end(), [&name] (const std::unique_ptr<Link> & link) { return link->settings.name == name; });

        if (found != m_started.end())
        {
            stopped = std::move (*found);
            m_started.erase (found);
        }
    }

    if (stopped)
    {
        stopped->session.reset();
        Notify();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::IsStarted
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::IsStarted (const std::string & name) const
{
    return FindLink (name) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::GetSession
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkSession * AppleLinkConnections::GetSession (const std::string & name)
{
    Link  * link = FindLink (name);



    return link != nullptr ? link->session.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::BuildTreeEntries
//
//  A drive's volume comes from its catalog once that has been read: a DOS
//  3.3 volume by its number, a ProDOS one by its name. A drive whose catalog
//  found no sectors at all is empty.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<LinkTreeEntry> AppleLinkConnections::BuildTreeEntries() const
{
    std::vector<LinkTreeEntry>   entries;
    std::lock_guard<std::mutex>  lock (m_lock);



    for (const AppleLinkSettings & settings : m_links)
    {
        LinkTreeEntry  entry;
        auto           found = std::find_if (m_started.begin(), m_started.end(), [&settings] (const std::unique_ptr<Link> & link) { return link->settings.name == settings.name; });

        auto           unusable = m_unusable.find (settings.name);

        entry.name = TextEncoding::NarrowToWide (settings.name);

        if (unusable != m_unusable.end())
        {
            entry.status = unusable->second;
            entries.push_back (entry);
            continue;
        }

        if (found == m_started.end())
        {
            entry.status = DescribeState (LinkState::Closed, false, settings, GetPcName());
        }
        else
        {
            LinkState  state = (*found)->session->GetState();

            entry.replying = state == LinkState::Ready || state == LinkState::Busy;
            entry.model    = entry.replying ? (*found)->session->GetModelName() : std::wstring();
            entry.status   = DescribeState (state, (*found)->session->IsServingAdtPro(), settings, GetPcName());
        }

        if (found == m_started.end())
        {
            entries.push_back (entry);
            continue;
        }

        Byte  map = (*found)->session->GetDriveMap();

        for (int drive = 1; entry.replying && drive <= AppleLinkSession::kDriveCount; drive++)
        {
            LinkTreeEntry::Drive  row;
            auto                  catalog = (*found)->requests.find (MakeKey (drive, std::string(), false));

            if ((map & (1 << (drive - 1))) == 0)
            {
                continue;
            }

            row.slot  = 6;
            row.drive = drive;

            if (catalog != (*found)->requests.end() && catalog->second->state == LinkOperationState::Done)
            {
                const VolumeListing  & listing = catalog->second->listing;

                row.volume = listing.hasVolumeName   ? L"/" + TextEncoding::NarrowToWide (listing.volumeName)
                           : listing.hasVolumeNumber ? std::format (L"DOS 3.3 volume {}", listing.volumeNumber)
                                                     : std::wstring();

                for (const FileEntry & entry : listing.entries)
                {
                    if (entry.isDirectory)
                    {
                        row.folders.push_back (TextEncoding::NarrowToWide (entry.name));
                    }
                }
            }
            else if (catalog != (*found)->requests.end() && catalog->second->state == LinkOperationState::Failed)
            {
                row.empty = catalog->second->result == HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND);
            }

            entry.drives.push_back (row);
        }

        entries.push_back (entry);
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::TryGetListing
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::TryGetListing (const std::string & name, int drive, const std::string & directory,
                                          VolumeListing & outListing, VolumeKind & outKind, HRESULT & outResult, std::string & outMessage)
{
    std::shared_ptr<LinkOperation>  operation;
    bool                            arrived = TryGetResult (name, drive, MakeKey (drive, directory, false), LinkOperationKind::Enumerate, directory, operation);



    if (arrived)
    {
        outListing = operation->listing;
        outKind    = operation->volumeKind;
        outResult  = operation->result;
        outMessage = operation->message;
    }

    return arrived;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::TryGetFile
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::TryGetFile (const std::string & name, int drive, const std::string & path,
                                       FilePayload & outPayload, HRESULT & outResult, std::string & outMessage)
{
    std::shared_ptr<LinkOperation>  operation;
    bool                            arrived = TryGetResult (name, drive, MakeKey (drive, path, true), LinkOperationKind::ReadFile, path, operation);



    if (arrived)
    {
        outPayload = operation->file;
        outResult  = operation->result;
        outMessage = operation->message;
    }

    return arrived;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Submit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkConnections::Submit (const std::string & name, const Queuer & queue, Completion completion)
{
    HRESULT  hr      = S_OK;
    Link   * link    = nullptr;
    bool     started = false;



    if (!IsStarted (name))
    {
        hr = Start (name);
        CHR (hr);
    }

    link    = FindLink (name);
    started = link != nullptr;
    CBREx (started, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_jobs.push_back (Job { name, queue (*link->session), std::move (completion) });
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::RunCompletions
//
//  The finished jobs are taken under the lock and completed outside it, as a
//  completion may queue more.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> AppleLinkConnections::RunCompletions()
{
    std::vector<Job>           finished;
    std::vector<std::wstring>  messages;



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        for (auto job = m_jobs.begin(); job != m_jobs.end(); )
        {
            LinkOperationState  state = job->operation->state;

            if (state == LinkOperationState::Queued || state == LinkOperationState::Running)
            {
                job++;
                continue;
            }

            finished.push_back (std::move (*job));
            job = m_jobs.erase (job);
        }
    }

    for (const Job & job : finished)
    {
        std::wstring  message = job.completion ? job.completion (*job.operation) : std::wstring();

        if (!message.empty())
        {
            messages.push_back (message);
        }
    }

    return messages;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::GetPendingJobs
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LinkOperation>> AppleLinkConnections::GetPendingJobs() const
{
    std::vector<std::shared_ptr<LinkOperation>>  pending;
    std::lock_guard<std::mutex>                  lock (m_lock);



    for (const Job & job : m_jobs)
    {
        LinkOperationState  state = job.operation->state;

        if (state == LinkOperationState::Queued || state == LinkOperationState::Running)
        {
            pending.push_back (job.operation);
        }
    }

    return pending;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::IsWritingDisk
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::IsWritingDisk (const std::string & name, int drive) const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return std::any_of (m_jobs.begin(), m_jobs.end(), [&] (const Job & job)
    {
        LinkOperationState  state = job.operation->state;

        return job.link == name && job.operation->kind == LinkOperationKind::WriteDisk && job.operation->drive == drive
            && (state == LinkOperationState::Queued || state == LinkOperationState::Running);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::WaitForFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AppleLinkConnections::WaitForFile (const std::string & name, int drive, const std::string & path, DWORD timeoutMs, FilePayload & outPayload)
{
    HRESULT      hr      = S_OK;
    std::string  message;
    auto         until   = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
    bool         arrived = false;



    while (!arrived && std::chrono::steady_clock::now() < until)
    {
        arrived = TryGetFile (name, drive, path, outPayload, hr, message);

        if (!arrived)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (kWaitPollMs));
        }
    }

    CBREx (arrived, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::DescribeJobs
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleLinkConnections::DescribeJobs (const std::vector<std::shared_ptr<LinkOperation>> & jobs)
{
    std::wstring  text;
    std::wstring  what;



    if (jobs.empty())
    {
        return text;
    }

    switch (jobs[0]->kind)
    {
        case LinkOperationKind::ReadFile:        what = L"Reading a file";    break;
        case LinkOperationKind::WriteFile:       what = L"Writing a file";    break;
        case LinkOperationKind::Delete:          what = L"Deleting";          break;
        case LinkOperationKind::Rename:          what = L"Renaming";          break;
        case LinkOperationKind::MakeDirectory:   what = L"Making a folder";   break;
        case LinkOperationKind::RemoveDirectory: what = L"Deleting a folder"; break;
        case LinkOperationKind::ReadDisk:        what = L"Reading the disk";  break;
        case LinkOperationKind::WriteDisk:       what = L"Writing the disk";  break;
        case LinkOperationKind::CheckLinkDisk:   what = L"Checking the disk"; break;
        default:                                 what = L"Working";           break;
    }

    text = std::format (L"{} in drive {}: {}%", what, jobs[0]->drive, jobs[0]->progress.load());

    if (jobs.size() > 1)
    {
        text += std::format (L", {} more queued", jobs.size() - 1);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::DescribeDial
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleLinkConnections::DescribeDial (const std::wstring & pcName, const AppleLinkSettings & settings)
{
    AppleLinkSettings  modem = settings;



    modem.transport = LinkTransport::WifiModem;

    return std::format (L"Dial {} from the modem.", TextEncoding::NarrowToWide (MakeDialCommand (pcName, modem)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::MakeDialCommand
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkConnections::MakeDialCommand (const std::wstring & pcName, const AppleLinkSettings & settings)
{
    size_t       colon = settings.address.rfind (':');
    std::string  port  = colon == std::string::npos ? settings.address : settings.address.substr (colon + 1);
    std::string  dial;



    if (settings.transport == LinkTransport::WifiModem)
    {
        dial = std::format ("ATDT {}.local:{}", TextEncoding::WideToNarrow (pcName), port);
    }

    return dial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::GetPcName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleLinkConnections::GetPcName()
{
    wchar_t  name[256] = {};
    DWORD    size      = (DWORD) std::size (name);



    if (!GetComputerNameExW (ComputerNameDnsHostname, name, &size))
    {
        size = 0;
    }

    return std::wstring (name, size);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::DescribeFailure
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleLinkConnections::DescribeFailure (const LinkOperation & operation)
{
    std::wstring  what = operation.path.IsEmpty() ? std::format (L"Drive {}", operation.drive) : TextEncoding::NarrowToWide (operation.path.ToString());
    std::wstring  why;



    if (!operation.message.empty())
    {
        return TextEncoding::NarrowToWide (operation.message);
    }

    switch (operation.result)
    {
        case HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT):    why = std::format (L"the disk in drive {} is write-protected.", operation.drive); break;
        case HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND): why = L"the disk could not be read; it may be missing or unformatted.";              break;
        case HRESULT_FROM_WIN32 (ERROR_CRC):              why = L"a sector on the disk is damaged.";                                         break;
        case HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT):      why = L"a sector did not read back as written.";                                   break;
        case HRESULT_FROM_WIN32 (ERROR_DISK_FULL):        why = L"the disk is full.";                                                        break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS):
        case HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS):   why = L"an entry with that name is already there.";                                break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND):   why = L"it is not on the disk.";                                                   break;
        case HRESULT_FROM_WIN32 (ERROR_CANCELLED):        why = L"it was canceled.";                                                         break;
        default:                                          why = L"the Apple did not complete it.";                                           break;
    }

    return what + L": " + why;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Invalidate
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Invalidate (const std::string & name, int drive)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    std::string                  prefix = std::format ("{}|", drive);



    for (std::unique_ptr<Link> & link : m_started)
    {
        if (link->settings.name == name)
        {
            std::erase_if (link->requests, [&prefix] (const auto & request)
            {
                LinkOperationState  state = request.second->state;

                return request.first.starts_with (prefix) && state != LinkOperationState::Queued && state != LinkOperationState::Running;
            });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::TakeChanged
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::TakeChanged()
{
    return m_changed.exchange (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::DescribeState
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleLinkConnections::DescribeState (LinkState state, bool servingAdtPro, const AppleLinkSettings & settings, const std::wstring & pcName)
{
    std::wstring  name    = TextEncoding::NarrowToWide (settings.name);
    std::wstring  address = TextEncoding::NarrowToWide (settings.address);
    std::wstring  port    = TextEncoding::NarrowToWide (settings.port);
    std::wstring  baud    = FormatBaud (settings.baud);
    std::wstring  text;



    if (servingAdtPro)
    {
        return std::format (L"ADTPro's own Apple program is running on {}. Browsing and copying work only with Casso's program: "
                            L"choose Bootstrap the Apple from {}'s menu to load it.", name, name);
    }

    switch (state)
    {
        case LinkState::Closed:          text = std::format (L"Click {} to connect.", name);                     break;
        case LinkState::Bootstrapping:   text = std::format (L"Typing the loader into {}...", name);              break;
        case LinkState::LoadingProgram:  text = std::format (L"Loading Casso's program into {}...", name);        break;
        case LinkState::Ready:
        case LinkState::Busy:            text = L"Connected.";                                                     break;

        case LinkState::Searching:
            text = std::format (L"Searching the COM ports for {} at {} and {} baud.\nBoot a CassoLink disk on the Apple, "
                                L"or choose Bootstrap the Apple from {}'s menu.", name, FormatBaud (settings.bootstrapBaud), baud, name);
            break;

        //  Opening, waiting for Ready, or no Apple yet: what to do on the Apple.
        default:
            switch (settings.transport)
            {
                case LinkTransport::Serial:
                    text = std::format (L"Searching for {} on {} at {} baud.\nBoot a CassoLink disk on the Apple, or choose Bootstrap the Apple from {}'s menu.",
                                        name, port.empty() ? std::wstring (L"the COM ports") : port, baud, name);
                    break;

                case LinkTransport::Emulated:
                    text = std::format (L"Waiting for {} on {}.\nBoot a CassoLink disk in that Casso to connect.", name, address);
                    break;

                case LinkTransport::WifiModem:
                    text = std::format (L"Waiting for {} to dial {}.local:{}.\nBoot that Apple from a CassoLink disk saved from {}.",
                                        name, pcName, address.substr (address.rfind (L':') + 1), name);
                    break;

                default:
                    text = std::format (L"Connecting to {} at {}.\nStart the CassoLink program on that machine.", name, address);
                    break;
            }

            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::FormatBaud
//
////////////////////////////////////////////////////////////////////////////////

std::wstring AppleLinkConnections::FormatBaud (DWORD baud)
{
    std::wstring  digits = std::to_wstring (baud);



    for (int at = (int) digits.size() - 3; at > 0; at -= 3)
    {
        digits.insert ((size_t) at, L",");
    }

    return digits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Sink::OnStateChanged
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Sink::OnStateChanged (LinkState)
{
    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Sink::OnProgress
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Sink::OnProgress (const std::shared_ptr<LinkOperation> &)
{
    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Sink::OnDone
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Sink::OnDone (const std::shared_ptr<LinkOperation> &)
{
    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::MakeKey
//
////////////////////////////////////////////////////////////////////////////////

std::string AppleLinkConnections::MakeKey (int drive, const std::string & path, bool file)
{
    return std::format ("{}|{}|{}", drive, file ? "file" : "dir", path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::FindLink
//
////////////////////////////////////////////////////////////////////////////////

AppleLinkConnections::Link * AppleLinkConnections::FindLink (const std::string & name)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    auto                         found = std::find_if (m_started.begin(), m_started.end(), [&name] (const std::unique_ptr<Link> & link) { return link->settings.name == name; });



    return found != m_started.end() ? found->get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::FindLink
//
////////////////////////////////////////////////////////////////////////////////

const AppleLinkConnections::Link * AppleLinkConnections::FindLink (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);
    auto                         found = std::find_if (m_started.begin(), m_started.end(), [&name] (const std::unique_ptr<Link> & link) { return link->settings.name == name; });



    return found != m_started.end() ? found->get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::TryGetResult
//
//  The operation under the key once it is done; false while it is queued or
//  running, and on the first request for it, when it is queued, the link
//  started first if it was not.
//
////////////////////////////////////////////////////////////////////////////////

bool AppleLinkConnections::TryGetResult (const std::string & name, int drive, const std::string & key, LinkOperationKind kind,
                                         const std::string & path, std::shared_ptr<LinkOperation> & outOperation)
{
    HRESULT   hr      = S_OK;
    Link    * link    = nullptr;
    FilePath  target  = path.empty() ? FilePath() : FilePath::Parse (path);
    bool      arrived = false;



    if (!IsStarted (name))
    {
        hr = Start (name);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    link = FindLink (name);

    if (link == nullptr)
    {
        outOperation          = std::make_shared<LinkOperation>();
        outOperation->result  = HRESULT_FROM_WIN32 (ERROR_NOT_FOUND);
        outOperation->message = "CassoLink could not be started.";
        return true;
    }

    std::lock_guard<std::mutex>  lock (m_lock);
    auto                         found = link->requests.find (key);

    if (found != link->requests.end())
    {
        LinkOperationState  state = found->second->state;

        arrived      = state != LinkOperationState::Queued && state != LinkOperationState::Running;
        outOperation = found->second;
    }
    else
    {
        link->requests[key] = (kind == LinkOperationKind::Enumerate) ? link->session->Enumerate (drive, target)
                                                                     : link->session->ReadFile (drive, target);
    }

    return arrived;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkConnections::Notify
//
////////////////////////////////////////////////////////////////////////////////

void AppleLinkConnections::Notify()
{
    m_changed = true;

    if (m_hwnd != nullptr)
    {
        PostMessageW (m_hwnd, m_message, 0, 0);
    }
}
