#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "Core/AppMessages.h"
#include "Seams/Win32ShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::ExplorerLayout
//
////////////////////////////////////////////////////////////////////////////////

ExplorerLayout::ExplorerLayout (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::ConfigureWidgets()
{
    HRESULT  hr = S_OK;



    //  CassoExplorer has no menu bar: Explorer's command bar takes its place. The
    //  strip stays, empty and hidden, so its Alt handling finds nothing.
    m_context.toolbars.Configure();
    m_context.treePane.Configure();

    m_context.listPane.Configure();
    m_context.preview.Configure();

    //  The commands installed apps add to Explorer's menus, and New's
    //  choices, read once.
    m_context.menus.StartPackagedCommands (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerNewChoices);

    m_treeSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_treeSplitter->SetOnMoved ([this] (int dip)
    {
        m_context.prefs.treeWidthDip = dip;
        RecomputeLayout();
    });

    m_previewSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_previewSplitter->SetOnMoved ([this] (int dip)
    {
        RECT  bounds = m_previewSplitter->GetBounds();
        int   width  = MulDiv (bounds.right - bounds.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_context.scaler.GetDpi());

        m_context.prefs.previewWidthDip = width - dip - DxuiSplitter::kSashDip;
        RecomputeLayout();
    });

    m_context.statusBar.Configure();
    m_context.tabManager.Configure();
    m_context.addressBar.Configure();

    m_context.browser.GetTypedPaths().Reset (m_context.prefs.typedPaths);
    m_context.listPane.GetColumns().ApplyStoredColumnWidths();

    m_context.folderSync.StartWatch();

    //  Deletions made anywhere reach the bin, so its list follows them.
    m_context.shellVerbs.WatchRecycleBin (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerRecycleBin);

    //  Slow shell folders, and host folders on the network, read on threads
    //  of their own and post a message when done.
    m_context.browser.GetShellListings().SetTarget (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerShellListed);
    m_context.browser.GetHostListings().SetTarget (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerShellListed);
    m_context.browser.GetHostListings().SetLister ([] (const std::wstring & path, std::vector<FileSystemEntry> & outEntries)
    {
        Win32FileSystem  fs;

        return fs.EnumerateAllEntries (path, outEntries);
    });

    //  The CassoLink connections are under CassoLink; each is started when it is
    //  opened, and posts here when an operation finishes or its state
    //  changes.
    if (m_context.app.links != nullptr)
    {
        m_context.app.links->SetTarget (m_context.window.GetHwnd(), (UINT) AppMessage::ExplorerCassoLink);
        m_context.app.links->SetLinks (m_context.prefs.links);
        m_context.browser.SetLinks (m_context.app.links);
        m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());
        m_context.treePane.RefreshTree();
        m_context.cassoLinkManager.StartAdtProServers();
    }

    //  Each running Casso with a serial port on TCP is a root as well, read
    //  now and again whenever a Casso starts, connects a port or exits.
    if (m_context.app.links != nullptr && m_context.app.running != nullptr)
    {
        HWND             hwnd   = m_context.window.GetHwnd();
        std::error_code  ignore;

        std::filesystem::create_directories (m_context.app.running->GetFolder(), ignore);

        if (m_context.app.watcher != nullptr)
        {
            m_context.app.watcher->Watch (m_context.app.running->GetFolder(), [hwnd] (const std::wstring &) { PostMessageW (hwnd, (UINT) AppMessage::ExplorerRunningCasso, 0, 0); });
        }

        m_context.cassoLinkManager.HandleRunningChanged();
    }

    //  The tabs of the request, from the session or a torn-off tab, or Home.
    m_context.browser.RestoreTabs (std::move (m_context.app.request.tabs), m_context.app.request.activeTab);
    m_context.browser.LeaveMissingLocation();

    m_context.folderSync.StartPolling();

    m_context.treePane.HandleFocusChanged (true);

    if (!m_context.app.request.openPath.empty())
    {
        bool  onHome = m_context.browser.GetBrowserModel().GetTabCount() == 1 && m_context.browser.GetLocation().kind == Location::Kind::None;

        m_context.tabManager.OpenPathInNewTab (m_context.app.request.openPath);

        //  A window opened for a path alone has no Home tab beside it, as File
        //  Explorer's new window has none.
        if (onHome && m_context.browser.GetBrowserModel().GetTabCount() > 1)
        {
            bool  closed = m_context.browser.CloseTab (0);

            IGNORE_RETURN_VALUE (closed, true);
        }
    }

    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::RecomputeLayout
//
//  Menu strip and toolbar across the top, status band across the bottom, and
//  the three panes in what the dock leaves between them. The tree splitter
//  spans the whole body so its limits are measured against it; the preview
//  splitter spans everything right of the tree, with its position the list's
//  width.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::RecomputeLayout()
{
    IDxuiControl  * bands[]     = { &m_tabBand, &m_toolbarBand, &m_menuBand, &m_statusBand, &m_bodyBand };
    RECT            body        = {};
    RECT            right       = {};
    RECT            sashRect    = {};
    int             rightDip    = 0;
    bool            preview     = m_context.prefs.previewVisible;
    PaneWidths      panes;



    if (!m_context.toolbars.IsCreated() || m_client.bottom <= m_client.top)
    {
        return;
    }

    m_context.toolbars.PlanForWidth (m_client.right - m_client.left);

    m_tabBand.SetThickness     (m_context.scaler.ToPx (kTabHeightDip));
    m_menuBand.SetThickness    ((int) std::floor (m_context.scaler.ToPxf (kCommandBarDip)));
    m_toolbarBand.SetThickness ((int) std::floor (m_context.scaler.ToPxf (kNavStripFillDip))   + DxuiToolbar::GetEdgePx (m_context.scaler));
    m_statusBand.SetThickness  (m_context.scaler.ToPx (ExplorerStatusBar::kStatusBandDip));

    m_dock.Arrange (m_client, m_context.scaler, bands);

    body = m_bodyBand.GetBounds();

    if (body.bottom <= body.top)
    {
        return;
    }

    m_context.tabManager.Layout (m_tabBand.GetBounds());

    m_context.toolbars.Layout (m_menuBand.GetBounds(), m_toolbarBand.GetBounds());

    panes = FitPanes (MulDiv (body.right - body.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_context.scaler.GetDpi()),
                      m_context.prefs.treeWidthDip, preview ? m_context.prefs.previewWidthDip : 0);

    m_context.addressBar.Layout (m_context.toolbars.GetFreeRect(), m_toolbarBand.GetBounds(), body.right - body.left - m_context.scaler.ToPx (panes.tree));

    m_context.tips.Layout (m_context.scaler.GetDpi(), m_client.right - m_client.left, m_client.bottom - m_client.top);

    //  A narrow window has already taken each pane below its minimum, so
    //  the sash can go no further than where the fit put it.
    m_treeSplitter->Layout (body, m_context.scaler);
    m_treeSplitter->SetLimitsDip ((std::min) (kMinTreeWidthDip, panes.tree),
                                           (std::min) (kMinListWidthDip, panes.list) + (preview ? (std::min) (kMinPreviewWidthDip, panes.preview) + DxuiSplitter::kSashDip : 0));
    m_treeSplitter->SetPositionDip (panes.tree);

    sashRect = m_treeSplitter->GetSashRect();
    m_context.treePane.Layout (RECT { body.left, body.top, sashRect.left, body.bottom });

    right    = RECT { sashRect.right, body.top, body.right, body.bottom };
    rightDip = MulDiv (right.right - right.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_context.scaler.GetDpi());

    m_previewSplitter->SetVisible (preview);

    if (preview)
    {
        m_previewSplitter->Layout (right, m_context.scaler);
        m_previewSplitter->SetLimitsDip ((std::min) (kMinListWidthDip, panes.list), (std::min) (kMinPreviewWidthDip, panes.preview));
        m_previewSplitter->SetPositionDip (rightDip - panes.preview - DxuiSplitter::kSashDip);

        sashRect = m_previewSplitter->GetSashRect();

        RECT  listRect    = { right.left, right.top, sashRect.left, right.bottom };
        RECT  previewRect = { sashRect.right, right.top, right.right, right.bottom };

        m_context.listPane.Layout        (listRect);
        m_context.cassoLinkManager.LayoutListActions();
        m_context.preview.Layout     (previewRect);
    }
    else
    {
        m_context.preview.Hide();
        m_context.listPane.Layout       (right);
        m_context.cassoLinkManager.LayoutListActions();
    }

    m_context.statusBar.Layout (m_statusBand.GetBounds());

    m_dropHits.Clear();
    m_dropHits.Register (DxuiHitRect { m_context.listPane.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagList });
    m_dropHits.Register (DxuiHitRect { m_context.treePane.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagTree });
    m_dropHits.Register (DxuiHitRect { m_context.tabManager.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagTabs });

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::GetMessageRect
//
//  A pane's message wraps at a reading width, centered in the pane, rather
//  than running the whole width of a wide window.
//
//  The part of a pane a message wraps within.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerLayout::GetMessageRect (const RECT & pane) const
{
    int   width = (std::min) ((int) (pane.right - pane.left), m_context.scaler.ToPx (kMessageWidthDip));
    RECT  rect  = pane;



    rect.left  = pane.left + ((pane.right - pane.left) - width) / 2;
    rect.right = rect.left + width;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::FitPanes
//
//  What the panes are short of comes out of the preview, then the tree,
//  each no further than its minimum; the list keeps its own. A body too
//  narrow even for the three minimums divides among them in proportion.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerLayout::PaneWidths ExplorerLayout::FitPanes (int bodyDip, int treeDip, int previewDip)
{
    PaneWidths  widths;
    int         sashes    = (previewDip > 0) ? 2 * DxuiSplitter::kSashDip : DxuiSplitter::kSashDip;
    int         available = (std::max) (0, bodyDip - sashes);
    int         listFloor = kMinListWidthDip;
    int         deficit   = 0;
    int         cut       = 0;
    int         total     = 0;



    widths.tree    = (std::max) (0, treeDip);
    widths.preview = (std::max) (0, previewDip);
    deficit        = widths.tree + widths.preview + listFloor - available;

    if (deficit > 0 && widths.preview > kMinPreviewWidthDip)
    {
        cut             = (std::min) (deficit, widths.preview - kMinPreviewWidthDip);
        widths.preview -= cut;
        deficit        -= cut;
    }

    if (deficit > 0 && widths.tree > kMinTreeWidthDip)
    {
        cut          = (std::min) (deficit, widths.tree - kMinTreeWidthDip);
        widths.tree -= cut;
        deficit     -= cut;
    }

    if (deficit > 0)
    {
        total          = widths.tree + widths.preview + listFloor;
        widths.tree    = MulDiv (widths.tree,    available, total);
        widths.preview = MulDiv (widths.preview, available, total);
    }

    widths.list = available - widths.tree - widths.preview;

    return widths;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::GetListRowHeightPx
//
//  File Explorer's list row, measured at nine scales from 100% to 350%:
//  twice 14 dip rounded up, and a pixel more at any scale that is not a
//  whole multiple -- 28, 37, 43, 51, 56, 65, 71, 84 and 99 pixels.
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerLayout::GetListRowHeightPx (UINT dpi)
{
    int  half    = (int) ((kListRowHalfDip * dpi + 95) / 96);
    int  partial = (dpi % 96 != 0) ? 1 : 0;



    return 2 * half + partial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::DockBands
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::DockBands()
{
    m_dock.SetDock (m_tabBand,     DxuiDock::Top);
    m_dock.SetDock (m_menuBand,    DxuiDock::Top);
    m_dock.SetDock (m_toolbarBand, DxuiDock::Top);
    m_dock.SetDock (m_statusBand,  DxuiDock::Bottom);
    m_dock.SetDock (m_bodyBand,    DxuiDock::Fill);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::SetSplitters
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::SetSplitters (DxuiSplitter * treeSplitter, DxuiSplitter * previewSplitter)
{
    m_treeSplitter    = treeSplitter;
    m_previewSplitter = previewSplitter;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLayout::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::ToLocal
//
//  List views take their events relative to their own origin.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMouseEvent ExplorerLayout::ToLocal (const DxuiMouseEvent & ev, const RECT & bounds)
{
    DxuiMouseEvent  local = ev;



    local.positionDip.x -= bounds.left;
    local.positionDip.y -= bounds.top;

    return local;
}
