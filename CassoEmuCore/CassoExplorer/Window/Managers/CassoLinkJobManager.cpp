#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkJobManager.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
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
    actions.closeTab     = [this] (const std::string & closed) { m_jobs.erase (closed); };
    actions.cancel       = [this] (const std::string & canceled)
    {
        CassoLinkSession  * session = (m_context.app.links != nullptr) ? m_context.app.links->GetSession (canceled) : nullptr;

        if (session != nullptr)
        {
            session->Cancel();
        }
    };
    actions.showInFolder = [this] (const std::wstring & imagePath) { SelectFile (imagePath, false); };
    actions.open         = [this] (const std::wstring & imagePath) { SelectFile (imagePath, true); };

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
    ULONGLONG                      now = GetTickCount64();



    Update();

    for (auto & [link, job] : m_jobs)
    {
        CassoLinkSession  * session = (m_context.app.links != nullptr) ? m_context.app.links->GetSession (link) : nullptr;
        std::wstring        model   = (session != nullptr) ? session->GetModelName() : std::wstring();
        ULONGLONG           from    = (job.startedMs != 0) ? job.startedMs : now;
        ULONGLONG           to      = (job.endedMs != 0) ? job.endedMs : now;
        CassoLinkJobView    view    = CassoLinkJobView::Build (*job.operation, link, model, (double) (to - from) / 1000.0);

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
        m_context.opener.OpenSelectedEntries();
    }
}
