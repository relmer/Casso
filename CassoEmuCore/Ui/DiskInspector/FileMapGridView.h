#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/SectorSource.h"
#include "Ui/DiskInspector/InspectorView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView
//
//  The File map tab's grid (FR-089): a row per track, 0 to 34, and a cell
//  per DOS 3.3 logical sector, ProDOS or Pascal block or CP/M sector, each
//  in its role's color with a word for the role, or its letter where the
//  word does not fit, and its result
//  marked apart from the color: a cross for bad, an outline for missing, a
//  dot for not checked (FR-086). The selected file's sectors have their
//  numbers in file order inside a marching marquee, every other sector is
//  dimmed, and the selected sector is outlined. A click
//  chooses a cell; a disk that is not mapped shows why above each sector's
//  result by track and physical sector (FR-094).
//
//  The grid has a zoom of its own (FR-157). Past 1 it fills its view and
//  pans in it, the track numbers and column headers pinned at its edges,
//  and each cell has its sector's bytes: squares by value, then hex, then
//  each byte's bits as the cell grows.
//
////////////////////////////////////////////////////////////////////////////////

class FileMapGridView : public InspectorView
{
public:
    using ChooseFn = std::function<void (int cell)>;
    using ZoomFn   = std::function<void ()>;

    static constexpr double  kMaxZoom = 32.0;

    explicit FileMapGridView (InspectorViewContext & context);

    void  Paint      (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnMouse    (const DxuiMouseEvent & ev) override;
    bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const override;
    bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const override;

    void  SetMap         (const FileMap * map)    { m_map = map; }
    void  SetSelected    (int cell, int file)     { m_selectedCell = cell; m_selectedFile = file; }
    void  SetOnChoose    (ChooseFn choose)        { m_choose = std::move (choose); }

    //  Called when a double-click returns the grid to a zoom of 1.
    void  SetOnZoom      (ZoomFn zoom)            { m_onZoom = std::move (zoom); }

    //  Zoom by a factor about a point, between 1 and kMaxZoom; back to 1;
    //  and a pan shifted, as when the grid's bounds move under its content.
    void    ZoomAbout    (double factor, POINT anchorPx);
    void    ResetZoom    ();
    void    ShiftPan     (int dxPx, int dyPx);
    bool    IsZoomed     () const { return m_zoom > 1.0; }
    double  GetZoom      () const { return m_zoom; }

    //  The grid's height for a width, so the file list gets the rest.
    int   GetHeightFor   (int widthPx) const;

    //  Where a cell is drawn; false for a cell not on the grid.
    bool  GetCellRect    (int cell, RECT & outRectPx) const;

private:
    struct Grid
    {
        float  left      = 0;
        float  top       = 0;
        float  cellW     = 1;
        float  rowH      = 1;
        float  labelW    = 0;
        float  cellsLeft = 0;
        float  cellsTop  = 0;
    };

    Grid  MakeGrid  () const;
    int   HitTest   (POINT pointPx) const;
    void  PaintMap  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme);
    void  PaintCell (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, int cell, int number, bool isDimmed,
                     const SectorSource * source);
    void  PaintMarquee (IDxuiPainter & painter, const Grid & g, const vector<int> & numbers) const;
    bool      IsDimmed     (int cell) const;
    bool      IsHitCell    (int cell) const;
    bool      IsEditedCell (int cell) const;
    uint32_t  GetCellColor (int cell) const;

    void      MakeRoleLabels (IDxuiTextRenderer & text, const Grid & g, float textPx);
    void      PaintLabels    (IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, float textPx) const;
    bool      PaintBytes     (IDxuiPainter & painter, IDxuiTextRenderer & text, const SectorSource & source, int cell, uint32_t ink) const;
    void      ClampPan       ();
    void      ConfigureBars  (const Grid & g);

    static constexpr size_t  kRoleCount = static_cast<size_t> (SectorRole::CrossLinked) + 1;

    const FileMap                         * m_map          = nullptr;
    int                                     m_selectedCell = -1;
    int                                     m_selectedFile = -1;
    ChooseFn                                m_choose;
    std::array<std::wstring, kRoleCount>    m_roleLabels;
    std::array<float, kRoleCount>           m_roleSizesPx  = {};
    ZoomFn                                  m_onZoom;
    double                                  m_zoom         = 1.0;
    float                                   m_baseCellW    = 1.0f;
    float                                   m_baseRowH     = 1.0f;
    float                                   m_panX         = 0.0f;
    float                                   m_panY         = 0.0f;
    bool                                    m_isPressed    = false;
    bool                                    m_isDragged    = false;
    POINT                                   m_pressAt      = {};
    POINT                                   m_lastAt       = {};
    int64_t                                 m_lastClickMs  = 0;
    DxuiScrollbar                           m_vBar;
    DxuiScrollbar                           m_hBar;
};
