#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Model/FolderOptions.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "Seams/IHostDialogs.h"
#include "Seams/IInfoTips.h"
#include "Seams/IShellIcons.h"
#include "Seams/IShellItemVerbs.h"
#include "Seams/Win32IntentChannel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerServices
//
//  The Windows services a Casso Explorer window uses: the Win32
//  implementations in the app, fakes in a test.
//
////////////////////////////////////////////////////////////////////////////////

struct ExplorerServices
{
    std::unique_ptr<IHostDialogs>     dialogs;
    std::unique_ptr<IShellIcons>      shellIcons;
    std::unique_ptr<IInfoTips>        infoTips;
    std::unique_ptr<IShellItemVerbs>  shellVerbs;

    static ExplorerServices  CreateWin32 ();
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts
//
//  The Casso Explorer window's services, its ExplorerContext and its
//  components, created with the window and destroyed with it. The window's
//  creation, its private messages and its timers run here, so a test runs
//  them with no window.
//
////////////////////////////////////////////////////////////////////////////////

struct ExplorerWindowParts
{
    ExplorerWindowParts (IExplorerWindow             & window,
                         CassoExplorerBrowser        & browser,
                         CassoExplorerActions        & actions,
                         CassoExplorerPrefs          & prefs,
                         CassoExplorerWindowContext  & app,
                         DxuiDpiScaler               & scaler,
                         const DxuiTheme *           & theme,
                         ExplorerServices              injected);

    void  Prepare          ();
    void  CreateControls   ();
    void  Configure        ();
    bool  HandleAppMessage (UINT msg, WPARAM wParam, LPARAM lParam);
    bool  HandleTimer      (UINT_PTR timerId);

    ExplorerServices                        services;
    FolderOptions                           explorerOptions;
    std::vector<Win32IntentChannel::Reply>  pendingReplies;
    CassoExplorerCommands                   commands;
    ExplorerContext                         context;
    ExplorerLayout                          layout           { context };
    ExplorerListPane                        listPane         { context };
    ExplorerFolderSync                      folderSync       { context };
    CassoLinkManager                        cassoLinkManager { context };
    ExplorerDragDrop                        dragDrop         { context };
    ExplorerTabManager                      tabManager       { context };
    ExplorerPreviewManager                  preview          { context };
    ExplorerAddressBar                      addressBar       { context };
    ExplorerMenus                           menus            { context };
    ExplorerCommandManager                  commandManager   { context };
    ExplorerFileOps                         fileOps          { context };
    ExplorerInput                           input            { context };
    ExplorerTips                            tips             { context };
    ExplorerThemes                          themes           { context };
    ExplorerStatusBar                       statusBar        { context };
    ExplorerToolbars                        toolbars         { context };
    ExplorerTreePane                        treePane         { context };
};
