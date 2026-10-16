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
    std::vector<std::string>  stale;



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

HRESULT AppleLinkConnections::Start (const std::string & name)
{
    HRESULT                hr       = S_OK;
    std::unique_ptr<Link>  link;
    auto                   settings = std::find_if (m_links.begin(), m_links.end(), [&name] (const AppleLinkSettings & s) { return s.name == name; });
    bool                   known    = settings != m_links.end();



    BAIL_OUT_IF (IsStarted (name), S_OK);
    CBREx (known, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    link           = std::make_unique<Link>();
    link->settings = *settings;
    link->sink     = std::make_unique<Sink> (*this);
    link->session  = std::make_unique<AppleLinkSession> (m_factory, m_folder, m_fileIo, *link->sink);

    hr = link->session->Start (link->settings);
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

        entry.name = TextEncoding::NarrowToWide (settings.name);

        if (found == m_started.end())
        {
            entry.status = DescribeState (LinkState::Closed, false);
            entries.push_back (entry);
            continue;
        }

        LinkState  state = (*found)->session->GetState();
        Byte       map   = (*found)->session->GetDriveMap();

        entry.replying = state == LinkState::Ready || state == LinkState::Busy;
        entry.status   = DescribeState (state, (*found)->session->IsServingAdtPro());

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

std::wstring AppleLinkConnections::DescribeState (LinkState state, bool servingAdtPro)
{
    std::wstring  text;



    if (servingAdtPro)
    {
        return L"ADTPro's own Apple program is connected. Browsing and copying work only with Casso's Apple program; bootstrap the Apple to load it.";
    }

    switch (state)
    {
        case LinkState::Closed:          text = L"Not connected. Select the link to connect.";               break;
        case LinkState::Opening:         text = L"Opening the connection...";                                 break;
        case LinkState::WaitingForReady: text = L"Waiting for the Apple to answer...";                        break;
        case LinkState::LoadingProgram:  text = L"Loading Casso's program into the Apple...";                 break;
        case LinkState::Ready:           text = L"Connected.";                                                break;
        case LinkState::Busy:            text = L"Connected.";                                                break;
        default:                         text = L"Not connected: the Apple did not answer. Select the link to try again."; break;
    }

    return text;
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
        outOperation->message = "The link could not be started.";
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
