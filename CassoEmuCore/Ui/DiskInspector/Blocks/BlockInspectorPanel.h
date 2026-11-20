#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockHeaderView.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"
#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"
#include "Ui/DiskInspector/Blocks/BlockJobQueue.h"
#include "Ui/DiskInspector/Blocks/BlockNavigator.h"
#include "Ui/DiskInspector/Blocks/BlockPanelSplitters.h"

class BlockFieldsView;
class BlockHexView;
class BlockInspectorModel;
class BlockMapView;
class EditBar;
class FloatingGroup;
class OverlayChip;
struct BlockField;
struct EditBarState;
struct InspectorCommands;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorPanel
//
//  Block mode's content between the menu bar and the status band, laid out
//  as Casso Explorer's window is. Across the top, the address bar, the
//  search box and Go to. Under them, BlockNavigator's tree of the card, its
//  partitions and each volume's directories; the grouped list of what is
//  selected in the tree, or for a block, the block's number with the
//  previous and next block buttons and its capsules, the field line, Copy
//  block, a row with the pending edits or a long job's progress, and the
//  block's hex view over its fields; and the map of the whole volume, with
//  the platter's corner groups: the overlays at the bottom left, and zoom -
//  and + at the bottom right. Splitters part
//  the three. A click on the map selects the row that covers the block,
//  and a double-click opens the block.
//
//  The model's reads run as jobs on the window's BlockInspectorWorker: a
//  block is loaded before it is drawn.
//  While a job is queued or running, input to the panel is ignored and the
//  model isn't read.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorPanel : public DxuiPanel, public IBlockView, public IBlockNavigation, public IBlockDisplay
{
public:
    static constexpr float  kPadDip       = 8.0f;
    static constexpr float  kSplitterDip  = 6.0f;
    static constexpr float  kSearchDip    = 220.0f;
    static constexpr int    kMinHexBytes  = 8;       // the fewest bytes in a hex row when the fields are fitted
    static constexpr float  kHexScrollDip = 12.0f;   // room for the hex view's vertical scroll bar
    static constexpr int    kBytesPerRow  = 16;

    using PressedFn = std::function<void (IDxuiControl * child)>;
    using ChangedFn = std::function<void ()>;
    using ActionFn  = std::function<void ()>;
    using HintFn    = std::function<void (const std::wstring & hint)>;

    BlockInspectorPanel  ();
    ~BlockInspectorPanel () override;

    //  `commands` is the window's commands, for the toolbar row; null for a
    //  panel with no toolbar row, as in a test.
    void  Init (BlockInspectorModel * model, BlockInspectorWorker * worker, const IDxuiTheme * theme, HWND hwnd, IDxuiTextRenderer * text,
                const InspectorCommands * commands = nullptr);

    void  SetOnChildPressed (PressedFn fn) { m_onPressed = std::move (fn); }
    void  SetOnChanged      (ChangedFn fn) { m_onChanged = std::move (fn); }
    void  SetOnJobQueued    (ActionFn fn)  { m_onQueued  = std::move (fn); }
    void  SetOnCopy         (ActionFn fn)  { m_onCopy    = std::move (fn); }
    void  SetOnHint         (HintFn fn)    { m_onHint    = std::move (fn); }

    //  Reads the model again after a change, once the current block is
    //  loaded. A block not loaded yet is loaded by a job first.
    void  Refresh () override;

    //  The controls enabled or disabled, and the progress read, from the
    //  worker's jobs. The window calls it on each frame.
    void  UpdateBusy ();

    IBlockJobs  & GetJobs   ()       { return m_jobs; }
    bool          IsBusy    () const { return m_jobs.IsBusy(); }
    bool          IsLongJob () const { return m_jobs.IsLongJob(); }

    //  A block number typed in decimal, or in hex after '$'.
    static bool  TryParseBlock (const std::wstring & text, uint32_t & outBlock);

    //  Steps to a block, or back or forward, and refreshes. OpenBlock draws
    //  a block below the list without leaving the list's place, and moves
    //  the keyboard focus to the hex view.
    void  SelectBlock (uint32_t block) override;
    void  OpenBlock   (uint32_t block);
    void  GoToBlock   (uint32_t block) override;

    //  Editing, as Edit block turns on and off: typing in the hex view
    //  changes the block only while it is on. The edit bar's state is the
    //  window's.
    void       SetEditing   (bool isEditing) override;
    bool       IsEditing    () const override { return m_isEditing; }
    void       SetEditState (const EditBarState & state) override;
    EditBar  & GetEditBar   () const override { return m_header.GetEditBar(); }
    void       Step         (int64_t delta) override;
    void       GoBack       () override;
    void       GoForward    () override;

    //  The overlays, as the corner chips and the View menu turn them.
    bool  IsFilesOn      () const override { return m_isFilesOn; }
    bool  IsDiffsOn      () const override { return m_isDiffsOn; }
    void  SetFilesOn     (bool isOn) override;
    void  SetDiffsOn     (bool isOn) override;

    //  The levels, Card, Volume, Files, File and Block.
    BlockNavigator        & GetNavigator ()       override { return m_nav; }
    const BlockNavigator  & GetNavigator () const override { return m_nav; }

    //  The current block as a hex dump with its text, for Copy block.
    std::wstring  BuildBlockText () const override;

    //  "Selected: block 2" at the Block level, the path of the file or the
    //  directory at the Files and File levels, the volume's name at the
    //  Volume level, or empty.
    std::wstring  GetSelectionLine () const override;


    //  The popup host for a cut-off field row's tip.
    void  SetPopupHost (DxuiHwndSource * host) { m_fieldTip.SetPopupHost (host); }

    //  The tip for a byte of the hex view under a point: its field, and what
    //  its color marks. False where there is no byte.
    bool  GetHexTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    BlockMapView  & GetMap     () const override { return *m_map; }
    BlockHexView  * GetHexView () const override { return m_hex; }

    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    //  A cut-off field row under the pointer, drawn whole over the row.
    void  UpdateFieldTip (const DxuiMouseEvent & ev);

    //  The most bytes in a hex row that fit `widthPx`, and the fields list
    //  as wide as its rows.
    void  FitHexRows (int widthPx);
    void  FitFields  ();

    void  InitCorners       ();
    void  InitRightColumn   (const IDxuiTheme * theme, HWND hwnd);
    bool  IsBlockLevel      () const;
    void  OnLevelChanged    ();
    void  OnMapSelect       (uint32_t block);
    bool  OnType            (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch);
    bool  OnNavigateKey     (const DxuiKeyEvent & ev);
    void  MarkField         (const BlockField * field);
    void  ShowBytes         ();
    void  Present           ();
    void  LayoutGrid        (const RECT & column, const DxuiDpiScaler & scaler);
    void  LayoutMiddle      (const RECT & column, const DxuiDpiScaler & scaler);
    void  ApplyHighlight    ();

    BlockInspectorModel   * m_model           = nullptr;
    IDxuiTextRenderer     * m_text            = nullptr;
    DxuiDpiScaler           m_scaler;
    DxuiToolbar           * m_tools           = nullptr;
    BlockMapView          * m_map             = nullptr;
    FloatingGroup         * m_chipsGroup      = nullptr;
    FloatingGroup         * m_zoomGroup       = nullptr;
    OverlayChip           * m_filesChip       = nullptr;
    OverlayChip           * m_diffsChip       = nullptr;
    DxuiIconButton        * m_zoomOut         = nullptr;
    DxuiIconButton        * m_zoomIn          = nullptr;
    BlockNavigator          m_nav;
    BlockHexView          * m_hex             = nullptr;
    BlockFieldsView       * m_fields          = nullptr;
    BlockHexSource          m_source;
    BlockPanelSplitters     m_splitters;
    BlockJobQueue           m_jobs;
    BlockHeaderView         m_header;
    PressedFn               m_onPressed;
    ChangedFn               m_onChanged;
    ActionFn                m_onQueued;

    ActionFn                m_onCopy;
    HintFn                  m_onHint;
    bool                    m_isFilesOn       = true;
    bool                    m_isDiffsOn       = true;
    DxuiInPlaceTip          m_fieldTip;
    const IDxuiTheme      * m_theme           = nullptr;
    bool                    m_isMapSelecting  = false;   // a click on the map is selecting its row, so the map stays where it is
    bool                    m_isEditing       = false;
    bool                    m_isRecording     = false;   // the next block drawn goes in the Back history
    bool                    m_isStepping      = false;   // the next block drawn replaces the current location
    int                     m_laidOutLevel    = -1;   // the level the right column was last laid out for
    int64_t                 m_shownBlock      = -1;
    int                     m_hexContentPx    = 0;   // the hex view's width for all its columns when the column was laid out
};
