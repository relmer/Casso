#include "Pch.h"

#include "Cassque/CassqueWindow.h"
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

        m_list->OnMouse (local);
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

    if (ev.vk == VK_DELETE && m_focus == Pane::List && m_browser.IsImageLocation() && !m_browser.GetSelectedRows().empty())
    {
        RunVerb (CassqueActions::Verb::Delete);
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
        case CassqueActions::Verb::Refresh:        return L"&Refresh";
        default:                                   return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow::ShowListContextMenu
//
//  The rows come from the actions' verb list; each command runs its verb.
//  Rename waits on its dialog and is not offered yet.
//
////////////////////////////////////////////////////////////////////////////////

void CassqueWindow::ShowListContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_menuCommands.clear();

    for (CassqueActions::Verb verb : m_actions.GetListVerbs())
    {
        std::unique_ptr<DxuiCommand>  command;

        if (verb == CassqueActions::Verb::Rename)
        {
            continue;
        }

        if (verb == CassqueActions::Verb::Refresh && !items.empty())
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        command           = std::make_unique<DxuiCommand>();
        command->id       = (int) verb;
        command->label    = GetVerbLabel (verb);
        command->dispatch = [this, verb]() { RunVerb (verb); };

        items.push_back (DxuiPopupMenuItem::ForCommand (command.get()));
        m_menuCommands.push_back (std::move (command));
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
    HRESULT                  hr      = S_OK;
    std::filesystem::path    picked;
    bool                     chosen  = false;
    FileDialogSpec           spec;
    int                      answer  = 0;
    CassqueActions::Outcome  outcome;
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
                outcome = m_actions.PutFiles ({ picked.wstring() });
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

        case CassqueActions::Verb::InsertDrive1:
        case CassqueActions::Verb::InsertDrive2:
            InsertIntoDrive (GetSelectedImagePath(), verb == CassqueActions::Verb::InsertDrive1 ? 1 : 2);
            break;

        case CassqueActions::Verb::OpenInNewCasso:
            OpenInNewCasso (GetSelectedImagePath());
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

    if (m_context.owner != nullptr && IsWindow (m_context.owner))
    {
        target = m_context.owner;
    }
    else
    {
        target = FindWindowExW (nullptr, nullptr, Win32IntentChannel::kWindowClass, nullptr);
    }

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
