#pragma once

#include "Ui/Debugger/Panes/HeatMapBands.h"

class HeatMapCells;
class HeatMapControls;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapGrid
//
//  Where the heat map's cells are on the pane: the map's area, the addresses
//  in each row at the zoom, and how far the map is scrolled. The cell under
//  a point and the rectangle of a cell are found here, and the zoom and the
//  scroll are changed here. Each change that moves a cell raises the
//  revision, so the frame is drawn again; the scrollbar and the zoom widget
//  are placed again as the map is scrolled and laid out.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapGrid
{
public:
    static constexpr int    kBarDip         = 22;      // the row of views over the map
    static constexpr int    kInsetDip       = 8;       // right of the map and below it
    static constexpr float  kLabelLine      = 1.4f;    // a row label's line, in its font's sizes
    static constexpr int    kScrollbarDip   = 10;
    static constexpr int    kMaxCellPx      = 64;
    static constexpr int    kHeaderDip      = 20;
    static constexpr int    kSectionPadDip  = 7;

    HeatMapGrid (const HeatMapCells & model, HeatMapBands & bands, HeatMapControls & controls) :
        m_model    (model),
        m_bands    (bands),
        m_controls (controls)
    {
    }

    void   Layout              (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    void   MeasureGutter       (IDxuiTextRenderer & text, const IDxuiTheme & theme);

    void   SetRanges           (std::vector<HeatMapBands::Band> ranges);
    void   ClearRanges         ();

    //  The map laid out again after the regions came or went, and after the
    //  machine changed, which cuts it into sections again.
    void   PlaceAgain          ();
    void   OnMachineChanged    ();

    RECT                         GetHeaderRect     (size_t range) const;

    int                          GetCellPx         () const { return m_cellPx; }
    int                          GetColumns        () const { return m_columns; }
    long                         GetRowPx          () const { return m_rowPx; }
    int                          GetPitch          () const { return m_cellPx + HeatMapSections::kStreetPx; }
    long                         GetScrollTop      () const { return m_scrollTop; }
    int                          GetGutterPx       () const { return m_gutterPx; }
    long                         GetLabelLinePx    () const { return m_labelLinePx; }
    RECT                         GetMapRect        () const { return m_map; }
    const RECT                 & GetBounds         () const { return m_boundsDip; }
    const DxuiDpiScaler        & GetScaler         () const { return m_scaler; }
    int                          GetStartCellPx    () const { return (m_fitCellPx > 0) ? m_fitCellPx : std::max (1, m_scaler.ToPx (kDefaultCellDip)); }

    //  Raised by every change that moves a cell, so a picture of the cells
    //  drawn at an older revision is stale.
    uint64_t                     GetRevision       () const { return m_revision; }

    void   ZoomAt              (POINT point, float notches);
    void   ZoomIn              ();
    void   ZoomOut             ();
    void   ZoomTo              (int cellPx);
    void   ApplyCellPx         (int cellPx, POINT point);
    void   ResetZoom           ();
    void   ShowAddress         (Word address);
    void   ScrollBy            (int dy);
    void   ScrollToPlace       (const HeatMapBands::Place & place);

    //  The map dragged to a scroll, from the scroll the drag began at.
    void   DragTo              (long top, long from);

    bool                       HasVerticalScroll  () const { return m_hasVertBar; }
    RECT                       GetVerticalBarRect () const;
    RECT                       GetViewRowFreeRect (bool isVisible) const;

    const std::optional<HeatMapBands::Place>  & GetHoverPlace () const { return m_hover; }
    void                                        SetHoverPlace (const std::optional<HeatMapBands::Place> & hover) { m_hover = hover; }
    std::optional<Word>                         GetHover      () const;

    std::optional<HeatMapBands::Place>  GetPlaceAt     (POINT point) const;
    RECT                                GetPlaceRect   (const HeatMapBands::Place & place) const;
    RECT                                GetCellRect    (Word address) const;
    std::optional<Word>                 GetAddressAt   (POINT point) const;
    bool                                IsOverMap      (POINT point) const;
    void                                ForEachVisible (const std::function<void (const HeatMapBands::Place &, Word)> & visit) const;

    //  Where a column starts within a band's row, and the column a distance
    //  into the row falls in.
    long                   GetColumnLeft       (const HeatMapBands::Placed & placed, long column) const;
    long                   GetColumnAt         (const HeatMapBands::Placed & placed, long x) const;

private:
    static constexpr int    kGutterDip      = 44;      // the row labels' column, until a paint measures them
    static constexpr int    kDefaultCellDip = 3;

    void                   FitRanges           ();
    int                    ComputeFitCellPx    ();
    void                   RescaleForDpi       (const RECT & boundsPx, const DxuiDpiScaler & scaler);
    void                   PlaceMap            ();
    void                   ClampScroll         ();
    void                   LimitScroll         (long lowest, long highest);
    long                   GetMaxScroll        () const;
    void                   SyncBands           ();
    bool                   CanSplit            () const;

    const HeatMapCells                  & m_model;
    HeatMapBands                        & m_bands;
    HeatMapControls                     & m_controls;
    DxuiDpiScaler                         m_scaler;
    RECT                                  m_boundsDip   = {};
    std::optional<HeatMapBands::Place>    m_hover;
    bool                                  m_isFitted    = true;
    RECT                                  m_fittedFor   = {};   // the bounds the map was last fitted to
    int                                   m_fitCellPx   = 0;   // the fitted cell size, the zoom's 100%
    int                                   m_cellPx      = 0;
    int                                   m_columns     = HeatMapSections::kMaxColumns / 4;
    long                                  m_rowPx       = 0;
    int                                   m_gutterPx    = 0;
    long                                  m_labelLinePx = 0;   // a row label's line, measured with the gutter
    RECT                                  m_map         = {};
    long                                  m_areaBottom  = 0;   // the bottom of the map's area, which a map scrolled past an end fills
    bool                                  m_hasVertBar  = false;
    long                                  m_scrollTop   = 0;
    uint64_t                              m_revision    = 0;
};
