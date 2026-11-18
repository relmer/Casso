#pragma once

#include "Ui/Debugger/Panes/HeatMapSections.h"
#include "Ui/Debugger/Panes/HeatMapZoomWidget.h"

class HeatMapCells;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid
//
//  Where the heat map's cells are on the pane: the bands stacked down the
//  map, the addresses in each row at the zoom, how far the map is
//  scrolled, and the picture of the cells in view a pixel at a time. The
//  cell under a point and the rectangle of a cell are found here. The zoom
//  and the scroll are changed here, and so are the scrollbar and the zoom
//  widget.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapGrid
{
public:
    using Band = HeatMapSections::Band;

    //  Where a range lies in the map, in pixels from the map's top: its
    //  header, then its rows, each with `columns` addresses.
    struct Placed
    {
        int   columns   = 1;
        long  headerTop = 0;
        long  rowsTop   = 0;
        long  rows      = 0;
    };

    //  A cell: the range it is in, and how far into the range.
    struct Place
    {
        size_t  band   = 0;
        long    offset = 0;
    };

    //  An instruction drawn as one cell across its bytes: its opcode's place
    //  and address, and its length.
    struct Merged
    {
        Place  first;
        Word   opcode = 0;
        int    length = 0;
    };

    static constexpr int    kAddressCount     = HeatMapSections::kAddressCount;
    static constexpr int    kMaxColumns       = HeatMapSections::kMaxColumns;
    static constexpr int    kStreetPx         = HeatMapSections::kStreetPx;
    static constexpr int    kBarDip           = 22;      // the row of views over the map
    static constexpr int    kGutterDip        = 44;      // the row labels' column, until a paint measures them
    static constexpr int    kInsetDip         = 8;       // right of the map and below it
    static constexpr float  kLabelLine        = 1.4f;    // a row label's line, in its font's sizes
    static constexpr int    kScrollbarDip     = 10;
    static constexpr int    kDefaultCellDip   = 3;
    static constexpr int    kMaxCellPx        = 64;
    static constexpr float  kZoomPerNotch     = 1.5f;
    static constexpr int    kComfortCellDip   = 8;
    static constexpr int    kSnapDip          = 4;
    static constexpr int    kHeaderDip        = 20;
    static constexpr int    kSectionAfterDip  = 10;
    static constexpr int    kSectionHeaderDip = 16;
    static constexpr int    kSectionPadDip    = 7;

    explicit HeatMapGrid (const HeatMapCells & model) : m_model (model) {}

    void   Layout              (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    void   MeasureGutter       (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    void   SetRanges           (std::vector<Band> ranges);
    void   ClearRanges         ();
    bool   HasRanges           () const { return m_hasRanges; }

    //  The map laid out again after the regions came or went, and after the
    //  machine changed, which cuts it into sections again.
    void   PlaceAgain          ();
    void   OnMachineChanged    ();

    bool                         IsSplit           () const { return m_isSplit; }
    const std::vector<Band>    & GetBands          () const { return m_bands; }
    const std::vector<Placed>  & GetPlaced         () const { return m_placed; }
    bool                         HasHeader         (size_t band) const { return m_hasRanges || (band < m_bands.size() && m_bands[band].isSection); }
    RECT                         GetHeaderRect     (size_t range) const;

    int                          GetCellPx         () const { return m_cellPx; }
    int                          GetColumns        () const { return m_columns; }
    int                          GetRows           () const;
    long                         GetRowPx          () const { return m_rowPx; }
    int                          GetPitch          () const { return m_cellPx + kStreetPx; }
    long                         GetScrollTop      () const { return m_scrollTop; }
    int                          GetGutterPx       () const { return m_gutterPx; }
    long                         GetLabelLinePx    () const { return m_labelLinePx; }
    RECT                         GetMapRect        () const { return m_map; }
    const RECT                 & GetBounds         () const { return m_boundsDip; }
    const DxuiDpiScaler        & GetScaler         () const { return m_scaler; }
    int                          GetStartCellPx    () const { return (m_fitCellPx > 0) ? m_fitCellPx : std::max (1, m_scaler.ToPx (kDefaultCellDip)); }

    void   ZoomAt              (POINT point, float notches);
    void   ZoomIn              ();
    void   ZoomOut             ();
    void   ZoomTo              (int cellPx);
    void   ApplyCellPx         (int cellPx, POINT point);
    void   ResetZoom           ();
    void   ShowAddress         (Word address);
    void   ScrollBy            (int dy);
    void   ScrollToPlace       (const Place & place);

    //  The map dragged to a scroll, from the scroll the drag began at.
    void   DragTo              (long top, long from);

    bool                       HasVerticalScroll  () const { return m_hasVertBar; }
    RECT                       GetVerticalBarRect () const;
    DxuiScrollbar            & GetScrollbar       ()       { return m_vertBar; }
    const DxuiScrollbar      & GetScrollbar       () const { return m_vertBar; }
    HeatMapZoomWidget        & GetZoomWidget      ()       { return m_zoomWidget; }
    const HeatMapZoomWidget  & GetZoomWidget      () const { return m_zoomWidget; }
    void                       PlaceZoomWidget    ();
    bool                       TickScrollbars     (int64_t nowMs);
    bool                       TryGetZoomTipAt    (POINT point, bool isVisible, RECT & anchor, std::wstring & text) const;
    RECT                       GetViewRowFreeRect (bool isVisible) const;

    //  The picture of the cells in view, drawn again first when anything
    //  changed it and the pane is visible.
    const std::vector<uint32_t> &  GetPixels      (bool isVisible);
    void                           MarkFrameStale () { m_isFrameStale = true; }

    const std::optional<Place> &  GetHoverPlace   () const { return m_hover; }
    void                          SetHoverPlace   (const std::optional<Place> & hover) { m_hover = hover; }
    std::optional<Word>           GetHover        () const;

    std::optional<Place>   GetPlaceAt          (POINT point) const;
    std::optional<Place>   GetPlaceOf          (Word address) const;
    std::optional<Place>   GetPickPlaceAt      (POINT point) const;
    Word                   GetAddressOf        (const Place & place) const;
    RECT                   GetPlaceRect        (const Place & place) const;
    RECT                   GetCellRect         (Word address) const;
    std::optional<Word>    GetAddressAt        (POINT point) const;
    std::optional<Word>    GetPickAt           (POINT point) const;
    bool                   IsOverMap           (POINT point) const;
    void                   ForEachVisible      (const std::function<void (const Place &, Word)> & visit) const;
    std::optional<Merged>  GetMergedAt         (const Place & place) const;
    std::vector<RECT>      GetMergedRects      (const Merged & merged) const;

private:
    long                   GetColumnLeft       (const Placed & placed, long column) const;
    long                   GetColumnAt         (const Placed & placed, long x) const;
    long                   GetBandAt           (long y) const;
    void                   PlaceBands          ();
    void                   FitRanges           ();
    int                    ComputeFitCellPx    ();
    void                   RescaleForDpi       (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    void                   PlaceMap            ();
    void                   ClampScroll         ();
    void                   LimitScroll         (long lowest, long highest);
    long                   GetMaxScroll        () const;
    void                   SyncScrollbars      ();
    void                   BuildFrame          (bool isVisible);
    RECT                   GetZoomCorner       () const;
    void                   SyncBands           ();
    bool                   CanSplit            () const;
    bool                   IsMergeZoom         () const;
    int                    GetMergeLength      (const Place & place, Word address) const;

    const HeatMapCells     & m_model;
    DxuiDpiScaler            m_scaler;
    RECT                     m_boundsDip     = {};
    DxuiScrollbar            m_vertBar;
    HeatMapZoomWidget        m_zoomWidget;
    RECT                     m_zoomPlacedFor = {};   // the bounds the zoom widget was placed in
    std::optional<Place>     m_hover;
    std::vector<Band>        m_bands         = { Band { {}, 0, kAddressCount } };
    std::vector<Placed>      m_placed;
    std::vector<Band>        m_ranges        = { Band { {}, 0, kAddressCount } };   // the set's ranges, or all of memory, before sections cut them
    bool                     m_isSplit       = false;
    long                     m_contentH      = 0;
    bool                     m_hasRanges     = false;
    bool                     m_isFitted      = true;
    RECT                     m_fittedFor     = {};   // the bounds the map was last fitted to
    int                      m_fitCellPx     = 0;   // the fitted cell size, the zoom's 100%
    int                      m_cellPx        = 0;
    int                      m_columns       = kMaxColumns / 4;
    long                     m_rowPx         = 0;
    int                      m_gutterPx      = 0;
    long                     m_labelLinePx   = 0;   // a row label's line, measured with the gutter
    RECT                     m_map           = {};
    long                     m_areaBottom    = 0;   // the bottom of the map's area, which a map scrolled past an end fills
    bool                     m_hasVertBar    = false;
    long                     m_scrollTop     = 0;
    bool                     m_isFrameStale  = false;
    std::vector<uint32_t>    m_frame;
};
