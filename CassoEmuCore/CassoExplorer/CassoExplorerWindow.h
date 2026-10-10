#pragma once

#include "Pch.h"

#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/CassoExplorerNamedControl.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/Model/FocusRing.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/IFileSystem.h"
#include "Seams/Win32HostDialogs.h"
#include "Seams/Win32IntentChannel.h"
#include "Seams/Win32ProcessLauncher.h"
#include "Seams/Win32ShellIcons.h"
#include "Seams/Win32InfoTips.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "CassoExplorer/Model/FolderWatch.h"
#include "CassoExplorer/Model/RefreshAnchor.h"





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
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerWindow : public DxuiWindow
{
public:
    struct Context
    {
        IFileSystem   * fs          = nullptr;
        std::wstring    baseDir;
        HWND            owner       = nullptr;
        std::wstring    titlePrefix;
        std::wstring    openPath;       // from the command line; empty: none

        //  Optional: without one, the browser shows what it read when it read
        //  it, and a change made elsewhere is seen on the next navigation.
        IFolderWatcher * watcher    = nullptr;
    };

    CassoExplorerWindow (CassoExplorerBrowser & browser, CassoExplorerActions & actions, CassoExplorerPrefs & prefs, Context context);
    ~CassoExplorerWindow() override;

    HRESULT  Open (HINSTANCE instance, const std::wstring & title, int showCommand);

    //  A second launch hands its path to the window already open, in a
    //  WM_COPYDATA with this id and the full path as UTF-16 text.
    static constexpr ULONG_PTR  kOpenPathCopyId = 0x43454F50;   // 'CEOP'

    //  Opens a folder, a disk image, or a file's folder in a new tab.
    void  OpenPathInNewTab (const std::wstring & path);

    //  The window's placement in the preferences' terms, for saving on exit.
    void  StorePlacement();

    //  Records the session -- tabs, typed paths, placement -- and saves the
    //  preferences now, rather than only on a clean exit.
    void  SaveSession();

    void    Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool    OnMouse           (const DxuiMouseEvent & ev) override;
    bool    OnKey             (const DxuiKeyEvent & ev) override;
    LPCWSTR GetCursorForPoint (POINT clientPx) const override;

    //  Windows changed its light or dark setting or its accent: Follow
    //  system takes the new one at once, and every theme takes the accent.
    void    OnThemeChanged    () override;

    static constexpr int       kMaxCatalogName     = 30;
    static constexpr UINT_PTR  kTooltipTimerId     = 0x5153;

    static constexpr const wchar_t *  kCassoNodeTip = L"Folders Casso has opened disk images from, most recently used first";

    static constexpr const wchar_t *  kExplorerTypedPathsKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\TypedPaths";
    static constexpr size_t           kMaxCompletions        = 200;

    static constexpr const wchar_t *  kExplorerIconFolder    = L"\\SystemApps\\MicrosoftWindows.Client.FileExp_cw5n1h2txyewy\\FileExplorerExtensions\\Assets\\images\\contrast-standard\\";

    //  A 5.25-inch floppy in File Explorer's icon hand: a 16-unit jacket with
    //  the write-protect notch cut in its edge, its hub ring and index hole,
    //  and the head slot in the accent. {INK}, {ACC}
    //  and {BODY} take the theme's outline, accent and fill.
    static constexpr const char *     kFloppy525Svg =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" fill=\"none\" viewBox=\"0 0 16 16\">"
        "<path fill=\"{BODY}\" stroke=\"{INK}\" d=\"M13.5 1.5h-11a1 1 0 0 0-1 1v11a1 1 0 0 0 1 1h11a1 1 0 0 0 1-1V5.5h-1V3.5h1v-1a1 1 0 0 0-1-1Z\"/>"
        "<circle cx=\"8\" cy=\"6.5\" r=\"2.25\" stroke=\"{INK}\"/>"
        "<circle cx=\"4.5\" cy=\"6.5\" r=\".6\" fill=\"{INK}\"/>"
        "<rect x=\"7.25\" y=\"10\" width=\"1.5\" height=\"4\" rx=\".75\" fill=\"{ACC}\"/>"
        "</svg>";
    static constexpr uint64_t  kDos33ImageBytes = 35 * 16 * 256;
    static constexpr UINT      kTooltipTickMs   = 16;   // the menus' reveal runs on it too, so display rate
    //  Explorer's tabs are 33 dip tall. Its strip is 9 dip taller because it is
    //  also the window's caption; this window has a caption bar of its own, so
    //  the strip keeps only a small gap above the tabs.
    static constexpr int       kTabHeightDip       = 37;
    static constexpr int       kTabTopDip          = 4;
    static constexpr float     kTabLeadDip         = 8.67f;  // the first tab from the strip's left edge, measured at 150%
    static constexpr float     kNewTabGapDip       = 6.67f;  // the last tab to the + button

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

    //  Explorer's two strips, measured at 100, 125, 150 and 200%: the command
    //  bar is 47 dip rounded down, its line included; the address bar's strip
    //  is 48 dip rounded down with its line added. The address box is 32 dip
    //  tall, and the command bar starts 4 dip in.
    static constexpr float     kCommandBarDip        = 47.0f;
    static constexpr float     kNavStripFillDip      = 48.0f;
    static constexpr int       kAddressBoxDip        = 32;
    static constexpr int       kFindBoxMinDip        = 100;
    static constexpr int       kFindBoxMaxDip        = 720;
    static constexpr float     kFindBoxShare         = 0.3f;    // of the width right of the tree, measured at 150%
    static constexpr float     kFindBoxRightInsetDip = 5.33f;   // past the toolbar's own padding
    static constexpr int       kFindBoxGapDip        = 8;
    static constexpr int       kCommandBarPadXDp     = 6;
    static constexpr UINT      kListRowHalfDip       = 14;

    //  Loaded at this size and scaled down by the caption, as Casso's is.
    static constexpr int       kCaptionIconPx      = 32;
    static constexpr int       kMenuIconDip        = 16;     // DxuiPopupMenu's row icon

    //  The preview's rows hold one line of fixed-width text each, so they
    //  are the line's height rather than a file listing's roomier row.
    static constexpr int  kPreviewRowHeightDip = 18;
    static constexpr int  kTabWidthDip         = 240;
    static constexpr int  kTabMinWidthDip      = 100;
    static constexpr int  kMinTreeWidthDip     = 140;
    static constexpr int  kMinListWidthDip     = 220;
    static constexpr int  kMinPreviewWidthDip  = 280;   // wide enough for the hex view's toolbar
    static constexpr int  kMinWindowWidthDip   = 200;

    //  Where DOS 3.3 on a 48K machine leaves HIMEM, against which Integer BASIC
    //  keeps its program.
    static constexpr int  kIntegerBasicHimem   = 0x9600;

    //  Preview text is drawn this far from the background toward the theme's
    //  foreground, a gray like a terminal's text rather than full white.
    static constexpr float  kPreviewTextStrength = 0.8f;

    //  Each zoom step, in percent of the theme's text size.
    static constexpr int    kPreviewZoomStep     = 10;

    //  The widest a pane's message runs before it wraps.
    static constexpr int     kMessageWidthDip = 400;

    //  How far each level of the tree sits in from its parent: File
    //  Explorer's, measured at 125% as 10 pixels.
    static constexpr int     kTreeIndentDip   = 8;

    //  A tree row's icon starts 18 dip past the middle of its twisty and its
    //  label 3.33 dip past the icon: measured at 150%.
    static constexpr float   kTreeIconLeadDip = 10.0f;
    static constexpr float   kTreeIconGapDip  = 3.33f;
    static constexpr float   kTreeLeftPadDip  = 8.0f;    // the top level from the pane's left edge

    //  The status bar's fields, left to right: Explorer's item count and
    //  selection, flowing from the left; the space between; then free space,
    //  the preview's detail and its zoom.
    static constexpr size_t  kStatusCount    = 0;
    static constexpr size_t  kStatusSelected = 1;
    static constexpr size_t  kStatusFill     = 2;
    static constexpr size_t  kStatusFree     = 3;
    static constexpr size_t  kStatusDetail   = 4;
    static constexpr size_t  kStatusZoom     = 5;

    //  Status bar field widths: free space, and the preview's detail and zoom
    //  when the preview is hidden and they cannot follow its edge.
    static constexpr int    kStatusBandDip       = 26;    // Explorer's status bar, measured at 150%
    static constexpr float  kStatusLeadDip       = 14.67f; // its item count, from the band's left edge
    static constexpr int    kStatusFreeDip       = 140;
    static constexpr int    kStatusDetailDip     = 280;
    static constexpr int    kStatusZoomDip       = 64;

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

public:
    static constexpr UINT_PTR  kFolderTimerId        = 0x5154;
    static constexpr UINT_PTR  kRestoreTimerId       = 0x5155;   // re-reads while the Recycle Bin restores
    static constexpr UINT_PTR  kRecycleBinTimerId    = 0x5156;   // settles the bin's changes before a re-read
    static constexpr int       kRestoreRetries       = 5;

    //  How long to let a burst settle before re-reading. Copying a hundred
    //  files reports a hundred changes; re-reading once at the end is both
    //  faster and steadier to look at.
    static constexpr UINT      kFolderSettleMs       = 200;
    static constexpr ULONGLONG kFolderMaxSettleMs    = 1000;  // the longest a busy folder holds off a re-read

    //  Which pane a drop landed on.
    static constexpr int       kDropTagList          = 0;
    static constexpr int       kDropTagTree          = 1;
    static constexpr int       kDropTagTabs          = 2;

    //  The strip a pointer event goes to: the command bar over its band, the
    //  navigation toolbar anywhere else.
    //  The panes' widths in a body this wide, from the widths the user left
    //  them at. The list gives up width first, down to its minimum; then the
    //  preview, down to its own; then the tree. Past all three minimums the
    //  three shrink together. A preview width of zero is a hidden preview.
    struct PaneWidths
    {
        int  tree    = 0;
        int  list    = 0;
        int  preview = 0;
    };

    static PaneWidths  FitPanes (int bodyDip, int treeDip, int previewDip);

    //  Whether a list column shows: the user's choice from the header's menu,
    //  and the catalog's two columns only inside a disk image.
    static bool  IsListColumnShown (size_t column, bool chosen, Location::Kind kind, bool searching = false);
    void         ApplyColumnOrder  ();

    //  File Explorer's list row, measured at nine scales from 100% to 350%:
    //  twice 14 dip rounded up, and a pixel more at any scale that is not a
    //  whole multiple -- 28, 37, 43, 51, 56, 65, 71, 84 and 99 pixels.
    static int  GetListRowHeightPx (UINT dpi);

    //  The address box: the toolbar's free span, 32 dip tall and centered
    //  in the strip above its bottom line.
    static RECT  GetAddressRect (const RECT & free, const RECT & strip, const DxuiDpiScaler & scaler);

    static DxuiToolbar &  GetToolbarUnder (const RECT & commandBarBand, POINT point, DxuiToolbar & navToolbar, DxuiToolbar & commandBar);

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

private:
    //  Keyboard focus. The toolbar is one pane, with its focused button in
    //  m_toolbarFocus; FocusRing defines the Tab order.
    enum class Pane { Toolbar, Address, Tabs, Tree, List, PreviewToolbar, GoTo, Search, Preview, CommandBar, LocationSearch };

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
    void  PreviewTheme (int index);
    void  SelectTheme  (const char * name);

    static bool  IsCassoThemeName (const std::string & name);
    void  RecomputeLayout();
    void  FillList();
    void  RevealLocationInTree();

    //  Pin to Quick access, or Unpin, for a folder, as Explorer's menus have it.
    void  AddPinMenuCommand (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder);
    void  OnShellListed     (const std::wstring & key);

    //  A verb's label as a window's title: no access key, no trailing dots.
    static std::wstring  GetVerbTitle (CassoExplorerActions::Verb verb);

    //  The raw read and write pickers' own history, apart from other pickers.
    static constexpr GUID  s_kRawPickerGuid = { 0xe347dd39, 0x4a18, 0x4a0d, { 0x93, 0x4a, 0xfd, 0x63, 0xb7, 0x9c, 0xba, 0x3e } };
    void  ScrollTreeToRow (int row);
    int   WalkTreeLabels (int row, const std::wstring & path);
    void  FillTabs();
    void  FillAddress();
    std::vector<BrowserModel::AddressSegment>  GetAddressSegmentsFor (const Location & location);
    void  SubmitAddress (const std::wstring & text);
    void  ShowAddressMenu (int index, const RECT & anchor);
    void  ShowAddressOverflowMenu (const RECT & anchor);
    void  ShowAddressRootsMenu    (const RECT & anchor);
    void  OpenSelectedEntries     ();
    void  SearchLocation          (const std::wstring & query);
    bool  OnFindBoxKey            (const DxuiKeyEvent & ev);
    void  ShowAddressHistoryMenu  (const RECT & anchor);

    //  The list under the address bar: the history when nothing is typed,
    //  what the typed path could go on to be once something is.
    void  ShowAddressSuggestions  (const std::wstring & typed);
    int   ChooseFormatDefault     (const std::wstring & image);
    bool  OnAddressKey            (WPARAM vk);
    std::vector<std::wstring>  GetAddressHistory () const;

    static std::vector<std::wstring>  GetCompletions            (const std::wstring & typed);
    void                              ShowAddressContextMenu    (int x, int y);
    void                              UpdateLabelTip            (POINT point);

    //  What Ctrl+Z puts back, as Explorer's undo does: a deletion to the
    //  Recycle Bin, a rename, a new folder, and inside a disk image a
    //  deletion (from a copy kept aside), a rename or a new folder.
    struct UndoStep
    {
        enum class Kind { Recycle, HostRename, HostNewFolder, ImageRename, ImageNewFolder, ImageDelete, HostCopy, HostMove, ImagePut };

        Kind                       kind = Kind::Recycle;
        Location                   location;    // where an image step was made
        std::vector<std::wstring>  paths;       // host paths, as they are now
        std::wstring               oldName;
        std::wstring               newName;
        std::wstring               savedDir;    // an image deletion's copy
        std::vector<std::wstring>  sources;     // where moved host items were
        std::vector<std::string>   entries;     // what a put or copy made in an image
    };

    void                              PushUndo                  (UndoStep step);
    void                              UndoLast                  ();
    static const wchar_t *            GetUndoLabel              (UndoStep::Kind kind);
    static std::vector<std::wstring>  ReadExplorerTypedPaths    ();
    void                              PasteHere                 (const std::wstring & folder);
    void                              PushPutUndo               (const CassoExplorerActions::Outcome & outcome, const std::wstring & image, const std::string & inner);
    static void                       WriteExplorerTypedPath    (const std::wstring & path);

    //  What an empty list says, named for the kind of thing being looked at.
    static std::wstring  GetEmptyLocationMessage (Location::Kind kind);

    //  The file list's column widths from the last run.
    void  ApplyStoredColumnWidths ();
    void  ApplyColumnWidths       (const std::vector<int> & widthsDip);
    void  RememberFolderColumnWidths (int column, int widthDip);
    void  RememberFolderColumns      ();

    //  The host folders worth watching: the one the list is showing and every
    //  one the tree has open. Cheap enough to call after anything that could
    //  have changed either.
    void  UpdateWatchedFolders ();

    //  Re-reads whatever the watcher reported, once the burst has settled.
    void  RefreshChangedFolders ();
    bool  IsShownFolderIn       (const std::vector<std::wstring> & folders) const;
    bool  RefreshOpenTreeFolders (const std::vector<std::wstring> & folders);
    static bool  IsSameFolder   (const std::wstring & a, const std::wstring & b);
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

    void  ShowListHeaderMenu (int x, int y, int column);
    void  ShowListContextMenu (int x, int y, int group = -1);
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

    //  Which part of the window the hover tooltip belongs to.
    enum class TipOwner { None, Toolbar, Tree, List, Status, Menu };

    void  ShowHoverTip        (TipOwner owner, const RECT & anchor, const std::wstring & text);
    void  HideHoverTip        (TipOwner owner);
    void  UpdateTreeTip       (const DxuiMouseEvent & ev, POINT point);
    void  UpdateStatusTip     (const DxuiMouseEvent & ev, POINT point);
    void  UpdateListTip       (const DxuiMouseEvent & ev, POINT point);
    void  RefreshListTip      ();
    void  ArmTick             ();
    void  RefreshListIcons    ();
    const std::vector<DxuiListView::Cell> &  GetListRowCells (int row);
    bool  IsTickWanted        () const;

    static int64_t  GetNowMs();
    void  BeginDragOut();
    void  OnDropFile (const std::wstring & path);
    CassoExplorerActions::AddressFn  MakeAddressPrompt();
    void  ShowTreeContextMenu (int x, int y, const std::wstring & id);

    //  The menu below the tree's last node, and the pane options it sets.
    void  ShowTreeEmptyMenu   (int x, int y);
    void  AddNavPaneToggle    (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, CassoExplorerPrefs::NavOption option);
    void  ApplyNavPaneOptions ();
    bool  IsNavOptionOn       (CassoExplorerPrefs::NavOption option) const;

    //  The tab strip's menu, and the pieces the menus share: one command row,
    //  Copy as path over the selection, and Properties, which is Windows' own
    //  sheet for a host item and the catalog details for an entry in an image.
    void  ShowTabContextMenu     (int x, int y, int index);
    void  AddMenuCommand         (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch, const wchar_t * accelerator = L"", const wchar_t * menuGlyph = nullptr);
    void  AddOpenWithMenu        (std::vector<DxuiPopupMenuItem> & items);
    static const wchar_t *  GetVerbGlyph (CassoExplorerActions::Verb verb);
    static int              GetIconOrder (CassoExplorerActions::Verb verb);
    std::wstring  GetPasteFolder () const;
    void  CreateDiskFromSelection (const CassoExplorerNewDiskDialog::Outcome & newDisk);
    void  SelectRowNamed         (const std::wstring & name);
    void  ShowOptions            ();
    void  SizeListIcons          ();
    void  ShowViewOnButton       ();
    bool  IsListVerbOffered      (CassoExplorerActions::Verb verb) const;
    bool  IsToolbarEntryAvailable (int index) const;
    bool  IsCommandBarEntryAvailable (int index) const;
    HostFileNaming::Style  GetNamingStyle () const;
    void  AddCopyAsMenu          (std::vector<DxuiPopupMenuItem> & items);
    void  CopyEntriesToClipboard (HostFileNaming::Style style);
    bool  RouteCommandBarKey      (const DxuiKeyEvent & ev);

    DxuiToolbar          * m_commandBar      = nullptr;
    void  SetCommandBarDropDowns ();

    //  Drag-in: the image location under a drop point, what a drop there
    //  would do, and the drop.
    bool  TryGetDropLocation     (int tag, POINT screen, Location & outLocation);
    DWORD GetDropEffect          (IDataObject * data, int tag, POINT screen);

    //  A host folder under the drag takes it as Explorer would: the drag goes
    //  to the folder's own shell drop target, which copies or moves it with
    //  Explorer's progress, conflict and undo handling.
    bool  TryGetHostDropFolder   (int tag, POINT screen, std::wstring & outFolder);
    DWORD ForwardHostDrag        (IDataObject * data, const std::wstring & folder, POINT screen);
    void  LeaveHostDrop          ();
    void  ShowDropTarget         (int tag, POINT screen, bool accepted);
    void  ShowDropMenu           ();
    void  RunDrop                (CassoExplorerActions::Conversion conversion);
    void  ClearDropTarget        ();
    void  OnDrop                 (IDataObject * data, int tag, POINT screen);

    //  What a drag holds: another image's entries, or host files.
    struct DropSource
    {
        bool                       fromImage = false;
        std::string                image;
        VolumeKind                 kind      = VolumeKind::Unknown;
        std::vector<std::string>   catalogPaths;
        std::vector<std::wstring>  hostPaths;
    };

    bool  ReadDropSource   (IDataObject * data, DropSource & outSource);
    DWORD ChooseDropEffect (const DropSource & source, const Location & target) const;
    void  DescribeDrop     (IDataObject * data, DWORD effect, const Location & target);
    void  HoverDropTab     (POINT screen);

    static void  RecycleHostFiles (const std::vector<std::wstring> & paths);
    void  RefreshAfterHostChange ();

    //  The part of a pane a message wraps within.
    RECT  GetMessageRect (const RECT & pane) const;

    //  Explorer's folder options read again; true when one changed.
    bool  ReadFolderOptions ();

    //  The folder the list shows, as FolderViews keys it, and its type when
    //  asked for, which a host folder costs a read of its desktop.ini.
    std::wstring  GetFolderViewKey (FolderViews::FolderType * outType) const;

    //  The list in the view of the folder it now shows, when that folder is
    //  not the one the view was last set for.
    void          ApplyFolderView ();
    void          RememberFolderSort ();
    void  CopySelectedPaths      ();
    void  OpenEachSelected       ();
    void  ShowRowProperties      (int row);
    void  ShowLocationProperties (const Location & location);
    void  ShowHostProperties     (const std::wstring & path);
    void  ChangeKnownFolder   (const std::wstring & folder, bool add);

    //  Re-reads the tree, keeping what was open, highlighted and on screen.
    void  RefreshTree();
    void  RunVerb             (CassoExplorerActions::Verb verb);
    bool  RunRecycleBinVerb (CassoExplorerActions::Verb verb);
    std::wstring  GetCommandLabel (int id) const;
    void  RunRawVerb          (CassoExplorerActions::Verb verb);
    void  ReportOutcome       (const CassoExplorerActions::Outcome & outcome, const wchar_t * verbName);
    void  BeginRename         ();
    void  EndRename           (bool commit);
    void  InsertIntoDrive     (const std::wstring & imagePath, int drive);
    void  OpenInNewCasso      (const std::wstring & imagePath);
    HWND  FindCassoTarget     () const;
    int   GetDefaultMachineDriveCount();
    void  AskCassoToDescribe  ();
    void  ShowMessage         (const std::wstring & text, UINT icon);
    std::wstring  GetSelectedImagePath() const;

    static const wchar_t *  GetVerbLabel (CassoExplorerActions::Verb verb);
    static const wchar_t *  GetVerbMenuGlyph (CassoExplorerActions::Verb verb);

    //  File Explorer's own menu icons, read from its package on this machine
    //  for the theme in use, or null where it has none; Casso's own art for a
    //  name that starts with "casso.". Held for the window's life.
    static const wchar_t *  GetVerbMenuSvgName (CassoExplorerActions::Verb verb);

    //  An icon-row button's tip, as Explorer words it: "Cut (Ctrl+X)".
    static std::wstring     GetIconButtonTip   (const DxuiCommand & command);
    void                    ApplyMenuSvgs      ();
    const std::string *     GetMenuSvg         (const wchar_t * name);

    //  The icon of the program that opens an item, for its Open row: Casso
    //  Explorer's own for what it browses, the file's default program's for
    //  anything else; null when there is none. Held for the window's life.
    std::shared_ptr<const DxuiIconImage>  GetOpenMenuImage (bool browsable, const std::wstring & path);

    static bool  Contains (const RECT & rect, POINT point);
    static DxuiMouseEvent  ToLocal (const DxuiMouseEvent & ev, const RECT & bounds);

    CassoExplorerBrowser                       & m_browser;
    CassoExplorerActions                       & m_actions;
    CassoExplorerPrefs                         & m_prefs;
    FolderOptions                                m_explorerOptions;
    bool                                         m_opened            = false;
    bool                                         m_inSizeMove        = false;
    Context                                      m_context;
    Win32HostDialogs                             m_dialogs;
    Win32ProcessLauncher                         m_launcher;
    Win32ShellIcons                              m_shellIcons;
    Win32ShellIcons                              m_listIcons;
    Win32InfoTips                                m_infoTips;
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
    int                                       m_cassoDriveCount     = 0;
    int                                       m_defaultDriveCount   = -1;  // the default machine's, once read
    DxuiDragDropTarget                        m_dropTarget;
    ComPtr<IDropTarget>                       m_hostDrop;   // the host folder the drag is over, if any
    std::wstring                              m_hostDropFolder;
    DxuiHitTester                             m_dropHits;
    POINT                                     m_dragStart           = {};
    CassoExplorerCommands                     m_commands;
    DxuiLightTheme                            m_lightTheme;
    DxuiDarkTheme                             m_darkTheme;
    CassoTheme                                m_cassoTheme;
    const DxuiTheme                         * m_theme               = nullptr;
    DxuiDpiScaler                             m_scaler;
    RECT                                      m_client              = {};
    RECT                                      m_previewRect         = {};
    Location                                  m_listLocation;

    //  Why each row's image is broken, as its cells are built; empty for the
    //  rest.
    std::vector<std::wstring>                 m_rowProblems;

    //  Whether the command bar has the Recycle Bin's buttons on it.
    bool                                      m_commandBarForBin    = false;

    //  The list's view, and the key of the folder it was chosen for: a new
    //  folder opens in its own view.
    DxuiListView::View  m_listView          = DxuiListView::View::Details;
    std::wstring        m_listViewKey;
    bool                m_treeRevealPending = false;
    Pane                m_focus             = Pane::Tree;
    int                 m_toolbarFocus      = 0;
    int                 m_commandBarFocus   = 0;

    //  What a right-drag dropped, held between the drop and the menu's
    //  answer: where it landed, and either the host files or the entries of
    //  the image they came from.
    struct PendingDrop
    {
        bool                       valid       = false;
        Location                   location;
        VolumeKind                 targetKind  = VolumeKind::Unknown;
        std::string                inner;
        POINT                      screen      = {};
        std::vector<std::wstring>  hostPaths;
        bool                       fromImage   = false;
        std::string                sourceImage;
        VolumeKind                 sourceKind  = VolumeKind::Unknown;
        std::vector<std::string>   catalogPaths;
    };

    PendingDrop                                  m_pendingDrop;

    int                                          m_treeDropRow       = -1;
    std::vector<BrowserModel::AddressSegment>    m_addressSegments;
    BrowserModel::AddressRoot                    m_addressRoot;

    DxuiMenuBar          * m_menuBar         = nullptr;
    DxuiTreeView         * m_tree            = nullptr;
    DxuiSplitter         * m_treeSplitter    = nullptr;
    DxuiListView         * m_list            = nullptr;

    //  Rename in place: an edit box laid over the row's name, as Explorer's
    //  F2 opens one. The row being renamed, or -1.
    DxuiTextInput        * m_renameBox       = nullptr;
    int                    m_renameRow       = -1;
    DxuiLabel            * m_listMessage     = nullptr;
    DxuiSplitter         * m_previewSplitter = nullptr;
    DxuiListView         * m_previewList     = nullptr;
    DxuiTextView         * m_textView        = nullptr;
    DxuiHexView          * m_hexView         = nullptr;
    PreviewBytes           m_previewBytes;
    FileBytes              m_fileBytes;
    DxuiFramebufferView  * m_picture         = nullptr;
    DxuiSelectableText   * m_previewMessage  = nullptr;
    DxuiStatusBar        * m_status          = nullptr;
    DxuiTabStrip         * m_tabs            = nullptr;
    DxuiToolbar          * m_toolbar         = nullptr;
    DxuiToolbar          * m_previewToolbar  = nullptr;

    //  What the preview toolbar holds: 0 for nothing, 1 for a listing's
    //  toggle, 2 for the hex view's commands.
    int                    m_previewBarMode  = 0;
    int                    m_previewBarFocus = 0;

    //  The last search, as typed and as the bytes it matches.
    std::wstring             m_findTyped;
    std::vector<Byte>        m_findBytes;
    bool                     m_findIsText    = false;
    DxuiToolbarEditBox       m_searchBox;
    DxuiToolbarEditBox       m_goToBox;
    DxuiAddressBar         * m_address       = nullptr;

    //  Explorer's search box, at the address bar's right: a search over the
    //  folder shown and below it.
    DxuiTextInput          * m_findBox       = nullptr;
    DxuiTooltip              m_tooltip;
    DxuiInPlaceTip           m_labelTip;   // a tree name the splitter cuts off, shown whole over its row
    std::vector<UndoStep>    m_undo;   // newest last
    int                      m_restoreTicks  = 0;
    bool                     m_caretOn       = true;   // the search caret as last drawn
    bool                     m_tickArmed     = false;   // the animation tick is running
    POINT                    m_listTipPoint  = {};   // where the pointer last was over the list
    bool                     m_listTipActive = false;

    //  The list's rows, built as it first asks for each; see GetListRowCells.
    std::vector<std::vector<DxuiListView::Cell>>  m_rowCells;
    std::vector<bool>                             m_rowBuilt;
    Location                                      m_rowLocation;
    bool                                          m_rowDark      = false;
    struct RightPress
    {
        bool   active  = false;
        bool   canDrag = false;   // pressed on a row
        int    group   = -1;      // pressed on a group's header
        POINT  start   = {};
    };

    RightPress            m_rightPress;
    std::vector<std::wstring>  m_cutPaths;   // cut to the clipboard and not yet pasted, drawn dimmed
    std::map<std::wstring, std::unique_ptr<std::string>>  m_menuSvgs;
    std::map<std::wstring, std::shared_ptr<const DxuiIconImage>>  m_openMenuImages;
    DxuiSuggestionList    m_suggest;
    std::wstring          m_addressTyped;
    TipOwner              m_tipOwner   = TipOwner::None;
    bool                  m_tipMuted    = false;
    RECT                  m_tipMuteRect = {};

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
