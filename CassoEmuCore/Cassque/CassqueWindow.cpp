#include "Pch.h"

#include "Cassque/CassqueWindow.h"
#include "Cassque/CassqueAbout.h"
#include "Cassque/CassqueDragOut.h"
#include "Cassque/CassqueNewDiskDialog.h"
#include "Cassque/CassquePromptDialog.h"
#include "Cassque/CassqueShell.h"
#include "Cassque/Model/KnownFolderStore.h"
#include "Cassque/Model/LaunchCommand.h"
#include "Core/TextEncoding.h"
#include "Widgets/DxuiContextMenu.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiWindowsThemeColors.h"
#include "Window/DxuiMessageBox.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::CassqueWindow
//
////////////////////////////////////////////////////////////////////////////////

CassqueWindow::CassqueWindow (CassqueBrowser & browser, CassqueActions & actions, CassquePrefs & prefs, Context context)
    : m_browser  (browser),
      m_actions  (actions),
      m_prefs    (prefs),
      m_context  (std::move (context)),
      m_commands (CassqueCommands::Handlers {
                      [this] (int id)       { Dispatch (id); },
                      [this] (int id)       { return IsEnabled (id); },
                      [this] (int id)       { return IsChecked (id); } })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::~CassqueWindow
//
////////////////////////////////////////////////////////////////////////////////

CassqueWindow::~CassqueWindow()
{
    m_dropTarget.Shutdown();
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Open
//
//  The class name is the one a second launch looks for, so it is fixed. A
//  remembered placement still on a monitor is applied before the first show.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassqueWindow::Open (HINSTANCE instance, const std::wstring & title, int showCommand)
{
    HRESULT                   hr     = S_OK;
    DxuiWindow::CreateParams  params;
    WINDOWPLACEMENT           place  = { sizeof (place) };
    RECT                      rect   = {};
    BOOL                      result = FALSE;



    AdoptSystemColors();

    m_theme = &CassqueShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Before the first layout, which the window's creation can bring on.
    m_dock.SetDock (m_menuBand,    DxuiDock::Top);
    m_dock.SetDock (m_toolbarBand, DxuiDock::Top);
    m_dock.SetDock (m_statusBand,  DxuiDock::Bottom);
    m_dock.SetDock (m_bodyBand,    DxuiDock::Fill);

    params.title                    = title;
    params.hInstance                = instance;
    params.initialSizeDip           = { CassqueShell::kDefaultWidthDip, CassqueShell::kDefaultHeightDip };
    params.minSizeDip               = { 640, 400 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = CassqueShell::kWindowClass;
    params.appIconBig               = LoadIconW (instance, MAKEINTRESOURCEW (IDI_CASSQUE));
    params.appIconSmall             = params.appIconBig;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    //  The caption draws the app's own icon, as Casso's does; the taskbar
    //  already has it from the create parameters. A failure leaves the caption
    //  without one rather than failing the window.
    {
        HICON          captionIcon = (HICON) LoadImageW (instance, MAKEINTRESOURCEW (IDI_CASSQUE), IMAGE_ICON,
                                                         kCaptionIconPx, kCaptionIconPx, LR_DEFAULTCOLOR);
        DxuiIconImage  image;
        HRESULT        hrIcon      = E_FAIL;

        if (captionIcon != nullptr)
        {
            hrIcon = DxuiIconImage::FromHicon (captionIcon, kCaptionIconPx, image);
            DestroyIcon (captionIcon);
        }

        if (SUCCEEDED (hrIcon))
        {
            GetPopupHost()->SetCaptionIcon (std::move (image.bgraPremul), image.width, image.height);
        }
    }

    ApplyTheme();

    //  The tooltip's dwell runs on a timer: nothing else ticks this window.
    SetTimer (GetHwnd(), kTooltipTimerId, kTooltipTickMs, nullptr);

    //  A file dropped on the list while it shows an image goes into it. A
    //  failure to register leaves the window without drops, not without a
    //  window.
    {
        HRESULT  hrDrop = m_dropTarget.Initialize (GetHwnd(), &m_dropHits,
                                                   [this] (int, const std::wstring & path) { OnDropFile (path); },
                                                   [this] (const std::wstring &) { return m_browser.IsImageLocation(); });

        IGNORE_RETURN_VALUE (hrDrop, S_OK);
    }

    if (m_prefs.placement.valid && m_prefs.placement.w > 0 && m_prefs.placement.h > 0)
    {
        rect = RECT { m_prefs.placement.x, m_prefs.placement.y,
                      m_prefs.placement.x + m_prefs.placement.w, m_prefs.placement.y + m_prefs.placement.h };

        if (MonitorFromRect (&rect, MONITOR_DEFAULTTONULL) != nullptr)
        {
            place.rcNormalPosition = rect;
            place.showCmd          = SW_HIDE;
            result = SetWindowPlacement (GetHwnd(), &place);
            IGNORE_RETURN_VALUE (result, TRUE);

            if (m_prefs.placement.maximized && showCommand != SW_SHOWMINIMIZED && showCommand != SW_SHOWMINNOACTIVE)
            {
                showCommand = SW_SHOWMAXIMIZED;
            }
        }
    }

    result = ShowWindow (GetHwnd(), showCommand);
    IGNORE_RETURN_VALUE (result, FALSE);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::StorePlacement
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::StorePlacement()
{
    WINDOWPLACEMENT  place = { sizeof (place) };



    if (GetHwnd() == nullptr || !GetWindowPlacement (GetHwnd(), &place))
    {
        return;
    }

    m_prefs.placement.x         = place.rcNormalPosition.left;
    m_prefs.placement.y         = place.rcNormalPosition.top;
    m_prefs.placement.w         = place.rcNormalPosition.right - place.rcNormalPosition.left;
    m_prefs.placement.h         = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
    m_prefs.placement.maximized = place.showCmd == SW_SHOWMAXIMIZED;
    m_prefs.placement.valid     = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnCreate
//
//  Children are created in paint order: panes first, then the splitters over
//  their edges, the status bar, and the menu bar last so its strip is on top.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnCreate()
{
    CassqueNamedControl<DxuiTreeView>         * tree            = CreateChild<CassqueNamedControl<DxuiTreeView>>();
    CassqueNamedControl<DxuiListView>         * list            = CreateChild<CassqueNamedControl<DxuiListView>>();
    CassqueNamedControl<DxuiListView>         * previewList     = nullptr;
    CassqueNamedControl<DxuiFramebufferView>  * picture         = nullptr;
    CassqueNamedControl<DxuiHexView>          * hexView         = nullptr;
    CassqueNamedControl<DxuiSplitter>         * treeSplitter    = nullptr;
    CassqueNamedControl<DxuiSplitter>         * previewSplitter = nullptr;



    tree->SetAccessibleName (L"Folders and disk images");
    list->SetAccessibleName (L"Files");

    m_tree            = tree;
    m_list            = list;
    m_listMessage     = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    previewList       = CreateChild<CassqueNamedControl<DxuiListView>>();
    picture           = CreateChild<CassqueNamedControl<DxuiFramebufferView>>();
    hexView           = CreateChild<CassqueNamedControl<DxuiHexView>>();
    m_previewMessage  = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    treeSplitter      = CreateChild<CassqueNamedControl<DxuiSplitter>>();
    previewSplitter   = CreateChild<CassqueNamedControl<DxuiSplitter>>();

    previewList->SetAccessibleName     (L"Preview");
    picture->SetAccessibleName         (L"Picture preview");
    hexView->SetAccessibleName         (L"Bytes");
    treeSplitter->SetAccessibleName    (L"Folder pane width");
    previewSplitter->SetAccessibleName (L"Preview pane width");

    m_previewList     = previewList;
    m_hexView         = hexView;
    m_picture         = picture;
    m_treeSplitter    = treeSplitter;
    m_previewSplitter = previewSplitter;
    m_status          = CreateChild<DxuiStatusBar>();
    m_tabs            = CreateChild<DxuiTabStrip>();
    m_toolbar         = CreateChild<DxuiToolbar>();
    m_menuBar         = CreateChild<DxuiMenuBar>();

    m_toolbar->SetTextRenderer (GetTextRenderer());
    m_toolbar->SetPopupHost    (GetPopupHost());
    m_toolbar->SetEntries      (m_commands.BuildToolbarEntries());
    m_tooltip.SetPopupHost     (GetPopupHost());

    //  Explorer's navigation glyphs are in Windows 11's Segoe Fluent Icons.
    //  Without that font, use MDL2, which has the same code points; text in a
    //  missing font renders as nothing.
    m_toolbar->SetIconFace (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                            ? DxuiToolbar::kFluentIconFace
                            : DxuiToolbar::kMdl2IconFace);

    m_menuBar->SetPopupHost (GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());

    //  Drawn by the shell at the pixel size a row's icon is laid out at, and
    //  handed to the browser before the first nodes and rows are built.
    m_shellIcons.SetSizePx (MulDiv (DxuiTreeView::s_kIconDip, (int) GetDpiForWindow (GetHwnd()), (int) DxuiDpiScaler::kBaseDpi));
    m_browser.SetShellIcons (&m_shellIcons);

    ConfigureWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ConfigureWidgets
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ConfigureWidgets()
{
    std::vector<DxuiTreeNode>  roots;
    bool                       grouped = false;



    m_menuBar->SetItems (m_commands.BuildMenuItems());

    m_browser.GetTreeRoots (roots);

    m_tree->SetShowCheckboxes (false);
    m_tree->SetNodes (std::move (roots));
    m_tree->SetChildProvider ([this] (const std::wstring & id) { return m_browser.GetTreeChildren (id); });

    m_tree->SetOnSelect ([this] (const std::wstring & id)
    {
        HRESULT  hr = m_browser.SelectTreeNode (id);

        IGNORE_RETURN_VALUE (hr, S_OK);
        FillList();
    });

    m_list->SetShowHeader (true);
    m_list->SetColumns (CassqueBrowser::GetColumns());

    //  Columns keep the widths they are given, and a pane too narrow for them
    //  scrolls, the way Explorer's details view does, rather than squeezing
    //  every column to fit.
    m_list->SetHorizontalScrollEnabled (true);
    m_list->SetMultiSelect (true);
    m_list->SetKeyboardColumnNav (true);
    m_list->SetActivateOnDoubleClick (true);
    m_list->SetAlwaysShowSelection (true);

    m_list->SetOnSelectionChanged ([this] (int)
    {
        m_browser.SetSelectedRows (m_list->GetSelectedRows());
        FillPreview();
        FillStatus();
    });

    m_list->SetOnSortColumn ([this] (int column)
    {
        m_browser.SortByColumn (column);
        FillList();
    });

    m_list->SetOnActivateRow ([this] (int row)
    {
        if (m_browser.OpenRow (row))
        {
            FillList();
        }
    });

    m_previewList->SetShowHeader (false);
    m_previewList->SetColumns ({ DxuiListView::Column { L"", 0, true } });

    //  A hex dump and a disassembly are columns of fixed-width text, and read
    //  as such only in a monospace face on rows close to the line height.
    m_previewList->SetMonospace (true);
    m_previewList->SetRowHeightDip (kPreviewRowHeightDip);
    m_previewList->SetHorizontalScrollEnabled (true);

    //  The bytes are Apple text in the column on the right, which is what
    //  the files here hold; the grouping is the user's, kept between runs.
    m_hexView->SetTextEncoding (DxuiHexView::TextEncoding::AppleHighBit);
    m_hexView->SetOwnerWindow  (GetHwnd());

    grouped = m_hexView->SetGrouping (m_prefs.hexGrouping);
    IGNORE_RETURN_VALUE (grouped, true);

    m_hexView->SetOnSelectionChanged ([this] () { FillStatus(); });

    m_hexView->SetOnContextMenu ([this] (POINT atDip)
    {
        ShowHexContextMenu (m_scaler.ToPx (atDip.x), m_scaler.ToPx (atDip.y));
    });

    m_treeSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_treeSplitter->SetOnMoved ([this] (int dip)
    {
        m_prefs.treeWidthDip = dip;
        RecomputeLayout();
    });

    m_previewSplitter->SetOrientation (DxuiSplitter::Orientation::Vertical);
    m_previewSplitter->SetOnMoved ([this] (int dip)
    {
        RECT  bounds = m_previewSplitter->GetBounds();
        int   width  = MulDiv (bounds.right - bounds.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());

        m_prefs.previewWidthDip = width - dip - DxuiSplitter::kSashDip;
        RecomputeLayout();
    });

    m_status->SetFields ({ { L"", 0, true }, { L"", 280, false }, { L"", 140, false } });

    m_tabs->SetOnChange ([this] (int index) { SwitchToTab ((size_t) index); });
    m_tabs->SetOnMove   ([this] (int from, int to)
    {
        bool  moved = m_browser.MoveTab ((size_t) from, (size_t) to);

        IGNORE_RETURN_VALUE (moved, true);
    });
    m_tabs->SetOnNewTab ([this]() { Dispatch (CassqueCommands::kNewTab); });

    m_browser.RestoreTabs (m_prefs.tabs);

    m_tree->OnFocusChanged (true);
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AdoptSystemColors
//
//  The Windows themes take the list surface and the accent from the system
//  each time a theme is chosen, which also picks up an accent changed while
//  the window was away.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AdoptSystemColors()
{
    const DxuiWindowsThemeColors::SystemColors &  system = DxuiWindowsThemeColors::Instance().GetSystemColors();



    m_lightTheme.ApplySystemColors (system);
    m_darkTheme.ApplySystemColors  (system);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ApplyTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ApplyTheme()
{
    AdoptSystemColors();

    m_theme = &CassqueShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Casso's own themes lend their chrome colors; the skeuomorphic one has
    //  no scene to draw here, so it is colors only.
    if (IsCassoThemeName (m_prefs.theme))
    {
        m_cassoTheme = CassoTheme::MakeByName (m_prefs.theme);
        m_theme      = &m_cassoTheme;
    }

    SetTheme (m_theme);

    if (m_menuBar != nullptr)
    {
        m_menuBar->SetStripColors    (m_theme->navStrip, m_theme->navHover, m_theme->navItemText);
        m_menuBar->SetDropdownColors (m_theme->dropdownBg, m_theme->dropdownHover, m_theme->dropdownItemText,
                                      m_theme->dropdownAccel, m_theme->panelEdge, m_theme->buttonBorder);
    }

    if (m_toolbar != nullptr)
    {
        m_toolbar->SetStripColors (m_theme->navStrip, m_theme->navItemText);
        m_tooltip.SetTheme (*m_theme);
    }

    if (GetHwnd() != nullptr)
    {
        uint32_t  background = m_theme->Background();
        int       luminance  = (int) (((background >> 16) & 0xFF) * 299 + ((background >> 8) & 0xFF) * 587 + (background & 0xFF) * 114) / 1000;

        DxuiDwm::ApplyImmersiveDarkMode (GetHwnd(), luminance < 128);
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_client = boundsDip;
    m_scaler.SetDpi (scaler.GetDpi());

    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RecomputeLayout
//
//  Menu strip and toolbar across the top, status band across the bottom, and
//  the three panes in what the dock leaves between them. The tree splitter
//  spans the whole body so its limits are measured against it; the preview
//  splitter spans everything right of the tree, with its position the list's
//  width.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RecomputeLayout()
{
    IDxuiControl *  bands[]  = { &m_menuBand, &m_toolbarBand, &m_statusBand, &m_bodyBand };
    RECT            body     = {};
    RECT            right    = {};
    RECT            sashRect = {};
    int             rightDip = 0;
    bool            preview  = m_prefs.previewVisible;



    if (m_menuBar == nullptr || m_client.bottom <= m_client.top)
    {
        return;
    }

    //  The toolbar's band thickness follows the responsive mode it plans for
    //  the width, so plan it before the bands are docked.
    m_toolbar->PlanForWidth (m_client.right - m_client.left, m_scaler);

    m_menuBand.SetThickness    (DxuiMenuBar::GetStripHeightPx (m_scaler.GetDpi()));
    m_toolbarBand.SetThickness (m_scaler.ToPx (m_toolbar->GetBandDp()));
    m_statusBand.SetThickness  (m_scaler.ToPx (DxuiStatusBar::GetBandDp()));

    m_dock.Arrange (m_client, m_scaler, bands);

    body = m_bodyBand.GetBounds();

    if (body.bottom <= body.top)
    {
        return;
    }

    m_menuBar->SetHostClientRect (m_client);
    m_menuBar->Layout (m_menuBand.GetBounds(), m_scaler);

    m_toolbar->SetHostClientRect (m_client);
    m_toolbar->Layout (m_toolbarBand.GetBounds(), m_scaler);

    m_tooltip.SetDpi (m_scaler.GetDpi());
    m_tooltip.SetViewportSize (m_client.right - m_client.left, m_client.bottom - m_client.top);
    m_status->Layout (m_statusBand.GetBounds(), m_scaler);

    m_treeSplitter->Layout (body, m_scaler);
    m_treeSplitter->SetLimitsDip (kMinTreeWidthDip, kMinListWidthDip + (preview ? kMinPreviewWidthDip : 0));
    m_treeSplitter->SetPositionDip (m_prefs.treeWidthDip);

    sashRect = m_treeSplitter->GetSashRect();
    m_tree->Layout (RECT { body.left, body.top, sashRect.left, body.bottom }, m_scaler);

    right    = RECT { sashRect.right, body.top, body.right, body.bottom };

    m_tabs->Layout (RECT { right.left, right.top, right.right, right.top + m_scaler.ToPx (kTabHeightDip) }, m_scaler);
    FillTabs();
    right.top += m_scaler.ToPx (kTabHeightDip);
    rightDip = MulDiv (right.right - right.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());

    m_previewSplitter->SetVisible (preview);
    m_previewList->SetVisible (preview && m_previewList->IsVisible());
    m_hexView->SetVisible (preview && m_hexView->IsVisible());
    m_picture->SetVisible (preview && m_picture->IsVisible());
    m_previewMessage->SetVisible (preview && m_previewMessage->IsVisible());

    if (preview)
    {
        m_previewSplitter->Layout (right, m_scaler);
        m_previewSplitter->SetLimitsDip (kMinListWidthDip, kMinPreviewWidthDip);
        m_previewSplitter->SetPositionDip (rightDip - m_prefs.previewWidthDip - DxuiSplitter::kSashDip);

        sashRect = m_previewSplitter->GetSashRect();

        RECT  listRect    = { right.left, right.top, sashRect.left, right.bottom };
        RECT  previewRect = { sashRect.right, right.top, right.right, right.bottom };

        m_list->Layout           (listRect,    m_scaler);
        m_listMessage->Layout    (listRect,    m_scaler);
        m_previewList->Layout    (previewRect, m_scaler);
        m_hexView->Layout        (previewRect, m_scaler);
        m_picture->Layout        (previewRect, m_scaler);
        m_previewMessage->Layout (previewRect, m_scaler);
    }
    else
    {
        m_list->Layout        (right, m_scaler);
        m_listMessage->Layout (right, m_scaler);
    }

    m_dropHits.Clear();
    m_dropHits.Register (DxuiHitRect { m_list->GetBounds(), DxuiHitSlot::Custom, 0 });

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  bounds = GetBounds();



    painter.FillRect ((float) bounds.left, (float) bounds.top,
                      (float) (bounds.right - bounds.left), (float) (bounds.bottom - bounds.top), theme.Background());

    DxuiWindow::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillList
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillList()
{
    std::vector<std::vector<DxuiListView::Cell>>    rows;
    const BrowserModel                            & model    = m_browser.GetBrowserModel();
    std::wstring                                    message  = m_browser.GetListError();
    Location                                        location = m_browser.GetLocation();



    for (const CatalogRow & row : m_browser.GetRows())
    {
        rows.push_back (CassqueBrowser::ToCells (row, location, &m_shellIcons));
    }

    m_list->SetRows (std::move (rows));
    m_list->SetSelectedRows (m_browser.GetSelectedRows(), m_browser.GetSelectedRows().empty() ? -1 : m_browser.GetSelectedRows()[0]);

    if (model.HasTabs())
    {
        m_list->SetSortIndicator ((int) model.GetActiveTab().sortColumn, model.GetActiveTab().sortDescending);
    }

    if (message.empty() && m_browser.GetRows().empty() && m_browser.GetLocation().kind != Location::Kind::None)
    {
        message = L"This location is empty.";
    }

    m_listMessage->SetText (message);
    m_listMessage->SetVisible (!message.empty());
    m_list->SetVisible (message.empty());

    FillTabs();
    FillPreview();
    FillStatus();
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillPreview
//
//  The preview's kind picks the widget: a picture in the framebuffer view, a
//  catalog as rows under the list's own columns, everything else as lines.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillPreview()
{
    const PreviewContent &                        preview = m_browser.GetPreview();
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    bool                                          visible = m_prefs.previewVisible;
    bool                                          picture = preview.kind == PreviewContent::Kind::Picture && !preview.bgra.empty();
    bool                                          error   = preview.kind == PreviewContent::Kind::Error;
    bool                                          catalog = preview.kind == PreviewContent::Kind::Catalog;
    bool                                          hex     = preview.kind == PreviewContent::Kind::Hex && !preview.bytes.empty();



    if (picture)
    {
        m_picture->SetFramebuffer (preview.bgra.data(), preview.width, preview.height);
    }
    else
    {
        m_picture->Clear();
    }

    if (catalog)
    {
        m_previewList->SetShowHeader (true);
        m_previewList->SetColumns (CassqueBrowser::GetCatalogPreviewColumns());

        for (const CatalogRow & row : preview.rows)
        {
            rows.push_back (CassqueBrowser::ToCatalogPreviewCells (row));
        }
    }
    else
    {
        m_previewList->SetShowHeader (false);
        m_previewList->SetColumns ({ DxuiListView::Column { L"", 0, true } });

        for (const std::wstring & line : preview.lines)
        {
            rows.push_back ({ DxuiListView::Cell { line, false } });
        }
    }

    m_previewList->SetRows (std::move (rows));

    //  The hex view reads the preview's own bytes where they lie. The source
    //  is re-pointed rather than refilled, so a file of any size costs the
    //  same here as a short one.
    m_previewBytes.SetBytes (hex ? &preview.bytes : nullptr);
    m_hexView->SetSource (hex ? &m_previewBytes : nullptr);
    m_hexView->SetOriginAddress (preview.origin);

    m_previewMessage->SetText (error ? preview.message : std::wstring());
    m_previewMessage->SetVisible (visible && error);
    m_picture->SetVisible (visible && picture);
    m_previewList->SetVisible (visible && !picture && !error && !hex);
    m_hexView->SetVisible (visible && hex);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillStatus
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillStatus()
{
    const CassqueBrowser::Status &  status = m_browser.GetStatus();



    m_status->SetText (0, status.selection);
    m_status->SetText (1, status.detail);
    m_status->SetText (2, status.freeSpace);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetFocusPane
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetFocusPane (Pane pane)
{
    if (pane == Pane::Preview && !m_prefs.previewVisible)
    {
        pane = Pane::Tree;
    }

    m_focus = pane;

    m_tree->OnFocusChanged        (pane == Pane::Tree);
    m_list->OnFocusChanged        (pane == Pane::List);
    m_previewList->OnFocusChanged (pane == Pane::Preview);
    m_hexView->OnFocusChanged     (pane == Pane::Preview);
    m_tabs->OnFocusChanged        (pane == Pane::Tabs);
    m_toolbar->SetFocusIndex      (pane == Pane::Toolbar ? m_toolbarFocus : -1);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

FocusStop CassqueWindow::GetFocusStop() const
{
    switch (m_focus)
    {
        case Pane::Toolbar: return FocusStop { FocusStop::Kind::ToolbarEntry, m_toolbarFocus };
        case Pane::Tabs:    return FocusStop { FocusStop::Kind::Tabs };
        case Pane::List:    return FocusStop { FocusStop::Kind::List };
        case Pane::Preview: return FocusStop { FocusStop::Kind::Preview };
        default:            return FocusStop { FocusStop::Kind::Tree };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetFocusStop
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetFocusStop (const FocusStop & stop)
{
    switch (stop.kind)
    {
        case FocusStop::Kind::ToolbarEntry:
            m_toolbarFocus = stop.entry;
            SetFocusPane (Pane::Toolbar);
            break;

        case FocusStop::Kind::Tabs:    SetFocusPane (Pane::Tabs);    break;
        case FocusStop::Kind::List:    SetFocusPane (Pane::List);    break;
        case FocusStop::Kind::Preview: SetFocusPane (Pane::Preview); break;
        default:                       SetFocusPane (Pane::Tree);    break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::BuildFocusStops
//
//  A disabled toolbar button, such as Back with no history, is not a stop,
//  as disabled controls are not tab stops in Windows.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<FocusStop> CassqueWindow::BuildFocusStops() const
{
    std::vector<bool>  enabled;



    for (size_t i = 0; i < CassqueCommands::GetToolbarEntryCount(); i++)
    {
        enabled.push_back (IsEnabled (CassqueCommands::GetToolbarCommandId (i)));
    }

    return FocusRing::BuildStops (enabled, m_prefs.previewVisible);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RouteToolbarKey
//
//  Enter, Space or Down presses the focused button; Left and Right walk the
//  strip's usable buttons and stop at its ends. A picker or a flyout opened
//  from the keyboard owns every key until it closes.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::RouteToolbarKey (const DxuiKeyEvent & ev)
{
    if (m_toolbar->OwnsKeyboard())
    {
        return m_toolbar->HandleKey (ev.vk);
    }

    switch (ev.vk)
    {
        case VK_RETURN:
        case VK_SPACE:
        case VK_DOWN:
            m_toolbar->ActivateFocused();
            return true;

        case VK_LEFT:
        case VK_RIGHT:
            StepToolbarFocus (ev.vk == VK_RIGHT);
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::StepToolbarFocus
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::StepToolbarFocus (bool forward)
{
    int  count = (int) CassqueCommands::GetToolbarEntryCount();
    int  step  = forward ? 1 : -1;
    int  at    = m_toolbarFocus + step;



    while (at >= 0 && at < count)
    {
        if (IsEnabled (CassqueCommands::GetToolbarCommandId ((size_t) at)))
        {
            SetFocusStop (FocusStop { FocusStop::Kind::ToolbarEntry, at });
            return;
        }

        at += step;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ToLocal
//
//  List views take their events relative to their own origin.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMouseEvent CassqueWindow::ToLocal (const DxuiMouseEvent & ev, const RECT & bounds)
{
    DxuiMouseEvent  local = ev;



    local.positionDip.x -= bounds.left;
    local.positionDip.y -= bounds.top;

    return local;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnMouse
//
//  The menu bar and the splitters take first refusal, since both reach over
//  the panes. A press in a pane moves focus to it; a list that is mid-drag
//  keeps every event until the button comes up.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  point = ev.positionDip;
    bool   press = ev.kind == DxuiMouseEventKind::Down;



    if (m_menuBar->OnMouse (ev))
    {
        return true;
    }

    if (RouteToolbarMouse (ev))
    {
        Invalidate();
        return true;
    }

    if (m_treeSplitter->OnMouse (ev) || (m_previewSplitter->IsVisible() && m_previewSplitter->OnMouse (ev)))
    {
        Invalidate();
        return true;
    }

    //  A tab being dragged keeps the pointer until the button comes up, so the
    //  drag can run past the strip's ends and scroll it.
    if (m_tabs->IsInteracting())
    {
        m_tabs->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (Contains (m_tabs->GetBounds(), point) && m_tabs->OnMouse (ev))
    {
        Invalidate();
        return true;
    }

    //  Mouse input goes to a widget mid-drag until the button is released,
    //  wherever the pointer is. Otherwise a scrollbar thumb dragged out of its
    //  pane transfers the drag to the pane under the pointer.
    if (m_list->IsInteracting())
    {
        m_list->OnMouse (ToLocal (ev, m_list->GetBounds()));
        Invalidate();
        return true;
    }

    if (m_tree->IsInteracting())
    {
        m_tree->OnMouse (ToLocal (ev, m_tree->GetBounds()));
        Invalidate();
        return true;
    }

    if (Contains (m_tree->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Tree);
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            DxuiMouseEvent  left = ev;

            left.button = DxuiMouseButton::Left;
            left.kind   = DxuiMouseEventKind::Down;
            m_tree->OnMouse (left);
            left.kind   = DxuiMouseEventKind::Up;
            m_tree->OnMouse (left);

            ShowTreeContextMenu (point.x, point.y, m_tree->GetHighlightedId());
            return true;
        }

        m_tree->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (m_list->IsVisible() && Contains (m_list->GetBounds(), point))
    {
        DxuiMouseEvent  local = ToLocal (ev, m_list->GetBounds());

        if (press)
        {
            SetFocusPane (Pane::List);
        }

        if (press && ev.button == DxuiMouseButton::Right)
        {
            int  row = m_list->HitTestRow (local.positionDip.x, local.positionDip.y);

            if (row >= 0 && !m_list->IsRowSelected (row))
            {
                m_list->ClickRow (row, false, false);
            }

            ShowListContextMenu (point.x, point.y);
            return true;
        }

        if (press && ev.button == DxuiMouseButton::Left)
        {
            m_dragArmed = true;
            m_dragStart = point;
        }
        else if (ev.kind == DxuiMouseEventKind::Up)
        {
            m_dragArmed = false;
        }
        else if (ev.kind == DxuiMouseEventKind::Move && m_dragArmed
              && (abs (point.x - m_dragStart.x) > GetSystemMetrics (SM_CXDRAG)
               || abs (point.y - m_dragStart.y) > GetSystemMetrics (SM_CYDRAG)))
        {
            m_dragArmed = false;
            BeginDragOut();
            return true;
        }

        m_list->OnMouse (local);
        Invalidate();
        return true;
    }

    //  DxuiHexView takes points in the same coordinates as its bounds, so the
    //  event is passed unchanged, not converted to widget-local coordinates as
    //  the older list views require.
    if (m_hexView->IsVisible() && Contains (m_hexView->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_hexView->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (m_previewList->IsVisible() && Contains (m_previewList->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Preview);
        }

        m_previewList->OnMouse (ToLocal (ev, m_previewList->GetBounds()));
        Invalidate();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassqueWindow::GetCursorForPoint (POINT clientPx) const
{
    LPCWSTR  cursor = m_treeSplitter->GetCursorForPoint (clientPx);
    RECT     bounds = m_list->GetBounds();



    if (cursor == nullptr && m_previewSplitter->IsVisible())
    {
        cursor = m_previewSplitter->GetCursorForPoint (clientPx);
    }

    if (cursor == nullptr && m_list->IsVisible() && Contains (bounds, clientPx))
    {
        cursor = m_list->GetCursorForPoint (POINT { clientPx.x - bounds.left, clientPx.y - bounds.top });
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnKey
//
//  Command keys first, then the menu bar's mnemonics and its open dropdown,
//  then Tab between panes, and the rest to the focused pane.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::OnKey (const DxuiKeyEvent & ev)
{
    int   command = 0;
    bool  handled = false;



    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    command = CassqueCommands::TranslateKey (ev.vk, ev.ctrl, ev.alt, ev.shift);

    if (command != 0)
    {
        Dispatch (command);
        return true;
    }

    if (m_menuBar->IsOpen() || m_menuBar->HasFocus())
    {
        return m_menuBar->OnKey (ev);
    }

    if (ev.alt && ev.vk >= 0x20 && ev.vk <= 0x7E && m_menuBar->HandleAltKey ((wchar_t) ev.vk))
    {
        return true;
    }

    if ((ev.vk == VK_APPS || (ev.vk == VK_F10 && ev.shift)) && m_focus == Pane::List)
    {
        RECT  bounds = m_list->GetBounds();

        ShowListContextMenu (bounds.left + m_scaler.ToPx (24), bounds.top + m_scaler.ToPx (40));
        return true;
    }

    if (ev.vk == VK_F2 && m_focus == Pane::List && m_browser.IsImageLocation() && m_browser.GetSelectedRows().size() == 1)
    {
        RunVerb (CassqueActions::Verb::Rename);
        return true;
    }

    if (ev.vk == VK_DELETE && m_focus == Pane::List && m_browser.IsImageLocation() && !m_browser.GetSelectedRows().empty())
    {
        RunVerb (CassqueActions::Verb::Delete);
        return true;
    }

    //  The hex view's two columns are stops of their own inside the preview
    //  pane: Tab moves between them while it has one left, and only then does
    //  the walk carry on to the next pane.
    if (ev.vk == VK_TAB && !ev.ctrl && m_focus == Pane::Preview && IsHexPreviewShowing()
        && m_hexView->OnKey (ev))
    {
        Invalidate();
        return true;
    }

    //  Tab moves through the enabled toolbar buttons, the tab strip, the tree,
    //  the list and the preview, in the order FocusRing defines.
    if (ev.vk == VK_TAB && !ev.ctrl)
    {
        FocusStop  next = FocusRing::GetNext (BuildFocusStops(), GetFocusStop(), !ev.shift);

        SetFocusStop (next);

        //  A pane with stops inside it starts at the one the walk's direction
        //  calls for.
        if (next.kind == FocusStop::Kind::Preview && IsHexPreviewShowing())
        {
            m_hexView->OnFocusEntered (!ev.shift);
        }

        return true;
    }

    switch (m_focus)
    {
        case Pane::Toolbar: handled = RouteToolbarKey (ev);      break;
        case Pane::Tabs:    handled = m_tabs->OnKey (ev);        break;
        case Pane::Tree:    handled = m_tree->OnKey (ev);        break;
        case Pane::List:    handled = m_list->OnKey (ev);        break;
        case Pane::Preview: handled = IsHexPreviewShowing() ? m_hexView->OnKey (ev)
                                                            : m_previewList->OnKey (ev);
                            break;
    }

    if (ev.vk == VK_BACK && !handled)
    {
        Dispatch (CassqueCommands::kUp);
        handled = true;
    }

    Invalidate();

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsEnabled (int id) const
{
    const BrowserModel &  model    = m_browser.GetBrowserModel();
    DxuiStandardCommand   standard = CassqueCommands::GetStandardCommand (id);
    bool                  enabled  = false;



    //  A standard command row is enabled when the focused control reports it
    //  available, and grayed when no control handles it.
    if (standard != DxuiStandardCommand::None)
    {
        return DxuiCommandRouter::Query (GetFocusedControl(), standard, enabled) && enabled;
    }

    switch (id)
    {
        case CassqueCommands::kBack:              return model.HasTabs() && model.CanGoBack();
        case CassqueCommands::kForward:           return model.HasTabs() && model.CanGoForward();
        case CassqueCommands::kUp:                return m_browser.CanGoUp();
        case CassqueCommands::kToggleDisassembly: return model.HasTabs();
        case CassqueCommands::kGoToOffset:
        case CassqueCommands::kGroup1:
        case CassqueCommands::kGroup2:
        case CassqueCommands::kGroup4:
        case CassqueCommands::kGroup8:            return IsHexPreviewShowing();
        case CassqueCommands::kCloseTab:
        case CassqueCommands::kNextTab:
        case CassqueCommands::kPreviousTab:       return model.GetTabCount() > 1;
        default:                                  return true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsChecked
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsChecked (int id) const
{
    const BrowserModel &  model = m_browser.GetBrowserModel();



    switch (id)
    {
        case CassqueCommands::kTogglePreview:     return m_prefs.previewVisible;
        case CassqueCommands::kToggleDisassembly: return model.HasTabs() && model.GetActiveTab().disassemble;
        case CassqueCommands::kThemeLight:        return m_prefs.theme == CassquePrefs::kThemeLight;
        case CassqueCommands::kThemeDark:         return m_prefs.theme == CassquePrefs::kThemeDark;
        case CassqueCommands::kThemeSystem:       return m_prefs.theme != CassquePrefs::kThemeLight && m_prefs.theme != CassquePrefs::kThemeDark
                                                             && !IsCassoThemeName (m_prefs.theme);
        case CassqueCommands::kThemeSkeuomorphic: return m_prefs.theme == CassquePrefs::kThemeSkeuomorphic;
        case CassqueCommands::kThemeDarkModern:   return m_prefs.theme == CassquePrefs::kThemeDarkModern;
        case CassqueCommands::kThemeRetroTerminal: return m_prefs.theme == CassquePrefs::kThemeRetroTerminal;
        case CassqueCommands::kGroup1:            return m_prefs.hexGrouping == 1;
        case CassqueCommands::kGroup2:            return m_prefs.hexGrouping == 2;
        case CassqueCommands::kGroup4:            return m_prefs.hexGrouping == 4;
        case CassqueCommands::kGroup8:            return m_prefs.hexGrouping == 8;
        case CassqueCommands::kNamingDescriptive: return m_prefs.hostNaming != CassquePrefs::kNamingCiderPress;
        case CassqueCommands::kNamingCiderPress:  return m_prefs.hostNaming == CassquePrefs::kNamingCiderPress;
        default:                                  return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetFocusedControl
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * CassqueWindow::GetFocusedControl() const
{
    switch (m_focus)
    {
        case Pane::Tabs:    return m_tabs;
        case Pane::Tree:    return m_tree;
        case Pane::List:    return m_list;
        case Pane::Preview: return IsHexPreviewShowing() ? (IDxuiControl *) m_hexView
                                                         : (IDxuiControl *) m_previewList;
        default:            return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsHexPreviewShowing
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsHexPreviewShowing() const
{
    return m_prefs.previewVisible && m_hexView->IsVisible();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SetHexGrouping
//
//  A grouping the row cannot divide is refused by the view, and a refused
//  one is not written to the preferences either.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SetHexGrouping (int grouping)
{
    if (!m_hexView->SetGrouping (grouping))
    {
        return;
    }

    m_prefs.hexGrouping = grouping;

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AskForOffset
//
//  Offsets are entered in hex, with or without a dollar sign, as the offset
//  column displays them. When the file has a load address, the value entered
//  is an address, matching the left-hand column.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AskForOffset()
{
    uint64_t      origin = m_hexView->GetOriginAddress();
    uint64_t      caret  = m_hexView->GetCaret();
    std::wstring  text   = std::format (L"${:04X}", (unsigned) (origin + caret));
    Word          typed  = 0;



    if (!IsHexPreviewShowing())
    {
        return;
    }

    for (;;)
    {
        if (!CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Go to Offset",
                                       L"Offset to go to:", text, 8, text))
        {
            return;
        }

        if (CassqueActions::TryParseAddress (text, typed) && ((uint64_t) typed >= origin))
        {
            m_hexView->GoToOffset ((uint64_t) typed - origin);
            Invalidate();
            return;
        }

        ShowMessage (std::format (L"Type an offset from ${:04X} to ${:04X}.",
                                  (unsigned) origin,
                                  (unsigned) (origin + m_hexView->GetSource()->GetByteCount() - 1)).c_str(),
                     MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowHexContextMenu
//
//  Copy, Select all and Go to offset, the commands that apply to a run of
//  bytes. Each item uses the same command object as the Edit menu, so the two
//  menus stay consistent.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowHexContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    static constexpr int  kIds[] = { (int) CassqueCommands::kCopy,
                                     (int) CassqueCommands::kSelectAll,
                                     kSeparatorId,
                                     (int) CassqueCommands::kGoToOffset };

    for (int id : kIds)
    {
        const DxuiCommand *  command = (id == kSeparatorId) ? nullptr : m_commands.Find (id);

        items.push_back ((command != nullptr) ? DxuiPopupMenuItem::ForCommand (command)
                                              : DxuiPopupMenuItem::ForSeparator());
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::Dispatch (int id)
{
    HRESULT  hr      = S_OK;
    bool     refill  = false;
    bool     routed  = DxuiCommandRouter::Invoke (GetFocusedControl(),
                                                  CassqueCommands::GetStandardCommand (id));



    //  A standard command belongs to whatever has focus, so it is routed
    //  rather than decided here.
    if (CassqueCommands::GetStandardCommand (id) != DxuiStandardCommand::None)
    {
        IGNORE_RETURN_VALUE (routed, true);
        Invalidate();
        return;
    }

    switch (id)
    {
        case CassqueCommands::kExit:
            OnWindowClose();
            break;

        case CassqueCommands::kRefresh:
            m_browser.GetBrowserModel().InvalidateAllCatalogs();
            hr     = m_browser.Refresh();
            refill = true;
            break;

        case CassqueCommands::kTogglePreview:
            m_prefs.previewVisible = !m_prefs.previewVisible;
            FillPreview();
            RecomputeLayout();
            break;

        case CassqueCommands::kToggleDisassembly:
            if (m_browser.GetBrowserModel().HasTabs())
            {
                m_browser.SetDisassemble (!m_browser.GetBrowserModel().GetActiveTab().disassemble);
                FillPreview();
            }

            break;

        case CassqueCommands::kThemeLight:         SelectTheme (CassquePrefs::kThemeLight);         break;
        case CassqueCommands::kThemeDark:          SelectTheme (CassquePrefs::kThemeDark);          break;
        case CassqueCommands::kThemeSystem:        SelectTheme (CassquePrefs::kThemeFollowSystem);  break;
        case CassqueCommands::kThemeSkeuomorphic:  SelectTheme (CassquePrefs::kThemeSkeuomorphic);  break;
        case CassqueCommands::kThemeDarkModern:    SelectTheme (CassquePrefs::kThemeDarkModern);    break;
        case CassqueCommands::kThemeRetroTerminal: SelectTheme (CassquePrefs::kThemeRetroTerminal); break;

        case CassqueCommands::kGoToOffset:
            AskForOffset();
            break;

        case CassqueCommands::kGroup1: SetHexGrouping (1); break;
        case CassqueCommands::kGroup2: SetHexGrouping (2); break;
        case CassqueCommands::kGroup4: SetHexGrouping (4); break;
        case CassqueCommands::kGroup8: SetHexGrouping (8); break;

        case CassqueCommands::kNamingDescriptive: m_prefs.hostNaming = CassquePrefs::kNamingDescriptive; break;
        case CassqueCommands::kNamingCiderPress:  m_prefs.hostNaming = CassquePrefs::kNamingCiderPress;  break;

        case CassqueCommands::kBack:    refill = m_browser.GoBack();    break;
        case CassqueCommands::kForward: refill = m_browser.GoForward(); break;
        case CassqueCommands::kUp:      refill = m_browser.GoUp();      break;

        case CassqueCommands::kAbout:
            ShowAbout();
            break;

        case CassqueCommands::kNewTab:
            m_browser.NewTab();
            refill = true;
            break;

        case CassqueCommands::kCloseTab:
            refill = m_browser.CloseTab (m_browser.GetBrowserModel().GetActiveIndex());
            break;

        case CassqueCommands::kNextTab:
        case CassqueCommands::kPreviousTab:
            if (m_browser.GetBrowserModel().GetTabCount() > 1)
            {
                size_t  count = m_browser.GetBrowserModel().GetTabCount();
                size_t  next  = (m_browser.GetBrowserModel().GetActiveIndex() + (id == CassqueCommands::kNextTab ? 1 : count - 1)) % count;

                SwitchToTab (next);
            }

            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    if (refill)
    {
        FillList();
    }

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowAbout
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowAbout()
{
    CassqueAbout::Show (GetHwnd(), m_theme, GetModuleHandleW (nullptr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnWindowClose
//
//  The preferences are saved by the shell once the loop ends; closing only
//  records where the window was and ends the loop.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnWindowClose()
{
    m_browser.GetBrowserModel().GetLocations (m_prefs.tabs);
    StorePlacement();
    Hide();
    PostQuitMessage (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetVerbLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassqueWindow::GetVerbLabel (CassqueActions::Verb verb)
{
    switch (verb)
    {
        case CassqueActions::Verb::Open:           return L"&Open";
        case CassqueActions::Verb::Get:            return L"&Copy to folder...";
        case CassqueActions::Verb::Put:            return L"&Put file...";
        case CassqueActions::Verb::Delete:         return L"&Delete";
        case CassqueActions::Verb::Rename:         return L"Re&name...";
        case CassqueActions::Verb::Boot:           return L"Set as &startup program";
        case CassqueActions::Verb::InsertDrive1:   return L"Insert into drive &1";
        case CassqueActions::Verb::InsertDrive2:   return L"Insert into drive &2";
        case CassqueActions::Verb::OpenInNewCasso: return L"Open in &new Casso";
        case CassqueActions::Verb::NewDisk:        return L"New disk &image...";
        case CassqueActions::Verb::Format:         return L"&Format disk image...";
        case CassqueActions::Verb::ReadSectors:    return L"Read &sectors to file...";
        case CassqueActions::Verb::WriteSectors:   return L"&Write sectors from file...";
        case CassqueActions::Verb::ReadBlocks:     return L"Read &blocks to file...";
        case CassqueActions::Verb::WriteBlocks:    return L"Write b&locks from file...";
        case CassqueActions::Verb::Refresh:        return L"&Refresh";
        default:                                   return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowListContextMenu
//
//  The rows come from the actions' verb list; each command runs its verb.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowListContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_menuCommands.clear();
    AskCassoToDescribe();

    for (CassqueActions::Verb verb : m_actions.GetListVerbs())
    {
        std::unique_ptr<DxuiCommand>  command;

        if (verb == CassqueActions::Verb::Refresh && !items.empty())
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        command           = std::make_unique<DxuiCommand>();
        command->id       = (int) verb;
        command->label    = GetVerbLabel (verb);
        command->dispatch = [this, verb]() { RunVerb (verb); };

        //  A machine known to have one drive cannot take drive 2; one not
        //  described yet is given the benefit of the doubt.
        if (verb == CassqueActions::Verb::InsertDrive2)
        {
            command->isEnabled = [this]() { return m_cassoDriveCount != 1; };
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (command.get()));
        m_menuCommands.push_back (std::move (command));

        if (verb == CassqueActions::Verb::Format)
        {
            std::vector<DxuiPopupMenuItem>  advanced;
            std::unique_ptr<DxuiCommand>    parent = std::make_unique<DxuiCommand>();

            for (CassqueActions::Verb raw : { CassqueActions::Verb::ReadSectors, CassqueActions::Verb::WriteSectors,
                                              CassqueActions::Verb::ReadBlocks,  CassqueActions::Verb::WriteBlocks })
            {
                std::unique_ptr<DxuiCommand>  child = std::make_unique<DxuiCommand>();

                child->id       = (int) raw;
                child->label    = GetVerbLabel (raw);
                child->dispatch = [this, raw]() { RunRawVerb (raw); };

                advanced.push_back (DxuiPopupMenuItem::ForCommand (child.get()));
                m_menuCommands.push_back (std::move (child));
            }

            parent->label = L"&Advanced";
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent.get(), std::move (advanced)));
            m_menuCommands.push_back (std::move (parent));
        }
    }

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetSelectedImagePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassqueWindow::GetSelectedImagePath() const
{
    std::vector<std::wstring>  paths;



    m_browser.GetSelectedHostPaths (paths);

    return (paths.size() == 1) ? paths[0] : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RunVerb
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RunVerb (CassqueActions::Verb verb)
{
    HRESULT                        hr      = S_OK;
    std::filesystem::path          picked;
    bool                           chosen  = false;
    FileDialogSpec                 spec;
    int                            answer  = 0;
    CassqueActions::Outcome        outcome;
    std::vector<FileEntry>         entries;
    std::wstring                   newName;
    CassqueNewDiskDialog::Outcome  newDisk;
    HostFileNaming::Style    style   = (m_prefs.hostNaming == CassquePrefs::kNamingCiderPress)
                                     ? HostFileNaming::Style::CiderPress : HostFileNaming::Style::Descriptive;



    switch (verb)
    {
        case CassqueActions::Verb::Open:
            if (!m_browser.GetSelectedRows().empty() && m_browser.OpenRow (m_browser.GetSelectedRows()[0]))
            {
                FillList();
            }

            break;

        case CassqueActions::Verb::Get:
            hr = m_dialogs.PickFolder (GetHwnd(), picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                ReportOutcome (m_actions.GetSelected (picked.wstring(), style), L"Copy");
            }

            break;

        case CassqueActions::Verb::Put:
            hr = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, chosen);

            if (SUCCEEDED (hr) && chosen)
            {
                outcome = m_actions.PutFiles ({ picked.wstring() }, MakeAddressPrompt());
                ReportOutcome (outcome, L"Put");
                FillList();
            }

            break;

        case CassqueActions::Verb::Delete:
            answer = DxuiMessageBox (GetHwnd(), m_theme,
                                     std::format (L"Delete {} selected file(s) from this disk image? This cannot be undone.",
                                                  m_browser.GetSelectedRows().size()).c_str(),
                                     L"Delete", MB_YESNO | MB_ICONWARNING);

            if (answer == IDYES)
            {
                ReportOutcome (m_actions.DeleteSelected(), L"Delete");
                FillList();
            }

            break;

        case CassqueActions::Verb::Boot:
            ReportOutcome (m_actions.BootSelected(), L"Set startup program");
            FillList();
            break;

        case CassqueActions::Verb::Rename:
            m_browser.GetSelectedEntries (entries);

            if (entries.size() == 1
             && CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Rename", L"New name:",
                                          TextEncoding::NarrowToWide (entries[0].name), kMaxCatalogName, newName))
            {
                ReportOutcome (m_actions.RenameSelected (newName), L"Rename");
                FillList();
            }

            break;

        case CassqueActions::Verb::InsertDrive1:
        case CassqueActions::Verb::InsertDrive2:
            InsertIntoDrive (GetSelectedImagePath(), verb == CassqueActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassqueActions::Verb::OpenInNewCasso:
            OpenInNewCasso (GetSelectedImagePath());
            break;

        case CassqueActions::Verb::NewDisk:
            newDisk = CassqueNewDiskDialog::Ask (GetHwnd(), m_theme, false);

            if (newDisk.confirmed)
            {
                ReportOutcome (m_actions.CreateImage (m_browser.GetLocation().path, newDisk.fileName, newDisk.request), L"New disk");
                FillList();
            }

            break;

        case CassqueActions::Verb::Format:
            newDisk = CassqueNewDiskDialog::Ask (GetHwnd(), m_theme, true);

            if (newDisk.confirmed)
            {
                answer = DxuiMessageBox (GetHwnd(), m_theme,
                                         (L"Format " + CassqueActions::GetLeafName (m_actions.GetFormatTarget())
                                          + L"? Everything on it will be erased.").c_str(),
                                         L"Format", MB_YESNO | MB_ICONWARNING);

                if (answer == IDYES)
                {
                    ReportOutcome (m_actions.FormatImage (newDisk.request), L"Format");
                    FillList();
                }
            }

            break;

        case CassqueActions::Verb::Refresh:
            Dispatch (CassqueCommands::kRefresh);
            break;

        default:
            break;
    }

    IGNORE_RETURN_VALUE (hr, S_OK);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ReportOutcome
//
//  A failure shows what the runner said; a success is quiet, since the list
//  already shows the result.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ReportOutcome (const CassqueActions::Outcome & outcome, const wchar_t * verbName)
{
    if (outcome.Succeeded())
    {
        m_status->SetText (1, std::format (L"{}: {} file(s)", verbName, outcome.written));
        return;
    }

    ShowMessage (outcome.message.empty() ? std::wstring (verbName) + L" did not complete." : outcome.message, MB_ICONERROR);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowMessage
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowMessage (const std::wstring & text, UINT icon)
{
    int  result = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), CassqueShell::kAppName, MB_OK | icon);



    IGNORE_RETURN_VALUE (result, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::InsertIntoDrive
//
//  The Casso that launched this browser when it is still there, otherwise
//  any running Casso, otherwise a new one with the disk in drive 1. The
//  answer arrives later as a reply message.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::InsertIntoDrive (const std::wstring & imagePath, int drive)
{
    HWND  target = nullptr;
    bool  sent   = false;



    if (imagePath.empty())
    {
        return;
    }

    target = FindCassoTarget();

    if (target == nullptr)
    {
        OpenInNewCasso (imagePath);
        return;
    }

    sent = Win32IntentChannel::SendTo (target, GetHwnd(), Win32IntentChannel::GetMessageId(),
                                       Win32IntentChannel::EncodeInsert (TextEncoding::WideToNarrow (imagePath), drive));

    if (!sent)
    {
        ShowMessage (L"Casso did not answer the request to insert the disk.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OpenInNewCasso
//
//  The folder is recorded here, since the new Casso learns of the disk from
//  its command line rather than from a hand-off it would record itself.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OpenInNewCasso (const std::wstring & imagePath)
{
    wchar_t       module[MAX_PATH] = {};
    DWORD         length           = GetModuleFileNameW (nullptr, module, ARRAYSIZE (module));
    std::wstring  exe;
    HRESULT       hr               = S_OK;



    if (imagePath.empty() || length == 0 || length >= ARRAYSIZE (module))
    {
        return;
    }

    exe = LaunchCommand::GetSiblingPath (std::filesystem::path (module).parent_path().wstring(), LaunchCommand::kCassoExe);

    if (!m_launcher.Exists (exe))
    {
        ShowMessage (LaunchCommand::DescribeMissing (exe), MB_ICONERROR);
        return;
    }

    hr = m_launcher.Launch (exe, LaunchCommand::MakeCassoArguments (imagePath, m_context.titlePrefix));

    if (FAILED (hr))
    {
        ShowMessage (L"Casso could not be started.", MB_ICONERROR);
        return;
    }

    if (m_context.fs != nullptr)
    {
        KnownFolderStore  store (*m_context.fs, m_context.baseDir);

        hr = store.Append (std::filesystem::path (imagePath).parent_path().wstring(), (int64_t) _time64 (nullptr));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnCopyData
//
//  A reply is queued and shown after the send returns: Casso is blocked in
//  its send until this handler does, and a dialog opened here would hold it
//  there until the send timed out.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnCopyData (WPARAM sender, LPARAM data)
{
    const COPYDATASTRUCT *     copy  = (const COPYDATASTRUCT *) data;
    Win32IntentChannel::Reply  reply;
    bool                       ours  = copy != nullptr && copy->dwData == Win32IntentChannel::GetReplyMessageId();



    UNREFERENCED_PARAMETER (sender);

    if (!ours || !Win32IntentChannel::DecodeReply ((const Byte *) copy->lpData, copy->cbData, reply))
    {
        return DxuiMessageResult::NotHandled;
    }

    m_pendingReplies.push_back (reply);
    PostMessageW (GetHwnd(), kReplyMessage, 0, 0);

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnAppMessage
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    std::vector<Win32IntentChannel::Reply>  replies;



    UNREFERENCED_PARAMETER (wParam);
    UNREFERENCED_PARAMETER (lParam);

    if (msg != kReplyMessage)
    {
        return DxuiMessageResult::NotHandled;
    }

    replies.swap (m_pendingReplies);

    for (const Win32IntentChannel::Reply & reply : replies)
    {
        std::wstring  text = TextEncoding::NarrowToWide (reply.text);

        switch (reply.kind)
        {
            case Win32IntentChannel::ReplyKind::MachineDescription:
                m_cassoDriveCount = reply.driveCount;
                break;

            case Win32IntentChannel::ReplyKind::InsertDone:
                m_status->SetText (1, L"Inserted into Casso");
                break;

            case Win32IntentChannel::ReplyKind::InsertRefused:
                ShowMessage (L"Casso did not insert the disk.\n\n" + text, MB_ICONWARNING);
                break;

            case Win32IntentChannel::ReplyKind::ReloadConflict:
            case Win32IntentChannel::ReplyKind::ReloadRefused:
                ShowMessage (L"Casso did not reload the changed disk.\n\n" + text, MB_ICONWARNING);
                break;

            default:
                break;
        }
    }

    Invalidate();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowTreeContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowTreeContextMenu (int x, int y, const std::wstring & id)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::wstring                    folder;
    std::unique_ptr<DxuiCommand>    command;



    m_menuCommands.clear();

    if (!m_browser.TryGetNodePath (id, folder))
    {
        return;
    }

    command = std::make_unique<DxuiCommand>();

    if (m_browser.CanRemoveFromCasso (id))
    {
        command->label    = L"&Remove from Casso";
        command->dispatch = [this, folder]() { ChangeKnownFolder (folder, false); };
    }
    else if (m_browser.CanAddToCasso (id))
    {
        command->label    = L"&Add to Casso";
        command->dispatch = [this, folder]() { ChangeKnownFolder (folder, true); };
    }
    else
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForCommand (command.get()));
    m_menuCommands.push_back (std::move (command));

    DxuiContextMenu::Show (*GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ChangeKnownFolder
//
//  The shared file is changed under its lock, then read back whole, so a
//  folder Casso recorded in the meantime shows too.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ChangeKnownFolder (const std::wstring & folder, bool add)
{
    HRESULT                               hr = S_OK;
    std::vector<KnownFolderStore::Entry>  entries;
    std::vector<std::wstring>             folders;



    if (m_context.fs == nullptr)
    {
        return;
    }

    KnownFolderStore  store (*m_context.fs, m_context.baseDir);

    hr = add ? store.Append (folder, (int64_t) _time64 (nullptr)) : store.Remove (folder);

    if (FAILED (hr))
    {
        ShowMessage (L"Casso's folder list could not be changed.", MB_ICONERROR);
        return;
    }

    hr = store.Load (entries);
    IGNORE_RETURN_VALUE (hr, S_OK);

    folders = KnownFolderStore::ListRootFolders (*m_context.fs, m_context.baseDir, entries);

    m_browser.GetTreeModel().SetKnownFolders (folders);
    RebuildTree();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RebuildTree
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RebuildTree()
{
    std::vector<DxuiTreeNode>  roots;



    m_browser.GetTreeModel().InvalidateAll();
    m_browser.GetTreeRoots (roots);
    m_tree->SetNodes (std::move (roots));
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnActivateApp
//
//  Coming back to the window rereads what it shows, since another window,
//  Casso or Explorer may have changed it meanwhile. The selection is kept by
//  name.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnActivateApp (bool active)
{
    HRESULT  hr = S_OK;



    //  Follow system has no settings-change message to wait for here, so the
    //  Windows mode is read again whenever the window comes back.
    if (active && m_menuBar != nullptr && !IsChecked (CassqueCommands::kThemeLight) && !IsChecked (CassqueCommands::kThemeDark))
    {
        DxuiWindowsThemeColors::Instance().Refresh();
        ApplyTheme();
    }

    if (active && m_list != nullptr && m_browser.GetBrowserModel().HasTabs())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        FillList();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FillTabs
//
//  Tabs from the left of the strip, labeled by where each tab is. They share
//  the strip's width less the + button, no wider than a full tab and no
//  narrower than the minimum; past that the strip scrolls.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::FillTabs()
{
    const BrowserModel &            model  = m_browser.GetBrowserModel();
    RECT                            strip  = m_tabs->GetBounds();
    int                             width  = m_scaler.ToPx (kTabWidthDip);
    int                             count  = (int) model.GetTabCount();
    std::vector<DxuiTabStrip::Tab>  tabs;
    size_t                          index  = 0;



    if (count > 0)
    {
        width = std::clamp (((int) (strip.right - strip.left) - m_scaler.ToPx (DxuiTabStrip::kNewTabWidthDip)) / count, m_scaler.ToPx (kTabMinWidthDip), width);
    }

    for (index = 0; index < model.GetTabCount(); index++)
    {
        DxuiTabStrip::Tab  tab;

        tab.label = m_browser.GetTabLabel (index);
        tab.rect  = RECT { strip.left + (int) index * width, strip.top, strip.left + (int) (index + 1) * width, strip.bottom };

        tabs.push_back (tab);
    }

    m_tabs->SetTabs (std::move (tabs));
    m_tabs->SetSelected ((int) model.GetActiveIndex());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SwitchToTab
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SwitchToTab (size_t index)
{
    if (m_browser.SwitchTab (index))
    {
        FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::BeginDragOut
//
//  The drag runs its own loop until the drop or the cancel, so the list is
//  told the button came up afterwards; it saw the press and never the
//  release.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::BeginDragOut()
{
    HRESULT                                  hr      = S_OK;
    DWORD                                    effect  = DROPEFFECT_NONE;
    DxuiMouseEvent                           release;
    HostFileNaming::Style                    style   = (m_prefs.hostNaming == CassquePrefs::kNamingCiderPress)
                                                     ? HostFileNaming::Style::CiderPress : HostFileNaming::Style::Descriptive;
    std::vector<DxuiDragDropSource::Format>  formats = CassqueDragOut::BuildFormats (m_browser, style);



    release.kind   = DxuiMouseEventKind::Up;
    release.button = DxuiMouseButton::Left;
    m_list->OnMouse (release);

    if (formats.empty())
    {
        return;
    }

    hr = DxuiDragDropSource::Begin (std::move (formats), DROPEFFECT_COPY, effect);
    IGNORE_RETURN_VALUE (hr, S_OK);

    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnDropFile
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::OnDropFile (const std::wstring & path)
{
    ReportOutcome (m_actions.PutFiles ({ path }, MakeAddressPrompt()), L"Put");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::SelectTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::SelectTheme (const char * name)
{
    m_prefs.theme = name;
    ApplyTheme();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::IsCassoThemeName
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::IsCassoThemeName (const std::string & name)
{
    return name == CassquePrefs::kThemeSkeuomorphic
        || name == CassquePrefs::kThemeDarkModern
        || name == CassquePrefs::kThemeRetroTerminal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::MakeAddressPrompt
//
//  Asks for a binary's load address, prefilled with the content's
//  suggestion, and asks again after an address that does not parse.
//
////////////////////////////////////////////////////////////////////////////////

CassqueActions::AddressFn CassqueWindow::MakeAddressPrompt()
{
    return [this] (const std::wstring & hostName, Word suggested, Word & outAddress)
    {
        std::wstring  text = std::format (L"${:04X}", suggested);

        for (;;)
        {
            if (!CassquePromptDialog::Ask (GetHwnd(), m_theme, L"Load Address",
                                           hostName + L" is a binary. Load address:", text, 8, text))
            {
                return false;
            }

            if (CassqueActions::TryParseAddress (text, outAddress))
            {
                return true;
            }

            ShowMessage (L"Type an address from $0000 to $FFFF.", MB_ICONWARNING);
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::FindCassoTarget
//
//  The Casso that launched this browser while it is still there, otherwise
//  any running Casso, otherwise none.
//
////////////////////////////////////////////////////////////////////////////////

HWND CassqueWindow::FindCassoTarget() const
{
    if (m_context.owner != nullptr && IsWindow (m_context.owner))
    {
        return m_context.owner;
    }

    return FindWindowExW (nullptr, nullptr, Win32IntentChannel::kWindowClass, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::AskCassoToDescribe
//
//  The answer arrives as a reply message and updates the drive count the
//  menus read. With no Casso running, a new one opens with the default
//  machine, which has two drives.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::AskCassoToDescribe()
{
    HWND  target = FindCassoTarget();
    bool  sent   = false;



    if (target == nullptr)
    {
        m_cassoDriveCount = Win32IntentChannel::kMaxDriveCount;
        return;
    }

    sent = Win32IntentChannel::SendTo (target, GetHwnd(), Win32IntentChannel::GetMessageId(), Win32IntentChannel::EncodeDescribe());
    IGNORE_RETURN_VALUE (sent, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RunRawVerb
//
//  A read asks where the bytes start and how many, then where to save them;
//  a write asks for the file, then where it goes, then confirms, since it
//  overwrites whatever is there with no file system to stop it.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RunRawVerb (CassqueActions::Verb verb)
{
    HRESULT                  hr       = S_OK;
    bool                     sectors  = verb == CassqueActions::Verb::ReadSectors || verb == CassqueActions::Verb::WriteSectors;
    bool                     reading  = verb == CassqueActions::Verb::ReadSectors || verb == CassqueActions::Verb::ReadBlocks;
    std::wstring             text     = sectors ? L"17 0 1" : L"2 1";
    std::wstring             prompt;
    std::vector<int>         numbers;
    std::filesystem::path    picked;
    bool                     chosen   = false;
    FileDialogSpec           spec;
    int                      answer   = 0;
    CassqueActions::Outcome  outcome;



    if (m_actions.GetFormatTarget().empty())
    {
        return;
    }

    if (!reading)
    {
        hr = m_dialogs.PickFileToOpen (GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }

        text = sectors ? L"17 0" : L"2";
    }

    prompt = sectors ? (reading ? L"Track, sector and count:" : L"Track and sector to write at:")
                     : (reading ? L"Block and count:"         : L"Block to write at:");

    do
    {
        if (!CassquePromptDialog::Ask (GetHwnd(), m_theme, GetVerbLabel (verb), prompt, text, 16, text))
        {
            return;
        }
    }
    while (!CassqueActions::TryParseNumbers (text, sectors ? 2 : 1, numbers));

    numbers.resize (3, 1);

    if (reading)
    {
        spec.defaultFileName  = sectors ? L"sectors.bin" : L"blocks.bin";
        spec.defaultExtension = L"bin";

        hr = m_dialogs.PickFileToSave (GetHwnd(), spec, picked, chosen);

        if (FAILED (hr) || !chosen)
        {
            return;
        }

        outcome = sectors ? m_actions.ReadSectors (numbers[0], numbers[1], numbers[2], picked.wstring())
                          : m_actions.ReadBlocks  (numbers[0], numbers[1], picked.wstring());

        ReportOutcome (outcome, reading ? L"Read" : L"Write");
        return;
    }

    answer = DxuiMessageBox (GetHwnd(), m_theme,
                             L"Write the file's bytes straight onto the disk image? What is there now is overwritten.",
                             GetVerbLabel (verb), MB_YESNO | MB_ICONWARNING);

    if (answer != IDYES)
    {
        return;
    }

    outcome = sectors ? m_actions.WriteSectors (numbers[0], numbers[1], picked.wstring())
                      : m_actions.WriteBlocks  (numbers[0], picked.wstring());

    ReportOutcome (outcome, L"Write");
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t CassqueWindow::GetNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::RouteToolbarMouse
//
//  The toolbar takes its pointer input through its own calls rather than
//  OnMouse. A move always reaches it, so its hover clears when the pointer
//  leaves; it is consumed only over the strip. The tooltip follows the hover.
//
////////////////////////////////////////////////////////////////////////////////

bool CassqueWindow::RouteToolbarMouse (const DxuiMouseEvent & ev)
{
    int              x      = ev.positionDip.x;
    int              y      = ev.positionDip.y;
    bool             took   = false;
    RECT             anchor = {};
    const wchar_t *  tip    = nullptr;
    int64_t          nowMs  = GetNowMs();



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Move:
            took = m_toolbar->OnToolbarMouseMove (x, y);
            tip  = took ? m_toolbar->GetTooltipAt (x, y, anchor) : nullptr;

            if (tip != nullptr && *tip != L'\0')
            {
                m_tooltip.RequestShow (anchor, tip, nowMs);
            }
            else
            {
                m_tooltip.RequestHide (nowMs);
            }

            break;

        case DxuiMouseEventKind::Down:
            took = ev.button == DxuiMouseButton::Left && m_toolbar->OnToolbarLButtonDown (x, y);

            if (took)
            {
                m_tooltip.HideImmediate();
            }

            break;

        case DxuiMouseEventKind::Up:
            took = ev.button == DxuiMouseButton::Left && m_toolbar->OnToolbarLButtonUp (x, y);
            break;

        case DxuiMouseEventKind::Leave:
            m_toolbar->OnToolbarMouseLeave();
            m_tooltip.RequestHide (nowMs);
            break;

        default:
            break;
    }

    return took;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::OnTimer
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassqueWindow::OnTimer (UINT_PTR timerId)
{
    if (timerId != kTooltipTimerId)
    {
        return DxuiMessageResult::NotHandled;
    }

    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (GetNowMs());
    }

    return DxuiMessageResult::Handled;
}
