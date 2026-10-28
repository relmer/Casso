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
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerWindowParts
//
//  The Casso Explorer window's services, its ExplorerContext and its
//  components, created with the window and destroyed with it.
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
                         const DxuiTheme *           & theme);

    FolderOptions                           explorerOptions;
    Win32HostDialogs                        dialogs;
    Win32ShellIcons                         shellIcons;
    Win32InfoTips                           infoTips;
    Win32ShellItemVerbs                     shellVerbs;
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
