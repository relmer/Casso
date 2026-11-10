#pragma once

#include "Pch.h"

#include "CassoExplorer/ICassoExplorerHost.h"

class CassoExplorerActions;
class CassoExplorerBrowser;
class CassoExplorerCommands;
class CassoLinkConnections;
class CassoLinkManager;
class ExplorerAddressBar;
class ExplorerCommandManager;
class ExplorerDragDrop;
class ExplorerClipboard;
class ExplorerTransferPrompts;
class ExplorerDeletion;
class ExplorerUndo;
class ExplorerRename;
class ExplorerManifestSync;
class ExplorerOpen;
class ExplorerCassoTarget;
class ExplorerProperties;
class ExplorerFolderSync;
class ExplorerInput;
class ExplorerLayout;
class ExplorerListPane;
class ExplorerMenus;
class ExplorerPreviewManager;
class ExplorerStatusBar;
class ExplorerTabManager;
class ExplorerThemes;
class ExplorerTips;
class ExplorerToolbars;
class ExplorerTreePane;
class FirewallMonitor;
class IExplorerWindow;
class IFolderWatcher;
class IHostDialogs;
class IInfoTips;
class ILinkAnnouncements;
class IShellIcons;
class IShellItemVerbs;
class PhysicalCards;
class RunningCassoRegistry;
struct CassoExplorerPrefs;
struct FolderOptions;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindowContext
//
//  What the app supplies to a Casso Explorer window. Every pointer is
//  optional.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoExplorerWindowContext
{
public:
    IFileSystem                * fs          = nullptr;
    std::wstring                 baseDir;
    std::wstring                 titlePrefix;
    CassoExplorerWindowRequest   request;
    ICassoExplorerHost         * host        = nullptr;   // other windows, the session and File > Exit; without it the window is the process's only window
    IFolderWatcher             * watcher     = nullptr;   // without it, a change made elsewhere appears on the next navigation
    CassoLinkConnections       * links       = nullptr;   // without it, no CassoLink connections are listed
    RunningCassoRegistry       * running     = nullptr;   // without it, running Cassos are not listed as connections
    ILinkAnnouncements         * announced   = nullptr;   // without it, no Apple is listed from a universal boot disk's announcements
    FirewallMonitor            * firewall    = nullptr;   // without it, no connection is reported blocked by Windows Firewall
    PhysicalCards              * cards       = nullptr;   // without it, no card can be opened
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerContext
//
//  The Casso Explorer window's services and components, as its components
//  access them.
//
////////////////////////////////////////////////////////////////////////////////

struct ExplorerContext
{
public:
    IExplorerWindow             & window;
    CassoExplorerBrowser        & browser;
    CassoExplorerActions        & actions;
    CassoExplorerPrefs          & prefs;
    CassoExplorerWindowContext  & app;
    FolderOptions               & explorerOptions;
    IHostDialogs                & dialogs;
    IShellIcons                 & shellIcons;
    IInfoTips                   & infoTips;
    IShellItemVerbs             & shellVerbs;
    CassoExplorerCommands       & commands;
    DxuiDpiScaler               & scaler;
    const DxuiTheme *           & theme;
    ExplorerLayout              & layout;
    ExplorerListPane            & listPane;
    ExplorerFolderSync          & folderSync;
    CassoLinkManager            & cassoLinkManager;
    ExplorerDragDrop            & dragDrop;
    ExplorerTabManager          & tabManager;
    ExplorerPreviewManager      & preview;
    ExplorerAddressBar          & addressBar;
    ExplorerMenus               & menus;
    ExplorerCommandManager      & commandManager;
    ExplorerClipboard           & clipboard;
    ExplorerTransferPrompts     & prompts;
    ExplorerDeletion            & deletion;
    ExplorerUndo                & undo;
    ExplorerRename              & rename;
    ExplorerManifestSync        & manifestSync;
    ExplorerOpen                & opener;
    ExplorerCassoTarget         & cassoTarget;
    ExplorerProperties          & properties;
    ExplorerInput               & input;
    ExplorerTips                & tips;
    ExplorerThemes              & themes;
    ExplorerStatusBar           & statusBar;
    ExplorerToolbars            & toolbars;
    ExplorerTreePane            & treePane;
};
