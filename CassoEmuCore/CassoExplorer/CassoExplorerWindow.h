#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Panes/ExplorerPreview.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Managers/ExplorerThemes.h"
#include "CassoExplorer/Window/Managers/ExplorerTips.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Ui/Chrome/CassoTheme.h"

class CassoLinkConnections;
class FirewallMonitor;
class RunningCassoRegistry;
class ILinkAnnouncements;
struct CassoExplorerSessionWindow;
class IFolderWatcher;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMessage
//
//  The Casso Explorer window's private messages. WM_APP + 0x3B and 0x3C are
//  Win32PreviewHelper's, which it posts to the same window.
//
////////////////////////////////////////////////////////////////////////////////

enum class ExplorerMessage : UINT
{
    Reply          = WM_APP + 0x31,
    FolderChanged  = WM_APP + 0x32,
    RunCommand     = WM_APP + 0x33,
    DropMenu       = WM_APP + 0x34,
    IconsLoaded    = WM_APP + 0x35,
    InfoTip        = WM_APP + 0x36,
    RecycleBin     = WM_APP + 0x37,
    ShellListed    = WM_APP + 0x38,
    ContentDetails = WM_APP + 0x39,
    ShellPreview   = WM_APP + 0x3A,
    NewChoices     = WM_APP + 0x3D,
    CassoLink      = WM_APP + 0x3E,
    RunningCasso   = WM_APP + 0x3F,
    Launch         = WM_APP + 0x41,
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerWindow
//
//  The browser's top-level window: menu bar, tree, splitter, file list,
//  splitter, preview, status bar.
//
//  EVERY BRANCH FORWARDS. A click or key is turned into a call on the browser
//  controller or a command id, and the widgets are then refilled from what
//  the controller holds. What to list, what to preview and what the status
//  says are all decided there, where tests reach them.
//
//  Keyboard focus is one of the three panes, moved by Tab and by a press.
//  The window is laid out by hand: the dock layout sizes a slab from the
//  child's current bounds, which a splitter changes on every drag.
//
//  Each area is a component class with the state it uses and a reference to
//  the window: the panes in CassoExplorer/Window/Panes/, and the managers in
//  CassoExplorer/Window/Managers/. The window has the widgets, its messages
//  and timers, and the session.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerWindow : public DxuiWindow
{
public:
    struct Context
    {
        IFileSystem   * fs          = nullptr;
        std::wstring    baseDir;
        std::wstring    titlePrefix;

        //  What this window opens on and where it goes.
        CassoExplorerWindowRequest  request;

        //  Optional: the app that owns the window, which implements other
        //  windows, the session, and File > Exit. Without one the window is
        //  the process's only window.
        ICassoExplorerHost * host   = nullptr;

        //  Optional: without one, the browser shows what it read when it read
        //  it, and a change made elsewhere is seen on the next navigation.
        IFolderWatcher * watcher    = nullptr;

        //  Optional: without them, no CassoLink connections are listed.
        CassoLinkConnections * links = nullptr;

        //  Optional: without it, running Cassos are not listed as links.
        RunningCassoRegistry * running = nullptr;

        //  Optional: without it, no Apple is listed from a universal boot
        //  disk's announcements.
        ILinkAnnouncements   * announced = nullptr;

        //  Optional: without it, no connection is reported blocked by
        //  Windows Firewall.
        FirewallMonitor      * firewall  = nullptr;
    };

    CassoExplorerWindow (CassoExplorerBrowser & browser, CassoExplorerActions & actions, CassoExplorerPrefs & prefs, Context context);
    ~CassoExplorerWindow() override;

    HRESULT  Open (HINSTANCE instance, const std::wstring & title, int showCommand);

    //  A second launch hands its path to the window already open, in a
    //  WM_COPYDATA with this id and the full path as UTF-16 text.
    static constexpr ULONG_PTR  kOpenPathCopyId = 0x43454F50;   // 'CEOP'

    //  A second launch with no window of its own to front sends its whole
    //  command line to the running app, in a WM_COPYDATA with this id and the
    //  arguments as UTF-16 text, its path made full.
    static constexpr ULONG_PTR  kLaunchCopyId   = 0x43454C41;   // 'CELA'

    //  The window's placement in the preferences' terms.
    CassoExplorerPrefs::Placement  GetPlacement () const;

    //  This window as the session writes it: its placement and every tab.
    CassoExplorerSessionWindow     GetSessionWindow () const;

    //  Saves the typed paths, the session and the preferences now,
    //  rather than only on a clean exit.
    void  SaveSession();

    //  CassoLink posts its changes to one window; this one, from now on.
    void  TakeLinkNotifications();

    //  Closed by the app, which has saved the session already.
    void  CloseForApp();

    void    Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool    OnMouse           (const DxuiMouseEvent & ev) override  { return m_input.HandleMouse (ev); }
    bool    OnKey             (const DxuiKeyEvent & ev) override    { return m_input.HandleKey (ev); }
    LPCWSTR GetCursorForPoint (POINT clientPx) const override       { return m_input.GetCursorForPoint (clientPx); }

    //  Windows changed its light or dark setting or its accent: Follow
    //  system takes the new one at once, and every theme takes the accent.
    void    OnThemeChanged    () override;
    static constexpr UINT      kTooltipTickMs  = 16;   // the menus' reveal runs on it too, so display rate
    static constexpr float     kNewTabGapDip   = 6.67f;   // the last tab to the + button

    //  Explorer's tree and list text: 9 points, 12 dip.
    static constexpr float     kProseFontDip         = 12.0f;

    //  Loaded at this size and scaled down by the caption, as Casso's is.
    static constexpr int       kCaptionIconPx      = 32;

    static constexpr int    kMinWindowWidthDip   = 200;
    static constexpr int    kCascadeStepDip      = 32;   // a second window's offset from the saved placement

private:

public:

    //  How long to let a burst settle before re-reading. Copying a hundred
    //  files reports a hundred changes; re-reading once at the end is both
    //  faster and steadier to look at.
    static constexpr UINT      kFolderSettleMs       = 200;
    static constexpr ULONGLONG kFolderMaxSettleMs    = 1000;  // the longest a busy folder holds off a re-read

    //  Which pane a drop landed on.
    static constexpr int       kDropTagList          = 0;
    static constexpr int       kDropTagTree          = 1;
    static constexpr int       kDropTagTabs          = 2;

protected:
    void  OnCreate        () override;
    void  OnWindowClose   () override;

    DxuiMessageResult  OnCopyData   (WPARAM sender, LPARAM data) override;
    DxuiMessageResult  OnActivateApp (bool active) override;
    DxuiMessageResult  OnTimer       (UINT_PTR timerId) override;
    DxuiMessageResult  OnSize        (UINT widthPx, UINT heightPx) override;
    void               OnExitSizeMove           () override;
    void               OnEnterSizeMove          () override;
    DxuiMessageResult  OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnEndSession (bool ending) override;

private:
    void  ShowAbout();

    //  Stands for a separator in a list of command ids.
    static constexpr int  kSeparatorId = 0;
    void  ArmTick             ();
    bool  IsTickWanted        () const;

    static int64_t  GetNowMs();

    void  RecordSession       ();
    void  ShowMessage         (const std::wstring & text, UINT icon);

    static bool  Contains (const RECT & rect, POINT point);
    static DxuiMouseEvent  ToLocal (const DxuiMouseEvent & ev, const RECT & bounds);

    CassoExplorerBrowser                       & m_browser;
    CassoExplorerActions                       & m_actions;
    CassoExplorerPrefs                         & m_prefs;
    FolderOptions                                m_explorerOptions;
    bool                                         m_opened          = false;
    bool                                         m_inSizeMove      = false;
    Context                                      m_context;
    Win32HostDialogs                             m_dialogs;
    Win32ShellIcons                              m_shellIcons;
    Win32InfoTips                                m_infoTips;
    Win32ShellItemVerbs                          m_shellVerbs;


    std::vector<Win32IntentChannel::Reply>    m_pendingReplies;
    CassoExplorerCommands                     m_commands;
    const DxuiTheme                         * m_theme               = nullptr;
    DxuiDpiScaler                             m_scaler;
    RECT                                      m_client              = {};


    ExplorerPane                               m_focus             = ExplorerPane::Tree;

    DxuiSplitter         * m_treeSplitter    = nullptr;

    //  Rename in place: an edit box laid over the row's name, as Explorer's
    //  F2 opens one. The row being renamed, or -1.
    DxuiSplitter    * m_previewSplitter    = nullptr;


    //  Other launches' arguments, run once the send that brought each returns.
    std::vector<std::wstring>  m_pendingLaunches;

    bool                                    m_caretOn      = true;   // the search caret as last drawn
    bool                                    m_tickArmed    = false;   // the animation tick is running

    //  The window's edges, docked. Each band is stamped with the thickness
    //  its widget needs and comes back with the rect that widget is laid
    //  into; the body band takes what the edges leave, and the panes and
    //  their splitters divide it.
    DxuiDockLayout         m_dock;
    DxuiLayoutBand         m_tabBand;
    DxuiLayoutBand         m_menuBand;
    DxuiLayoutBand         m_toolbarBand;
    DxuiLayoutBand         m_statusBand;
    DxuiLayoutBand         m_bodyBand;

    //  The window's parts, each with the state it uses.
    friend class ExplorerLayout;
    friend class ExplorerListPane;
    friend class ExplorerListColumns;
    friend class ExplorerListMenus;
    friend class ExplorerFolderSync;
    friend class CassoLinkManager;
    friend class ExplorerDragDrop;
    friend class ExplorerTabManager;
    friend class ExplorerPreview;
    friend class ExplorerPreviewArchive;
    friend class ExplorerPreviewFind;
    friend class ExplorerAddressBar;
    friend class ExplorerMenus;
    friend class ExplorerCommandManager;
    friend class ExplorerFileOps;
    friend class ExplorerInput;
    friend class ExplorerStatusBar;
    friend class ExplorerThemes;
    friend class ExplorerTips;
    friend class ExplorerToolbars;
    friend class ExplorerTreePane;
    ExplorerLayout  m_layout { *this };
    ExplorerListPane  m_listPane { *this };
    ExplorerFolderSync  m_folderSync { *this };
    CassoLinkManager  m_cassoLinkManager { *this };
    ExplorerDragDrop  m_dragDrop { *this };
    ExplorerTabManager  m_tabManager { *this };
    ExplorerPreview  m_previewPane { *this };
    ExplorerAddressBar  m_addressBar { *this };
    ExplorerMenus  m_menus { *this };
    ExplorerCommandManager  m_commandManager { *this };
    ExplorerFileOps  m_fileOps { *this };
    ExplorerInput  m_input { *this };
    ExplorerTips  m_tips { *this };
    ExplorerThemes  m_themes { *this };
    ExplorerStatusBar  m_statusBar { *this };
    ExplorerToolbars  m_toolbars { *this };
    ExplorerTreePane  m_treePane { *this };
};

