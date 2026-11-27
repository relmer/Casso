#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerListBars.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/SearchExpression.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerOpen.h"





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
//  a form A2retroNET doesn't read, with Convert to .po, which replaces each
//  with a .po. A warning when a drive has a problem, the settings file has
//  CR LF line ends, or an image is offered.
//  And a selected or converted image the firmware reads that isn't a
//  drive, with Set as drive for a free drive. An image copied to the card
//  is set as a free drive right away.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListBars::FillCardBar()
{
    const Location                                      & location  = m_context.browser.GetLocation();
    std::wstring                                          root      = std::filesystem::path (location.path).root_path().wstring();
    std::vector<std::wstring>                             labels;
    std::vector<std::wstring>                             arrived;
    std::vector<std::pair<std::wstring, std::wstring>>    replaced;
    std::wstring                                          text;
    HRESULT                                               hr        = S_OK;
    bool                                                  visible   = m_cardBar->IsVisible();
    bool                                                  onCard    = false;
    bool                                                  atRoot    = false;
    bool                                                  problem   = false;
    bool                                                  show      = false;
    bool                                                  relabeled = false;
    bool                                                  isOne     = false;



    m_cardActions.clear();
    m_convertible.clear();
    m_setPath.clear();
    m_setDrive = 0;

    //  Files dropped on or from a card that Windows has moved since.
    m_context.actions.GetA2retroNet().ResolveMoves();

    //  Images copied to a card are set as its drives once Windows has
    //  written them.
    m_context.actions.GetA2retroNet().TakeArrived (arrived);

    if (!arrived.empty())
    {
        m_context.verbs.ReportOutcome (m_context.actions.GetA2retroNet().AddArrived (arrived, replaced), L"Add to A2retroNET drives");

        //  Undoing the copy of an image replaced with a .po removes the .po,
        //  and the tree lists the .po in its folder.
        for (const auto & [from, to] : replaced)
        {
            m_context.undo.ReplacePath (from, to);
            m_context.folderSync.RefreshTreeFolder (std::filesystem::path (to).parent_path().wstring());
        }

        m_context.folderSync.RefreshAfterHostChange();
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

        FindSettable (root);
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

    //  Each offer is on a line of its own.
    if (!m_convertible.empty())
    {
        isOne   = m_convertible.size() == 1;
        text   += std::format (L"{}Only .po images can be mounted as drives. Convert {} to .po?", text.empty() ? L"" : L"\n",
                               isOne ? std::format (L"\"{}\"", std::filesystem::path (m_convertible[0]).filename().wstring())
                                     : std::format (L"{} images", m_convertible.size()));
        problem = true;

        m_cardActions.push_back (CardAction::WritePo);
    }

    if (m_setDrive != 0)
    {
        text += std::format (L"{}Drive {} is free. Set \"{}\" as drive {}?", text.empty() ? L"" : L"\n", m_setDrive, std::filesystem::path (m_setPath).filename().wstring(), m_setDrive);

        m_cardActions.push_back (CardAction::SetDrive);
    }

    show = !text.empty();

    for (CardAction action : m_cardActions)
    {
        labels.push_back (action == CardAction::Drives   ? std::wstring (L"Drives...")
                        : action == CardAction::Repair   ? std::wstring (L"Repair")
                        : action == CardAction::SetDrive ? std::format (L"Set as drive {}", m_setDrive)
                        :                                  std::wstring (L"Convert to .po"));
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
//  ExplorerListBars::FindSettable
//
//  The selected images come first, then those copied or written to the card.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListBars::FindSettable (const std::wstring & root)
{
    std::vector<std::wstring>  candidates;
    int                        drive = 0;



    m_context.browser.GetSelection().GetSelectedHostPaths (candidates);
    candidates.insert (candidates.end(), m_settable.begin(), m_settable.end());

    for (const std::wstring & path : candidates)
    {
        bool  onCard = _wcsicmp (std::filesystem::path (path).root_path().wstring().c_str(), root.c_str()) == 0;

        if (onCard && m_context.actions.GetA2retroNet().TryGetFreeDrive (path, drive))
        {
            m_setPath  = path;
            m_setDrive = drive;
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListBars::RunCardAction
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListBars::RunCardAction (size_t index)
{
    CardAction             action = index < m_cardActions.size() ? m_cardActions[index] : CardAction::Drives;
    std::wstring           written;
    ActionsTypes::Outcome  outcome;



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
        m_context.verbs.ReportOutcome (m_context.actions.RepairA2retroNetConfig (m_card.root), L"Repair A2retroNET.txt");
    }
    else if (action == CardAction::SetDrive)
    {
        outcome = m_context.actions.GetA2retroNet().Add (m_setPath);
        m_context.verbs.ReportOutcome (outcome, L"Set as drive");

        std::erase_if (m_settable, [this] (const std::wstring & path) { return _wcsicmp (path.c_str(), m_setPath.c_str()) == 0; });
    }
    else
    {
        for (const std::wstring & path : m_convertible)
        {
            outcome = m_context.actions.GetA2retroNet().ReplaceWithPo (path, written);
            m_context.verbs.ReportOutcome (outcome, L"Convert to .po");

            //  The .po is offered as a drive.
            if (outcome.Succeeded())
            {
                m_settable.push_back (written);
            }
        }

        m_context.folderSync.RefreshAfterHostChange();
    }

    FillCardBar();
    m_relayout();
}