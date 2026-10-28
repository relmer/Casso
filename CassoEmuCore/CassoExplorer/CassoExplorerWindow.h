#pragma once

#include "Pch.h"

#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerDragDrop.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
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

struct CassoExplorerSessionWindow;





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
//  the window's ExplorerContext: the panes in CassoExplorer/Window/Panes/, and the managers in
//  CassoExplorer/Window/Managers/. The window has the widgets, its messages
//  and timers, and the session.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerWindow : public DxuiWindow, public IExplorerWindow
{
public:
    using Context = CassoExplorerWindowContext;

    CassoExplorerWindow (CassoExplorerBrowser & browser, CassoExplorerActions & actions, CassoExplorerPrefs & prefs, Context context);
    ~CassoExplorerWindow() override;

    //  A second launch hands its path to the window already open, in a
    //  WM_COPYDATA with this id and the full path as UTF-16 text.
    static constexpr ULONG_PTR  kOpenPathCopyId = 0x43454F50;   // 'CEOP'

    //  A second launch with no window of its own to front sends its whole
    //  command line to the running app, in a WM_COPYDATA with this id and the
    //  arguments as UTF-16 text, its path made full.
    static constexpr ULONG_PTR  kLaunchCopyId  = 0x43454C41;   // 'CELA'
    static constexpr UINT       kTooltipTickMs = 16;   // the menus' reveal runs on it too, so display rate

    //  Loaded at this size and scaled down by the caption, as Casso's is.
    static constexpr int  kCaptionIconPx     = 32;
    static constexpr int  kMinWindowWidthDip = 200;
    static constexpr int  kCascadeStepDip    = 32;   // a second window's offset from the saved placement

    //  How long to let a burst settle before re-reading. Copying a hundred
    //  files reports a hundred changes; re-reading once at the end is both
    //  faster and steadier to look at.
    static constexpr UINT      kFolderSettleMs       = 200;
    static constexpr ULONGLONG kFolderMaxSettleMs    = 1000;  // the longest a busy folder holds off a re-read

    //  Which pane a drop landed on.
    static constexpr int       kDropTagList          = 0;
    static constexpr int       kDropTagTree          = 1;
    static constexpr int       kDropTagTabs          = 2;

    HRESULT                          Open                  (HINSTANCE instance, const std::wstring & title, int showCommand);
    CassoExplorerPrefs::Placement    GetPlacement          () const override;
    CassoExplorerSessionWindow       GetSessionWindow      () const;
    void                             SaveSession           () override;
    HWND                             GetHwnd               () const override                   { return DxuiWindow::GetHwnd(); }
    void                             Invalidate            () override                         { DxuiWindow::Invalidate(); }
    DxuiHwndSource                 * GetPopupHost          () const override                   { return DxuiWindow::GetPopupHost(); }
    IDxuiTextRenderer              * GetTextRenderer       () const override                   { return DxuiWindow::GetTextRenderer(); }
    void                             SetTheme              (const IDxuiTheme * theme) override { DxuiWindow::SetTheme (theme); }
    DxuiPanel                      & GetChildPanel         () override                         { return *this; }
    void                             ShowMessage           (const std::wstring & text, UINT icon) override;
    void                             ShowAbout             () override;
    void                             ArmTick               () override;
    void                             CloseWindow           () override                         { OnWindowClose(); }
    ExplorerTabManager             & GetTabManager         ()                              { return m_tabManager; }
    CassoLinkManager               & GetCassoLinkManager   ()                              { return m_cassoLinkManager; }
    static int64_t                   GetNowMs              ();
    void                             TakeLinkNotifications ();
    void                             CloseForApp           ();
    void                             Layout                (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                             Paint                 (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    LPCWSTR                          GetCursorForPoint     (POINT clientPx) const override       { return m_input.GetCursorForPoint (clientPx); }

    bool    OnMouse           (const DxuiMouseEvent & ev) override  { return m_input.HandleMouse (ev); }
    bool    OnKey             (const DxuiKeyEvent & ev) override    { return m_input.HandleKey (ev); }
    void    OnThemeChanged    () override;

protected:
    void               OnCreate        () override;
    void               OnWindowClose   () override;
    DxuiMessageResult  OnCopyData      (WPARAM sender, LPARAM data) override;
    DxuiMessageResult  OnActivateApp   (bool active) override;
    DxuiMessageResult  OnTimer         (UINT_PTR timerId) override;
    DxuiMessageResult  OnSize          (UINT widthPx, UINT heightPx) override;
    void               OnExitSizeMove  () override;
    void               OnEnterSizeMove () override;
    DxuiMessageResult  OnAppMessage    (UINT msg, WPARAM wParam, LPARAM lParam) override;
    DxuiMessageResult  OnEndSession    (bool ending) override;

private:
    //  Stands for a separator in a list of command ids.
    static constexpr int  kSeparatorId = 0;

    bool  IsTickWanted        () const;
    void  RecordSession       ();

    CassoExplorerBrowser                    & m_browser;
    CassoExplorerActions                    & m_actions;
    CassoExplorerPrefs                      & m_prefs;
    FolderOptions                             m_explorerOptions;
    bool                                      m_opened          = false;
    bool                                      m_inSizeMove      = false;
    Context                                   m_context;
    Win32HostDialogs                          m_dialogs;
    Win32ShellIcons                           m_shellIcons;
    Win32InfoTips                             m_infoTips;
    Win32ShellItemVerbs                       m_shellVerbs;
    std::vector<Win32IntentChannel::Reply>    m_pendingReplies;
    CassoExplorerCommands                     m_commands;
    const DxuiTheme                         * m_theme           = nullptr;
    DxuiDpiScaler                             m_scaler;
    std::vector<std::wstring>                 m_pendingLaunches;   // other launches' arguments, run once the send that brought each returns
    bool                                      m_caretOn         = true;   // the search caret as last drawn
    bool                                      m_tickArmed       = false;   // the animation tick is running
    ExplorerContext                            m_explorer        { .window           = *this,
                                                                   .browser          = m_browser,
                                                                   .actions          = m_actions,
                                                                   .prefs            = m_prefs,
                                                                   .app              = m_context,
                                                                   .explorerOptions  = m_explorerOptions,
                                                                   .dialogs          = m_dialogs,
                                                                   .shellIcons       = m_shellIcons,
                                                                   .infoTips         = m_infoTips,
                                                                   .shellVerbs       = m_shellVerbs,
                                                                   .commands         = m_commands,
                                                                   .scaler           = m_scaler,
                                                                   .theme            = m_theme,
                                                                   .layout           = m_layout,
                                                                   .listPane         = m_listPane,
                                                                   .folderSync       = m_folderSync,
                                                                   .cassoLinkManager = m_cassoLinkManager,
                                                                   .dragDrop         = m_dragDrop,
                                                                   .tabManager       = m_tabManager,
                                                                   .preview          = m_previewManager,
                                                                   .addressBar       = m_addressBar,
                                                                   .menus            = m_menus,
                                                                   .commandManager   = m_commandManager,
                                                                   .fileOps          = m_fileOps,
                                                                   .input            = m_input,
                                                                   .tips             = m_tips,
                                                                   .themes           = m_themes,
                                                                   .statusBar        = m_statusBar,
                                                                   .toolbars         = m_toolbars,
                                                                   .treePane         = m_treePane };
    ExplorerLayout          m_layout           { m_explorer };
    ExplorerListPane        m_listPane         { m_explorer };
    ExplorerFolderSync      m_folderSync       { m_explorer };
    CassoLinkManager        m_cassoLinkManager { m_explorer };
    ExplorerDragDrop        m_dragDrop         { m_explorer };
    ExplorerTabManager      m_tabManager       { m_explorer };
    ExplorerPreviewManager  m_previewManager   { m_explorer };
    ExplorerAddressBar      m_addressBar       { m_explorer };
    ExplorerMenus           m_menus            { m_explorer };
    ExplorerCommandManager  m_commandManager   { m_explorer };
    ExplorerFileOps         m_fileOps          { m_explorer };
    ExplorerInput           m_input            { m_explorer };
    ExplorerTips            m_tips             { m_explorer };
    ExplorerThemes          m_themes           { m_explorer };
    ExplorerStatusBar       m_statusBar        { m_explorer };
    ExplorerToolbars        m_toolbars         { m_explorer };
    ExplorerTreePane        m_treePane         { m_explorer };
};

