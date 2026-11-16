#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorWorker.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"

class BlockFieldsView;
class BlockFilesView;
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
//  as the track inspector's is. Across the top, the toolbar row with Go to
//  at its right end. On the left, the grid of blocks with the platter's
//  corner groups: the legend at the top left, the overlays (Files, and
//  Differences while comparing) at the bottom left, and
//  zoom - and + at the bottom right; the cell under the pointer is outlined.
//  Past the splitter, on the right: the block's number with the previous
//  and next block buttons and its capsules (Free or Used, its role, its
//  files), the view tabs (Files, Blocks, and Header for 2IMG and Disk Copy
//  4.2 images), the field line, Copy block, a row with the pending edits or
//  a long job's progress, then the Files list or the block's hex view and
//  fields. The Files view opens first; a click on a file highlights its
//  blocks on the grid and selects its first block.
//
//  The model's reads run as jobs on the window's BlockInspectorWorker: a
//  block is loaded before it is drawn, and the search for data in free
//  blocks runs there.
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
    static constexpr float  kMinSplit    = 0.3f;
    static constexpr float  kMaxSplit    = 0.7f;
    static constexpr float  kHexShare    = 0.60f;
    static constexpr float  kBarDip      = 240.0f;
    static constexpr float  kButtonDip   = 110.0f;
    static constexpr float  kViewsDip    = 260.0f;
    static constexpr int    kBytesPerRow = 16;

    //  The view tabs.
    enum View
    {
        kViewFiles,
        kViewBlocks,
        kViewHeader,
    };

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

    //  The view tabs.
    void  ShowView  (View view);
    View  GetView   () const { return m_view; }
    bool  HasHeader () const;

    //  The current block as a hex dump with its text, for Copy block.
    std::wstring  BuildBlockText () const;

    //  "Selected: block 2", or empty with no image open.
    std::wstring  GetSelectionLine () const;

    void  ToggleLegend ();

    BlockMapView  & GetMap     () const { return *m_map; }
    BlockHexView  * GetHexView () const { return m_hex; }

    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    //  A capsule beside the block's number: Free or Used, the block's role,
    //  or a file the block is in. A damaged role's capsule is drawn red.
    struct Capsule
    {
        std::wstring  text;
        bool          isBad = false;
    };

    void  InitCorners       ();
    void  InitRightColumn   ();
    bool  OnType            (uint64_t offset, bool isHexColumn, bool isHighNibble, wchar_t ch);
    bool  OnNavigateKey     (const DxuiKeyEvent & ev);
    void  MarkField         (const BlockField * field);
    void  ShowBytes         ();
    void  BuildFieldLine    ();
    void  FillFiles         ();
    void  Present           ();
    void  LayoutGrid        (const RECT & column, const DxuiDpiScaler & scaler);
    void  LayoutRight       (const RECT & column, const DxuiDpiScaler & scaler);
    void  PaintHeader       (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  PaintProgress     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  ApplyHighlight    ();
    void  BuildCapsules     ();

    BlockInspectorModel   * m_model        = nullptr;
    BlockInspectorWorker  * m_worker       = nullptr;
    IDxuiTextRenderer     * m_text         = nullptr;
    DxuiDpiScaler           m_scaler;
    DxuiToolbar           * m_tools        = nullptr;
    BlockMapView          * m_map          = nullptr;
    FloatingGroup         * m_legendGroup  = nullptr;
    FloatingGroup         * m_chipsGroup   = nullptr;
    FloatingGroup         * m_zoomGroup    = nullptr;
    DxuiIconButton        * m_legendButton = nullptr;
    BlockLegendView       * m_legend       = nullptr;
    OverlayChip           * m_filesChip    = nullptr;
    OverlayChip           * m_diffsChip    = nullptr;
    DxuiIconButton        * m_zoomOut      = nullptr;
    DxuiIconButton        * m_zoomIn       = nullptr;
    DxuiSegmentedControl  * m_views        = nullptr;
    DxuiButton            * m_copy         = nullptr;
    DxuiIconButton        * m_previous     = nullptr;
    DxuiIconButton        * m_next         = nullptr;
    DxuiLabel             * m_editNote     = nullptr;
    DxuiButton            * m_discard      = nullptr;
    DxuiButton            * m_apply        = nullptr;
    DxuiButton            * m_cancel       = nullptr;
    BlockHexView          * m_hex          = nullptr;
    BlockFieldsView       * m_fields       = nullptr;
    BlockFilesView        * m_files        = nullptr;
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
    uint64_t                m_jobDone      = 0;
    uint64_t                m_jobTotal     = 0;
    bool                    m_isLongJob    = false;
    bool                    m_isFilesOn    = false;
    bool                    m_isDiffsOn    = true;
    bool                    m_isSplitting  = false;
    bool                    m_hasEdits     = false;
    View                    m_view         = kViewFiles;
    bool                    m_hasHeader    = false;
    int64_t                 m_shownBlock   = -1;
    float                   m_split        = 0.5f;
    RECT                    m_splitterPx   = {};
    RECT                    m_titleRect    = {};
    RECT                    m_fieldRect    = {};
    RECT                    m_rowRect      = {};
};
