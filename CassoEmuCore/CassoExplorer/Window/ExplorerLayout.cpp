#include "Pch.h"

#include "CassoExplorer/Window/ExplorerLayout.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Model/FolderWatch.h"

//  The Theme menu's rows, in order, by the theme each one selects: Casso's
//  themes in Casso's order, then Windows' dark and light and Follow system.
static constexpr const char *  s_kpszThemeRows[] =
{
    CassoExplorerPrefs::kThemeSkeuomorphic,
    CassoExplorerPrefs::kThemeRetroTerminal,
    CassoExplorerPrefs::kThemeDarkModern,
    CassoExplorerPrefs::kThemeDark,
    CassoExplorerPrefs::kThemeLight,
    CassoExplorerPrefs::kThemeFollowSystem,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::ExplorerLayout
//
////////////////////////////////////////////////////////////////////////////////

ExplorerLayout::ExplorerLayout (CassoExplorerWindow & window)
    : m_window (window)
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
    m_window.m_menuBar->SetItems   ({});
    m_window.m_menuBar->SetVisible (false);
    m_window.m_treePane.Configure();

    m_window.m_listPane.Configure();
    m_window.m_previewPane.Configure();

    //  The commands installed apps add to Explorer's menus, and New's
    //  choices, read once.
    m_window.m_packagedCommands.Start (m_window.GetHwnd(), CassoExplorerWindow::kNewChoicesMessage);

    m_window.m_treeSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_window.m_treeSplitter->SetOnMoved ([this] (int dip)
    {
        m_window.m_prefs.treeWidthDip = dip;
        RecomputeLayout();
    });

    m_window.m_previewSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_window.m_previewSplitter->SetOnMoved ([this] (int dip)
    {
        RECT  bounds = m_window.m_previewSplitter->GetBounds();
        int   width  = MulDiv (bounds.right - bounds.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_window.m_scaler.GetDpi());

        m_window.m_prefs.previewWidthDip = width - dip - DxuiSplitter::kSashDip;
        RecomputeLayout();
    });

    m_window.m_status->SetFields ({ { L"", 0, false, -1, true }, { L"", 0, false, -1, true }, { L"", 0, true },
                           { L"", kStatusFreeDip, false }, { L"", kStatusDetailDip, false }, { L"", kStatusZoomDip, false } });

    m_window.m_tabStrip.Configure();

    m_window.m_addressBar.Configure();

    m_window.m_browser.GetTypedPaths().Reset (m_window.m_prefs.typedPaths);
    m_window.m_listPane.GetColumns().ApplyStoredColumnWidths();

    //  Posting is all that happens on the watcher's thread; the settle timer
    //  and the re-read run here.
    if (m_window.m_context.watcher != nullptr)
    {
        m_window.m_folderWatch = std::make_unique<FolderWatch> (*m_window.m_context.watcher);

        m_window.m_folderWatch->SetOnChanged ([this] ()
        {
            PostMessageW (m_window.GetHwnd(), CassoExplorerWindow::kFolderChangedMessage, 0, 0);
        });
    }

    //  Deletions made anywhere reach the bin, so its list follows them.
    m_window.m_shellVerbs.WatchRecycleBin (m_window.GetHwnd(), CassoExplorerWindow::kRecycleBinMessage);

    //  Slow shell folders, and host folders on the network, read on threads
    //  of their own and post a message when done.
    m_window.m_browser.GetShellListings().SetTarget (m_window.GetHwnd(), CassoExplorerWindow::kShellListedMessage);
    m_window.m_browser.GetHostListings().SetTarget (m_window.GetHwnd(), CassoExplorerWindow::kShellListedMessage);
    m_window.m_browser.GetHostListings().SetLister ([] (const std::wstring & path, std::vector<FileSystemEntry> & outEntries)
    {
        Win32FileSystem  fs;

        return fs.EnumerateAllEntries (path, outEntries);
    });

    //  The CassoLink connections are under CassoLink; each is started when it is
    //  opened, and posts here when an operation finishes or its state
    //  changes.
    if (m_window.m_context.links != nullptr)
    {
        m_window.m_context.links->SetTarget (m_window.GetHwnd(), CassoExplorerWindow::kLinkMessage);
        m_window.m_context.links->SetLinks (m_window.m_prefs.links);
        m_window.m_browser.SetLinks (m_window.m_context.links);
        m_window.m_browser.GetTreeModel().SetLinks (m_window.m_context.links->BuildTreeEntries());
        m_window.m_treePane.RefreshTree();
        m_window.m_links.StartAdtProServers();
    }

    //  Each running Casso with a serial port on TCP is a root as well, read
    //  now and again whenever a Casso starts, connects a port or exits.
    if (m_window.m_context.links != nullptr && m_window.m_context.running != nullptr)
    {
        HWND             hwnd   = m_window.GetHwnd();
        std::error_code  ignore;

        std::filesystem::create_directories (m_window.m_context.running->GetFolder(), ignore);

        if (m_window.m_context.watcher != nullptr)
        {
            m_window.m_context.watcher->Watch (m_window.m_context.running->GetFolder(), [hwnd] (const std::wstring &) { PostMessageW (hwnd, CassoExplorerWindow::kRunningMessage, 0, 0); });
        }

        m_window.m_links.OnRunningChanged();
    }

    //  The tabs of the request, from the session or a torn-off tab, or Home.
    m_window.m_browser.RestoreTabs (std::move (m_window.m_context.request.tabs), m_window.m_context.request.activeTab);
    m_window.m_browser.LeaveMissingLocation();

    m_window.m_driveMask = GetLogicalDrives();
    SetTimer (m_window.GetHwnd(), CassoExplorerWindow::kDriveTimerId, kDrivePollMs, nullptr);

    {
        PWSTR    recent   = nullptr;
        HRESULT  hrRecent = SHGetKnownFolderPath (FOLDERID_Recent, 0, nullptr, &recent);

        if (SUCCEEDED (hrRecent))
        {
            m_window.m_quickAccessDir   = std::wstring (recent) + L"\\AutomaticDestinations";
            m_window.m_quickAccessStamp = ExplorerFolderSync::GetQuickAccessStamp (m_window.m_quickAccessDir);
        }

        CoTaskMemFree (recent);
    }

    m_window.m_treePane.OnFocusChanged (true);

    if (!m_window.m_context.request.openPath.empty())
    {
        bool  onHome = m_window.m_browser.GetBrowserModel().GetTabCount() == 1 && m_window.m_browser.GetLocation().kind == Location::Kind::None;

        m_window.m_tabStrip.OpenPathInNewTab (m_window.m_context.request.openPath);

        //  A window opened for a path alone has no Home tab beside it, as File
        //  Explorer's new window has none.
        if (onHome && m_window.m_browser.GetBrowserModel().GetTabCount() > 1)
        {
            bool  closed = m_window.m_browser.CloseTab (0);

            IGNORE_RETURN_VALUE (closed, true);
        }
    }

    m_window.m_listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::AdoptSystemColors
//
//  The Windows themes take the list surface and the accent from the system
//  each time a theme is chosen, which also picks up an accent changed while
//  the window was away.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::AdoptSystemColors()
{
    const DxuiWindowsThemeColors::SystemColors &  system = DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_window.m_lightTheme.ApplySystemColors (system);
    m_window.m_darkTheme.ApplySystemColors  (system);

    //  The caption and the tab row are the window's Mica Alt, and the row under
    //  the tabs lightens it, as Explorer's do.
    m_window.m_lightTheme.titleBarTop    = 0x00000000u;
    m_window.m_darkTheme.titleBarTop     = 0x00000000u;
    m_window.m_lightTheme.titleBarBottom = 0x00000000u;
    m_window.m_darkTheme.titleBarBottom  = 0x00000000u;
    m_window.m_lightTheme.navStrip       = 0xB3FFFFFFu;
    m_window.m_darkTheme.navStrip        = 0x0EFFFFFFu;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::ApplyTheme
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::ApplyTheme()
{
    AdoptSystemColors();

    m_window.m_theme = &CassoExplorerShell::ChooseTheme (m_window.m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_window.m_lightTheme, m_window.m_darkTheme);

    //  Casso's own themes lend their chrome colors; the skeuomorphic one has
    //  no scene to draw here, so it is colors only.
    if (IsCassoThemeName (m_window.m_prefs.theme))
    {
        m_window.m_cassoTheme = CassoTheme::MakeByName (m_window.m_prefs.theme);
        m_window.m_theme      = &m_window.m_cassoTheme;
    }

    //  The menus' icons come in a set per theme.
    m_window.m_menus.ApplyMenuSvgs();

    m_window.SetTheme (m_window.m_theme);

    if (m_window.m_menuBar != nullptr)
    {
        m_window.m_menuBar->SetStripColors    (m_window.m_theme->navStrip, m_window.m_theme->navHover, m_window.m_theme->navItemText);
        m_window.m_menuBar->SetDropdownColors (m_window.m_theme->dropdownBg, m_window.m_theme->dropdownHover, m_window.m_theme->dropdownItemText,
                                               m_window.m_theme->dropdownAccel, m_window.m_theme->panelEdge, m_window.m_theme->buttonBorder);
    }

    if (m_window.m_toolbar != nullptr)
    {
        //  Explorer's command bar is the list body's color, not the strip's
        //  above it, with a border line above and below it.
        m_window.m_toolbar->SetStripColors (m_window.m_theme->navStrip, m_window.m_theme->navItemText);
        m_window.m_toolbar->SetEdgeColor   (m_window.m_theme->Border());

        if (m_window.m_commandBar != nullptr)
        {
            m_window.m_commandBar->SetStripColors (m_window.m_theme->ContentBackground(), m_window.m_theme->navItemText);
            m_window.m_commandBar->SetEdgeColor   (m_window.m_theme->Border());
        }

        m_window.m_tooltip.SetTheme (*m_window.m_theme);
        m_window.m_addressBar.ApplyTheme (m_window.m_theme);
    }

    m_window.m_tabStrip.ApplyTheme (*m_window.m_theme);

    if (m_window.GetHwnd() != nullptr)
    {
        uint32_t  background = m_window.m_theme->Background();
        int       luminance  = (int) (((background >> 16) & 0xFF) * 299 + ((background >> 8) & 0xFF) * 587 + (background & 0xFF) * 114) / 1000;

        DxuiDwm::ApplyImmersiveDarkMode (m_window.GetHwnd(), luminance < 128);
    }

    m_window.Invalidate();
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
    IDxuiControl  * bands[]     = { &m_window.m_tabBand, &m_window.m_toolbarBand, &m_window.m_menuBand, &m_window.m_statusBand, &m_window.m_bodyBand };
    RECT            body        = {};
    RECT            right       = {};
    RECT            sashRect    = {};
    int             rightDip    = 0;
    bool            preview     = m_window.m_prefs.previewVisible;
    PaneWidths      panes;



    if (m_window.m_menuBar == nullptr || m_window.m_client.bottom <= m_window.m_client.top)
    {
        return;
    }

    //  The toolbar's band thickness follows the responsive mode it plans for
    //  the width, so plan it before the bands are docked.
    m_window.m_toolbar->PlanForWidth    (m_window.m_client.right - m_window.m_client.left, m_window.m_scaler);
    m_window.m_commandBar->PlanForWidth (m_window.m_client.right - m_window.m_client.left, m_window.m_scaler);

    m_window.m_tabBand.SetThickness     (m_window.m_scaler.ToPx (kTabHeightDip));
    m_window.m_menuBand.SetThickness    ((int) std::floor (m_window.m_scaler.ToPxf (kCommandBarDip)));
    m_window.m_toolbarBand.SetThickness ((int) std::floor (m_window.m_scaler.ToPxf (kNavStripFillDip))   + DxuiToolbar::GetEdgePx (m_window.m_scaler));
    m_window.m_statusBand.SetThickness  (m_window.m_scaler.ToPx (kStatusBandDip));

    m_window.m_dock.Arrange (m_window.m_client, m_window.m_scaler, bands);

    body = m_window.m_bodyBand.GetBounds();

    if (body.bottom <= body.top)
    {
        return;
    }

    m_window.m_tabStrip.Layout (m_window.m_tabBand.GetBounds());

    m_window.m_commandBar->SetHostClientRect (m_window.m_client);
    m_window.m_commandBar->Layout (m_window.m_menuBand.GetBounds(), m_window.m_scaler);

    m_window.m_toolbar->SetHostClientRect (m_window.m_client);
    m_window.m_toolbar->Layout (m_window.m_toolbarBand.GetBounds(), m_window.m_scaler);

    panes = FitPanes (MulDiv (body.right - body.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_window.m_scaler.GetDpi()),
                      m_window.m_prefs.treeWidthDip, preview ? m_window.m_prefs.previewWidthDip : 0);

    m_window.m_addressBar.Layout (m_window.m_toolbar->GetFreeRect(), m_window.m_toolbarBand.GetBounds(), body.right - body.left - m_window.m_scaler.ToPx (panes.tree));

    m_window.m_tooltip.SetDpi (m_window.m_scaler.GetDpi());
    m_window.m_tooltip.SetViewportSize (m_window.m_client.right - m_window.m_client.left, m_window.m_client.bottom - m_window.m_client.top);
    m_window.m_status->Layout (m_window.m_statusBand.GetBounds(), m_window.m_scaler);

    //  A narrow window has already taken each pane below its minimum, so
    //  the sash can go no further than where the fit put it.
    m_window.m_treeSplitter->Layout (body, m_window.m_scaler);
    m_window.m_treeSplitter->SetLimitsDip ((std::min) (kMinTreeWidthDip, panes.tree),
                                           (std::min) (kMinListWidthDip, panes.list) + (preview ? (std::min) (kMinPreviewWidthDip, panes.preview) + DxuiSplitter::kSashDip : 0));
    m_window.m_treeSplitter->SetPositionDip (panes.tree);

    sashRect = m_window.m_treeSplitter->GetSashRect();
    m_window.m_treePane.Layout (RECT { body.left, body.top, sashRect.left, body.bottom });

    right    = RECT { sashRect.right, body.top, body.right, body.bottom };
    rightDip = MulDiv (right.right - right.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_window.m_scaler.GetDpi());

    m_window.m_previewSplitter->SetVisible (preview);

    if (preview)
    {
        m_window.m_previewSplitter->Layout (right, m_window.m_scaler);
        m_window.m_previewSplitter->SetLimitsDip ((std::min) (kMinListWidthDip, panes.list), (std::min) (kMinPreviewWidthDip, panes.preview));
        m_window.m_previewSplitter->SetPositionDip (rightDip - panes.preview - DxuiSplitter::kSashDip);

        sashRect = m_window.m_previewSplitter->GetSashRect();

        RECT  listRect    = { right.left, right.top, sashRect.left, right.bottom };
        RECT  previewRect = { sashRect.right, right.top, right.right, right.bottom };

        m_window.m_listPane.Layout        (listRect);
        m_window.m_links.LayoutListActions();
        m_window.m_previewPane.Layout     (previewRect);
    }
    else
    {
        m_window.m_previewPane.Hide();
        m_window.m_listPane.Layout       (right);
        m_window.m_links.LayoutListActions();
    }

    LayoutStatusFields();

    m_dropHits.Clear();
    m_dropHits.Register (DxuiHitRect { m_window.m_listPane.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagList });
    m_dropHits.Register (DxuiHitRect { m_window.m_treePane.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagTree });
    m_dropHits.Register (DxuiHitRect { m_window.m_tabStrip.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagTabs });

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::UpdateCommandBar
//
//  The Recycle Bin, and an archive selected or open, have buttons of their
//  own on the command bar, after View, as Explorer's.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::UpdateCommandBar()
{
    using Extra = CassoExplorerCommands::CommandBarExtra;

    Extra  extra = Extra::None;



    if (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        extra = Extra::RecycleBin;
    }
    else if (m_window.m_browser.IsAppleDriveLocation() || m_window.m_links.IsOnLinkedMachine())
    {
        extra = Extra::AppleDrive;
    }
    else if (!m_window.m_browser.GetExtractTarget().empty())
    {
        extra = Extra::Archive;
    }

    if (extra == m_commandBarExtra)
    {
        return;
    }

    m_commandBarExtra = extra;

    //  Explorer's own icons for the buttons, in the theme's set.
    m_window.m_commands.SetSvg (CassoExplorerCommands::kEmptyRecycleBin, m_window.m_menus.GetMenuSvg (L"windows.recyclebin.empty"));
    m_window.m_commands.SetSvg (CassoExplorerCommands::kRestoreItems,    m_window.m_menus.GetMenuSvg (L"windows.recyclebin.restoreall"));
    m_window.m_commands.SetSvg (CassoExplorerCommands::kExtractAll,      m_window.m_menus.GetMenuSvg (L"windows.compressedfile.extract"));
    m_window.m_commandBar->SetEntries (m_window.m_commands.BuildCommandBarEntries (extra));
    m_window.m_commandBar->Layout     (m_window.m_menuBand.GetBounds(), m_window.m_scaler);
    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::FillStatus
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::FillStatus()
{
    const CassoExplorerBrowser::Status &  status = m_window.m_browser.GetStatus();
    std::wstring                          detail = status.detail;



    //  Bytes or characters selected in the preview take the detail section.
    m_window.m_previewPane.TryGetSelectionDetail (detail);

    //  A link operation's progress has the field while one is pending, so a
    //  refill on a hover or a selection does not replace it between its
    //  updates.
    if (m_window.m_context.links != nullptr && !m_window.m_context.links->GetPendingJobs().empty())
    {
        detail                   = CassoLinkConnections::DescribeJobs (m_window.m_context.links->GetPendingJobs());
        m_window.m_linkJobsShown = true;
    }

    m_window.m_status->SetText (kStatusCount,    status.selection);
    m_window.m_status->SetText (kStatusSelected, status.selected);
    m_window.m_status->SetText (kStatusFree,     status.freeSpace);
    m_window.m_status->SetText (kStatusDetail,   detail);
    m_window.m_status->SetText (kStatusZoom,     std::format (L"{}%", m_window.m_prefs.previewZoom));
    m_window.m_status->SetText (kStatusFill,     m_window.m_links.GetLatestActivity());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::LayoutStatusFields
//
//  The item count stretches, and free space follows it. The preview's two
//  fields together are as wide as the preview pane, so they begin at its
//  left edge; with the preview hidden they keep a fixed width.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::LayoutStatusFields()
{
    RECT   band    = m_window.m_statusBand.GetBounds();
    bool   preview = m_window.m_prefs.previewVisible && (m_window.m_previewPane.GetRect().right > m_window.m_previewPane.GetRect().left);
    RECT   sash    = m_window.m_previewSplitter->GetSashRect();
    int    seam    = (int) (DxuiSplitter::GetSeam (sash.left, sash.right, m_window.m_scaler) + DxuiSplitter::GetLinePx (m_window.m_scaler));
    int    zoomPx  = m_window.m_scaler.ToPx (kStatusZoomDip);
    int    detail  = preview ? (std::max) ((int) band.right - seam - zoomPx, 0) : m_window.m_scaler.ToPx (kStatusDetailDip);



    //  The detail field starts on the splitter's visible line: the lighter of
    //  the two it draws in the middle of its sash, since the darker one is the
    //  color of the panes beside it. The two dividers then meet.
    m_window.m_status->SetFields ({ { L"", 0, false, -1, true },
                           { L"", 0, false, -1, true },
                           { L"", 0, true },
                           { L"", kStatusFreeDip, false },
                           { L"", 0, false, detail },
                           { L"", kStatusZoomDip, false } });
    m_window.m_status->Layout (band, m_window.m_scaler);

    FillStatus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::GetMessageRect
//
//  A pane's message wraps at a reading width, centered in the pane, rather
//  than running the whole width of a wide window.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerLayout::GetMessageRect (const RECT & pane) const
{
    int   width = (std::min) ((int) (pane.right - pane.left), m_window.m_scaler.ToPx (kMessageWidthDip));
    RECT  rect  = pane;



    rect.left  = pane.left + ((pane.right - pane.left) - width) / 2;
    rect.right = rect.left + width;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::PreviewTheme
//
//  The theme on a row of the Theme menu, in the menu's order, applied while
//  the pointer is over it. A row past the end, or none, changes nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::PreviewTheme (int index)
{
    if (index < 0 || index >= (int) std::size (s_kpszThemeRows) || m_window.m_prefs.theme == s_kpszThemeRows[index])
    {
        return;
    }

    SelectTheme (s_kpszThemeRows[index]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::SelectTheme
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLayout::SelectTheme (const char * name)
{
    m_window.m_prefs.theme = name;
    ApplyTheme();

    //  A Casso theme brings its own CRT effects to a picture preview.
    m_window.m_previewPane.UpdateCrtPicture (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLayout::IsCassoThemeName
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLayout::IsCassoThemeName (const std::string & name)
{
    return name == CassoExplorerPrefs::kThemeSkeuomorphic
        || name == CassoExplorerPrefs::kThemeDarkModern
        || name == CassoExplorerPrefs::kThemeRetroTerminal;
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
////////////////////////////////////////////////////////////////////////////////

int ExplorerLayout::GetListRowHeightPx (UINT dpi)
{
    int  half    = (int) ((kListRowHalfDip * dpi + 95) / 96);
    int  partial = (dpi % 96 != 0) ? 1 : 0;



    return 2 * half + partial;
}
