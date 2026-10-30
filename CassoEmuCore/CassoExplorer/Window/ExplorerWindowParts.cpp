#include "Pch.h"

#include "CassoExplorer/Window/ExplorerWindowParts.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "Core/AppMessages.h"
#include "Core/AppTimerIds.h"
#include "Core/TextEncoding.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Seams/ILinkAnnouncements.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerServices::CreateWin32
//
////////////////////////////////////////////////////////////////////////////////

ExplorerServices ExplorerServices::CreateWin32()
{
    ExplorerServices  services;



    services.dialogs    = std::make_unique<Win32HostDialogs>();
    services.shellIcons = std::make_unique<Win32ShellIcons>();
    services.infoTips   = std::make_unique<Win32InfoTips>();
    services.shellVerbs = std::make_unique<Win32ShellItemVerbs>();

    return services;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::ExplorerWindowParts
//
//  The context's references to the components bind before the components
//  are constructed; each component only stores the context.
//
////////////////////////////////////////////////////////////////////////////////

ExplorerWindowParts::ExplorerWindowParts (IExplorerWindow             & window,
                                          CassoExplorerBrowser        & browser,
                                          CassoExplorerActions        & actions,
                                          CassoExplorerPrefs          & prefs,
                                          CassoExplorerWindowContext  & app,
                                          DxuiDpiScaler               & scaler,
                                          const DxuiTheme *           & theme,
                                          ExplorerServices              services)
    : services (std::move (services)),
      commands (CassoExplorerCommands::Handlers {
                    [this] (int id)       { commandManager.Dispatch (id); },
                    [this] (int id)       { return commandManager.IsEnabled (id); },
                    [this] (int id)       { return commandManager.IsChecked (id); },
                    [this] (int id)       { return commandManager.GetCommandLabel (id); } }),
      context  { .window           = window,
                 .browser          = browser,
                 .actions          = actions,
                 .prefs            = prefs,
                 .app              = app,
                 .explorerOptions  = explorerOptions,
                 .dialogs          = *this->services.dialogs,
                 .shellIcons       = *this->services.shellIcons,
                 .infoTips         = *this->services.infoTips,
                 .shellVerbs       = *this->services.shellVerbs,
                 .commands         = commands,
                 .scaler           = scaler,
                 .theme            = theme,
                 .layout           = layout,
                 .listPane         = listPane,
                 .folderSync       = folderSync,
                 .cassoLinkManager = cassoLinkManager,
                 .dragDrop         = dragDrop,
                 .tabManager       = tabManager,
                 .preview          = preview,
                 .addressBar       = addressBar,
                 .menus            = menus,
                 .commandManager   = commandManager,
                 .fileOps          = fileOps,
                 .input            = input,
                 .tips             = tips,
                 .themes           = themes,
                 .statusBar        = statusBar,
                 .toolbars         = toolbars,
                 .treePane         = treePane }
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::Prepare
//
//  Before the window is created, whose creation can bring on the first
//  layout: the theme chosen, and the bands docked. The tabs run across the
//  top, above the bars each tab's location drives, as a browser's do.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerWindowParts::Prepare()
{
    themes.ChooseInitial();
    layout.DockBands();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::CreateControls
//
//  The window's controls, in the order they are drawn: each is created as a
//  child of the window, and the later ones are drawn over the earlier.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerWindowParts::CreateControls()
{
    IExplorerWindow                          & window          = context.window;
    CassoExplorerNamedControl<DxuiSplitter>  * treeSplitter    = nullptr;
    CassoExplorerNamedControl<DxuiSplitter>  * previewSplitter = nullptr;



    treePane.Create();
    listPane.Create();
    cassoLinkManager.CreateListActions();
    preview.Create();
    treeSplitter    = window.CreateChild<CassoExplorerNamedControl<DxuiSplitter>>();
    previewSplitter = window.CreateChild<CassoExplorerNamedControl<DxuiSplitter>>();

    treeSplitter->SetAccessibleName    (L"Folder pane width");
    previewSplitter->SetAccessibleName (L"Preview pane width");

    layout.SetSplitters (treeSplitter, previewSplitter);
    statusBar.Create();
    tabManager.Create();
    toolbars.CreateNavBar();
    addressBar.Create();
    toolbars.CreateMenuBar();
    fileOps.Create();
    toolbars.CreateCommandBar();
    preview.CreateToolbar();
    toolbars.Create();

    toolbars.SetCommandBarDropDowns();
    tips.Create (window.GetPopupHost());

    //  The buttons along a context menu's edge have only a word under their
    //  icons, so each has a tip, as Explorer's do.
    if (window.GetPopupHost() != nullptr)
    {
        window.GetPopupHost()->GetContextMenu().SetOnIconHover ([this] (const DxuiCommand * command)
        {
            POINT  pointer = {};
            RECT   anchor  = {};

            if (command == nullptr || !GetCursorPos (&pointer) || !ScreenToClient (context.window.GetHwnd(), &pointer))
            {
                tips.HideHoverTip (ExplorerTips::TipOwner::Menu);
                return;
            }

            anchor = { pointer.x, pointer.y, pointer.x + 1, pointer.y + 1 };
            tips.ShowHoverTip (ExplorerTips::TipOwner::Menu, anchor, ExplorerMenus::GetIconButtonTip (*command));
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::Configure
//
//  The services set up and the controls configured once they all exist. The
//  shell draws a row's icon at the pixel size it is laid out at, and the
//  browser has the icons before the first nodes and rows are built.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerWindowParts::Configure()
{
    HWND  hwnd = context.window.GetHwnd();
    int   dpi  = (hwnd != nullptr) ? (int) GetDpiForWindow (hwnd) : (int) DxuiDpiScaler::kBaseDpi;



    services.shellIcons->SetSizePx (MulDiv (DxuiTreeView::s_kIconDip, dpi, (int) DxuiDpiScaler::kBaseDpi));
    context.browser.SetShellIcons (services.shellIcons.get());
    context.browser.SetShellVerbs (services.shellVerbs.get());
    context.browser.SetPreviewMonitor (preview.GetPicture().GetMonitor(), context.prefs.previewCustomArgb);
    explorerOptions = FolderOptions::ReadFromShell();
    context.browser.SetFolderOptions (explorerOptions);
    treePane.ApplyNavPaneOptions();
    listPane.SizeListIcons();
    listPane.StartLoaders();
    services.infoTips->Start (hwnd, (UINT) AppMessage::ExplorerInfoTip);

    layout.ConfigureWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::HandleAppMessage
//
//  The window's private messages, other than a second launch's arguments.
//  Returns whether the message is one of them.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerWindowParts::HandleAppMessage (UINT msg, WPARAM wParam, LPARAM lParam)
{
    std::vector<Win32IntentChannel::Reply>  replies;



    if (preview.GetShell().HandleHelperMessage (msg, wParam, lParam))
    {
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerShellPreview)
    {
        preview.GetShell().HandleResult();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerFolderChanged)
    {
        folderSync.HandleFolderChanged();
        return true;
    }

    //  The bin's changes come in bursts, as a folder's do, and matter only
    //  while it is shown.
    if (msg == (UINT) AppMessage::ExplorerRecycleBin)
    {
        if (context.browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            SetTimer (context.window.GetHwnd(), (UINT_PTR) AppTimerId::ExplorerRecycleBin, CassoExplorerWindow::kFolderSettleMs, nullptr);
        }

        return true;
    }

    //  A shell folder's listing is in: the list shows it if it is still the
    //  folder shown, and the tree fills the node that asked for it.
    if (msg == (UINT) AppMessage::ExplorerShellListed)
    {
        for (const std::wstring & key : context.browser.GetShellListings().TakeArrivedKeys())
        {
            listPane.HandleShellListed (key);
        }

        for (const std::wstring & key : context.browser.GetHostListings().TakeArrivedKeys())
        {
            listPane.HandleShellListed (key);
        }

        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerCassoLink)
    {
        cassoLinkManager.HandleConnectionChanged();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerRunningCasso)
    {
        cassoLinkManager.HandleRunningChanged();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerDropMenu)
    {
        dragDrop.ShowDropMenu();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerCutPasted)
    {
        fileOps.FinishCut();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerInfoTip)
    {
        listPane.RefreshTip();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerContentDetails)
    {
        listPane.HandleContentDetailsArrived();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerIconsLoaded)
    {
        listPane.HandleIconsLoaded();
        return true;
    }

    //  New's file types join the command bar's New once the shell lists them.
    if (msg == (UINT) AppMessage::ExplorerNewChoices)
    {
        toolbars.SetCommandBarDropDowns();
        return true;
    }

    if (msg == (UINT) AppMessage::ExplorerRunCommand)
    {
        if (commandManager.IsEnabled ((int) wParam))
        {
            commandManager.Dispatch ((int) wParam);
        }

        return true;
    }

    if (msg != (UINT) AppMessage::ExplorerReply)
    {
        return false;
    }

    replies.swap (pendingReplies);

    for (const Win32IntentChannel::Reply & reply : replies)
    {
        std::wstring  text = TextEncoding::NarrowToWide (reply.text);

        switch (reply.kind)
        {
            case Win32IntentChannel::ReplyKind::MachineDescription:
                fileOps.SetCassoDriveCount (reply.driveCount);
                break;

            case Win32IntentChannel::ReplyKind::InsertDone:
                statusBar.SetDetail (L"Inserted into Casso");
                break;

            case Win32IntentChannel::ReplyKind::InsertRefused:
                context.window.ShowMessage (L"Casso did not insert the disk.\n\n" + text, MB_ICONWARNING);
                break;

            case Win32IntentChannel::ReplyKind::ReloadConflict:
            case Win32IntentChannel::ReplyKind::ReloadRefused:
                context.window.ShowMessage (L"Casso did not reload the changed disk.\n\n" + text, MB_ICONWARNING);
                break;

            default:
                break;
        }
    }

    context.window.Invalidate();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts::HandleTimer
//
//  The window's timers, other than the animation tick. Returns whether the
//  timer is one of them.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerWindowParts::HandleTimer (UINT_PTR timerId)
{
    HWND  hwnd = context.window.GetHwnd();



    if (timerId == (UINT_PTR) AppTimerId::ExplorerShellPreview)
    {
        preview.GetShell().ShowBytes();
        return true;
    }

    //  The Recycle Bin restores on its own time, so the folder is read again
    //  each second for a while after an undo.
    if (timerId == (UINT_PTR) AppTimerId::ExplorerRestore)
    {
        if (fileOps.CountDownRestore())
        {
            KillTimer (hwnd, (UINT_PTR) AppTimerId::ExplorerRestore);
        }

        folderSync.RefreshAfterHostChange();
        return true;
    }

    if (timerId == (UINT_PTR) AppTimerId::ExplorerRecycleBin)
    {
        KillTimer (hwnd, (UINT_PTR) AppTimerId::ExplorerRecycleBin);

        if (context.browser.GetLocation().kind == Location::Kind::RecycleBin)
        {
            folderSync.RefreshAfterHostChange();
        }

        return true;
    }

    if (timerId == (UINT_PTR) AppTimerId::ExplorerDrive)
    {
        folderSync.CheckDrives();
        cassoLinkManager.PollFirewall();

        //  An announcement since the last look is listed, on the same poll
        //  as a drive.
        if (context.app.announced != nullptr && context.app.running != nullptr && context.app.links != nullptr
            && context.app.announced->GetChangeCount() != cassoLinkManager.GetAnnouncedChanges())
        {
            cassoLinkManager.HandleRunningChanged();
        }

        return true;
    }

    if (timerId == (UINT_PTR) AppTimerId::ExplorerRefresh)
    {
        KillTimer (hwnd, (UINT_PTR) AppTimerId::ExplorerRefresh);
        listPane.SetContentHidden (false);
        listPane.FillList();
        context.window.Invalidate();
        return true;
    }

    if (timerId == (UINT_PTR) AppTimerId::ExplorerFolder)
    {
        folderSync.HandleSettleTimer();
        return true;
    }

    return false;
}
