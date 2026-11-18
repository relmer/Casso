#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
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
    m_saved = links;

    //  The verdicts are read before a link starts or the listener opens, so
    //  no port the firewall does not admit is opened at launch.
    if (m_firewall != nullptr)
    {
        m_firewall->Update();
        m_gate.Set (m_firewall->GetSnapshot(), m_firewall->GetProgram());
    }

    ApplyListening();
    Merge();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ApplyFirewall
//
//  A Wi-Fi modem whose port the gate did not admit opens on its session's next attempt.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ApplyFirewall()
{
    if (m_firewall != nullptr)
    {
        m_gate.Set (m_firewall->GetSnapshot(), m_firewall->GetProgram());
    }

    ApplyListening();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ApplyListening
//
//  With the port bound by another program without reuse, no Apple is listed
//  from an announcement; nothing else depends on them.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ApplyListening()
{
    HRESULT  hr = S_OK;



    if (m_announcements != nullptr)
    {
        hr = m_announcements->SetListening (IsSearching() && IsAnnounceAdmitted());
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsAnnounceAdmitted
//
//  Until the firewall is read, nothing is known to admit the port.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsAnnounceAdmitted() const
{
    return m_firewall == nullptr || (m_firewall->GetVersion() != 0 && m_firewall->Check (HostSocketKind::Udp, LinkProgram::kAnnouncePort).IsAdmitted());
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
//  The cause replaces the connection's state in the list, since none of the
//  state's instructions can work until the port is allowed.
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

    entry.firewall       = FirewallText::DescribeBlocked (kFirewallTitle, { port });
    entry.firewallPublic = verdict.IsOnPublic();
    entry.status         = std::format (L"{}{}{}", entry.firewall, entry.firewallPublic ? L" " : L"",
                                        entry.firewallPublic ? FirewallText::kPublicChoice : L"");
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
//  Each Apple an announcement lists is connected to, as a saved connection
//  is at launch, so an Apple is connected again after the link drops or
//  Casso Explorer restarts. One the user disconnected is left disconnected.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetAnnounced (const std::vector<LinkAnnouncement> & announced)
{
    std::vector<std::string>  discovered;



    m_announced = announced;
    Merge();

    for (const CassoLinkSettings & settings : m_links)
    {
        bool  apple = settings.transport == LinkTransport::Uthernet && !settings.IsSearch() && IsDiscovered (settings.name);

        if (apple && !IsStarted (settings.name) && !IsHeld (settings.name))
        {
            discovered.push_back (settings.name);
        }
    }

    for (const std::string & name : discovered)
    {
        HRESULT  hr = StartIfFree (name);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }
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

        //  A saved link at the running Casso's address has its port, and so
        //  does an ADTPro server there: the port is one serial line, and a
        //  second connection to it would wait for good on a line the first
        //  one holds.
        covered = std::any_of (links.begin(), links.end(), [&settings] (const CassoLinkSettings & link)
        {
            bool  onTcp       = link.transport == LinkTransport::Emulated || link.transport == LinkTransport::Tcp
                             || (link.transport == LinkTransport::AdtProServer && !link.IsComPort());
            bool  sameAddress = onTcp && link.address == settings.address;

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
    HRESULT  hr = S_OK;



    if (!search)
    {
        hr = TakePort (name);
        CHR (hr);
    }

    hr = StartLink (name, search, nullptr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::StartIfFree
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::StartIfFree (const std::string & name)
{
    HRESULT                   hr     = S_OK;
    std::vector<std::string>  rivals = GetPortRivals (name);



    if (!rivals.empty())
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_held.insert (name);
        m_heldFor[name] = rivals[0];
    }

    BAIL_OUT_IF (!rivals.empty(), S_OK);

    hr = StartLink (name, false, nullptr);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::SharesPort
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::SharesPort (const CassoLinkSettings & a, const CassoLinkSettings & b, size_t comPortCount)
{
    if (!a.IsComPort() || !b.IsComPort())
    {
        return false;
    }

    if (a.port == b.port)
    {
        return true;
    }

    return (a.port.empty() || b.port.empty()) && comPortCount <= 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetPortRivals
//
//  The started connections on the port a connection would open.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CassoLinkConnections::GetPortRivals (const std::string & name)
{
    std::vector<std::string>   rivals;
    std::vector<std::wstring>  ports;
    HRESULT                    listed   = ListComPorts (ports);
    auto                       settings = std::find_if (m_links.begin(), m_links.end(), [&name] (const CassoLinkSettings & s) { return s.name == name; });



    IGNORE_RETURN_VALUE (listed, S_OK);

    if (settings != m_links.end())
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        for (const std::unique_ptr<Link> & link : m_started)
        {
            if (link->settings.name != name && SharesPort (*settings, link->settings, ports.size()))
            {
                rivals.push_back (link->settings.name);
            }
        }
    }

    return rivals;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::TakePort
//
//  The connections on the port are disconnected with the reason. A connection
//  with a transfer running keeps the port, and this start fails.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkConnections::TakePort (const std::string & name)
{
    HRESULT                   hr     = S_OK;
    std::vector<std::string>  rivals = GetPortRivals (name);
    bool                      idle   = true;



    {
        std::lock_guard<std::mutex>  lock (m_lock);

        for (const std::unique_ptr<Link> & link : m_started)
        {
            if (std::find (rivals.begin(), rivals.end(), link->settings.name) != rivals.end())
            {
                idle = idle && link->session->GetState() != LinkState::Busy;
            }
        }
    }

    CBREx (idle, HRESULT_FROM_WIN32 (ERROR_BUSY));

    for (const std::string & rival : rivals)
    {
        Disconnect (rival);

        std::lock_guard<std::mutex>  lock (m_lock);

        m_heldFor[rival] = name;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetOpenPort
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkConnections::GetOpenPort (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);
    auto                         found = std::find_if (m_started.begin(), m_started.end(), [&name] (const std::unique_ptr<Link> & link) { return link->settings.name == name; });



    return found == m_started.end() ? std::wstring() : (*found)->session->GetPort();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Hold
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Hold (const std::string & name)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_held.insert (name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::IsHeldByUser
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsHeldByUser (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return m_held.contains (name) && !m_heldFor.contains (name);
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

    hr = TakePort (name);
    CHR (hr);

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
        m_heldFor.erase (name);
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
    link->session->SetListenGate (&m_gate);

    {
        std::lock_guard<std::mutex>  lock (m_lock);

        ApplyVerify (*link);
    }

    //  A search opens the ports in turn even when the link has one set.
    opening = link->settings;

    if (search)
    {
        opening.port.clear();
    }
    else if (opening.port.empty())
    {
        opening.port = opening.lastPort;
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
            auto  heldFor = m_heldFor.find (settings.name);

            entry.status = heldFor != m_heldFor.end()          ? std::format (L"{} is disconnected because {} is using its port.", entry.name, TextEncoding::NarrowToWide (heldFor->second))
                         : m_held.contains (settings.name)     ? std::format (L"{} is disconnected.", entry.name)
                                                           : CassoLinkConnectionText::DescribeState (LinkState::Closed, settings, CassoLinkShimBuilder::GetPcName());
        }
        else
        {
            LinkState  state = (*found)->session->GetState();

            std::string  failure = (*found)->session->GetOpenFailure();

            entry.replying = state == LinkState::Ready || state == LinkState::Busy;
            entry.transferring = (*found)->sink->IsTransferring ((*found)->session->IsServingAdtPro());
            entry.fullSending  = (*found)->sink->IsFullSending();
            entry.model    = entry.replying ? (*found)->session->GetModelName() : std::wstring();
            entry.status   = failure.empty() ? CassoLinkConnectionText::DescribeState (state, settings, CassoLinkShimBuilder::GetPcName()) : TextEncoding::NarrowToWide (failure);
        }

        AddFirewall (settings, entry);

        if (found == m_started.end())
        {
            entries.push_back (entry);
            continue;
        }

        Byte  map = (*found)->session->GetDriveMap();

        for (int drive = 1; entry.replying && drive <= CassoLinkSessionTypes::kDriveCount; drive++)
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

                row.volume = (catalog->second->volumeKind == VolumeKind::Pascal)    ? TextEncoding::NarrowToWide (listing.volumeName) + L":"
                           : (catalog->second->volumeKind == VolumeKind::Cpm)       ? std::wstring (L"CP/M")
                           : (catalog->second->volumeKind == VolumeKind::Gutenberg) ? TextEncoding::NarrowToWide (listing.volumeName)
                           : (catalog->second->volumeKind == VolumeKind::Rdos)      ? std::wstring (L"RDOS")
                           : listing.hasVolumeName                                  ? L"/" + TextEncoding::NarrowToWide (listing.volumeName)
                           : listing.hasVolumeNumber                                ? std::format (L"DOS 3.3 volume {}", listing.volumeNumber)
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

        if (entry.replying)
        {
            AddBlockDevices (**found, entry);
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
//  CassoLinkConnections::TryGetListing
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::TryGetListing (const std::string & name, int drive, const std::string & directory,
                                          VolumeListing & outListing, VolumeKind & outKind, HRESULT & outResult, std::string & outMessage,
                                          bool * outKept)
{
    std::shared_ptr<LinkOperation>  operation;
    std::string                     key     = MakeKey (drive, directory, false);
    bool                            arrived = TryGetResult (name, drive, key, LinkOperationKind::Enumerate, directory, operation);



    if (outKept != nullptr)
    {
        *outKept = false;
    }

    //  While a listing is read again, the one before it is listed, so the
    //  rows stay in place rather than empty and refill.
    if (!arrived)
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        for (const std::unique_ptr<Link> & link : m_started)
        {
            if (link->settings.name == name && link->shown.contains (key))
            {
                operation = link->shown.at (key);
                arrived   = true;

                if (outKept != nullptr)
                {
                    *outKept = true;
                }
            }
        }
    }

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
        hr = StartIfFree (name);
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
//  CassoLinkConnections::IsAnyTransferring
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::IsAnyTransferring() const
{
    std::lock_guard<std::mutex>  lock (m_lock);



    return std::any_of (m_started.begin(), m_started.end(), [] (const std::unique_ptr<Link> & link)
    {
        return link->session != nullptr && link->sink->IsTransferring (link->session->IsServingAdtPro());
    });
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
//  CassoLinkConnections::SetChosenKind
//
//  The drive's catalog is read again by the next listing.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetChosenKind (const std::string & name, int drive, VolumeKind kind)
{
    //  FindLink locks m_lock, which isn't recursive.
    Link  * link = FindLink (name);



    if (link != nullptr && link->session != nullptr)
    {
        link->session->SetChosenKind (drive, kind);
    }

    Invalidate (name, drive);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetChosenKind
//
////////////////////////////////////////////////////////////////////////////////

VolumeKind CassoLinkConnections::GetChosenKind (const std::string & name, int drive) const
{
    //  FindLink locks m_lock, which isn't recursive.
    const Link  * link = FindLink (name);



    return (link != nullptr && link->session != nullptr) ? link->session->GetChosenKind (drive) : VolumeKind::Unknown;
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
            for (const auto & request : link->requests)
            {
                bool  listed = request.second->kind == LinkOperationKind::Enumerate && request.second->state == LinkOperationState::Done
                               && SUCCEEDED (request.second->result);

                if (request.first.starts_with (prefix) && listed)
                {
                    link->shown[request.first] = request.second;
                }
            }

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
//  CassoLinkConnections::RemoveListed
//
//  An entry deleted from a drive is removed from its directory's listing,
//  before the drive is read again.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::RemoveListed (const std::string & name, int drive, const std::string & directory, const std::string & entry)
{
    std::lock_guard<std::mutex>  lock (m_lock);
    std::string                  key  = MakeKey (drive, directory, false);



    for (const std::unique_ptr<Link> & link : m_started)
    {
        for (auto * lists : { &link->shown, &link->requests })
        {
            auto  found = lists->find (key);

            if (link->settings.name == name && found != lists->end() && found->second->state == LinkOperationState::Done)
            {
                std::erase_if (found->second->listing.entries, [&entry] (const FileEntry & each) { return each.name == entry; });
            }
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

void CassoLinkConnections::Sink::OnProgress (const std::shared_ptr<LinkOperation> & operation)
{
    m_lastProgress = GetTickCount64();
    m_fullSending  = operation != nullptr && operation->kind == LinkOperationKind::FullSend;

    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::IsTransferring
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::Sink::IsTransferring (bool serving) const
{
    ULONGLONG  now = GetTickCount64();



    return now - m_lastProgress < kTransferWindowMs || (serving && now - m_lastActivity < kTransferWindowMs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::IsFullSending
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkConnections::Sink::IsFullSending() const
{
    return m_fullSending && GetTickCount64() - m_lastProgress < kTransferWindowMs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::SetVerifyDefaults
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::SetVerifyDefaults (bool reads, bool writes, bool autoReads)
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_verifyReads  = reads;
    m_verifyWrites = writes;
    m_autoVerify   = autoReads;

    for (const std::unique_ptr<Link> & link : m_started)
    {
        ApplyVerify (*link);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ApplyVerify
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ApplyVerify (Link & link) const
{
    int  choice = link.settings.verify;



    link.session->SetVerifySettings (choice == CassoLinkSettings::kVerifyOn || (choice == CassoLinkSettings::kVerifyDefault && m_verifyReads),
                                     choice == CassoLinkSettings::kVerifyOn || (choice == CassoLinkSettings::kVerifyDefault && m_verifyWrites),
                                     choice != CassoLinkSettings::kVerifyOff && m_autoVerify);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::NoteBoot
//
//  The lock is taken for the whole call, as the thread that booted the image
//  isn't the one that starts and stops connections.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::NoteBoot (const std::string & link, const std::string & file, const std::string & boot, const std::string & text, const std::string & renamedTo, const std::string & fsCheck)
{
    Activity  entry;



    GetLocalTime (&entry.time);
    entry.connection = TextEncoding::NarrowToWide (link);
    entry.text       = TextEncoding::NarrowToWide (text);

    m_logFile.Append (link, CassoLinkLogFile::kActivity, text);

    {
        std::lock_guard<std::mutex>  lock (m_lock);
        auto                         found = std::find_if (m_started.begin(), m_started.end(), [&link] (const std::unique_ptr<Link> & each) { return each->settings.name == link; });

        if (found != m_started.end() && (*found)->session != nullptr)
        {
            (*found)->session->NoteBoot (file, boot, text, renamedTo, fsCheck);
        }

        m_activity.push_back (std::move (entry));

        while (m_activity.size() > kLogLength)
        {
            m_activity.pop_front();
        }
    }

    Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnOperationQueued
//
//  A job with no completion: the session's result is in the activity log, as
//  the user is at the Apple.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnOperationQueued (const std::shared_ptr<LinkOperation> & operation)
{
    {
        std::lock_guard<std::mutex>  lock (m_owner.m_lock);

        m_owner.m_jobs.push_back (Job { m_name, operation, nullptr });
    }

    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnDone
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnDone (const std::shared_ptr<LinkOperation> & operation)
{
    //  An operation blocked while a Full Send runs never ran, so the Full
    //  Send goes on.
    if (!operation || !operation->isBlocked)
    {
        m_lastProgress = 0;
        m_fullSending  = false;
    }

    m_owner.Notify();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnActivity
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnActivity (const std::string & text)
{
    m_lastActivity = GetTickCount64();

    Log (Activity(), text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnTrace
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnTrace (bool sent, const std::string & text)
{
    m_owner.m_logFile.Append (m_name, sent ? CassoLinkLogFile::kSent : CassoLinkLogFile::kReceived, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::OnFinished
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::OnFinished (const LinkOperation & operation, const std::string & text)
{
    Activity  entry;



    entry.started   = operation.started;
    entry.operation = true;

    Log (std::move (entry), text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::Sink::Log
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::Sink::Log (Activity entry, const std::string & text)
{
    GetLocalTime (&entry.time);
    entry.connection = TextEncoding::NarrowToWide (m_name);
    entry.text       = TextEncoding::NarrowToWide (text);

    m_owner.m_logFile.Append (m_name, CassoLinkLogFile::kActivity, text);

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
        hr = StartIfFree (name);
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
