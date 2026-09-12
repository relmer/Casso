#pragma once

#include "Pch.h"

#include "Cassque/CassqueActions.h"
#include "Cassque/CassqueBrowser.h"
#include "Cassque/CassqueCommands.h"
#include "Cassque/CassqueNamedControl.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "Theme/DxuiDarkTheme.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Theme/DxuiLightTheme.h"
#include "Widgets/DxuiFramebufferView.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiSplitter.h"
#include "Widgets/DxuiStatusBar.h"
#include "Widgets/DxuiTabStrip.h"
#include "Widgets/DxuiToolbar.h"
#include "Widgets/DxuiTooltip.h"
#include "Widgets/DxuiTreeView.h"
#include "Core/DxuiDockLayout.h"
#include "Core/DxuiHitTester.h"
#include "Core/DxuiLayoutBand.h"
#include "Window/DxuiDragDropTarget.h"
#include "Window/DxuiWindow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueWindow
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
////////////////////////////////////////////////////////////////////////////////

class CassqueWindow : public DxuiWindow
{
public:
    struct Context
    {
        IFileSystem   * fs          = nullptr;
        std::wstring    baseDir;
        HWND            owner       = nullptr;
        std::wstring    titlePrefix;
    };

    CassqueWindow (CassqueBrowser & browser, CassqueActions & actions, CassquePrefs & prefs, Context context);
    ~CassqueWindow() override;

    HRESULT  Open (HINSTANCE instance, const std::wstring & title, int showCommand);

    //  The window's placement in the preferences' terms, for saving on exit.
    void  StorePlacement();

    void    Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool    OnMouse           (const DxuiMouseEvent & ev) override;
    bool    OnKey             (const DxuiKeyEvent & ev) override;
    LPCWSTR GetCursorForPoint (POINT clientPx) const override;

    static constexpr int       kMaxCatalogName     = 30;
    static constexpr UINT_PTR  kTooltipTimerId     = 0x5153;
    static constexpr UINT      kTooltipTickMs      = 50;
    static constexpr int       kTabHeightDip       = 32;

    //  The preview's rows carry one line of fixed-width text each, so they
    //  are the line's height rather than a file listing's roomier row.
    static constexpr int  kPreviewRowHeightDip = 18;
    static constexpr int  kTabWidthDip         = 180;
    static constexpr int  kMinTreeWidthDip     = 140;
    static constexpr int  kMinListWidthDip     = 220;
    static constexpr int  kMinPreviewWidthDip  = 160;

    //  The private message that carries a deferred Casso reply to the UI.
    static constexpr UINT  kReplyMessage = WM_APP + 0x31;

protected:
    void  OnCreate        () override;
    void  OnWindowClose   () override;

    DxuiMessageResult  OnCopyData   (WPARAM sender, LPARAM data) override;
    DxuiMessageResult  OnActivateApp (bool active) override;
    DxuiMessageResult  OnTimer       (UINT_PTR timerId) override;
    DxuiMessageResult  OnAppMessage (UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    enum class Pane { Tree, List, Preview };

    ////////////////////////////////////////////////////////////////////////////
    //
    //  PreviewBytes
    //
    //  The bytes of the previewed file, as the hex view reads them. The
    //  preview's content is owned by the browser and replaced whole on every
    //  selection change, so this holds a pointer to it rather than a copy and
    //  is pointed at the new content each time the preview is refilled.
    //
    ////////////////////////////////////////////////////////////////////////////

    class PreviewBytes : public IDxuiHexSource
    {
    public:
        void  SetBytes (const std::vector<Byte> * bytes) { m_bytes = bytes; }

        uint64_t  GetByteCount() const override
        {
            return (m_bytes != nullptr) ? (uint64_t) m_bytes->size() : 0;
        }

        void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
        {
            size_t  start = (size_t) offset;

            for (size_t idx = 0; idx < out.size(); idx++)
            {
                out[idx] = (m_bytes != nullptr && (start + idx) < m_bytes->size())
                         ? (uint8_t) (*m_bytes)[start + idx]
                         : (uint8_t) 0;
            }
        }

    private:
        const std::vector<Byte> *  m_bytes = nullptr;
    };

    void  ConfigureWidgets();
    void  ApplyTheme();
    void  SelectTheme (const char * name);

    static bool  IsCassoThemeName (const std::string & name);
    void  RecomputeLayout();
    void  FillList();
    void  FillTabs();
    void  SwitchToTab (size_t index);
    void  FillPreview();
    void  FillStatus();
    void  SetFocusPane (Pane pane);
    void  Dispatch     (int id);
    bool  IsEnabled    (int id) const;
    bool  IsChecked    (int id) const;
    void  ShowAbout();

    void  ShowListContextMenu (int x, int y);
    void  ShowHexContextMenu  (int x, int y);
    void  AskForOffset();
    void  SetHexGrouping (int grouping);

    //  Stands for a separator in a list of command ids.
    static constexpr int  kSeparatorId = 0;

    //  Whether the preview pane is currently showing bytes rather than lines,
    //  a picture or a message -- which is what decides where a key, a copy or
    //  a Tab inside the pane goes.
    bool  IsHexPreviewShowing() const;
    bool  RouteToolbarMouse   (const DxuiMouseEvent & ev);

    static int64_t  GetNowMs();
    void  BeginDragOut();
    void  OnDropFile (const std::wstring & path);
    CassqueActions::AddressFn  MakeAddressPrompt();
    void  ShowTreeContextMenu (int x, int y, const std::wstring & id);
    void  ChangeKnownFolder   (const std::wstring & folder, bool add);
    void  RebuildTree();
    void  RunVerb             (CassqueActions::Verb verb);
    void  RunRawVerb          (CassqueActions::Verb verb);
    void  ReportOutcome       (const CassqueActions::Outcome & outcome, const wchar_t * verbName);
    void  InsertIntoDrive     (const std::wstring & imagePath, int drive);
    void  OpenInNewCasso      (const std::wstring & imagePath);
    HWND  FindCassoTarget     () const;
    void  AskCassoToDescribe  ();
    void  ShowMessage         (const std::wstring & text, UINT icon);
    std::wstring  GetSelectedImagePath() const;

    static const wchar_t *  GetVerbLabel (CassqueActions::Verb verb);

    static bool  Contains (const RECT & rect, POINT point);
    static DxuiMouseEvent  ToLocal (const DxuiMouseEvent & ev, const RECT & bounds);

    CassqueBrowser                             & m_browser;
    CassqueActions                             & m_actions;
    CassquePrefs                               & m_prefs;
    Context                                      m_context;
    Win32HostDialogs                             m_dialogs;
    Win32ProcessLauncher                         m_launcher;
    std::vector<std::unique_ptr<DxuiCommand>>    m_menuCommands;
    std::vector<Win32IntentChannel::Reply>       m_pendingReplies;
    bool                                         m_dragArmed       = false;
    int                                          m_cassoDriveCount = 0;
    DxuiDragDropTarget                           m_dropTarget;
    DxuiHitTester                                m_dropHits;
    POINT                                        m_dragStart       = {};
    CassqueCommands                              m_commands;
    DxuiLightTheme                               m_lightTheme;
    DxuiDarkTheme                                m_darkTheme;
    CassoTheme                                   m_cassoTheme;
    const DxuiTheme                            * m_theme           = nullptr;
    DxuiDpiScaler                                m_scaler;
    RECT                                         m_client          = {};
    Pane                                         m_focus           = Pane::Tree;

    DxuiMenuBar          * m_menuBar         = nullptr;
    DxuiTreeView         * m_tree            = nullptr;
    DxuiSplitter         * m_treeSplitter    = nullptr;
    DxuiListView         * m_list            = nullptr;
    DxuiLabel            * m_listMessage     = nullptr;
    DxuiSplitter         * m_previewSplitter = nullptr;
    DxuiListView         * m_previewList     = nullptr;
    DxuiHexView          * m_hexView         = nullptr;
    PreviewBytes           m_previewBytes;
    DxuiFramebufferView  * m_picture         = nullptr;
    DxuiLabel            * m_previewMessage  = nullptr;
    DxuiStatusBar        * m_status          = nullptr;
    DxuiTabStrip         * m_tabs            = nullptr;
    DxuiToolbar          * m_toolbar         = nullptr;
    DxuiTooltip            m_tooltip;

    //  The window's edges, docked. Each band is stamped with the thickness
    //  its widget needs and comes back with the rect that widget is laid
    //  into; the body band takes what the edges leave, and the panes and
    //  their splitters divide it.
    DxuiDockLayout         m_dock;
    DxuiLayoutBand         m_menuBand;
    DxuiLayoutBand         m_toolbarBand;
    DxuiLayoutBand         m_statusBand;
    DxuiLayoutBand         m_bodyBand;
};
