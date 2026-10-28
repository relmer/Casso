#include "Pch.h"

#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "Net/FirewallText.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Core/TextEncoding.h"
#include "resource.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/Model/IFolderWatcher.h"
#include "CassoExplorer/Model/FolderWatch.h"

#pragma comment(lib, "winmm.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::CassoExplorerWindow
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow::CassoExplorerWindow (CassoExplorerBrowser & browser, CassoExplorerActions & actions, CassoExplorerPrefs & prefs, Context context)
    : m_browser  (browser),
      m_actions  (actions),
      m_prefs    (prefs),
      m_context  (std::move (context)),
      m_commands (CassoExplorerCommands::Handlers {
                      [this] (int id)       { m_dispatch.Dispatch (id); },
                      [this] (int id)       { return m_dispatch.IsEnabled (id); },
                      [this] (int id)       { return m_dispatch.IsChecked (id); },
                      [this] (int id)       { return m_dispatch.GetCommandLabel (id); } })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::~CassoExplorerWindow
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerWindow::~CassoExplorerWindow()
{
    if (m_context.running != nullptr && m_context.watcher != nullptr)
    {
        m_context.watcher->Unwatch (m_context.running->GetFolder());
    }

    //  The link that was this window's location can now be stopped once no other window has it as theirs.
    if (m_context.links != nullptr)
    {
        m_context.links->SetViewing (this, std::string());
    }


    m_shellVerbs.UnwatchRecycleBin();
    m_dragDrop.m_dropTarget.Shutdown();
    m_treePane.HideLabelTip();
    DestroyBackend();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Open
//
//  The class name is the one a second launch looks for, so it is fixed. A
//  remembered placement still on a monitor is applied before the first show.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerWindow::Open (HINSTANCE instance, const std::wstring & title, int showCommand)
{
    HRESULT                           hr     = S_OK;
    DxuiWindow::CreateParams          params;
    WINDOWPLACEMENT                   place  = { sizeof (place) };
    RECT                              rect   = {};
    BOOL                              result = FALSE;
    CassoExplorerPrefs::Placement     saved  = m_context.request.placement;



    m_layout.AdoptSystemColors();

    m_theme = &CassoExplorerShell::ChooseTheme (m_prefs.theme, DxuiWindowsThemeColors::Instance().IsDarkMode(), m_lightTheme, m_darkTheme);

    //  Before the first layout, which the window's creation can bring on. The
    //  tabs run across the top, above the bars each tab's location drives, as
    //  a browser's do.
    m_dock.SetDock (m_tabBand,     DxuiDock::Top);
    m_dock.SetDock (m_menuBand,    DxuiDock::Top);
    m_dock.SetDock (m_toolbarBand, DxuiDock::Top);
    m_dock.SetDock (m_statusBand,  DxuiDock::Bottom);
    m_dock.SetDock (m_bodyBand,    DxuiDock::Fill);

    params.title                    = title;
    params.hInstance                = instance;
    params.initialSizeDip           = { CassoExplorerShell::kDefaultWidthDip, CassoExplorerShell::kDefaultHeightDip };
    params.minSizeDip               = { kMinWindowWidthDip, 400 };
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.classNameOverride        = CassoExplorerShell::kWindowClass;
    params.appIconBig               = LoadIconW (instance, MAKEINTRESOURCEW (IDI_CASSO_EXPLORER));
    params.appIconSmall             = params.appIconBig;
    params.micaBackdrop             = true;
    params.micaAlt                  = true;

    //  A key or a click paints more than once, and the animation tick paints
    //  on top of that; paced, each of those frames reads the clock one vsync
    //  after the last, so a page slides at one steady speed from its first
    //  frame rather than crawling until the queue fills.
    params.paceFrames               = true;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    //  The caption draws the app's own icon, as Casso's does; the taskbar
    //  already has it from the create parameters. A failure leaves the caption
    //  without one rather than failing the window.
    {
        HICON          captionIcon = (HICON) LoadImageW (instance, MAKEINTRESOURCEW (IDI_CASSO_EXPLORER), IMAGE_ICON,
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

    m_layout.ApplyTheme();

    //  The animation tick starts with the first input; see ArmTick.

    //  Files and folders dropped on the list or the tree go into the image or
    //  directory under the pointer, and entries dragged out of another image
    //  go in with their types. A failure to register leaves the window
    //  without drops, not without a window.
    {
        HRESULT  hrDrop = m_dragDrop.m_dropTarget.Initialize (GetHwnd(), &m_layout.m_dropHits,
                                                              [this] (int, const std::wstring & path) { m_dragDrop.OnDropFile (path); });

        m_dragDrop.m_dropTarget.SetDataHandlers ([this] (IDataObject * data, int tag, POINT screen) { return m_dragDrop.GetDropEffect (data, tag, screen); },
                                                 [this] (IDataObject * data, int tag, POINT screen) { m_dragDrop.OnDrop (data, tag, screen); },
                                  [this]                                          { m_dragDrop.ClearDropTarget(); });

        IGNORE_RETURN_VALUE (hrDrop, S_OK);
    }

    if (saved.valid && saved.w > 0 && saved.h > 0)
    {
        rect = RECT { saved.x, saved.y, saved.x + saved.w, saved.y + saved.h };

        if (MonitorFromRect (&rect, MONITOR_DEFAULTTONULL) != nullptr)
        {
            place.rcNormalPosition = rect;
            place.showCmd          = SW_HIDE;

            //  Twice. Windows counts a window as on the monitor that holds most
            //  of it, and placing it can change which one that is, and so its
            //  DPI -- which scales it by the ratio of the two. The second time
            //  its DPI is already the saved one, so the size is kept as saved.
            result = SetWindowPlacement (GetHwnd(), &place);
            IGNORE_RETURN_VALUE (result, TRUE);
            result = SetWindowPlacement (GetHwnd(), &place);
            IGNORE_RETURN_VALUE (result, TRUE);

            //  A new window steps down and right from the one it was opened
            //  from, as File Explorer's do, rather than opening exactly over it.
            if (m_context.request.cascade)
            {
                int  step = MulDiv (kCascadeStepDip, (int) GetDpiForWindow (GetHwnd()), USER_DEFAULT_SCREEN_DPI);

                OffsetRect (&place.rcNormalPosition, step, step);
                result = SetWindowPlacement (GetHwnd(), &place);
                IGNORE_RETURN_VALUE (result, TRUE);
            }

            if (saved.maximized && showCommand != SW_SHOWMINIMIZED && showCommand != SW_SHOWMINNOACTIVE)
            {
                showCommand = SW_SHOWMAXIMIZED;
            }
        }
    }

    //  A window for a torn-off tab opens where the drop put it, at the size
    //  of the window it came from, and never maximized. Twice, for the DPI,
    //  as above.
    if (m_context.request.hasScreenRect)
    {
        rect = m_context.request.screenRect;

        result = SetWindowPos (GetHwnd(), nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        IGNORE_RETURN_VALUE (result, TRUE);
        result = SetWindowPos (GetHwnd(), nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        IGNORE_RETURN_VALUE (result, TRUE);

        if (showCommand == SW_SHOWMAXIMIZED)
        {
            showCommand = SW_SHOWNORMAL;
        }
    }

    result = ShowWindow (GetHwnd(), showCommand);
    IGNORE_RETURN_VALUE (result, FALSE);

    m_opened = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetPlacement
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerPrefs::Placement CassoExplorerWindow::GetPlacement() const
{
    WINDOWPLACEMENT                place  = { sizeof (place) };
    CassoExplorerPrefs::Placement  placed;



    if (GetHwnd() == nullptr || !GetWindowPlacement (GetHwnd(), &place))
    {
        return placed;
    }

    //  A snapped window's normal position is where it was before the snap,
    //  which is not where it was left. Its own rectangle is, moved into the
    //  work-area terms the normal position uses. A minimized window's
    //  rectangle is its icon's, off the screen, so its normal position stays.
    if (IsWindowArranged (GetHwnd()) && !IsZoomed (GetHwnd()) && !IsIconic (GetHwnd()))
    {
        MONITORINFO  monitor = { sizeof (monitor) };
        RECT         window  = {};

        if (GetWindowRect (GetHwnd(), &window)
            && GetMonitorInfoW (MonitorFromWindow (GetHwnd(), MONITOR_DEFAULTTONEAREST), &monitor))
        {
            OffsetRect (&window, monitor.rcMonitor.left - monitor.rcWork.left, monitor.rcMonitor.top - monitor.rcWork.top);
            place.rcNormalPosition = window;
        }
    }

    placed.x         = place.rcNormalPosition.left;
    placed.y         = place.rcNormalPosition.top;
    placed.w         = place.rcNormalPosition.right - place.rcNormalPosition.left;
    placed.h         = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
    placed.maximized = place.showCmd == SW_SHOWMAXIMIZED
                    || (place.showCmd == SW_SHOWMINIMIZED && (place.flags & WPF_RESTORETOMAXIMIZED) != 0);
    placed.valid     = true;

    return placed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetSessionWindow
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerSessionWindow CassoExplorerWindow::GetSessionWindow() const
{
    const BrowserModel &        model  = m_browser.GetBrowserModel();
    CassoExplorerSessionWindow  window;
    size_t                      i      = 0;



    window.placement = GetPlacement();
    window.activeTab = model.GetActiveIndex();

    //  A tab's cached catalog is read again when the session is restored.
    for (i = 0; i < model.GetTabCount(); i++)
    {
        window.tabs.push_back (model.GetTab (i));
        window.tabs.back().hasCatalog = false;
        window.tabs.back().catalog    = VolumeListing();
    }

    return window;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnCreate
//
//  Children are created in paint order: panes first, then the splitters over
//  their edges, the status bar, and the menu bar last so its strip is on top.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnCreate()
{
    CassoExplorerNamedControl<DxuiListView>         * list            = nullptr;
    CassoExplorerNamedControl<DxuiSplitter>         * treeSplitter    = nullptr;
    CassoExplorerNamedControl<DxuiSplitter>         * previewSplitter = nullptr;



    m_treePane.Create();

    list = CreateChild<CassoExplorerNamedControl<DxuiListView>>();
    list->SetAccessibleName (L"Files");

    m_list            = list;
    m_listMessage     = CreateChild<DxuiLabel> (L"", DxuiTextRole::Muted, DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    m_listAction      = CreateChild<DxuiButton> (L"Bootstrap the Apple");
    m_listAction->SetVisible (false);
    m_listAction->SetOnClick ([this] ()
    {
        std::string  link = m_links.m_listActionLink;

        if (m_listActionFirewall)
        {
            m_links.FixFirewall (false);
            return;
        }

        if (link.empty())
        {
            return;
        }

        if (m_links.m_listActionConnects)
        {
            m_links.ConnectLink (link);
        }
        else
        {
            m_links.BootstrapLink (link);
        }
    });
    m_listActionAlt   = CreateChild<DxuiButton> (FirewallText::kPrivateAction);
    m_listActionAlt->SetVisible (false);
    m_listActionAlt->SetOnClick ([this] () { m_links.FixFirewall (true); });
    m_previewPane.Create();
    treeSplitter      = CreateChild<CassoExplorerNamedControl<DxuiSplitter>>();
    previewSplitter   = CreateChild<CassoExplorerNamedControl<DxuiSplitter>>();

    treeSplitter->SetAccessibleName    (L"Folder pane width");
    previewSplitter->SetAccessibleName (L"Preview pane width");

    m_treeSplitter    = treeSplitter;
    m_previewSplitter = previewSplitter;
    m_status          = CreateChild<DxuiStatusBar>();
    m_tabStrip.Create();
    m_toolbar         = CreateChild<DxuiToolbar>();
    m_addressBar.Create();
    m_menuBar         = CreateChild<DxuiMenuBar>();

    //  Explorer's status bar runs on from the list above it, with no lines.
    m_status->SetDividers (false);
    m_status->SetLeadDip  (kStatusLeadDip);

    m_renameBox       = CreateChild<DxuiTextInput>();

    m_renameBox->SetVisible      (false);
    m_renameBox->SetOverText     (true);
    m_renameBox->SetHwnd         (GetHwnd());
    m_renameBox->SetTextRenderer (GetTextRenderer());

    m_toolbar->SetTextRenderer (GetTextRenderer());
    m_toolbar->SetPopupHost    (GetPopupHost());
    m_toolbar->SetEntries      (m_commands.BuildToolbarEntries());

    m_commandBar = CreateChild<DxuiToolbar>();
    m_commandBar->SetTextRenderer (GetTextRenderer());
    m_commandBar->SetPopupHost    (GetPopupHost());

    //  Explorer's flat buttons, on this bar and the others.
    m_toolbar->SetFlatStyle    (true);
    m_commandBar->SetFlatStyle (true);

    m_commandBar->EnableSeeMore   (s_kpszMdl2More, L"See more", &CassoExplorerIcons::s_kMore);
    m_commandBar->SetEntries      (m_commands.BuildCommandBarEntries());
    m_commandBar->SetIconFace     (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                                   ? DxuiToolbar::kFluentIconFace
                                   : DxuiToolbar::kMdl2IconFace);
    m_commandBar->SetIconDip      (kCommandBarIconDip);
    m_commandBar->SetChevronOnIcons (true);
    m_commandBar->SetGroupSeparators (true);
    m_commandBar->SetButtonPadDip    (kCommandBarPadDip);
    m_commandBar->SetLabeledLeadDip  (kCommandBarLabeledDip);
    m_commandBar->SetContentDropDip  (kCommandBarDropDip);
    m_commandBar->SetChevronGapDip   (kCommandBarChevronGapDip);
    m_commandBar->SetIconGapDip      (kCommandBarIconGapDip);
    m_commandBar->SetLabelFontDip    (kCommandBarLabelDip);
    m_commandBar->SetGroupGapDp      (kCommandBarGroupGapDp);
    m_commandBar->SetBarPadDp        (kCommandBarPadXDp);
    m_menus.SetCommandBarDropDowns();
    m_tooltip.SetPopupHost     (GetPopupHost());
    m_tooltip.SetFollowPointer (true);

    //  The buttons along a context menu's edge have only a word under their
    //  icons, so each has a tip, as Explorer's do.
    GetPopupHost()->GetContextMenu().SetOnIconHover ([this] (const DxuiCommand * command)
    {
        POINT  pointer = {};
        RECT   anchor  = {};

        if (command == nullptr || !GetCursorPos (&pointer) || !ScreenToClient (GetHwnd(), &pointer))
        {
            m_tips.HideHoverTip (ExplorerTips::TipOwner::Menu);
            return;
        }

        anchor = { pointer.x, pointer.y, pointer.x + 1, pointer.y + 1 };
        m_tips.ShowHoverTip (ExplorerTips::TipOwner::Menu, anchor, ExplorerMenus::GetIconButtonTip (*command));
    });

    //  Explorer's navigation glyphs are in Windows 11's Segoe Fluent Icons.
    //  Without that font, use MDL2, which has the same code points; text in a
    //  missing font renders as nothing.
    m_toolbar->SetIconFace (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                            ? DxuiToolbar::kFluentIconFace
                            : DxuiToolbar::kMdl2IconFace);
    m_toolbar->SetIconDip       (kNavIconDip);
    m_toolbar->SetButtonPadDip  (kNavButtonPadDip);
    m_toolbar->SetBarPadDp      (kNavBarPadDp);
    m_toolbar->SetGroupGapDp    (kNavAddressGapDp);

    m_menuBar->SetPopupHost (GetPopupHost());
    m_menuBar->SetTextRendererForMeasure (GetTextRenderer());

    //  Drawn by the shell at the pixel size a row's icon is laid out at, and
    //  handed to the browser before the first nodes and rows are built.
    m_shellIcons.SetSizePx (MulDiv (DxuiTreeView::s_kIconDip, (int) GetDpiForWindow (GetHwnd()), (int) DxuiDpiScaler::kBaseDpi));
    m_browser.SetShellIcons (&m_shellIcons);
    m_browser.SetShellVerbs (&m_shellVerbs);
    m_browser.SetPreviewMonitor (m_previewPane.GetPreviewMonitor(), m_prefs.previewCustomArgb);
    m_explorerOptions = FolderOptions::ReadFromShell();
    m_browser.SetFolderOptions (m_explorerOptions);
    m_treePane.ApplyNavPaneOptions();
    m_listPane.SizeListIcons();
    m_listPane.m_listIcons.LoadInBackground (GetHwnd(), kIconsLoadedMessage);
    m_infoTips.Start (GetHwnd(), kInfoTipMessage);
    m_listPane.m_contentDetails.Start (GetHwnd(), kContentDetailsMessage);

    m_layout.ConfigureWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_client = boundsDip;
    m_scaler.SetDpi (scaler.GetDpi());

    m_layout.RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  bounds = GetBounds();
    LONG  top    = (std::max) (bounds.top, (std::max) (m_tabBand.GetBounds().bottom, m_toolbarBand.GetBounds().bottom));



    //  The tab row and the navigation band are left to the window's Mica,
    //  which the strips over them tint; everything below them is opaque.
    painter.FillRect ((float) bounds.left, (float) top,
                      (float) (bounds.right - bounds.left), (float) (bounds.bottom - top), theme.Background());

    m_previewPane.PaintBackground (painter, theme);

    DxuiWindow::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnThemeChanged
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnThemeChanged()
{
    DxuiWindow::OnThemeChanged();

    //  Applying a theme announces a theme change to the controls, this one
    //  included, which must not apply it again.
    if (m_applyingTheme)
    {
        return;
    }

    m_applyingTheme = true;

    DxuiWindowsThemeColors::Instance().Refresh();
    m_layout.ApplyTheme();
    Invalidate();

    m_applyingTheme = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::Contains (const RECT & rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ToLocal
//
//  List views take their events relative to their own origin.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMouseEvent CassoExplorerWindow::ToLocal (const DxuiMouseEvent & ev, const RECT & bounds)
{
    DxuiMouseEvent  local = ev;



    local.positionDip.x -= bounds.left;
    local.positionDip.y -= bounds.top;

    return local;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowAbout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowAbout()
{
    CassoExplorerAbout::Show (GetHwnd(), m_theme, GetModuleHandleW (nullptr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnWindowClose
//
//  The app saves the session and lets the window go. A window with no app is
//  the process's only one, and closing it ends the loop; the shell saves the
//  preferences then.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnWindowClose()
{
    if (m_tabStrip.IsDragging())
    {
        m_tabStrip.CancelTabDrag();
    }

    RecordSession();

    if (m_context.host != nullptr)
    {
        m_context.host->CloseWindow (*this);
        return;
    }

    Hide();
    PostQuitMessage (0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::CloseForApp
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::CloseForApp()
{
    if (m_tabStrip.IsDragging())
    {
        m_tabStrip.CancelTabDrag();
    }

    RecordSession();
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnEndSession
//
//  Windows is signing out or shutting down: every window is saved as it is,
//  for the next sign-in to open again.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnEndSession (bool ending)
{
    if (ending && m_context.host != nullptr)
    {
        m_context.host->ExitAll();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::SaveSession
//
//  Saved as each change is made, so a window that never closes cleanly -- a
//  crash, a process ended from outside, Windows signing out -- still opens
//  where it was left.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::SaveSession()
{
    HRESULT  hr = S_OK;



    RecordSession();

    if (m_context.host != nullptr)
    {
        m_context.host->SaveSession();
        return;
    }

    if (m_context.fs != nullptr)
    {
        hr = m_prefs.Save (m_context.baseDir, *m_context.fs);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::RecordSession
//
//  With an app, the tabs and the placement are in the session; for a window
//  with no app they are in the preferences.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::RecordSession()
{
    if (m_context.host == nullptr)
    {
        m_browser.GetBrowserModel().GetLocations (m_prefs.tabs);
        m_prefs.placement = GetPlacement();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnEnterSizeMove
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnEnterSizeMove()
{
    m_inSizeMove = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnExitSizeMove
//
//  The end of a move or resize the user dragged. A drag that snaps the window
//  resizes it after this, and OnSize saves that.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnExitSizeMove()
{
    m_inSizeMove = false;
    SaveSession();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnSize
//
//  A resize outside a drag: maximizing, restoring, or a snap, whether from
//  the keyboard or at the end of a drag. One during a drag waits for its end,
//  and one while the window opens is where it was saved already.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnSize (UINT widthPx, UINT heightPx)
{
    UNREFERENCED_PARAMETER (widthPx);
    UNREFERENCED_PARAMETER (heightPx);

    //  Minimizing is no place to reopen at.
    if (m_opened && !m_inSizeMove && !IsIconic (GetHwnd()))
    {
        SaveSession();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ShowMessage
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ShowMessage (const std::wstring & text, UINT icon)
{
    int  result = DxuiMessageBox (GetHwnd(), m_theme, text.c_str(), CassoExplorerShell::kAppName, MB_OK | icon);



    IGNORE_RETURN_VALUE (result, IDOK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnCopyData
//
//  A reply is queued and shown after the send returns: Casso is blocked in
//  its send until this handler does, and a dialog opened here would hold it
//  there until the send timed out.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnCopyData (WPARAM sender, LPARAM data)
{
    const COPYDATASTRUCT *     copy  = (const COPYDATASTRUCT *) data;
    Win32IntentChannel::Reply  reply;
    bool                       ours  = copy != nullptr && copy->dwData == Win32IntentChannel::GetReplyMessageId();



    UNREFERENCED_PARAMETER (sender);

    if (copy != nullptr && copy->dwData == kOpenPathCopyId && copy->lpData != nullptr)
    {
        m_tabStrip.OpenPathInNewTab (std::wstring ((const wchar_t *) copy->lpData, copy->cbData / sizeof (wchar_t)));
        m_listPane.FillList();

        return DxuiMessageResult::Handled;
    }

    //  Another launch's arguments, run once its send has returned.
    if (copy != nullptr && copy->dwData == kLaunchCopyId && copy->lpData != nullptr && m_context.host != nullptr)
    {
        m_pendingLaunches.emplace_back ((const wchar_t *) copy->lpData, copy->cbData / sizeof (wchar_t));
        PostMessageW (GetHwnd(), kLaunchMessage, 0, 0);

        return DxuiMessageResult::Handled;
    }

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
//  CassoExplorerWindow::OnAppMessage
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    std::vector<Win32IntentChannel::Reply>  replies;



    if (m_previewPane.OnHelperMessage (msg, wParam, lParam))
    {
        return DxuiMessageResult::Handled;
    }

    if (msg == kLaunchMessage)
    {
        std::vector<std::wstring>  launches = std::move (m_pendingLaunches);

        m_pendingLaunches.clear();

        for (const std::wstring & arguments : launches)
        {
            m_context.host->RunLaunch (arguments);
        }

        return DxuiMessageResult::Handled;
    }

    //  A change arrived. Restarting the timer rather than re-reading now is
    //  what collapses a burst: a copy of a hundred files re-reads once, when
    //  the copying stops. Only for so long, though: a folder that never stops
    //  changing (a log being written, say) would hold off every re-read.
    if (msg == kShellPreviewMessage)
    {
        m_previewPane.OnShellPreviewResult();
        return DxuiMessageResult::Handled;
    }

    if (msg == kFolderChangedMessage)
    {
        ULONGLONG  now = GetTickCount64();

        if (m_folderFirstChangeMs == 0)
        {
            m_folderFirstChangeMs = now;
        }

        if (now - m_folderFirstChangeMs < kFolderMaxSettleMs)
        {
            SetTimer (GetHwnd(), kFolderTimerId, kFolderSettleMs, nullptr);
        }

        return DxuiMessageResult::Handled;
    }

    //  The bin's changes come in bursts, as a folder's do, and matter only
    //  while it is shown.
    if (msg == kRecycleBinMessage)
    {
        if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            SetTimer (GetHwnd(), kRecycleBinTimerId, kFolderSettleMs, nullptr);
        }

        return DxuiMessageResult::Handled;
    }

    //  A shell folder's listing is in: the list shows it if it is still the
    //  folder shown, and the tree fills the node that asked for it.
    if (msg == kShellListedMessage)
    {
        for (const std::wstring & key : m_browser.GetShellListings().TakeArrivedKeys())
        {
            m_listPane.OnShellListed (key);
        }

        for (const std::wstring & key : m_browser.GetHostListings().TakeArrivedKeys())
        {
            m_listPane.OnShellListed (key);
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == kLinkMessage)
    {
        m_links.OnLinkChanged();

        return DxuiMessageResult::Handled;
    }

    if (msg == kRunningMessage)
    {
        m_links.OnRunningChanged();

        return DxuiMessageResult::Handled;
    }

    if (msg == kDropMenuMessage)
    {
        m_dragDrop.ShowDropMenu();

        return DxuiMessageResult::Handled;
    }

    if (msg == kInfoTipMessage)
    {
        m_tips.RefreshListTip();

        return DxuiMessageResult::Handled;
    }

    if (msg == kContentDetailsMessage)
    {
        if (m_listPane.m_contentDetails.TakeArrived())
        {
            m_listPane.RefreshContentDetails();
        }

        return DxuiMessageResult::Handled;
    }

    if (msg == kIconsLoadedMessage)
    {
        if (m_listPane.m_listIcons.TakeLoaded())
        {
            m_listPane.RefreshListIcons();
        }

        return DxuiMessageResult::Handled;
    }

    //  New's file types join the command bar's New once the shell lists them.
    if (msg == kNewChoicesMessage)
    {
        m_menus.SetCommandBarDropDowns();

        return DxuiMessageResult::Handled;
    }

    if (msg == kRunCommandMessage)
    {
        if (m_dispatch.IsEnabled ((int) wParam))
        {
            m_dispatch.Dispatch ((int) wParam);
        }

        return DxuiMessageResult::Handled;
    }

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
                m_status->SetText (ExplorerLayout::kStatusDetail, L"Inserted into Casso");
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
//  CassoExplorerWindow::TakeLinkNotifications
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::TakeLinkNotifications()
{
    if (m_context.links != nullptr)
    {
        m_context.links->SetTarget (GetHwnd(), kLinkMessage);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnActivateApp
//
//  Coming back to the window reads again the settings that change without a
//  message to say so: the Windows colors, when the theme follows them, and
//  Explorer's folder options. Each is applied only when it changed.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnActivateApp (bool active)
{
    HRESULT  hr = S_OK;



    //  The address's suggestions go when the app does; they hold no capture
    //  to see the click that took it elsewhere.
    if (!active)
    {
        m_addressBar.CloseSuggestions();

        for (DxuiToolbar * bar : { m_commandBar, m_toolbar, &m_previewPane.GetToolbar() })
        {
            bar->CloseMenu();
        }
    }

    //  Applying a theme repaints the whole window and its title bar, so it is
    //  done only when the colors Windows reports are different.
    if (active && m_menuBar != nullptr && !m_dispatch.IsChecked (CassoExplorerCommands::kThemeLight) && !m_dispatch.IsChecked (CassoExplorerCommands::kThemeDark))
    {
        DxuiWindowsThemeColors &                    colors     = DxuiWindowsThemeColors::Instance();
        bool                                        wasDark    = colors.IsDarkMode();
        DxuiWindowsThemeColors::SystemColors        wasSystem  = colors.GetSystemColors();

        colors.Refresh();

        if (colors.IsDarkMode() != wasDark || colors.GetSystemColors() != wasSystem)
        {
            m_layout.ApplyTheme();
        }
    }

    //  Explorer's folder options send no message either, so they too are read
    //  again, and the folders relisted only when one changed.
    if (active && m_list != nullptr && m_listPane.ReadFolderOptions())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_listPane.FillList();
    }

    //  NO RE-READ HERE. Coming back to the window used to re-enumerate the
    //  folder and rebuild every row, whether or not anything had changed,
    //  which on a folder of a few thousand files is a stall for nothing. The
    //  watcher says what changed instead, and says it as it happens.
    //
    //  Without a watcher -- an unwatchable share, or a host that supplied
    //  none -- the browser shows what it read when it read it, and the next
    //  navigation picks up the rest.
    if (active && m_list != nullptr && m_folderWatch == nullptr && m_browser.GetBrowserModel().HasTabs())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_listPane.FillList();
    }

    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetNowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t CassoExplorerWindow::GetNowMs()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now().time_since_epoch()).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnTimer
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnTimer (UINT_PTR timerId)
{
    bool  barsMoved = false;



    if (timerId == kShellPreviewTimerId)
    {
        m_previewPane.ShowShellPreviewBytes();
        return DxuiMessageResult::Handled;
    }

    //  The Recycle Bin restores on its own time, so the folder is read again
    //  each second for a while after an undo.
    if (timerId == kRestoreTimerId)
    {
        if (--m_fileOps.m_restoreTicks <= 0)
        {
            KillTimer (GetHwnd(), kRestoreTimerId);
        }

        m_folderSync.RefreshAfterHostChange();

        return DxuiMessageResult::Handled;
    }

    if (timerId == kRecycleBinTimerId)
    {
        KillTimer (GetHwnd(), kRecycleBinTimerId);

        if (m_browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            m_folderSync.RefreshAfterHostChange();
        }

        return DxuiMessageResult::Handled;
    }

    if (timerId == kDriveTimerId)
    {
        m_folderSync.CheckDrives();
        m_links.PollFirewall();

        //  An announcement since the last look is listed, on the same poll
        //  as a drive.
        if (m_context.announced != nullptr && m_context.running != nullptr && m_context.links != nullptr
            && m_context.announced->GetChangeCount() != m_links.m_announcedChanges)
        {
            m_links.OnRunningChanged();
        }

        return DxuiMessageResult::Handled;
    }

    if (timerId == kRefreshTimerId)
    {
        KillTimer (GetHwnd(), kRefreshTimerId);
        m_list->SetContentHidden (false);
        m_listPane.FillList();
        Invalidate();

        return DxuiMessageResult::Handled;
    }

    if (timerId == kFolderTimerId)
    {
        KillTimer (GetHwnd(), kFolderTimerId);
        m_folderFirstChangeMs = 0;
        m_folderSync.RefreshChangedFolders();

        return DxuiMessageResult::Handled;
    }

    if (timerId != kTooltipTimerId)
    {
        return DxuiMessageResult::NotHandled;
    }

    if (m_tooltip.WantsTick())
    {
        m_tooltip.Tick (GetNowMs());
    }

    if (m_addressBar.Tick (GetNowMs()))
    {
        Invalidate();
    }

    //  Menus slide open and submenus wait out a delay, both on ticks the host
    //  supplies; Casso supplies them from its frame loop, and this window from
    //  its timer.
    if (m_menuBar->WantsTick() || m_toolbar->WantsTick() || m_commandBar->WantsTick() || m_previewPane.GetToolbar().WantsTick() || GetPopupHost()->GetContextMenu().WantsTick())
    {
        m_menuBar->TickMenus (GetNowMs());
        m_toolbar->TickMenus (GetNowMs());
        m_commandBar->TickMenus (GetNowMs());
        m_previewPane.GetToolbar().TickMenus (GetNowMs());
        GetPopupHost()->GetContextMenu().Tick (GetNowMs());
        Invalidate();
    }

    //  Scrollbars widen and narrow over a few frames as the pointer comes and
    //  goes.
    barsMoved = ((int) m_previewPane.TickScrollbars (GetNowMs())
               | (int) m_list->TickScrollbars (GetNowMs())
               | (int) m_list->IsHeaderSliding (GetNowMs())
               | (int) m_list->IsGroupSliding()
               | (int) m_treePane.TickScrollbars (GetNowMs())) != 0;

    //  Tabs slide apart for a dragged tab and back together after it.
    barsMoved = barsMoved || m_tabStrip.IsSliding();

    if (barsMoved)
    {
        Invalidate();
    }

    //  The search box's caret blinks on the frames this asks for: one each
    //  time it turns on or off, not one every tick.
    if (m_focus == ExplorerPane::Search || m_focus == ExplorerPane::GoTo || m_focus == ExplorerPane::LocationSearch)
    {
        bool  caretOn = (m_focus == ExplorerPane::LocationSearch) ? m_addressBar.IsCaretOn() : m_previewPane.IsCaretOn (m_focus);

        if (caretOn != m_caretOn)
        {
            m_caretOn = caretOn;
            Invalidate();
        }
    }

    //  Nothing left to animate: the tick stops until the next input.
    if (!barsMoved && !IsTickWanted())
    {
        KillTimer (GetHwnd(), kTooltipTimerId);
        m_tickArmed = false;
    }

    //  Not handled, so the host does not repaint after every tick: the work
    //  above repaints only when something moved.
    return DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::ArmTick
//
//  Tooltips, menus, the address chevron, scrollbars and the search caret
//  animate on a display-rate tick. Each animation starts from input, so input
//  starts the tick, and the tick stops itself once nothing wants it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::ArmTick()
{
    if (!m_tickArmed && GetHwnd() != nullptr)
    {
        m_tickArmed = SetTimer (GetHwnd(), kTooltipTimerId, kTooltipTickMs, nullptr) != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsTickWanted
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsTickWanted() const
{
    return m_tooltip.WantsTick() || m_tooltip.IsVisible() || m_addressBar.WantsTick() || m_tabStrip.IsSliding()
        || m_menuBar->IsOpen() || m_toolbar->IsMenuOpen() || m_commandBar->IsMenuOpen() || m_previewPane.GetToolbar().IsMenuOpen()
        || m_menuBar->WantsTick() || m_toolbar->WantsTick() || m_commandBar->WantsTick() || m_previewPane.GetToolbar().WantsTick()
        || GetPopupHost()->GetContextMenu().IsVisible() || GetPopupHost()->GetContextMenu().WantsTick()
        || m_focus == ExplorerPane::Search || m_focus == ExplorerPane::GoTo || m_focus == ExplorerPane::LocationSearch;
}
