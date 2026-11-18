#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerListBars.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/SearchExpression.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"
#include "Core/AppMessages.h"





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
        m_cardBarClosed = m_context.browser.GetLocation().path;
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
//  At an A2Pico card's root, its drives in a sentence. In any folder on the
//  card, where a copy to the card lands, its images with a ProDOS volume in
//  a form A2retroNET doesn't read, with Write as .po. A warning when a drive has a
//  problem, the settings file has CR LF line ends, or an image is offered.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListBars::FillCardBar()
{
    const Location             & location  = m_context.browser.GetLocation();
    std::wstring                 root      = std::filesystem::path (location.path).root_path().wstring();
    std::vector<std::wstring>    labels;
    std::vector<std::wstring>    arrived;
    std::wstring                 text;
    HRESULT                      hr        = S_OK;
    BOOL                         posted    = TRUE;
    bool                         visible   = m_cardBar->IsVisible();
    bool                         onCard    = false;
    bool                         atRoot    = false;
    bool                         problem   = false;
    bool                         show      = false;
    bool                         relabeled = false;



    m_cardActions.clear();
    m_convertible.clear();

    //  Files dropped on or from a card that Windows has moved since.
    m_context.actions.GetA2retroNet().ResolveMoves();

    //  Images copied to a card that A2retroNET doesn't read. The question
    //  about them waits for a message, so the list is filled first.
    m_context.actions.GetA2retroNet().TakeArrived (arrived);

    if (!arrived.empty())
    {
        m_arrived.insert (m_arrived.end(), arrived.begin(), arrived.end());

        if (m_context.window.GetHwnd() != nullptr)
        {
            posted = PostMessageW (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerCardArrivals, 0, 0);
            IGNORE_RETURN_VALUE (posted, TRUE);
        }
    }

    onCard = location.kind == Location::Kind::HostFolder && !root.empty() && m_context.actions.IsA2PicoCard (root) && location.path != m_cardBarClosed;
    atRoot = onCard && location.path == root;

    if (onCard)
    {
        hr     = m_context.actions.ReadA2PicoCard (root, m_card);
        onCard = SUCCEEDED (hr);
    }

    if (onCard)
    {
        hr = m_context.actions.GetA2retroNet().FindConvertible (location.path, m_convertible);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (atRoot)
    {
        text    = ActionsA2retroNet::Summarize (m_card);
        problem = m_card.config.hasCarriageReturns || std::any_of (m_card.drives.begin(), m_card.drives.end(), [] (const ActionsA2retroNet::Drive & drive) { return !drive.problem.empty(); });

        m_cardActions.push_back (CardAction::Drives);

        if (m_card.config.hasCarriageReturns)
        {
            m_cardActions.push_back (CardAction::Repair);
        }
    }

    if (!m_convertible.empty())
    {
        text   += std::format (L"{}{} {} a ProDOS volume in a form A2retroNET doesn't read. Write as .po makes a copy it reads.", text.empty() ? L"" : L" ", DescribeFiles (m_convertible), m_convertible.size() == 1 ? L"has" : L"have");
        problem = true;

        m_cardActions.push_back (CardAction::WritePo);
    }

    show = !text.empty();

    for (CardAction action : m_cardActions)
    {
        labels.push_back (action == CardAction::Drives ? L"Drives..." : action == CardAction::Repair ? L"Repair" : L"Write as .po");
    }

    //  New buttons have no place until the bar is laid out, so they are
    //  replaced only when their labels change, and the pane is laid out again.
    relabeled = show && labels != m_cardLabels;

    if (show)
    {
        m_cardBar->SetText     (text);
        m_cardBar->SetSeverity (problem ? DxuiInfoBanner::Severity::Warning : DxuiInfoBanner::Severity::Info);
    }

    if (relabeled)
    {
        m_cardBar->SetActions (labels);
        m_cardLabels = labels;
    }

    m_cardBar->SetVisible (show);

    return show != visible || relabeled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::DescribeFiles
//
//  "a.dsk", "a.dsk and b.dsk", or "a.dsk, b.dsk and c.dsk".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerListBars::DescribeFiles (const std::vector<std::wstring> & paths)
{
    std::wstring  text;



    for (size_t i = 0; i < paths.size(); i++)
    {
        text += std::format (L"{}{}", i == 0 ? L"" : i + 1 == paths.size() ? L" and " : L", ", std::filesystem::path (paths[i]).filename().wstring());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::OfferArrived
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListBars::OfferArrived()
{
    std::vector<std::wstring>  paths  = std::move (m_arrived);
    std::wstring               text;
    std::wstring               written;
    int                        choice = IDNO;



    m_arrived.clear();

    if (paths.empty())
    {
        return;
    }

    text = paths.size() == 1
         ? std::format (L"A2Pico doesn't support {}. Would you like to convert it to {}?", DescribeFiles (paths), std::filesystem::path (paths[0]).filename().replace_extension (L".po").wstring())
         : std::format (L"A2Pico doesn't support {}. Would you like to convert them to .po images?", DescribeFiles (paths));

    choice = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, text.c_str(), L"Convert for A2Pico", MB_YESNO, { L"Convert", L"Don't convert" });

    if (choice != IDYES)
    {
        return;
    }

    for (const std::wstring & path : paths)
    {
        m_context.commandManager.ReportOutcome (m_context.actions.GetA2retroNet().ReplaceWithPo (path, written), L"Convert for A2Pico");
    }

    m_context.folderSync.RefreshAfterHostChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::RunCardAction
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListBars::RunCardAction (size_t index)
{
    CardAction    action = index < m_cardActions.size() ? m_cardActions[index] : CardAction::Drives;
    std::wstring  written;



    if (index >= m_cardActions.size())
    {
        return;
    }

    if (action == CardAction::Drives)
    {
        m_context.opener.EditA2retroNetDrives (m_card.root);
    }
    else if (action == CardAction::Repair)
    {
        m_context.commandManager.ReportOutcome (m_context.actions.RepairA2retroNetConfig (m_card.root), L"Repair A2retroNET.txt");
    }
    else
    {
        for (const std::wstring & path : m_convertible)
        {
            m_context.commandManager.ReportOutcome (m_context.actions.GetA2retroNet().ConvertToPo (path, written), L"Write as .po");
        }

        m_context.folderSync.RefreshAfterHostChange();
    }

    FillCardBar();
    m_relayout();
}