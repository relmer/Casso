#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Ui/DiskInspector/ZoomStops.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapView
//
//  Block mode's map: the partitions as bands across the top, then a cell for
//  each block or run of blocks, colored by its role. The current block is
//  outlined, and the highlighted file's blocks are marked. While comparing,
//  a block that differs has a mark in its top right corner. Pointing at a
//  cell reports its blocks, role and files; a click on a cell or a band
//  steps to its first block. Refresh copies what is drawn from the model,
//  so Paint doesn't read the model while a worker's thread uses it.
//
////////////////////////////////////////////////////////////////////////////////

class BlockMapView : public IDxuiControl
{
public:
    static constexpr float  kBandDip     = 18.0f;
    static constexpr float  kBandGapDip  = 6.0f;
    static constexpr float  kFontDip     = 11.0f;
    static constexpr float  kOutlineDip  = 2.0f;

    using SelectFn = std::function<void (uint32_t block)>;
    using HoverFn  = std::function<void (const std::wstring & text)>;

    void  SetModel    (BlockInspectorModel * model) { m_model = model; }
    void  SetOnSelect (SelectFn fn) { m_onSelect = std::move (fn); }
    void  SetOnHover  (HoverFn fn)  { m_onHover  = std::move (fn); }

    //  A double-click on a cell, with the cell's first block.
    void  SetOnOpen   (SelectFn fn) { m_onOpen   = std::move (fn); }

    //  Whether the current block's cell is outlined.
    void  SetOutlined (bool isOutlined) { m_isOutlined = isOutlined; }

    //  While frozen, Layout moves the map without reading the model again.
    void  SetFrozen   (bool isFrozen)   { m_isFrozen = isFrozen; }

    //  Whether the wheel, a drag and a double-click zoom and pan the map, as
    //  they do the platter. A map that doesn't zoom selects on a press.
    void  SetZoomable (bool isZoomable) { m_isZoomable = isZoomable; }

    //  Whether a block that differs from the other image has its mark.
    void  SetDiffsShown (bool isShown) { m_isDiffsShown = isShown; }

    //  Whether each file's blocks have a different color, in place of the
    //  one color for all files' data.
    void  SetFilesColored (bool isColored) { m_isFilesColored = isColored; }

    //  The zoom, from 1 for the whole volume, in even steps up to a cell of
    //  kMostCellDip with one block in it. StepZoom steps about
    //  the map's center, StepZoomAt about a point, and Fit returns to the
    //  whole volume.
    void    StepZoom   (int steps);
    void    StepZoomAt (int steps, POINT anchorPx);
    void    Fit        ();
    bool    IsAtFit    () const { return m_zoom <= 1.0 && !m_isFlying; }
    bool    IsAtMost   () const { return m_stops.empty() || m_zoom >= m_stops.back(); }
    double  GetZoom    () const { return m_zoom; }

    //  Reads each cell's role and highlight from the model again.
    void  Refresh ();

    //  A flight zooms the map out until both the view's center and the
    //  highlighted blocks are in view, then in to the highlighted blocks, as
    //  the disk inspector's platter flies to a sector. A zoom, a pan, Fit and
    //  StopFlight end the flight.
    void  StartFlight (double nowMs);
    bool  StepFlight  (double nowMs);
    void  StopFlight  () { m_isFlying = false; }
    bool  IsFlying    () const { return m_isFlying; }

    //  What pointing at a cell reports.
    std::wstring  DescribeCell   (uint32_t cell) const;
    std::wstring  DescribeBlocks (uint32_t cell) const;

    //  The tooltip at a point, as the disk inspector's File map grid has it:
    //  the blocks, their role, and the files that use them, with the cell's
    //  color beside the role.
    bool  GetTooltip         (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const;
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const;

    //  Whether the map is animated: the marquee around the highlighted
    //  blocks marches, the current block shimmers, and a flight moves it.
    bool  IsAnimating () const;

    void  Layout  (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  Paint   (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    std::wstring        GetAccessibleName () const override { return L"Block map"; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }

protected:
    const DxuiDpiScaler & GetScaler () const { return m_scaler; }

private:
    static constexpr const wchar_t *  s_kpszFont = L"Segoe UI";

    static constexpr double  kZoomRatio    = 1.25;
    static constexpr double  kMinStepRatio = 1.05;
    static constexpr double  kFlightFill   = 0.75;   // the highlighted blocks' share of the view at a flight's end
    static constexpr float   kMostCellDip  = 48.0f;  // a cell at the last stop
    static constexpr float   kLabelDip     = 18.0f;  // the least cell a run of blocks is labeled in
    static constexpr float   kLabelPadDip  = 4.0f;

    RECT             GetGridRect    () const;
    bool             IsDiffersShown (int64_t cell) const;
    void             PaintBands     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const;
    void             PaintCells     (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & grid) const;
    void             PaintMarquee   (IDxuiPainter & painter, const DiskInspectorPalette & palette, const RECT & grid) const;
    uint32_t         GetCellColor   (const DiskInspectorPalette & palette, uint32_t cell) const;
    void             PaintLabels    (IDxuiTextRenderer & text, const DiskInspectorPalette & palette, const RECT & grid, float left, float top) const;
    void             GetCellsInView (const RECT & grid, int & outFirstColumn, int & outLastColumn, int & outFirstRow, int & outLastRow) const;
    void             ReadBlocks     ();
    static uint32_t  GetFileColor   (int64_t key, bool isDark);
    void             ReadLabels     (const RECT & grid);
    std::wstring     GetBlockLabel  (uint32_t block) const;
    void             BuildStops     ();
    void             ZoomAbout      (double zoom, POINT anchorPx);
    void             ClampPan       ();
    void             Regrid         ();
    int64_t          FindCellAt     (POINT pointPx) const;

    //  Views as the platter's flight has them: a point d of the fit grid, in
    //  pixels, is at pan + zoom * d in the view.
    InspectorViewModel::View  GetView    () const { return { m_zoom, { -m_panX, -m_panY } }; }
    InspectorViewModel::View  GetBoxView (double left, double top, double right, double bottom) const;

    BlockInspectorModel       * m_model            = nullptr;
    DxuiDpiScaler               m_scaler;
    BlockMapGrid                m_grid;
    vector<SectorRole>          m_roles;
    vector<int64_t>             m_owners;   // each cell's file, as GetOwnerKey has it, or -1
    vector<SectorRole>          m_blockRoles;   // each block's role and file, read once for the map's generation
    vector<int64_t>             m_blockOwners;
    uint32_t                    m_blocksGeneration = 0;
    bool                        m_isFilesColored   = false;
    vector<bool>                m_highlighted;
    vector<std::wstring>        m_labels;   // each cell in view's file or structure, where the cells are labeled
    vector<std::wstring>        m_blockLabels;   // each block's label once read, for the map's generation
    vector<bool>                m_isLabelRead;
    uint32_t                    m_labelGeneration  = 0;
    bool                        m_hasHighlight     = false;
    RECT                        m_highlightBox     = {};   // the highlighted blocks' columns and rows, right and bottom past the last
    InspectorViewModel::View    m_flightFrom;
    InspectorViewModel::View    m_flightMid;
    InspectorViewModel::View    m_flightTo;
    double                      m_flightStartMs    = 0.0;
    bool                        m_isFlying         = false;
    bool                        m_isTwoLegs        = false;
    bool                        m_isDark           = true;   // the theme's surface the last time the map was drawn
    uint32_t                    m_differsArgb      = 0xFFF06060;   // the theme's error color the last time the map was drawn
    vector<bool>                m_differs;
    SelectFn                    m_onSelect;
    SelectFn                    m_onOpen;
    HoverFn                     m_onHover;
    int64_t                     m_hoverCell        = -1;
    bool                        m_isOutlined       = true;
    bool                        m_isFrozen         = false;
    vector<BlockBand>           m_bands;
    uint32_t                    m_blockCount       = 0;
    uint32_t                    m_current          = 0;
    bool                        m_isZoomable       = false;
    bool                        m_isDiffsShown     = true;
    double                      m_zoom             = 1.0;
    double                      m_panX             = 0.0;   // where the view starts in the zoomed grid, in pixels
    double                      m_panY             = 0.0;
    vector<double>              m_stops;
    SIZE                        m_stopsSize        = {};   // the grid's size and block count the stops were built for
    uint32_t                    m_stopsCount       = 0;
    bool                        m_isPressed        = false;
    bool                        m_isPanning        = false;
    POINT                       m_pressAt          = {};
    POINT                       m_lastAt           = {};
    int64_t                     m_lastClickMs      = 0;
};
