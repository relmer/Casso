#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorWorker.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"
#include "Ui/DiskInspector/Blocks/BlockNavigator.h"

class BlockFieldsView;
class BlockHexView;
class BlockInspectorModel;
class BlockLegendView;
class BlockMapView;
class FloatingGroup;
class OverlayChip;
struct BlockField;
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
//  the platter's corner groups: the legend at the top left, the overlays at
//  the bottom left, and zoom - and + at the bottom right. Splitters part
//  the three. A click on the map selects the row that covers the block,
//  and a double-click opens the block.
//
//  The model's reads run as jobs on the window's BlockInspectorWorker: a
//  block is loaded before it is drawn.
//  While a job is queued or running, input to the panel is ignored and the
//  model isn't read.
//
////////////////////////////////////////////////////////////////////////////////

class BlockInspectorPanel : public DxuiPanel
{
public:
    static constexpr float  kPadDip      = 8.0f;
    static constexpr float  kRowDip      = 28.0f;
    static constexpr float  kLineDip     = 20.0f;
    static constexpr float  kTitleDip    = 16.0f;
    static constexpr float  kFontDip     = 12.0f;
    static constexpr float  kSplitterDip = 6.0f;
    static constexpr float  kMinSplit    = 0.45f;   // the map's left edge, a share of the width
    static constexpr float  kMaxSplit    = 0.85f;
    static constexpr float  kTreeShare   = 0.18f;   // the tree's share of the width at first
    static constexpr float  kMinTree     = 0.08f;
    static constexpr float  kMaxTree     = 0.35f;
    static constexpr float  kSearchDip   = 220.0f;
    static constexpr float  kHexShare    = 0.60f;
    static constexpr float  kBarDip      = 240.0f;
    static constexpr float  kButtonDip   = 110.0f;
    static constexpr int    kBytesPerRow = 16;

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
    void  SetOnApply        (ActionFn fn)  { m_onApply   = std::move (fn); }
    void  SetOnDiscard      (ActionFn fn)  { m_onDiscard = std::move (fn); }
    void  SetOnCopy         (ActionFn fn)  { m_onCopy    = std::move (fn); }
    void  SetOnHint         (HintFn fn)    { m_onHint    = std::move (fn); }

    //  Reads the model again after a change, once the current block is
    //  loaded. A block not loaded yet is loaded by a job first.
    void  Refresh ();

    //  The job queued on the worker. The window is notified, so the job's
    //  completion runs soon after its work ends. With no worker, as in a
    //  test, the job runs on the calling thread.
    void  QueueJob (BlockInspectorWorker::Job job);

    //  The controls enabled or disabled, and the progress read, from the
    //  worker's jobs. The window calls it on each frame.
    void  UpdateBusy ();

    bool  IsBusy     () const { return m_worker != nullptr && m_worker->IsBusy(); }
    bool  IsLongJob  () const { return m_isLongJob; }

    //  A block number typed in decimal, or in hex after '$'.
    static bool  TryParseBlock (const std::wstring & text, uint32_t & outBlock);

    //  Steps to a block, or back or forward, and refreshes.
    void  SelectBlock (uint32_t block);
    void  Step        (int64_t delta);
    void  GoBack      ();
    void  GoForward   ();

    //  The overlays, as the corner chips and the View menu turn them.
    bool  IsFilesOn      () const { return m_isFilesOn; }
    bool  IsDiffsOn      () const { return m_isDiffsOn; }
    void  SetFilesOn     (bool isOn);
    void  SetDiffsOn     (bool isOn);

    //  The levels, Card, Volume, Files, File and Block.
    BlockNavigator        & GetNavigator ()       { return m_nav; }
    const BlockNavigator  & GetNavigator () const { return m_nav; }

    //  The current block as a hex dump with its text, for Copy block.
    std::wstring  BuildBlockText () const;

    //  "Selected: block 2" at the Block level, the path of the file or the
    //  directory at the Files and File levels, the volume's name at the
    //  Volume level, or empty.
    std::wstring  GetSelectionLine () const;


    BlockMapView    & GetMap          () const { return *m_map; }
    BlockHexView    * GetHexView      () const { return m_hex; }
    DxuiIconButton  & GetLegendButton () const { return *m_legendButton; }
    bool              IsLegendShown   () const;

    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    //  The legend is visible while the pointer is over its button or over
    //  the legend, as a tooltip is.
    void  TrackLegendHover (POINT pointPx);

    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    //  A capsule beside the block's number: Free or Used, the block's role,
    //  or a file the block is in. A damaged role's capsule is drawn red.
    struct Capsule
    {
        std::wstring  text;
        bool          isBad = false;
    };

    void  InitCorners       ();
    void  InitRightColumn   (const IDxuiTheme * theme, HWND hwnd);
    bool  IsBlockLevel      () const;
    void  OnLevelChanged    ();
    void  OnMapSelect       (uint32_t block);
    bool  OnType            (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch);
    bool  OnNavigateKey     (const DxuiKeyEvent & ev);
    void  MarkField         (const BlockField * field);
    void  ShowBytes         ();
    void  BuildFieldLine    ();
    void  Present           ();
    void  LayoutGrid        (const RECT & column, const DxuiDpiScaler & scaler);
    void  LayoutMiddle      (const RECT & column, const DxuiDpiScaler & scaler);
    void  PaintHeader       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  PaintProgress     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  ApplyHighlight    ();
    void  BuildCapsules     ();

    BlockInspectorModel   * m_model           = nullptr;
    BlockInspectorWorker  * m_worker          = nullptr;
    IDxuiTextRenderer     * m_text            = nullptr;
    DxuiDpiScaler           m_scaler;
    DxuiToolbar           * m_tools           = nullptr;
    BlockMapView          * m_map             = nullptr;
    FloatingGroup         * m_legendGroup     = nullptr;
    FloatingGroup         * m_chipsGroup      = nullptr;
    FloatingGroup         * m_zoomGroup       = nullptr;
    DxuiIconButton        * m_legendButton    = nullptr;
    BlockLegendView       * m_legend          = nullptr;
    OverlayChip           * m_filesChip       = nullptr;
    OverlayChip           * m_diffsChip       = nullptr;
    DxuiIconButton        * m_zoomOut         = nullptr;
    DxuiIconButton        * m_zoomIn          = nullptr;
    BlockNavigator          m_nav;
    DxuiButton            * m_copy            = nullptr;
    DxuiIconButton        * m_previous        = nullptr;
    DxuiIconButton        * m_next            = nullptr;
    DxuiLabel             * m_editNote        = nullptr;
    DxuiButton            * m_discard         = nullptr;
    DxuiButton            * m_apply           = nullptr;
    DxuiButton            * m_cancel          = nullptr;
    BlockHexView          * m_hex             = nullptr;
    BlockFieldsView       * m_fields          = nullptr;
    BlockHexSource          m_source;
    PressedFn               m_onPressed;
    ChangedFn               m_onChanged;
    ActionFn                m_onQueued;
    ActionFn                m_onApply;
    ActionFn                m_onDiscard;
    ActionFn                m_onCopy;
    HintFn                  m_onHint;
    std::wstring            m_title;
    vector<Capsule>         m_capsules;
    std::wstring            m_fieldLine;
    std::wstring            m_jobLabel;
    uint64_t                m_jobDone         = 0;
    uint64_t                m_jobTotal        = 0;
    bool                    m_isLongJob       = false;
    bool                    m_isFilesOn       = false;
    bool                    m_isDiffsOn       = true;
    bool                    m_isSplitting     = false;
    bool                    m_isTreeSplitting = false;
    bool                    m_isMapSelecting  = false;   // a click on the map is selecting its row, so the map stays where it is
    bool                    m_hasEdits        = false;
    bool                    m_isRecording     = false;   // the next block drawn goes in the Back history
    bool                    m_isStepping      = false;   // the next block drawn replaces the current location
    int                     m_laidOutLevel    = -1;   // the level the right column was last laid out for
    int64_t                 m_shownBlock      = -1;
    float                   m_split           = 0.62f;
    float                   m_treeShare       = kTreeShare;
    RECT                    m_splitterPx      = {};
    RECT                    m_treeSplitterPx  = {};
    RECT                    m_titleRect       = {};
    RECT                    m_fieldRect       = {};
    RECT                    m_rowRect         = {};
};
