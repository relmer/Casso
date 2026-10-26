#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "Core/TextEncoding.h"
#include "Seams/IByteStreamFactory.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallText.h"
#include "Seams/ILinkAnnouncements.h"
#include "Seams/Win32NetworkAdapters.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::CassoLinkConnections
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnections::CassoLinkConnections (IByteStreamFactory & factory, ICassoLinkFolder & folder, IDiskFileIo & fileIo) :
    m_factory (factory),
    m_folder  (folder),
    m_fileIo  (fileIo)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::~CassoLinkConnections
//
//  Each session's thread is stopped before the state it posts to goes.
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnections::~CassoLinkConnections()
{
    for (std::unique_ptr<Link> & link : m_started)
    {
        link->session.reset();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::SetLinks
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetLinks (const std::vector<CassoLinkSettings> & links)
{
    HRESULT  hr = S_OK;



    m_saved = links;

    //  With the port bound by another program without reuse, no Apple is
    //  listed from an announcement; nothing else depends on them.
    if (m_announcements != nullptr)
    {
        hr = m_announcements->SetListening (IsSearching());
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    Merge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsSearching
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsSearching() const
{
    return std::any_of (m_saved.begin(), m_saved.end(), [] (const CassoLinkSettings & settings) { return settings.IsSearch(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetInboundPort
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::GetInboundPort (const CassoLinkSettings & settings, FirewallPort & outPort)
{
    bool  inbound = settings.IsSearch() || settings.transport == LinkTransport::WifiModem;



    if (inbound)
    {
        outPort.protocol = settings.IsSearch() ? HostSocketKind::Udp : HostSocketKind::Tcp;
        outPort.port     = settings.IsSearch() ? LinkProgram::kAnnouncePort : CassoLinkSettings::kModemPort;
    }

    return inbound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::AddFirewall
//
//  The cause comes before the connection's state in the list, since none of
//  the state's instructions can work until the port is allowed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::AddFirewall (const CassoLinkSettings & settings, LinkTreeEntry & entry) const
{
    FirewallPort     port;
    FirewallVerdict  verdict;



    if (m_firewall == nullptr || !GetInboundPort (settings, port))
    {
        return;
    }

    verdict = m_firewall->Check (port.protocol, port.port);

    if (verdict.IsAdmitted())
    {
        return;
    }

    entry.firewall       = FirewallText::DescribeCause (verdict, kFirewallTitle, port);
    entry.firewallPublic = verdict.IsOnPublic();
    entry.status         = std::format (L"{}{}{}\n\n{}", entry.firewall, entry.firewallPublic ? L" " : L"",
                                        entry.firewallPublic ? FirewallText::kPublicChoice : L"", entry.status);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::SetRunning
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetRunning (const std::vector<RunningCasso> & running)
{
    m_running = running;
    Merge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::SetAnnounced
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetAnnounced (const std::vector<LinkAnnouncement> & announced)
{
    m_announced = announced;
    Merge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsDiscovered
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsDiscovered (const std::string & name) const
{
    bool  saved  = std::any_of (m_saved.begin(), m_saved.end(), [&name] (const CassoLinkSettings & settings) { return settings.name == name; });
    bool  listed = std::any_of (m_links.begin(), m_links.end(), [&name] (const CassoLinkSettings & settings) { return settings.name == name; });



    return listed && !saved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::MakeRunningName
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkConnections::MakeRunningName (const RunningCasso & running)
{
    size_t       colon   = running.address.rfind (':');
    std::string  port    = colon == std::string::npos ? running.address : running.address.substr (colon + 1);
    std::string  machine = running.machine.empty() ? std::string ("Casso") : running.machine;



    UNREFERENCED_PARAMETER (machine);

    return std::format ("Casso on port {}", port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ListComPorts
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::ListComPorts (std::vector<std::wstring> & outPorts)
{
    return m_factory.ListComPorts (outPorts);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Merge
//
//  The saved links, the running Cassos and the announcements' Apples into the
//  one list the tree and the sessions use; a started link no longer in it, or
//  changed, is stopped.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Merge()
{
    std::vector<CassoLinkSettings>  links = m_saved;
    std::vector<std::string>        stale;



    for (const RunningCasso & running : m_running)
    {
        CassoLinkSettings  settings;
        bool               covered  = false;

        settings.name      = MakeRunningName (running);
        settings.transport = LinkTransport::Emulated;
        settings.address   = running.address;

        covered = std::any_of (links.begin(), links.end(), [&settings] (const CassoLinkSettings & link)
        {
            bool  sameAddress = (link.transport == LinkTransport::Emulated || link.transport == LinkTransport::Tcp) && link.address == settings.address;

            return link.name == settings.name || sameAddress;
        });

        if (covered)
        {
            continue;
        }

        links.push_back (settings);
    }

    for (const LinkAnnouncement & announced : IsSearching() ? m_announced : std::vector<LinkAnnouncement>())
    {
        CassoLinkSettings  settings;
        bool               covered  = false;

        settings.transport = LinkTransport::Uthernet;
        settings.address   = announced.GetAddressText();
        settings.name      = CassoLinkSettings::MakeName (settings);

        covered = std::any_of (links.begin(), links.end(), [&settings] (const CassoLinkSettings & link)
        {
            return link.name == settings.name || (link.transport == LinkTransport::Uthernet && link.address == settings.address);
        });

        if (!covered)
        {
            links.push_back (settings);
        }
    }

    for (const std::unique_ptr<Link> & link : m_started)
    {
        bool  current = std::any_of (links.begin(), links.end(), [&link] (const CassoLinkSettings & settings) { return settings == link->settings; });

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
//  CassoLinkConnections::Start
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::Start (const std::string & name, bool search)
{
    return StartLink (name, search, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Bootstrap
//
//  The link is started again, typing the loader in first; the completion
//  runs when the program is running or the Apple does not answer.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::Bootstrap (const std::string & name, Completion completion)
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
//  CassoLinkConnections::StartLink
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::StartLink (const std::string & name, bool search, std::shared_ptr<LinkOperation> * outBootstrap)
{
    HRESULT                hr       = S_OK;
    std::unique_ptr<Link>  link;
    CassoLinkSettings      opening;
    auto                   settings = std::find_if (m_links.begin(), m_links.end(), [&name] (const CassoLinkSettings & s) { return s.name == name; });
    bool                   known    = settings != m_links.end();



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_held.erase (name);
    }

    BAIL_OUT_IF (IsStarted (name), S_OK);
    CBREx (known, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    //  A search has no Apple to connect to; each Apple it finds is listed as
    //  a separate link.
    BAIL_OUT_IF (settings->IsSearch(), S_OK);

    link           = std::make_unique<Link>();
    link->settings = *settings;
    link->sink     = std::make_unique<Sink> (*this, name);

    if (settings->transport == LinkTransport::AdtProServer && m_makeFolder)
    {
        link->folder = m_makeFolder (settings->folder);
    }

    link->session  = std::make_unique<CassoLinkSession> (m_factory, link->folder ? *link->folder : m_folder, m_fileIo, *link->sink);

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
//  CassoLinkConnections::Stop
//
//  The session is stopped outside the lock, as its thread's last calls to the
//  sink take the lock too.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Stop (const std::string & name)
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
//  CassoLinkConnections::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Disconnect (const std::string & name)
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_held.insert (name);
    }

    Stop (name);
    Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsHeld
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsHeld (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_held.contains (name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::SetViewing
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetViewing (const void * viewer, const std::string & name)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    if (name.empty())
    {
        m_viewing.erase (viewer);
    }
    else
    {
        m_viewing[viewer] = name;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsViewed
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsViewed (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return std::ranges::any_of (m_viewing, [&name] (const auto & viewing) { return viewing.second == name; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsStarted
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsStarted (const std::string & name) const
{
    return FindLink (name) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetSession
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkSession * CassoLinkConnections::GetSession (const std::string & name)
{
    Link  * link = FindLink (name);



    return link != nullptr ? link->session.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::BuildTreeEntries
//
//  A drive's volume comes from its catalog once that has been read: a DOS
//  3.3 volume by its number, a ProDOS one by its name. A drive whose catalog
//  found no sectors at all is empty.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<LinkTreeEntry> CassoLinkConnections::BuildTreeEntries() const
{
    std::vector<LinkTreeEntry>   entries;
    std::lock_guard<std::mutex>  lock (m_lock);



    for (const CassoLinkSettings & settings : m_links)
    {
        LinkTreeEntry  entry;
        auto           found = std::find_if (m_started.begin(), m_started.end(), [&settings] (const std::unique_ptr<Link> & link) { return link->settings.name == settings.name; });

        entry.name   = TextEncoding::NarrowToWide (settings.name);
        entry.folder = (settings.transport == LinkTransport::AdtProServer) ? settings.folder : std::wstring();

        if (found == m_started.end())
        {
            entry.status = m_held.contains (settings.name) ? std::format (L"{} is disconnected.", entry.name)
                                                           : DescribeState (LinkState::Closed, settings, GetPcName());
        }
        else
        {
            LinkState  state = (*found)->session->GetState();

            std::string  failure = (*found)->session->GetOpenFailure();

            entry.replying = state == LinkState::Ready || state == LinkState::Busy;
            entry.model    = entry.replying ? (*found)->session->GetModelName() : std::wstring();
            entry.status   = failure.empty() ? DescribeState (state, settings, GetPcName()) : TextEncoding::NarrowToWide (failure);
        }

        AddFirewall (settings, entry);

        if (found == m_started.end())
        {
            entries.push_back (entry);
            continue;
        }

        Byte  map = (*found)->session->GetDriveMap();

        for (int drive = 1; entry.replying && drive <= CassoLinkSession::kDriveCount; drive++)
        {
            LinkTreeEntry::Drive  row;
            auto                  catalog = (*found)->requests.find (MakeKey (drive, std::string(), false));

            if ((map & (1 << (drive - 1))) == 0)
            {
                continue;
            }

            row.slot  = 6;
            row.drive = drive;

            if (catalog == (*found)->requests.end() || catalog->second->state == LinkOperationState::Queued || catalog->second->state == LinkOperationState::Running)
            {
                entry.probing = true;
                continue;
            }

            if (catalog->second->state == LinkOperationState::Done)
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
            else if (catalog->second->state == LinkOperationState::Failed)
            {
                row.empty = catalog->second->result == HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND);
            }

            entry.drives.push_back (row);
        }

        if (entry.probing && entry.drives.empty())
        {
            entry.status = std::format (L"Reading the drives on {}...", entry.model.empty() ? std::wstring (L"the Apple") : entry.model);
        }

        entries.push_back (entry);
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ProbeDrives
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ProbeDrives()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    for (const std::unique_ptr<Link> & link : m_started)
    {
        LinkState  state     = link->session->GetState();
        Byte       map       = link->session->GetDriveMap();
        bool       connected = state == LinkState::Ready || state == LinkState::Busy;
        bool       again     = connected && !link->connected;

        link->connected = connected;

        //  A link connected again drops the reads that finished before:
        //  they may have failed with the line, and the Apple's disks may
        //  have changed while it was gone. A drive read then has its volume
        //  again, for its row and Write image's confirmation.
        if (again)
        {
            std::erase_if (link->requests, [] (const auto & request)
            {
                LinkOperationState  done = request.second->state;

                return done != LinkOperationState::Queued && done != LinkOperationState::Running;
            });
        }

        if (!connected)
        {
            continue;
        }

        for (int drive = 1; drive <= CassoLinkSession::kDriveCount; drive++)
        {
            std::string  key = MakeKey (drive, std::string(), false);

            if ((map & (1 << (drive - 1))) != 0 && link->requests.find (key) == link->requests.end())
            {
                link->requests[key] = link->session->Enumerate (drive, FilePath());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::TryGetListing
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::TryGetListing (const std::string & name, int drive, const std::string & directory,
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
//  CassoLinkConnections::TryGetFile
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::TryGetFile (const std::string & name, int drive, const std::string & path,
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
//  CassoLinkConnections::Submit
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::Submit (const std::string & name, const Queuer & queue, Completion completion)
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
//  CassoLinkConnections::RunCompletions
//
//  The finished jobs are taken under the lock and completed outside it, as a
//  completion may queue more.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> CassoLinkConnections::RunCompletions()
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
//  CassoLinkConnections::GetPendingJobs
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LinkOperation>> CassoLinkConnections::GetPendingJobs() const
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
//  CassoLinkConnections::IsWritingDisk
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsWritingDisk (const std::string & name, int drive) const
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
//  CassoLinkConnections::WaitForFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::WaitForFile (const std::string & name, int drive, const std::string & path, DWORD timeoutMs, const std::function<void (DWORD)> & pause,
                                           FilePayload & outPayload)
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
            pause (kWaitPollMs);
        }
    }

    CBREx (arrived, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::DescribeJobs
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::DescribeJobs (const std::vector<std::shared_ptr<LinkOperation>> & jobs)
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
//  CassoLinkConnections::DescribeDial
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::DescribeDial (const std::wstring & pcName, const CassoLinkSettings & settings)
{
    CassoLinkSettings  modem = settings;



    modem.transport = LinkTransport::WifiModem;

    return std::format (L"Dial {} from the modem.", TextEncoding::NarrowToWide (MakeDialCommand (pcName, modem)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::MakeDialCommand
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkConnections::MakeDialCommand (const std::wstring & pcName, const CassoLinkSettings & settings)
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
//  CassoLinkConnections::MakeShimOptions
//
//  For a link with no address or one that cannot be used, the shim's
//  network is zeros; the link dialog accepts only an IPv4 address or none
//  for an Uthernet II.
//
////////////////////////////////////////////////////////////////////////////////

LinkShimOptions CassoLinkConnections::MakeShimOptions (const std::wstring & pcName, const CassoLinkSettings & settings)
{
    HRESULT                      hr       = S_OK;
    LinkShimOptions              options;
    std::vector<NetworkAdapter>  adapters;



    options.baud = settings.baud;
    options.slot = (settings.transport == LinkTransport::Uthernet) ? kUthernetSlot : options.slot;
    options.dial = MakeDialCommand (pcName, settings);

    BAIL_OUT_IF (settings.transport != LinkTransport::Uthernet, S_OK);

    options.chip = LinkChip::W5100;

    //  With no adapters listed, the defaults apply.
    hr = Win32NetworkAdapters::List (adapters);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = MakeNetworkConfig (settings, adapters, options.network);
    IGNORE_RETURN_VALUE (hr, S_OK);

Error:
    return options;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::MakeNetworkConfig
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::MakeNetworkConfig (const CassoLinkSettings & settings, const std::vector<NetworkAdapter> & adapters, LinkNetworkConfig & outConfig)
{
    constexpr std::array<Byte, 5>         kMacPrefix   = { 0x02, 'C', 'A', 'S', 'S' };   // locally administered, unicast
    constexpr Byte                        kGatewayHost = 1;
    HRESULT                               hr           = S_OK;
    size_t                                colon        = settings.address.rfind (':');
    std::string                           host         = settings.address.substr (0, colon);
    std::string                           port         = (colon == std::string::npos) ? std::string() : settings.address.substr (colon + 1);
    IN_ADDR                               parsed       = {};
    int                                   converted    = 0;
    unsigned                              number       = 0;
    std::from_chars_result                read         = {};
    bool                                  numeric      = false;
    bool                                  sameSubnet   = false;



    outConfig = LinkNetworkConfig();

    converted = inet_pton (AF_INET, host.c_str(), &parsed);
    CBREx (converted == 1, E_INVALIDARG);

    std::memcpy (outConfig.address.data(), &parsed, outConfig.address.size());

    if (!port.empty())
    {
        read    = std::from_chars (port.data(), port.data() + port.size(), number);
        numeric = read.ec == std::errc() && read.ptr == port.data() + port.size() && number >= 1 && number <= UINT16_MAX;
        CBREx (numeric, E_INVALIDARG);

        outConfig.port = (uint16_t) number;
    }

    for (const NetworkAdapter & adapter : adapters)
    {
        sameSubnet = true;

        for (size_t i = 0; i < outConfig.address.size(); i++)
        {
            sameSubnet = sameSubnet && (adapter.address[i] & adapter.mask[i]) == (outConfig.address[i] & adapter.mask[i]);
        }

        if (sameSubnet)
        {
            outConfig.mask    = adapter.mask;
            outConfig.gateway = adapter.gateway;
            break;
        }
    }

    if (!sameSubnet)
    {
        for (size_t i = 0; i < outConfig.address.size(); i++)
        {
            outConfig.gateway[i] = (Byte) (outConfig.address[i] & outConfig.mask[i]);
        }

        outConfig.gateway.back() |= kGatewayHost;
    }

    std::copy (kMacPrefix.begin(), kMacPrefix.end(), outConfig.mac.begin());
    outConfig.mac.back() = outConfig.address.back();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetPcName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::GetPcName()
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
//  CassoLinkConnections::DescribeFailure
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::DescribeFailure (const LinkOperation & operation)
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
        case HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE):      why = L"the volume directory is full. ProDOS has room for 51 entries there.";      break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_EXISTS):
        case HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS):   why = L"an entry with that name is already there.";                                break;
        case HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND):   why = L"it is not on the disk.";                                                   break;
        case HRESULT_FROM_WIN32 (ERROR_CANCELLED):        why = L"it was canceled.";                                                         break;
        case HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE):      why = L"the connection to the Apple closed.";                                      break;
        case HRESULT_FROM_WIN32 (ERROR_TIMEOUT):          why = L"the Apple stopped answering.";                                             break;
        default:                                          why = L"the Apple did not complete it.";                                           break;
    }

    return what + L": " + why;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Invalidate
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Invalidate (const std::string & name, int drive)
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
//  CassoLinkConnections::TakeChanged
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::TakeChanged()
{
    return m_changed.exchange (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::DescribeState
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::DescribeState (LinkState state, const CassoLinkSettings & settings, const std::wstring & pcName)
{
    std::wstring  name    = TextEncoding::NarrowToWide (settings.name);
    std::wstring  address = TextEncoding::NarrowToWide (settings.address);
    std::wstring  port    = TextEncoding::NarrowToWide (settings.port);
    std::wstring  baud    = FormatBaud (settings.baud);
    std::wstring  text;
    bool          server  = settings.transport == LinkTransport::AdtProServer;



    switch ((server || settings.IsSearch()) ? LinkState::Opening : state)
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

                case LinkTransport::Uthernet:
                    text = settings.IsSearch()
                         ? std::wstring (L"Each Apple on this PC's network that boots a CassoLink disk on an Uthernet II is listed "
                                         L"when its announcement arrives.")
                         : std::format (L"Connecting to {} at {}.\nBoot that Apple from a CassoLink disk saved from {}.", name, address, name);
                    break;

                case LinkTransport::AdtProServer:
                    text = (state == LinkState::Closed)
                         ? std::format (L"Click {} to serve ADTPro's client from {}.", name, settings.folder)
                         : std::format (L"Serving ADTPro's client from {}.\nStart ADTPro's client on the Apple, and choose {} in its configuration.",
                                        settings.folder, settings.IsComPort() ? port : address);
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
//  CassoLinkConnections::FormatBaud
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::FormatBaud (DWORD baud)
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
//  CassoLinkConnections::Sink::OnStateChanged
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnStateChanged (LinkState)
{
    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnProgress
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnProgress (const std::shared_ptr<LinkOperation> &)
{
    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnDone
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnDone (const std::shared_ptr<LinkOperation> &)
{
    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnActivity
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnActivity (const std::string & text)
{
    Activity  entry;



    GetLocalTime (&entry.time);
    entry.connection = TextEncoding::NarrowToWide (m_name);
    entry.text       = TextEncoding::NarrowToWide (text);

    {
        std::lock_guard<std::mutex>  lock (m_owner.m_lock);

        m_owner.m_activity.push_back (std::move (entry));

        while (m_owner.m_activity.size() > kLogLength)
        {
            m_owner.m_activity.pop_front();
        }
    }

    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetActivity
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoLinkConnections::Activity> CassoLinkConnections::GetActivity() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return std::vector<Activity> (m_activity.begin(), m_activity.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::MakeKey
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkConnections::MakeKey (int drive, const std::string & path, bool file)
{
    return std::format ("{}|{}|{}", drive, file ? "file" : "dir", path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::FindLink
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkConnections::Link * CassoLinkConnections::FindLink (const std::string & name)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    auto                         found = std::find_if (m_started.begin(), m_started.end(), [&name] (const std::unique_ptr<Link> & link) { return link->settings.name == name; });



    return found != m_started.end() ? found->get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::FindLink
//
////////////////////////////////////////////////////////////////////////////////

const CassoLinkConnections::Link * CassoLinkConnections::FindLink (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);
    auto                         found = std::find_if (m_started.begin(), m_started.end(), [&name] (const std::unique_ptr<Link> & link) { return link->settings.name == name; });



    return found != m_started.end() ? found->get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::TryGetResult
//
//  The operation under the key once it is done; false while it is queued or
//  running, and on the first request for it, when it is queued, the link
//  started first if it was not.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::TryGetResult (const std::string & name, int drive, const std::string & key, LinkOperationKind kind,
                                         const std::string & path, std::shared_ptr<LinkOperation> & outOperation)
{
    HRESULT   hr      = S_OK;
    Link    * link    = nullptr;
    FilePath  target  = path.empty() ? FilePath() : FilePath::Parse (path);
    bool      arrived = false;



    if (IsHeld (name))
    {
        outOperation          = std::make_shared<LinkOperation>();
        outOperation->result  = HRESULT_FROM_WIN32 (ERROR_NOT_CONNECTED);
        outOperation->message = "The connection is disconnected.";
        return true;
    }

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
//  CassoLinkConnections::Notify
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Notify()
{
    m_changed = true;

    if (m_hwnd != nullptr)
    {
        PostMessageW (m_hwnd, m_message, 0, 0);
    }
}
