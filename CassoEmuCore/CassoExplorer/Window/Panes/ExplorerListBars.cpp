#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerListBars.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/SearchExpression.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListBars::Create (RelayoutFn relayout)
{
    m_relayout  = std::move (relayout);
    m_searchBar = m_context.window.CreateChild<DxuiInfoBanner> (L"");
    m_cardBar   = m_context.window.CreateChild<DxuiInfoBanner> (L"");

    m_searchBar->SetSeverity    (DxuiInfoBanner::Severity::Warning);
    m_searchBar->SetDismissible (true);
    m_searchBar->SetVisible     (false);

    m_searchBar->SetOnDismiss ([this]()
    {
        m_searchBarClosed = m_context.browser.GetLocation().path;
        m_searchBar->SetVisible (false);
        m_relayout();
    });

    m_cardBar->SetDismissible (true);
    m_cardBar->SetVisible     (false);
    m_cardBar->SetOnAction    ([this] (size_t index) { RunCardAction (index); });

    m_cardBar->SetOnDismiss ([this]()
    {
        m_cardBarClosed = m_card.root;
        m_cardBar->SetVisible (false);
        m_relayout();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::Fill
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListBars::Fill()
{
    bool  searchChanged = FillSearchBar();
    bool  cardChanged   = FillCardBar();



    return searchChanged || cardChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::Layout
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerListBars::Layout (const RECT & rect)
{
    RECT  below = rect;



    for (DxuiInfoBanner * bar : { m_searchBar, m_cardBar })
    {
        if (bar->IsVisible())
        {
            float  barPx   = bar->GetPreferredHeightPx ((float) (rect.right - rect.left), m_context.scaler);
            RECT   barRect = below;

            barRect.bottom = (std::min) (rect.bottom, below.top + (LONG) std::ceil (barPx));
            below.top      = barRect.bottom;

            bar->Layout (barRect, m_context.scaler);
        }
    }

    return below;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::FillSearchBar
//
//  A search's query is parsed again here, on the window's thread, for its
//  regular expressions alone; the search itself reads on another thread.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListBars::FillSearchBar()
{
    std::wstring                    scope;
    std::wstring                    query;
    std::wstring                    error;
    SearchExpression::MatchOptions  options;
    std::wstring                    id      = m_context.browser.GetLocation().path;
    bool                            visible = m_searchBar->IsVisible();
    bool                            show    = false;



    if (SearchQuery::TryParseId (id, scope, query))
    {
        error = SearchExpression::Parse (query, options).GetError();
    }

    show = !error.empty() && id != m_searchBarClosed;

    m_searchBar->SetText    (error);
    m_searchBar->SetVisible (show);

    return show != visible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::FillCardBar
//
//  At an A2Pico card's root, its drives in a sentence, drawn as a warning
//  when a drive has a problem or the settings file has CR LF line ends.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListBars::FillCardBar()
{
    const Location  & location = m_context.browser.GetLocation();
    HRESULT           hr       = S_OK;
    bool              visible  = m_cardBar->IsVisible();
    bool              show     = false;
    bool              problem  = false;



    show = location.kind == Location::Kind::HostFolder && m_context.actions.IsA2PicoCard (location.path) && location.path != m_cardBarClosed;

    if (show)
    {
        hr   = m_context.actions.ReadA2PicoCard (location.path, m_card);
        show = SUCCEEDED (hr);
    }

    if (show)
    {
        problem = m_card.config.hasCarriageReturns || std::any_of (m_card.drives.begin(), m_card.drives.end(), [] (const ActionsA2retroNet::Drive & drive) { return !drive.problem.empty(); });

        m_cardBar->SetText     (ActionsA2retroNet::Summarize (m_card));
        m_cardBar->SetSeverity (problem ? DxuiInfoBanner::Severity::Warning : DxuiInfoBanner::Severity::Info);
        m_cardBar->SetActions  (m_card.config.hasCarriageReturns ? std::vector<std::wstring> { L"Repair" } : std::vector<std::wstring>());
    }

    m_cardBar->SetVisible (show);

    return show != visible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::RunCardAction
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListBars::RunCardAction (size_t index)
{
    if (index == kRepair)
    {
        m_context.commandManager.ReportOutcome (m_context.actions.RepairA2retroNetConfig (m_card.root), L"Repair A2retroNET.txt");
    }

    FillCardBar();
    m_relayout();
}
