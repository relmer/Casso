#include "Pch.h"

#include "Cassque/CassqueWindow.h"
#include "Cassque/CassqueShell.h"
#include "Theme/DxuiDwm.h"
#include "Theme/DxuiWindowsThemeColors.h"
#include "Window/DxuiMessageBox.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::CassqueWindow
//
////////////////////////////////////////////////////////////////////////////////

CassqueWindow::CassqueWindow (CassqueBrowser & browser, CassquePrefs & prefs)
    : m_browser  (browser),
      m_prefs    (prefs),
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



    m_theme = &CassqueShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

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

    ApplyTheme();

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
    m_tree            = CreateChild<DxuiTreeView>();
    m_list            = CreateChild<DxuiListView>();
    m_listMessage     = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    m_previewList     = CreateChild<DxuiListView>();
    m_picture         = CreateChild<DxuiFramebufferView>();
    m_previewMessage  = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    m_treeSplitter    = CreateChild<DxuiSplitter>();
    m_previewSplitter = CreateChild<DxuiSplitter>();
    m_status          = CreateChild<DxuiStatusBar>();
    m_menuBar         = CreateChild<DxuiMenuBar>();

    m_menuBar->SetPopupHost (GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());

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

    m_tree->OnFocusChanged (true);
    FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ApplyTheme
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ApplyTheme()
{
    m_theme = &CassqueShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    SetTheme (m_theme);

    if (m_menuBar != nullptr)
    {
        m_menuBar->SetStripColors    (m_theme->navStrip, m_theme->navHover, m_theme->navItemText);
        m_menuBar->SetDropdownColors (m_theme->dropdownBg, m_theme->dropdownHover, m_theme->dropdownItemText,
                                      m_theme->dropdownAccel, m_theme->panelEdge, m_theme->buttonBorder);
    }

    if (GetHwnd() != nullptr)
    {
        DxuiDwm::ApplyImmersiveDarkMode (GetHwnd(), m_theme == &m_darkTheme);
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
//  Menu strip across the top, status band across the bottom, and the three
//  panes between them. The tree splitter spans the whole body so its limits
//  are measured against it; the preview splitter spans everything right of
//  the tree, with its position the list's width.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::RecomputeLayout()
{
    int   menuH     = DxuiMenuBar::GetStripHeightPx (m_scaler.GetDpi());
    int   statusH   = m_scaler.ToPx (DxuiStatusBar::GetBandDp());
    RECT  menu      = { m_client.left, m_client.top, m_client.right, m_client.top + menuH };
    RECT  status    = { m_client.left, m_client.bottom - statusH, m_client.right, m_client.bottom };
    RECT  body      = { m_client.left, menu.bottom, m_client.right, status.top };
    RECT  right     = {};
    RECT  sashRect  = {};
    int   rightDip  = 0;
    bool  preview   = m_prefs.previewVisible;



    if (m_menuBar == nullptr || body.bottom <= body.top)
    {
        return;
    }

    m_menuBar->SetHostClientRect (m_client);
    m_menuBar->Layout (menu, m_scaler);
    m_status->Layout (status, m_scaler);

    m_treeSplitter->Layout (body, m_scaler);
    m_treeSplitter->SetLimitsDip (kMinTreeWidthDip, kMinListWidthDip + (preview ? kMinPreviewWidthDip : 0));
    m_treeSplitter->SetPositionDip (m_prefs.treeWidthDip);

    sashRect = m_treeSplitter->GetSashRect();
    m_tree->Layout (RECT { body.left, body.top, sashRect.left, body.bottom }, m_scaler);

    right    = RECT { sashRect.right, body.top, body.right, body.bottom };
    rightDip = MulDiv (right.right - right.left, (int) DxuiDpiScaler::kBaseDpi, (int) m_scaler.GetDpi());

    m_previewSplitter->SetVisible (preview);
    m_previewList->SetVisible (preview && m_previewList->IsVisible());
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
        m_picture->Layout        (previewRect, m_scaler);
        m_previewMessage->Layout (previewRect, m_scaler);
    }
    else
    {
        m_list->Layout        (right, m_scaler);
        m_listMessage->Layout (right, m_scaler);
    }

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
    std::vector<std::vector<DxuiListView::Cell>>  rows;
    const BrowserModel &                          model   = m_browser.GetBrowserModel();
    std::wstring                                  message = m_browser.GetListError();



    for (const CatalogRow & row : m_browser.GetRows())
    {
        rows.push_back (CassqueBrowser::ToCells (row));
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

    m_previewMessage->SetText (error ? preview.message : std::wstring());
    m_previewMessage->SetVisible (visible && error);
    m_picture->SetVisible (visible && picture);
    m_previewList->SetVisible (visible && !picture && !error);

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

    Invalidate();
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

    if (m_treeSplitter->OnMouse (ev) || (m_previewSplitter->IsVisible() && m_previewSplitter->OnMouse (ev)))
    {
        Invalidate();
        return true;
    }

    if (m_list->IsInteracting())
    {
        m_list->OnMouse (ToLocal (ev, m_list->GetBounds()));
        Invalidate();
        return true;
    }

    if (Contains (m_tree->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::Tree);
        }

        m_tree->OnMouse (ev);
        Invalidate();
        return true;
    }

    if (m_list->IsVisible() && Contains (m_list->GetBounds(), point))
    {
        if (press)
        {
            SetFocusPane (Pane::List);
        }

        m_list->OnMouse (ToLocal (ev, m_list->GetBounds()));
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

    if (ev.vk == VK_TAB && !ev.ctrl)
    {
        static constexpr int  kPanes = 3;

        int  next = ((int) m_focus + (ev.shift ? kPanes - 1 : 1)) % kPanes;

        if ((Pane) next == Pane::Preview && !m_prefs.previewVisible)
        {
            next = ((int) next + (ev.shift ? kPanes - 1 : 1)) % kPanes;
        }

        SetFocusPane ((Pane) next);
        return true;
    }

    switch (m_focus)
    {
        case Pane::Tree:    handled = m_tree->OnKey (ev);        break;
        case Pane::List:    handled = m_list->OnKey (ev);        break;
        case Pane::Preview: handled = m_previewList->OnKey (ev); break;
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
    const BrowserModel &  model = m_browser.GetBrowserModel();



    switch (id)
    {
        case CassqueCommands::kBack:              return model.HasTabs() && model.CanGoBack();
        case CassqueCommands::kForward:           return model.HasTabs() && model.CanGoForward();
        case CassqueCommands::kUp:                return m_browser.CanGoUp();
        case CassqueCommands::kToggleDisassembly: return model.HasTabs();
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
        case CassqueCommands::kThemeSystem:       return m_prefs.theme != CassquePrefs::kThemeLight && m_prefs.theme != CassquePrefs::kThemeDark;
        default:                                  return false;
    }
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

        case CassqueCommands::kThemeLight:  m_prefs.theme = CassquePrefs::kThemeLight;        ApplyTheme(); break;
        case CassqueCommands::kThemeDark:   m_prefs.theme = CassquePrefs::kThemeDark;         ApplyTheme(); break;
        case CassqueCommands::kThemeSystem: m_prefs.theme = CassquePrefs::kThemeFollowSystem; ApplyTheme(); break;

        case CassqueCommands::kBack:    refill = m_browser.GoBack();    break;
        case CassqueCommands::kForward: refill = m_browser.GoForward(); break;
        case CassqueCommands::kUp:      refill = m_browser.GoUp();      break;

        case CassqueCommands::kAbout:
            ShowAbout();
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
    int  result = 0;



    result = DxuiMessageBox (GetHwnd(), m_theme,
                             L"Cassque browses Apple II disk images.\n\n"
                             L"The cassowary's casque is the helmet-like crest on its head. "
                             L"Casso is named for the bird; Cassque is its casque, the part that holds what's inside.",
                             L"About Cassque", MB_OK | MB_ICONINFORMATION);
    IGNORE_RETURN_VALUE (result, IDOK);
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
    StorePlacement();
    Hide();
    PostQuitMessage (0);
}
