#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkJobManager.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoLink/CassoLinkSession.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::CassoLinkJobManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkJobManager::CassoLinkJobManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::~CassoLinkJobManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkJobManager::~CassoLinkJobManager() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::Update
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobManager::Update()
{
    ULONGLONG    now   = GetTickCount64();
    std::string  ended;
    std::string  started;



    if (m_context.app.links == nullptr)
    {
        return;
    }

    for (const auto & [link, operation] : m_context.app.links->GetPendingJobsByLink())
    {
        Job  & job = m_jobs[link];

        if (!CassoLinkJobView::IsDiskJob (operation->kind) || job.operation == operation)
        {
            continue;
        }

        job           = Job();
        job.operation = operation;
        job.seenMs    = now;
    }

    for (auto & [link, job] : m_jobs)
    {
        LinkOperationState  state = (job.operation != nullptr) ? job.operation->state.load() : LinkOperationState::Done;

        if (job.startedMs == 0 && state != LinkOperationState::Queued)
        {
            job.startedMs = now;
            started       = link;
        }

        if (job.endedMs == 0 && job.operation != nullptr && state != LinkOperationState::Queued && state != LinkOperationState::Running)
        {
            job.endedMs  = now;
            job.isUnseen = true;
            ended        = link;
        }
    }

    std::erase_if (m_jobs, [] (const auto & entry) { return entry.second.operation == nullptr; });

    if (!ended.empty())
    {
        Show (ended, false);
    }
    else if (!started.empty() && m_context.prefs.openJobsWindow)
    {
        Show (started, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::ShowWindow
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobManager::ShowWindow()
{
    Update();

    if (!m_jobs.empty())
    {
        Show (GetNewest(), true);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::GetNewest
//
//  The job seen last.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkJobManager::GetNewest() const
{
    std::string  newest;
    ULONGLONG    seenMs = 0;



    for (const auto & [link, job] : m_jobs)
    {
        if (newest.empty() || job.seenMs > seenMs)
        {
            newest = link;
            seenMs = job.seenMs;
        }
    }

    return newest;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::Show
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobManager::Show (const std::string & link, bool activate)
{
    CassoLinkJobDialog::Actions  actions;



    m_selected = link;

    if (m_window != nullptr && m_window->IsCreated())
    {
        m_window->Select (link);
        m_window->ShowModelessDialog (IDCANCEL, activate);
        return;
    }

    actions.views        = [this] () { return BuildViews(); };
    actions.select       = [this] (const std::string & selected)
    {
        auto  found = m_jobs.find (selected);

        m_selected = selected;

        if (found != m_jobs.end())
        {
            found->second.isUnseen = false;
        }
    };
    actions.closeTab     = [this] (const std::string & closed)
    {
        m_jobs.erase (closed);
        m_offers.erase (closed);
    };
    actions.captureRaw   = [this] (const std::string & offered) { TakeOffer (offered, true); };
    actions.readAgain    = [this] (const std::string & offered) { TakeOffer (offered, false); };
    actions.cancel       = [this] (const std::string & canceled)
    {
        CassoLinkSession  * session = (m_context.app.links != nullptr) ? m_context.app.links->GetSession (canceled) : nullptr;

        auto                found   = m_jobs.find (canceled);

        if (session != nullptr)
        {
            session->Cancel();
        }

        //  The tab reads Canceling until the Apple stops at its next pause.
        if (found != m_jobs.end())
        {
            found->second.isCanceling = true;
        }
    };
    //  The job window is owned by the main window and covers it, so it closes
    //  once the file is selected or opened.
    actions.autoOpen     = m_context.prefs.openJobsWindow;
    actions.setAutoOpen  = [this] (bool checked)
    {
        m_context.prefs.openJobsWindow = checked;
        m_context.window.SaveSession();
    };
    actions.showInFolder = [this] (const std::wstring & imagePath)
    {
        m_context.tabManager.SelectInFolder (imagePath);
        m_window->EndDialog (IDCANCEL);
    };
    actions.open         = [this] (const std::wstring & imagePath)
    {
        SelectFile (imagePath, true);
        m_window->EndDialog (IDCANCEL);
    };

    m_window = CassoLinkJobDialog::Open (m_context.window.GetHwnd(), m_context.theme, activate, std::move (actions), link);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::BuildViews
//
//  A job counts as seen once its tab is selected.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoLinkJobView> CassoLinkJobManager::BuildViews()
{
    std::vector<CassoLinkJobView>  views;



    Update();

    for (auto & [link, job] : m_jobs)
    {
        CassoLinkJobView  view = MakeView (link, *job.operation);

        if (job.isCanceling && view.state == CassoLinkJobView::State::Running)
        {
            view.state = CassoLinkJobView::State::Canceling;
        }

        view.hasOffer = m_offers.contains (link);

        if (link == m_selected)
        {
            job.isUnseen = false;
        }

        view.isUnseen = job.isUnseen;

        views.push_back (std::move (view));
    }

    return views;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::MakeView
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkJobView CassoLinkJobManager::MakeView (const std::string & link, const LinkOperation & operation) const
{
    CassoLinkSession  * session = (m_context.app.links != nullptr) ? m_context.app.links->GetSession (link) : nullptr;
    std::wstring        model   = (session != nullptr) ? session->GetModelName() : std::wstring();
    std::wstring        port    = (session != nullptr) ? session->GetPort() : std::wstring();
    LinkTransport       kind    = LinkTransport::Serial;



    for (const CassoLinkSettings & settings : (m_context.app.links != nullptr) ? m_context.app.links->GetLinks() : std::vector<CassoLinkSettings>())
    {
        kind = (settings.name == link) ? settings.transport : kind;
    }

    return CassoLinkJobView::Build (operation, link, model, kind, port);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::SetOffer
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobManager::SetOffer (const std::string & link, std::function<void()> captureRaw, std::function<void()> readAgain)
{
    m_offers[link] = Offer { std::move (captureRaw), std::move (readAgain) };

    Update();
    Show (link, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::TakeOffer
//
//  The offer is cleared before its action runs, which can start a new job
//  on the same connection.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobManager::TakeOffer (const std::string & link, bool captureRaw)
{
    auto                   found  = m_offers.find (link);
    std::function<void()>  action;



    if (found == m_offers.end())
    {
        return;
    }

    action = captureRaw ? found->second.captureRaw : found->second.readAgain;
    m_offers.erase (found);

    if (action)
    {
        action();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::DescribeJobs
//
//  The status bar's line for each running disk job: its Apple and how it's
//  connected, what it does, how far along it is, and its step.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkJobManager::DescribeJobs() const
{
    std::wstring  text;



    if (m_context.app.links == nullptr)
    {
        return text;
    }

    for (const auto & [link, operation] : m_context.app.links->GetPendingJobsByLink())
    {
        CassoLinkJobView  view   = MakeView (link, *operation);
        std::wstring      step   = TextEncoding::NarrowToWide (operation->GetDetail());
        std::wstring      line;

        if (!CassoLinkJobView::IsDiskJob (operation->kind))
        {
            continue;
        }

        line  = std::format (L"{}: {}, {}%", view.tabLabel, view.heading, std::clamp (operation->progress.load(), 0, 100));
        line += step.empty() ? std::wstring() : L", " + step;
        text += (text.empty() ? L"" : L"     ") + line;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkJobManager::SelectFile
//
//  The image selected in its folder in the list, and for Open, opened.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkJobManager::SelectFile (const std::wstring & imagePath, bool open)
{
    std::filesystem::path  image    = imagePath;
    bool                   selected = false;



    m_context.addressBar.GoToLocation (Location::MakeHostFolder (image.parent_path().wstring()));
    m_context.listPane.FillList();

    selected = m_context.listPane.SelectRowNamed (image.filename().wstring());

    if (selected && open)
    {
        m_context.opener.OpenEachSelected();
    }
}
