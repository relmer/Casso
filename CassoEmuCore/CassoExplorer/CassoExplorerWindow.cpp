#include "Pch.h"

#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/ExplorerWindowParts.h"
#include "Net/FirewallText.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Core/TextEncoding.h"
#include "resource.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/Model/IFolderWatcher.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "Core/AppTimerIds.h"
#include "Core/AppMessages.h"

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
      m_parts    (std::make_unique<ExplorerWindowParts> (*this, browser, actions, prefs, m_context, m_scaler, m_theme, ExplorerServices::CreateWin32()))
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
        m_context.links->SetViewing (&m_parts->context, std::string());
    }


    m_parts->services.shellVerbs->UnwatchRecycleBin();
    m_parts->dragDrop.GetDropTarget().Shutdown();
    m_parts->treePane.HideLabelTip();
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



    m_parts->Prepare();

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

    m_parts->themes.Apply();

    //  The animation tick starts with the first input; see ArmTick.

    //  Files and folders dropped on the list or the tree go into the image or
    //  directory under the pointer, and entries dragged out of another image
    //  go in with their types. A failure to register leaves the window
    //  without drops, not without a window.
    {
        HRESULT  hrDrop = m_parts->dragDrop.GetDropTarget().Initialize (GetHwnd(), &m_parts->layout.GetDropHits(),
                                                              [this] (int, const std::wstring & path) { m_parts->dragDrop.HandleDropFile (path); });

        m_parts->dragDrop.GetDropTarget().SetDataHandlers ([this] (IDataObject * data, int tag, POINT screen) { return m_parts->dragDrop.GetDropEffect (data, tag, screen); },
                                                 [this] (IDataObject * data, int tag, POINT screen) { m_parts->dragDrop.HandleDrop (data, tag, screen); },
                                  [this]                                          { m_parts->dragDrop.ClearDropTarget(); });

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
//  The window's placement in the preferences' terms.
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
//  This window as the session writes it: its placement and every tab.
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
    m_parts->CreateControls();
    m_parts->Configure();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);

    m_parts->layout.SetClient (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    m_parts->layout.RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT  bounds = GetBounds();
    LONG  top    = (std::max) (bounds.top, (std::max) (m_parts->layout.GetTabBand().GetBounds().bottom, m_parts->layout.GetToolbarBand().GetBounds().bottom));



    //  The tab row and the navigation band are left to the window's Mica,
    //  which the strips over them tint; everything below them is opaque.
    painter.FillRect ((float) bounds.left, (float) top,
                      (float) (bounds.right - bounds.left), (float) (bounds.bottom - top), theme.Background());

    m_parts->preview.PaintBackground (painter, theme);

    DxuiWindow::Paint (painter, text, theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnThemeChanged
//
//  Windows changed its light or dark setting or its accent: Follow
//  system takes the new one at once, and every theme takes the accent.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::OnThemeChanged()
{
    DxuiWindow::OnThemeChanged();
    m_parts->themes.HandleSystemThemeChanged();
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
    if (m_parts->tabManager.IsDragging())
    {
        m_parts->tabManager.CancelTabDrag();
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
//  Closed by the app, which has saved the session already.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::CloseForApp()
{
    if (m_parts->tabManager.IsDragging())
    {
        m_parts->tabManager.CancelTabDrag();
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
//  Saves the typed paths, the session and the preferences now,
//  rather than only on a clean exit.
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
        m_parts->tabManager.OpenPathInNewTab (std::wstring ((const wchar_t *) copy->lpData, copy->cbData / sizeof (wchar_t)));
        m_parts->listPane.FillList();

        return DxuiMessageResult::Handled;
    }

    //  Another launch's arguments, run once its send has returned.
    if (copy != nullptr && copy->dwData == kLaunchCopyId && copy->lpData != nullptr && m_context.host != nullptr)
    {
        m_pendingLaunches.emplace_back ((const wchar_t *) copy->lpData, copy->cbData / sizeof (wchar_t));
        PostMessageW (GetHwnd(), (UINT) AppMessage::ExplorerLaunch, 0, 0);

        return DxuiMessageResult::Handled;
    }

    if (!ours || !Win32IntentChannel::DecodeReply ((const Byte *) copy->lpData, copy->cbData, reply))
    {
        return DxuiMessageResult::NotHandled;
    }

    m_parts->pendingReplies.push_back (reply);
    PostMessageW (GetHwnd(), (UINT) AppMessage::ExplorerReply, 0, 0);

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnAppMessage
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult CassoExplorerWindow::OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == (UINT) AppMessage::ExplorerLaunch)
    {
        std::vector<std::wstring>  launches = std::move (m_pendingLaunches);

        m_pendingLaunches.clear();

        for (const std::wstring & arguments : launches)
        {
            m_context.host->RunLaunch (arguments);
        }

        return DxuiMessageResult::Handled;
    }

    return m_parts->HandleAppMessage (msg, wParam, lParam) ? DxuiMessageResult::Handled : DxuiMessageResult::NotHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::TakeLinkNotifications
//
//  CassoLink posts its changes to one window; this one, from now on.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerWindow::TakeLinkNotifications()
{
    if (m_context.links != nullptr)
    {
        m_context.links->SetTarget (GetHwnd(), (UINT) AppMessage::ExplorerCassoLink);
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
        m_parts->addressBar.CloseSuggestions();

        m_parts->toolbars.CloseMenus();
    }

    //  Applying a theme repaints the whole window and its title bar, so it is
    //  done only when the colors Windows reports are different.
    if (active && m_parts->toolbars.IsCreated() && !m_parts->commandManager.IsChecked (CassoExplorerCommands::kThemeLight) && !m_parts->commandManager.IsChecked (CassoExplorerCommands::kThemeDark))
    {
        DxuiWindowsThemeColors &                    colors     = DxuiWindowsThemeColors::Instance();
        bool                                        wasDark    = colors.IsDarkMode();
        DxuiWindowsThemeColors::SystemColors        wasSystem  = colors.GetSystemColors();

        colors.Refresh();

        if (colors.IsDarkMode() != wasDark || colors.GetSystemColors() != wasSystem)
        {
            m_parts->themes.Apply();
        }
    }

    //  Explorer's folder options send no message either, so they too are read
    //  again, and the folders relisted only when one changed.
    if (active && m_parts->listPane.GetControl() != nullptr && m_parts->listPane.GetColumns().ReadFolderOptions())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_parts->listPane.FillList();
    }

    //  NO RE-READ HERE. Coming back to the window used to re-enumerate the
    //  folder and rebuild every row, whether or not anything had changed,
    //  which on a folder of a few thousand files is a stall for nothing. The
    //  watcher says what changed instead, and says it as it happens.
    //
    //  Without a watcher -- an unwatchable share, or a host that supplied
    //  none -- the browser shows what it read when it read it, and the next
    //  navigation picks up the rest.
    if (active && m_parts->listPane.GetControl() != nullptr && !m_parts->folderSync.IsWatching() && m_browser.GetBrowserModel().HasTabs())
    {
        hr = m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);
        m_parts->listPane.FillList();
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



    if (m_parts->HandleTimer (timerId))
    {
        return DxuiMessageResult::Handled;
    }

    if (timerId != (UINT_PTR) AppTimerId::ExplorerTooltip)
    {
        return DxuiMessageResult::NotHandled;
    }

    m_parts->tips.Tick (GetNowMs());

    if (m_parts->addressBar.Tick (GetNowMs()))
    {
        Invalidate();
    }

    //  Menus slide open and submenus wait out a delay, both on ticks the host
    //  supplies; Casso supplies them from its frame loop, and this window from
    //  its timer.
    if (m_parts->toolbars.WantsTick() || m_parts->preview.GetToolbar().WantsTick() || GetPopupHost()->GetContextMenu().WantsTick())
    {
        m_parts->toolbars.TickMenus (GetNowMs());
        m_parts->preview.GetToolbar().TickMenus (GetNowMs());
        GetPopupHost()->GetContextMenu().Tick (GetNowMs());
        Invalidate();
    }

    //  Scrollbars widen and narrow over a few frames as the pointer comes and
    //  goes.
    barsMoved = ((int) m_parts->preview.TickScrollbars (GetNowMs())
               | (int) m_parts->listPane.Tick (GetNowMs())
               | (int) m_parts->treePane.TickScrollbars (GetNowMs())) != 0;

    //  Tabs slide apart for a dragged tab and back together after it.
    barsMoved = barsMoved || m_parts->tabManager.IsSliding();

    if (barsMoved)
    {
        Invalidate();
    }

    //  The search box's caret blinks on the frames this asks for: one each
    //  time it turns on or off, not one every tick.
    if (m_parts->input.HasCaretFocus())
    {
        ExplorerPane  focus   = m_parts->input.GetFocusPane();
        bool          caretOn = (focus == ExplorerPane::LocationSearch) ? m_parts->addressBar.IsCaretOn() : m_parts->preview.GetFind().IsCaretOn (focus);

        if (caretOn != m_caretOn)
        {
            m_caretOn = caretOn;
            Invalidate();
        }
    }

    //  Nothing left to animate: the tick stops until the next input.
    if (!barsMoved && !IsTickWanted())
    {
        KillTimer (GetHwnd(), (UINT_PTR) AppTimerId::ExplorerTooltip);
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
        m_tickArmed = SetTimer (GetHwnd(), (UINT_PTR) AppTimerId::ExplorerTooltip, kTooltipTickMs, nullptr) != 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::IsTickWanted
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::IsTickWanted() const
{
    return m_parts->tips.WantsTick() || m_parts->addressBar.WantsTick() || m_parts->tabManager.IsSliding()
        || m_parts->toolbars.IsMenuOpen() || m_parts->preview.GetToolbar().IsMenuOpen()
        || m_parts->toolbars.WantsTick() || m_parts->preview.GetToolbar().WantsTick()
        || GetPopupHost()->GetContextMenu().IsVisible() || GetPopupHost()->GetContextMenu().WantsTick()
        || m_parts->input.HasCaretFocus();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetTabManager
//
////////////////////////////////////////////////////////////////////////////////

ExplorerTabManager & CassoExplorerWindow::GetTabManager()
{
    return m_parts->tabManager;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetCassoLinkManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkManager & CassoExplorerWindow::GetCassoLinkManager()
{
    return m_parts->cassoLinkManager;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassoExplorerWindow::GetCursorForPoint (POINT clientPx) const
{
    return m_parts->input.GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::OnMouse (const DxuiMouseEvent & ev)
{
    return m_parts->input.HandleMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerWindow::OnKey (const DxuiKeyEvent & ev)
{
    return m_parts->input.HandleKey (ev);
}
