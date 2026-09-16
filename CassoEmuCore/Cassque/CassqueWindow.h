#pragma once

#include "Pch.h"

#include "Cassque/CassqueActions.h"
#include "Cassque/CassqueBrowser.h"
#include "Cassque/CassqueCommands.h"
#include "Cassque/CassqueNamedControl.h"
#include "Cassque/Model/FocusRing.h"
#include "Cassque/Model/CassquePrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "Seams/Win32ShellIcons.h"
#include "Theme/DxuiDarkTheme.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Theme/DxuiLightTheme.h"
#include "Widgets/DxuiAddressBar.h"
#include "Widgets/DxuiFramebufferView.h"
#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiMenuBar.h"
#include "Widgets/DxuiSplitter.h"
#include "Widgets/DxuiStatusBar.h"
#include "Widgets/DxuiTabStrip.h"
#include "Widgets/DxuiTextView.h"
#include "Widgets/DxuiToolbar.h"
#include "Widgets/DxuiToolbarEditBox.h"
#include "Widgets/DxuiTooltip.h"
#include "Render/DxuiTextRenderer.h"
#include "Widgets/DxuiTreeView.h"
#include "Core/DxuiDockLayout.h"
#include "Core/DxuiHitTester.h"
#include "Core/DxuiLayoutBand.h"
#include "Widgets/DxuiPopupMenu.h"
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
    static constexpr UINT      kTooltipTickMs      = 16;   // the menus' reveal runs on it too, so display rate
    static constexpr int       kTabHeightDip       = 42;   // Explorer's strip: tabs 33 dip tall, 9 below its top
    static constexpr int       kTabTopDip          = 9;

    //  Explorer's navigation glyphs are smaller than Casso's toolbar icons:
    //  15 pixels of ink at 120 DPI.
    static constexpr float     kNavIconDip         = 12.0f;

    //  Loaded at this size and scaled down by the caption, as Casso's is.
    static constexpr int       kCaptionIconPx      = 32;

    //  The preview's rows carry one line of fixed-width text each, so they
    //  are the line's height rather than a file listing's roomier row.
    static constexpr int  kPreviewRowHeightDip = 18;
    static constexpr int  kTabWidthDip         = 240;
    static constexpr int  kTabMinWidthDip      = 100;
    static constexpr int  kMinTreeWidthDip     = 140;
    static constexpr int  kMinListWidthDip     = 220;
    static constexpr int  kMinPreviewWidthDip  = 280;   // wide enough for the hex view's toolbar

    //  Where DOS 3.3 on a 48K machine leaves HIMEM, against which Integer BASIC
    //  keeps its program.
    static constexpr int  kIntegerBasicHimem   = 0x9600;

    //  Preview text is drawn this far from the background toward the theme's
    //  foreground, a gray like a terminal's text rather than full white.
    static constexpr float  kPreviewTextStrength = 0.8f;

    //  Each zoom step, in percent of the theme's text size.
    static constexpr int    kPreviewZoomStep     = 10;

    //  Status bar field widths: free space, and the preview's detail and zoom
    //  when the preview is hidden and they cannot follow its edge.
    static constexpr int    kStatusFreeDip       = 140;
    static constexpr int    kStatusDetailDip     = 280;
    static constexpr int    kStatusZoomDip       = 64;

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
    //  Keyboard focus. The toolbar is one pane, with its focused button in
    //  m_toolbarFocus; FocusRing defines the Tab order.
    enum class Pane { Toolbar, Address, Tabs, Tree, List, PreviewToolbar, GoTo, Search, Preview };

    ////////////////////////////////////////////////////////////////////////////
    //
    //  PreviewBytes
    //
    //  The previewed file's bytes, as a source for the hex view. The browser
    //  owns the preview content and replaces it on every selection change, so
    //  this stores a pointer rather than a copy, updated each time the preview
    //  is refilled.
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

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FileBytes
    //
    //  A host file, as a source for the hex view. The view asks only for the
    //  rows it draws, and those come from a 1 MB window of the file that moves
    //  when a row falls outside it, so a file of any size costs about the
    //  same. A read larger than half the window, as a search makes, goes to
    //  the file directly.
    //
    ////////////////////////////////////////////////////////////////////////////

    class FileBytes : public IDxuiHexSource
    {
    public:
        ~FileBytes() override { Close(); }

        bool  Open  (const std::wstring & path);
        void  Close ();

        //  Whether the start of the file is printable ASCII, tabs and line
        //  breaks, and nothing else.
        bool  LooksLikeText () const;

        const std::wstring &  GetPath () const { return m_path; }

        uint64_t  GetByteCount () const override { return m_size; }
        void      ReadBytes    (uint64_t offset, std::span<uint8_t> out) const override;

        static constexpr size_t  kWindowBytes     = 1024 * 1024;
        static constexpr size_t  kTextSampleBytes = 64 * 1024;

    private:
        void  ReadAt (uint64_t offset, std::span<uint8_t> out) const;

        HANDLE                        m_file       = INVALID_HANDLE_VALUE;
        std::wstring                  m_path;
        uint64_t                      m_size       = 0;
        mutable std::vector<uint8_t>  m_window;
        mutable uint64_t              m_windowBase = 0;
    };

    void  ConfigureWidgets();
    void  ApplyTheme();
    void  AdoptSystemColors();
    void  SelectTheme (const char * name);

    static bool  IsCassoThemeName (const std::string & name);
    void  RecomputeLayout();
    void  FillList();
    void  RevealLocationInTree();
    int   WalkTreeLabels (int row, const std::wstring & path);
    void  FillTabs();
    void  FillAddress();
    void  SubmitAddress (const std::wstring & text);
    void  ShowAddressMenu (int index, const RECT & anchor);
    void  ShowAddressOverflowMenu (const RECT & anchor);
    void  ShowAddressHistoryMenu  (const RECT & anchor);

    //  What an empty list says, named for the kind of thing being looked at.
    static std::wstring  GetEmptyLocationMessage (Location::Kind kind);

    //  The file list's column widths from the last run.
    void  ApplyStoredColumnWidths ();
    void  ShowHistoryMenu (bool forward, const RECT & anchor);

    static std::wstring  EscapeMnemonics (const std::wstring & text);
    static BrowserModel::AddressRoot  GetProfileRoot();
    void  SwitchToTab (size_t index);
    void  FillPreview();
    void  FillStatus();
    void  SetFocusPane (Pane pane);

    FocusStop               GetFocusStop     () const;
    void                    SetFocusStop     (const FocusStop & stop);
    std::vector<FocusStop>  BuildFocusStops  () const;
    bool                    RouteToolbarKey  (bool preview, const DxuiKeyEvent & ev);
    void                    StepToolbarFocus (bool preview, bool forward);
    void  Dispatch     (int id);
    bool  IsEnabled    (int id) const;
    bool  IsChecked    (int id) const;
    void  ShowAbout();

    void  ShowListContextMenu (int x, int y);
    void  ShowHexContextMenu  (int x, int y);
    void  ShowTextContextMenu (int x, int y);
    void  GoToTyped (const std::wstring & text);
    void  SetHexGrouping (int grouping);

    //  Stands for a separator in a list of command ids.
    static constexpr int  kSeparatorId = 0;

    //  The focused pane's control, where routing of a standard command such as
    //  Copy or Select all starts.
    IDxuiControl *  GetFocusedControl() const;

    //  Whether the preview pane currently displays the hex view rather than
    //  lines, a picture or a message. Key, copy and Tab routing in the pane
    //  depend on it.
    bool  IsHexPreviewShowing() const;
    bool  IsTextPreviewShowing() const;

    //  The text view's rows for a preview: a BASIC line as its number and its
    //  statement, a detail as its label and value, anything else as one cell.
    static std::vector<DxuiTextView::Row>  BuildTextRows   (const PreviewContent & preview, bool lineAddresses);
    static std::vector<Word>               GetLineAddresses (const std::vector<Byte> & program, bool integerBasic);
    void  OnSearchChanged  (const std::wstring & text);
    void  FindNext         (bool incremental = false);
    void  SetHexColumns    (int columns);
    void  SetPreviewZoom   (int percent);
    void  LayoutStatusFields ();

    static std::wstring  FormatPreviewError (const std::wstring & message);
    void  SetHexFormat     (const char * format);
    void  SetHexShowValues (bool show);
    int   GetPreviewStopIndex (int commandId) const;

    static DxuiHexView::ValueFormat  ParseHexFormat (const std::string & name);
    static std::vector<std::wstring>       SplitLineNumber (const std::wstring & line);
    bool  RouteToolbarMouse   (DxuiToolbar & toolbar, const DxuiMouseEvent & ev);

    static int64_t  GetNowMs();
    void  BeginDragOut();
    void  OnDropFile (const std::wstring & path);
    CassqueActions::AddressFn  MakeAddressPrompt();
    void  ShowTreeContextMenu (int x, int y, const std::wstring & id);

    //  The tab strip's menu, and the pieces the menus share: one command row,
    //  Copy as path over the selection, and Properties, which is Windows' own
    //  sheet for a host item and the catalog details for an entry in an image.
    void  ShowTabContextMenu     (int x, int y, int index);
    void  AddMenuCommand         (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch, const wchar_t * accelerator = L"");
    void  CopySelectedPaths      ();
    void  ShowRowProperties      (int row);
    void  ShowLocationProperties (const Location & location);
    void  ShowHostProperties     (const std::wstring & path);
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
    Win32ShellIcons                              m_shellIcons;
    std::vector<std::unique_ptr<DxuiCommand>>    m_menuCommands;
    std::vector<Win32IntentChannel::Reply>       m_pendingReplies;
    bool                                         m_dragArmed         = false;
    int                                          m_cassoDriveCount   = 0;
    DxuiDragDropTarget                           m_dropTarget;
    DxuiHitTester                                m_dropHits;
    POINT                                        m_dragStart         = {};
    CassqueCommands                              m_commands;
    DxuiLightTheme                               m_lightTheme;
    DxuiDarkTheme                                m_darkTheme;
    CassoTheme                                   m_cassoTheme;
    const DxuiTheme                            * m_theme             = nullptr;
    DxuiDpiScaler                                m_scaler;
    RECT                                         m_client            = {};
    RECT                                         m_previewRect       = {};
    Location                                     m_listLocation;
    bool                                         m_treeRevealPending = false;
    Pane                                         m_focus             = Pane::Tree;
    int                                          m_toolbarFocus      = 0;
    std::vector<BrowserModel::AddressSegment>    m_addressSegments;
    BrowserModel::AddressRoot                    m_addressRoot;

    DxuiMenuBar          * m_menuBar         = nullptr;
    DxuiTreeView         * m_tree            = nullptr;
    DxuiSplitter         * m_treeSplitter    = nullptr;
    DxuiListView         * m_list            = nullptr;
    DxuiLabel            * m_listMessage     = nullptr;
    DxuiSplitter         * m_previewSplitter = nullptr;
    DxuiListView         * m_previewList     = nullptr;
    DxuiTextView         * m_textView        = nullptr;
    DxuiHexView          * m_hexView         = nullptr;
    PreviewBytes           m_previewBytes;
    FileBytes              m_fileBytes;
    DxuiFramebufferView  * m_picture         = nullptr;
    DxuiLabel            * m_previewMessage  = nullptr;
    DxuiStatusBar        * m_status          = nullptr;
    DxuiTabStrip         * m_tabs            = nullptr;
    DxuiToolbar          * m_toolbar         = nullptr;
    DxuiToolbar          * m_previewToolbar  = nullptr;

    //  What the preview toolbar holds: 0 for nothing, 1 for a listing's
    //  toggle, 2 for the hex view's commands.
    int                    m_previewBarMode  = 0;
    int                    m_previewBarFocus = 0;

    //  The last search, as typed and as the bytes it matches.
    std::wstring           m_findTyped;
    std::vector<Byte>      m_findBytes;
    bool                   m_findIsText      = false;
    DxuiToolbarEditBox     m_searchBox;
    DxuiToolbarEditBox     m_goToBox;
    DxuiAddressBar       * m_address         = nullptr;
    DxuiTooltip            m_tooltip;

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
};
