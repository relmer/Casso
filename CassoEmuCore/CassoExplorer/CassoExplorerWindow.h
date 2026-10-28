#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/ExplorerLayout.h"
#include "CassoExplorer/Window/ExplorerListPane.h"
#include "CassoExplorer/Window/ExplorerFolderSync.h"
#include "CassoExplorer/Window/ExplorerLinks.h"
#include "CassoExplorer/Window/ExplorerDragDrop.h"
#include "CassoExplorer/Window/ExplorerTabStrip.h"
#include "CassoExplorer/Window/ExplorerPreview.h"
#include "CassoExplorer/Window/ExplorerAddressBar.h"
#include "CassoExplorer/Window/ExplorerMenus.h"
#include "CassoExplorer/Window/ExplorerDispatch.h"
#include "CassoExplorer/Window/ExplorerFileOps.h"
#include "CassoExplorer/Window/ExplorerInput.h"
#include "CassoExplorer/Window/ExplorerTips.h"
#include "CassoExplorer/Window/ExplorerTreePane.h"
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
class FolderWatch;
class IFolderWatcher;





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
//  Each area is a component class in CassoExplorer/Window/, with the state
//  it uses and a reference to the window: the layout, the list pane, folder
//  sync, links, drag and drop, the tab strip, the preview pane, the address
//  bar, menus, dispatch, file operations, input and tips. The window has the
//  widgets, its messages and timers, and the session.
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
    bool    OnMouse           (const DxuiMouseEvent & ev) override  { return m_input.OnMouse (ev); }
    bool    OnKey             (const DxuiKeyEvent & ev) override    { return m_input.OnKey (ev); }
    LPCWSTR GetCursorForPoint (POINT clientPx) const override       { return m_input.GetCursorForPoint (clientPx); }

    //  Windows changed its light or dark setting or its accent: Follow
    //  system takes the new one at once, and every theme takes the accent.
    void    OnThemeChanged    () override;
    static constexpr UINT_PTR  kTooltipTimerId = 0x5153;
    static constexpr UINT      kTooltipTickMs  = 16;   // the menus' reveal runs on it too, so display rate
    static constexpr float     kNewTabGapDip   = 6.67f;   // the last tab to the + button

    //  Explorer's navigation glyphs are smaller than Casso's toolbar icons:
    //  15 pixels of ink at 120 DPI.
    static constexpr float     kNavIconDip         = 12.0f;

    //  Its back, forward, up and refresh buttons are 48 dip apart, center to
    //  center, the first 34.67 dip from the window's left edge: measured at
    //  150%. A 12 dip glyph, 16 dip either side and the toolbar's 4 dip gap.
    static constexpr float     kNavButtonPadDip    = 16.0f;
    static constexpr int       kNavBarPadDp        = 5;
    static constexpr int       kNavAddressGapDp    = 8;      // from the refresh button to the address box

    //  File Explorer's command bar, measured at 125%: buttons 48 dip apart
    //  center to center, label cap height 11 px against the chrome font's 13,
    //  and its 20-unit Fluent icons drawn one unit to the dip -- the scissors
    //  are 20 pixels tall. A 20 dip square, 12 dip either side and the
    //  toolbar's 4 dip gap keep the 48.
    static constexpr float  kCommandBarIconDip       = 20.0f;
    static constexpr float  kCommandBarPadDip        = 12.0f;
    static constexpr float  kCommandBarLabeledDip    = 10.0f;   // a labeled button's pad before its icon, measured at 150%
    static constexpr float  kCommandBarDropDip       = 0.67f;   // Explorer's buttons sit a pixel below the bar's middle at 150%
    static constexpr float  kCommandBarLabelDip      = 12.0f;
    static constexpr float  kCommandBarChevronGapDip = 6.0f;    // a drop-down's label to its chevron, measured at 150%
    static constexpr float  kCommandBarIconGapDip    = 6.33f;   // a button's icon to its label, measured at 150%

    //  Explorer's tree and list text: 9 points, 12 dip.
    static constexpr float     kProseFontDip         = 12.0f;
    static constexpr int       kCommandBarGroupGapDp = 9;    // 14 px at 150%, with the separator centered in it
    static constexpr int       kCommandBarPadXDp     = 6;

    //  Loaded at this size and scaled down by the caption, as Casso's is.
    static constexpr int       kCaptionIconPx      = 32;

    static constexpr int    kMinWindowWidthDip   = 200;
    static constexpr int    kCascadeStepDip      = 32;   // a second window's offset from the saved placement
    static constexpr float  kStatusLeadDip       = 14.67f;   // its item count, from the band's left edge

    //  The private message that holds a deferred Casso reply to the UI.
    static constexpr UINT  kReplyMessage = WM_APP + 0x31;

    //  A watched folder changed. Posted from the watcher's thread, which does
    //  nothing else.
    static constexpr UINT      kFolderChangedMessage = WM_APP + 0x32;

    //  Runs the command whose id is in wParam, as its button or menu row
    //  would, when it is enabled. A posted message reaches every command
    //  without the pointer or a menu, which is how a walkthrough is driven
    //  from outside the process; a popup menu does not take posted clicks.
    static constexpr UINT      kRunCommandMessage    = WM_APP + 0x33;

private:
    //  A right-drag's menu, opened after the drop returns. Holding the drop
    //  open while a menu is up would hold the dragging program's own loop
    //  open with it.
    static constexpr UINT      kDropMenuMessage      = WM_APP + 0x34;

    //  Posted by the list's icon loader when icons it was asked for are ready.
    static constexpr UINT      kIconsLoadedMessage   = WM_APP + 0x35;

    //  Posted by the tip reader when an item's shell tip is ready.
    static constexpr UINT      kInfoTipMessage       = WM_APP + 0x36;

    //  Posted by the shell when the Recycle Bin's contents may have changed.
    static constexpr UINT      kRecycleBinMessage    = WM_APP + 0x37;

    //  Posted by a shell folder's reader when its listing is in.
    static constexpr UINT      kShellListedMessage   = WM_APP + 0x38;

    //  Posted by the Content view's reader when items' details are in.
    static constexpr UINT      kContentDetailsMessage = WM_APP + 0x39;

    //  Posted by the shell preview's thread when a load is done.
    static constexpr UINT      kShellPreviewMessage  = WM_APP + 0x3A;

    //  Posted once the shell's New menu choices are read.
    static constexpr UINT      kNewChoicesMessage    = WM_APP + 0x3D;

    //  Posted by an CassoLink when an operation is done or its state changes.
    static constexpr UINT      kLinkMessage          = WM_APP + 0x3E;
    static constexpr UINT      kRunningMessage       = WM_APP + 0x3F;

    //  Posted to itself once another launch has forwarded its arguments, so
    //  the launch's send returns before a window opens.
    static constexpr UINT      kLaunchMessage        = WM_APP + 0x41;

public:
    static constexpr UINT_PTR  kFolderTimerId        = 0x5154;
    static constexpr UINT_PTR  kRestoreTimerId       = 0x5155;   // re-reads while the Recycle Bin restores
    static constexpr UINT_PTR  kRecycleBinTimerId    = 0x5156;   // settles the bin's changes before a re-read
    static constexpr UINT_PTR  kDriveTimerId         = 0x5157;   // polls for drives attached or removed
    static constexpr UINT_PTR  kShellPreviewTimerId  = 0x5158;   // gives up on a shell preview that has not arrived
    static constexpr UINT_PTR  kRefreshTimerId       = 0x5159;   // refills the list after a refresh's blank

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
    bool                              IsRenaming                () const { return m_renameRow >= 0 || !m_fileOps.m_renameTreeId.empty(); }
    void  ShowAbout();

    //  Stands for a separator in a list of command ids.
    static constexpr int  kSeparatorId = 0;
    void  ArmTick             ();
    bool  IsTickWanted        () const;

    static int64_t  GetNowMs();

    DxuiToolbar          * m_commandBar      = nullptr;
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
    Win32ShellIcons                              m_dragIcons;   // drawn as each drag starts, so not in the background
    Win32InfoTips                                m_infoTips;
    Win32PackagedCommands                        m_packagedCommands;
    Win32ShellItemVerbs                          m_shellVerbs;
    std::vector<std::shared_ptr<DxuiCommand>>    m_menuCommands;

    //  The list columns the user has chosen to show, from the header's menu.
    std::vector<bool>                         m_listColumnChosen;

    //  The column order the folder shown uses: its own, or the latest.
    std::vector<int>                          m_folderColumnOrder;
    std::unique_ptr<FolderWatch>              m_folderWatch;
    bool                                      m_applyingTheme       = false;
    std::vector<Win32IntentChannel::Reply>    m_pendingReplies;
    bool                                      m_dragArmed           = false;
    ULONGLONG                                 m_folderFirstChangeMs = 0;   // 0: no change waiting
    DWORD                                     m_driveMask           = 0;   // GetLogicalDrives at the last look
    std::wstring                              m_quickAccessDir;            // where File Explorer keeps Quick access
    int64_t                                   m_quickAccessStamp    = 0;   // its file's last write at the last look
    int                                       m_cassoDriveCount     = 0;
    CassoExplorerCommands                     m_commands;
    DxuiLightTheme                            m_lightTheme;
    DxuiDarkTheme                             m_darkTheme;
    CassoTheme                                m_cassoTheme;
    const DxuiTheme                         * m_theme               = nullptr;
    DxuiDpiScaler                             m_scaler;
    RECT                                      m_client              = {};

    //  Why each row's image is broken, as its cells are built; empty for the
    //  rest.
    std::vector<std::wstring>                 m_rowProblems;

    //  Whether the status bar's detail has the CassoLink connections' pending operations.
    bool  m_linkJobsShown = false;

    //  The list's view, and the key of the folder it was chosen for: a new
    //  folder opens in its own view.
    DxuiListView::View                         m_listView          = DxuiListView::View::Details;
    std::wstring                               m_listViewKey;
    ExplorerPane                               m_focus             = ExplorerPane::Tree;

    DxuiMenuBar          * m_menuBar         = nullptr;
    DxuiSplitter         * m_treeSplitter    = nullptr;
    DxuiListView         * m_list            = nullptr;

    //  Rename in place: an edit box laid over the row's name, as Explorer's
    //  F2 opens one. The row being renamed, or -1.
    DxuiTextInput   * m_renameBox          = nullptr;
    int               m_renameRow          = -1;
    DxuiLabel       * m_listMessage        = nullptr;
    bool              m_listMessageTop     = false;
    DxuiButton      * m_listAction         = nullptr;
    DxuiButton      * m_listActionAlt      = nullptr;
    bool              m_listActionFirewall = false;
    DxuiSplitter    * m_previewSplitter    = nullptr;
    DxuiStatusBar   * m_status             = nullptr;
    DxuiToolbar     * m_toolbar            = nullptr;

    DxuiTooltip              m_tooltip;

    //  Other launches' arguments, run once the send that brought each returns.
    std::vector<std::wstring>  m_pendingLaunches;

    std::vector<ExplorerFileOps::UndoStep>  m_undo;   // newest last
    bool                                    m_caretOn      = true;   // the search caret as last drawn
    bool                                    m_tickArmed    = false;   // the animation tick is running
    std::vector<std::wstring>               m_cutPaths;   // cut to the clipboard and not yet pasted, drawn dimmed

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
    friend class ExplorerFolderSync;
    friend class ExplorerLinks;
    friend class ExplorerDragDrop;
    friend class ExplorerTabStrip;
    friend class ExplorerPreview;
    friend class ExplorerPreviewArchive;
    friend class ExplorerPreviewFind;
    friend class ExplorerAddressBar;
    friend class ExplorerMenus;
    friend class ExplorerDispatch;
    friend class ExplorerFileOps;
    friend class ExplorerInput;
    friend class ExplorerTips;
    friend class ExplorerTreePane;
    ExplorerLayout  m_layout { *this };
    ExplorerListPane  m_listPane { *this };
    ExplorerFolderSync  m_folderSync { *this };
    ExplorerLinks  m_links { *this };
    ExplorerDragDrop  m_dragDrop { *this };
    ExplorerTabStrip  m_tabStrip { *this };
    ExplorerPreview  m_previewPane { *this };
    ExplorerAddressBar  m_addressBar { *this };
    ExplorerMenus  m_menus { *this };
    ExplorerDispatch  m_dispatch { *this };
    ExplorerFileOps  m_fileOps { *this };
    ExplorerInput  m_input { *this };
    ExplorerTips  m_tips { *this };
    ExplorerTreePane  m_treePane { *this };
};

