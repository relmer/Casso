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
    m_window.m_toolbars.Configure();
    m_window.m_treePane.Configure();

    m_window.m_listPane.Configure();
    m_window.m_previewPane.Configure();

    //  The commands installed apps add to Explorer's menus, and New's
    //  choices, read once.
    m_window.m_menus.StartPackagedCommands (m_window.GetHwnd(), CassoExplorerWindow::kNewChoicesMessage);

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

    m_window.m_statusBar.Configure();

    //  The tabs of the request, from the session or a torn-off tab, or Home.
    m_window.m_browser.RestoreTabs (std::move (m_window.m_context.request.tabs), m_window.m_context.request.activeTab);
    m_window.m_browser.LeaveMissingLocation();

    m_window.m_folderSync.StartPolling();

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

    m_window.m_toolbars.ApplyTheme (*m_window.m_theme);

    if (m_window.m_toolbars.IsCreated())
    {
        m_window.m_tips.ApplyTheme (*m_window.m_theme);
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



    if (!m_window.m_toolbars.IsCreated() || m_window.m_client.bottom <= m_window.m_client.top)
    {
        return;
    }

    m_window.m_toolbars.PlanForWidth (m_window.m_client.right - m_window.m_client.left);

    m_window.m_tabBand.SetThickness     (m_window.m_scaler.ToPx (kTabHeightDip));
    m_window.m_menuBand.SetThickness    ((int) std::floor (m_window.m_scaler.ToPxf (kCommandBarDip)));
    m_window.m_toolbarBand.SetThickness ((int) std::floor (m_window.m_scaler.ToPxf (kNavStripFillDip))   + DxuiToolbar::GetEdgePx (m_window.m_scaler));
    m_window.m_statusBand.SetThickness  (m_window.m_scaler.ToPx (ExplorerStatusBar::kStatusBandDip));

    m_window.m_dock.Arrange (m_window.m_client, m_window.m_scaler, bands);

    body = m_window.m_bodyBand.GetBounds();

    if (body.bottom <= body.top)
    {
        return;
    }

    m_window.m_tabStrip.Layout (m_window.m_tabBand.GetBounds());

    m_window.m_toolbars.Layout (m_window.m_menuBand.GetBounds(), m_window.m_toolbarBand.GetBounds());

    panes = FitPanes (MulDiv (body.right - body.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_window.m_scaler.GetDpi()),
                      m_window.m_prefs.treeWidthDip, preview ? m_window.m_prefs.previewWidthDip : 0);

    m_window.m_addressBar.Layout (m_window.m_toolbars.GetFreeRect(), m_window.m_toolbarBand.GetBounds(), body.right - body.left - m_window.m_scaler.ToPx (panes.tree));

    m_window.m_tips.Layout (m_window.m_scaler.GetDpi(), m_window.m_client.right - m_window.m_client.left, m_window.m_client.bottom - m_window.m_client.top);

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

    m_window.m_statusBar.Layout (m_window.m_statusBand.GetBounds());

    m_dropHits.Clear();
    m_dropHits.Register (DxuiHitRect { m_window.m_listPane.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagList });
    m_dropHits.Register (DxuiHitRect { m_window.m_treePane.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagTree });
    m_dropHits.Register (DxuiHitRect { m_window.m_tabStrip.GetBounds(), DxuiHitSlot::Custom, CassoExplorerWindow::kDropTagTabs });

    m_window.Invalidate();
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
