#include "Pch.h"
#include "Widgets/HexView/DxuiHexRegions.h"
#include "Widgets/DxuiHexView.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"
#include "Render/IDxuiPainter.h"
#include "Render/DxuiRoundedOutline.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::DxuiHexRegions
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexRegions::DxuiHexRegions (DxuiHexView & view) :
    m_view (view)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::SetShow
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexRegions::SetShow (bool show)
{
    m_showRegions = show;
    m_lanesTopRow = UINT64_MAX;

    m_view.m_layout.RecomputeRowWidth();
    m_view.m_layout.ClampTopRow();
    m_view.m_scroll.SyncScrollbar();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::IsDrawn
//
//  In line mode, and with the values hidden, there are no outlines.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexRegions::IsDrawn() const
{
    return m_showRegions && m_view.m_showValues && !m_view.IsLineMode() && m_view.m_source != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetUnitsPerRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexRegions::GetUnitsPerRow() const
{
    return (std::max) (m_view.m_bytesPerRow / (std::max) (m_view.m_grouping, 1), 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetUnitRegion
//
//  The region of a value, read at its first byte. A row before the first or
//  past the last, and a value past a short last row's end, is in none.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t DxuiHexRegions::GetUnitRegion (int64_t row, int unit) const
{
    uint16_t  region = 0;
    uint64_t  bytes  = (m_view.m_source != nullptr) ? m_view.m_source->GetByteCount() : 0;
    uint64_t  offset = 0;



    if (m_view.m_source == nullptr || row < 0 || unit < 0 || unit >= GetUnitsPerRow())
    {
        return 0;
    }

    offset = ((uint64_t) row * (uint64_t) m_view.m_bytesPerRow) + ((uint64_t) unit * (uint64_t) m_view.m_grouping);

    if (offset >= bytes)
    {
        return 0;
    }

    m_view.m_source->ReadRegions (offset, std::span<uint16_t> (&region, 1));

    return (region == IDxuiHexSource::kRegionUnknown) ? GetNearestKnownRegion (offset) : region;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetNearestKnownRegion
//
//  The region of the first known byte after an unknown one, or before it
//  when none after is known within kNearestBytes; none when neither is.
//
////////////////////////////////////////////////////////////////////////////////

uint16_t DxuiHexRegions::GetNearestKnownRegion (uint64_t offset) const
{
    static constexpr uint64_t  kNearestBytes = 1024;
    uint64_t                   bytes         = m_view.m_source->GetByteCount();
    uint64_t                   after         = std::min (bytes, offset + kNearestBytes) - offset;
    uint64_t                   before        = std::min (offset, kNearestBytes);
    std::vector<uint16_t>      regions;



    regions.resize ((size_t) after);
    m_view.m_source->ReadRegions (offset, regions);

    for (uint16_t each : regions)
    {
        if (each != IDxuiHexSource::kRegionUnknown)
        {
            return each;
        }
    }

    regions.resize ((size_t) before);
    m_view.m_source->ReadRegions (offset - before, regions);

    for (auto each = regions.rbegin(); each != regions.rend(); ++each)
    {
        if (*each != IDxuiHexSource::kRegionUnknown)
        {
            return *each;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::IsRegionStart
//
//  A value in a region whose value before it, at the end of the row above
//  for a row's first, is in another or in none.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexRegions::IsRegionStart (int64_t row, int unit) const
{
    uint16_t  region = GetUnitRegion (row, unit);
    uint16_t  before = (unit > 0) ? GetUnitRegion (row, unit - 1) : GetUnitRegion (row - 1, GetUnitsPerRow() - 1);



    return region != 0 && region != before;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetUnitLeft
//
//  Halfway into the space before a value, so an edge between two values
//  touches neither; the row's first value takes the middle of the gutter
//  before the hex column.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiHexRegions::GetUnitLeft (int unit) const
{
    int    each  = m_view.m_layout.GetValueCells() + 1;
    int    cellX = m_view.m_layout.GetColumnStartCell (DxuiHexViewTypes::Column::Hex) + (unit * each);
    float  space = (unit == 0) ? (float) (DxuiHexView::kGutterCells * m_view.m_cellWidthDip) / 2.0f : (float) m_view.m_cellWidthDip / 2.0f;



    return (float) m_view.m_layout.GetCellLeft (cellX) - space;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetUnitRight
//
////////////////////////////////////////////////////////////////////////////////

float DxuiHexRegions::GetUnitRight (int unit) const
{
    int    each  = m_view.m_layout.GetValueCells() + 1;
    int    cellX = m_view.m_layout.GetColumnStartCell (DxuiHexViewTypes::Column::Hex) + (unit * each) + m_view.m_layout.GetValueCells();
    float  space = (unit == GetUnitsPerRow() - 1) ? (float) (DxuiHexView::kGutterCells * m_view.m_cellWidthDip) / 2.0f : (float) m_view.m_cellWidthDip / 2.0f;



    return (float) m_view.m_layout.GetCellLeft (cellX) + space;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetLabelLane
//
//  A label's height at its scale, with a pixel of air above and below.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexRegions::GetLabelLane() const
{
    constexpr int  kAirDip = 2;
    int            label   = (int) (((float) m_view.m_layout.m_cellHeightDip * kLabelScale) + 0.5f) + m_view.m_scaler.ToPx (kAirDip);



    return (std::max) (label, m_view.m_scaler.ToPx (kEdgeLaneDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetLabelWidth
//
//  The fixed-width face makes a label's width its length in cells at the
//  label's scale, so it is known without a renderer; the padding each side is
//  where the edge stops short of it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexRegions::GetLabelWidth (uint16_t region) const
{
    uint32_t      argb  = 0;
    std::wstring  label;



    if (m_view.m_source == nullptr || !m_view.m_source->TryGetRegionStyle (region, argb, label) || label.empty())
    {
        return 0;
    }

    return (int) (((float) label.size() * (float) m_view.m_cellWidthDip * kLabelScale) + 0.5f) + (2 * m_view.m_scaler.ToPx (kLabelPadDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::TryGetLabelSegment
//
//  The top edge of a region that starts partway along a row is two segments:
//  along the first row from its start to the row's end, and along the second
//  row from the row's start to below where the region began. The label takes
//  the first that fits it, or the longer when neither does.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexRegions::TryGetLabelSegment (uint64_t row, int unit, LabelSegment & out) const
{
    int           units  = GetUnitsPerRow();
    uint16_t      region = GetUnitRegion ((int64_t) row, unit);
    LabelSegment  first  = { row, unit, unit };
    LabelSegment  second = { row + 1, 0, -1 };
    float         need   = 0.0f;
    float         width1 = 0.0f;
    float         width2 = 0.0f;



    if (!IsDrawn() || !IsRegionStart ((int64_t) row, unit))
    {
        return false;
    }

    while (first.last + 1 < units && GetUnitRegion ((int64_t) row, first.last + 1) == region)
    {
        first.last++;
    }

    //  The second segment runs under the values before the start, as far as
    //  the region reaches along the next row.
    if (unit > 0 && first.last == units - 1)
    {
        while (second.last + 1 < unit && GetUnitRegion ((int64_t) row + 1, second.last + 1) == region)
        {
            second.last++;
        }
    }

    need   = (float) GetLabelWidth (region);
    width1 = GetUnitRight (first.last) - GetUnitLeft (first.first);
    width2 = (second.last >= 0) ? GetUnitRight (second.last) - GetUnitLeft (0) : 0.0f;

    if (width1 >= need || second.last < 0)
    {
        out = first;
    }
    else if (width2 >= need || width2 > width1)
    {
        out = second;
    }
    else
    {
        out = first;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetLaneAbove
//
//  Space opens between two rows only where an outline's edge runs between
//  them: a lane a label fits in where a label sits on the edge, a thin one
//  for an edge alone.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexRegions::GetLaneAbove (uint64_t row) const
{
    int           units    = GetUnitsPerRow();
    bool          hasEdge  = false;
    bool          hasLabel = false;
    LabelSegment  segment;



    if (!IsDrawn() || m_view.m_layout.m_cellHeightDip <= 0)
    {
        return 0;
    }

    for (int unit = 0; unit < units && !hasLabel; unit++)
    {
        uint16_t  above = GetUnitRegion ((int64_t) row - 1, unit);
        uint16_t  below = GetUnitRegion ((int64_t) row, unit);

        hasEdge = hasEdge || above != below;

        if (below != 0 && TryGetLabelSegment (row, unit, segment) && segment.row == row)
        {
            hasLabel = true;
        }

        if (row > 0 && above != 0 && TryGetLabelSegment (row - 1, unit, segment) && segment.row == row)
        {
            hasLabel = true;
        }
    }

    if (hasLabel)
    {
        return GetLabelLane();
    }

    return hasEdge ? m_view.m_scaler.ToPx (kEdgeLaneDip) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::RefreshLanes
//
//  Each row's top from the top row down, as many rows as the bounds could
//  fit without lanes and two more. Anything but painting refreshes them only
//  when the top row or the row's makeup has moved.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexRegions::RefreshLanes() const
{
    int  cellH  = m_view.m_layout.m_cellHeightDip;
    int  height = m_view.m_layout.GetHeightDip();
    int  rows   = (cellH > 0) ? (height / cellH) + 2 : 0;
    int  y      = 0;



    m_laneTops.clear();
    m_lanesTopRow = m_view.m_scroll.GetTopRow();
    m_lanesUnits  = GetUnitsPerRow();
    m_lanesCellH  = cellH;
    m_lanesHeight = height;

    for (int index = 0; index < rows; index++)
    {
        y += GetLaneAbove (m_lanesTopRow + (uint64_t) index);
        m_laneTops.push_back (y);
        y += cellH;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetRowTop
//
//  Rows past the ones the lanes were found for are placed a row height apart,
//  which is only ever off the bounds.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexRegions::GetRowTop (uint64_t row) const
{
    uint64_t  topRow = m_view.m_scroll.GetTopRow();
    int       cellH  = m_view.m_layout.m_cellHeightDip;
    int64_t   rowUp  = (int64_t) row - (int64_t) topRow;
    int64_t   last   = 0;



    if (!IsDrawn())
    {
        return (int) (rowUp * cellH);
    }

    if (m_lanesTopRow != topRow || m_lanesUnits != GetUnitsPerRow() || m_lanesCellH != cellH || m_lanesHeight != m_view.m_layout.GetHeightDip())
    {
        RefreshLanes();
    }

    if (m_laneTops.empty())
    {
        return (int) (rowUp * cellH);
    }

    if (rowUp < 0)
    {
        return m_laneTops.front() - (int) (-rowUp * cellH);
    }

    if (rowUp < (int64_t) m_laneTops.size())
    {
        return m_laneTops[(size_t) rowUp];
    }

    last = (int64_t) m_laneTops.size() - 1;
    return m_laneTops.back() + (int) ((rowUp - last) * cellH);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetRowAtY
//
//  A lane belongs to the row below it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexRegions::GetRowAtY (int clientY) const
{
    int       y      = clientY - m_view.m_boundsDip.top;
    int       cellH  = m_view.m_layout.m_cellHeightDip;
    uint64_t  topRow = m_view.m_scroll.GetTopRow();
    uint64_t  row    = topRow;



    if (cellH <= 0 || y < 0)
    {
        return topRow;
    }

    if (!IsDrawn())
    {
        return topRow + (uint64_t) (y / cellH);
    }

    while (y >= GetRowTop (row) + cellH)
    {
        row++;
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetEdgeY
//
//  The line along the top of a row runs down the middle of the lane above it.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiHexRegions::GetEdgeY (int64_t row) const
{
    uint64_t  at = (uint64_t) (std::max) (row, (int64_t) 0);



    if (row < 0)
    {
        return (float) (m_view.m_boundsDip.top + GetRowTop (0) - m_view.m_layout.m_cellHeightDip);
    }

    return (float) m_view.m_boundsDip.top + (float) GetRowTop (at) - (float) GetLaneAbove (at) / 2.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetRowCapWithLanes
//
//  The rows whose values end inside the bounds.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexRegions::GetRowCapWithLanes() const
{
    int       height = m_view.m_layout.GetHeightDip();
    int       cellH  = m_view.m_layout.m_cellHeightDip;
    uint64_t  topRow = m_view.m_scroll.GetTopRow();
    int       rows   = 0;



    while (GetRowTop (topRow + (uint64_t) rows) + cellH <= height)
    {
        rows++;
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::GetMaxTopRowWithLanes
//
//  Counted up from the last row: the rows that fit with the lanes above each
//  and the one below the last.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiHexRegions::GetMaxTopRowWithLanes() const
{
    uint64_t  row    = m_view.m_layout.GetRowCount();
    int       height = m_view.m_layout.GetHeightDip();
    int       used   = GetLaneAbove (row);
    int       needed = 0;



    while (row > 0)
    {
        needed = m_view.m_layout.m_cellHeightDip + GetLaneAbove (row - 1);

        if (used + needed > height)
        {
            break;
        }

        used += needed;
        row--;
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::Paint
//
//  Each run of values in one region, from the row above the top to the row
//  below the last shown, so a region running off either end has its edge
//  there drawn outside the bounds rather than across them.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexRegions::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    int       units     = GetUnitsPerRow();
    int64_t   topRow    = (int64_t) m_view.m_scroll.GetTopRow();
    int64_t   firstRow  = topRow - 1;
    int64_t   lastRow   = topRow + m_view.m_layout.GetRowCap() + 1;
    int64_t   runRow    = 0;
    int       runUnit   = 0;
    uint16_t  runRegion = 0;
    int64_t   prevRow   = 0;
    int       prevUnit  = 0;



    lastRow = (std::min) (lastRow, (int64_t) m_view.m_layout.GetRowCount() - 1);

    for (int64_t row = firstRow; row <= lastRow; row++)
    {
        for (int unit = 0; unit < units; unit++)
        {
            uint16_t  region = GetUnitRegion (row, unit);

            if (region != runRegion)
            {
                if (runRegion != 0)
                {
                    PaintRun (painter, text, theme, runRow, runUnit, prevRow, prevUnit, runRegion);
                }

                runRegion = region;
                runRow    = row;
                runUnit   = unit;
            }

            prevRow  = row;
            prevUnit = unit;
        }
    }

    if (runRegion != 0)
    {
        PaintRun (painter, text, theme, runRow, runUnit, prevRow, prevUnit, runRegion);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::PaintRun
//
//  One run's outline, around the values it covers: a box when it sits in one
//  row, two when it covers the end of one row and the start of the next
//  without the two meeting, and otherwise one outline stepping in where the
//  run starts and ends partway along a row. The label goes on the top edge
//  segment TryGetLabelSegment chooses, over a patch of the background that
//  breaks the line there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexRegions::PaintRun (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    int64_t              firstRow,
    int                  firstUnit,
    int64_t              lastRow,
    int                  lastUnit,
    uint16_t             region)
{
    int                      units  = GetUnitsPerRow();
    uint32_t                 argb   = 0;
    uint32_t                 stroke = 0;
    std::wstring             label;
    std::vector<DxuiPointF>  points;
    LabelSegment             segment;
    DxuiFontHandle           font   = theme.MonospaceFont();
    float                    left   = 0.0f;
    float                    edgeY  = 0.0f;
    float                    width  = 0.0f;
    float                    height = 0.0f;
    HRESULT                  hr     = S_OK;



    if (m_view.m_source == nullptr || !m_view.m_source->TryGetRegionStyle (region, argb, label))
    {
        return;
    }

    stroke = DxuiColor::Mix (theme.TextViewBackground(), argb, kOutlineMix);

    if (firstRow == lastRow)
    {
        points = { { GetUnitLeft (firstUnit), GetEdgeY (firstRow) }, { GetUnitRight (lastUnit), GetEdgeY (firstRow) },
                   { GetUnitRight (lastUnit), GetEdgeY (lastRow + 1) }, { GetUnitLeft (firstUnit), GetEdgeY (lastRow + 1) } };
        DrawOutline (painter, points, stroke);
    }
    else if (lastRow == firstRow + 1 && lastUnit < firstUnit)
    {
        points = { { GetUnitLeft (firstUnit), GetEdgeY (firstRow) }, { GetUnitRight (units - 1), GetEdgeY (firstRow) },
                   { GetUnitRight (units - 1), GetEdgeY (lastRow) },  { GetUnitLeft (firstUnit), GetEdgeY (lastRow) } };
        DrawOutline (painter, points, stroke);

        points = { { GetUnitLeft (0), GetEdgeY (lastRow) },          { GetUnitRight (lastUnit), GetEdgeY (lastRow) },
                   { GetUnitRight (lastUnit), GetEdgeY (lastRow + 1) }, { GetUnitLeft (0), GetEdgeY (lastRow + 1) } };
        DrawOutline (painter, points, stroke);
    }
    else
    {
        points.push_back ({ GetUnitLeft (firstUnit),  GetEdgeY (firstRow) });
        points.push_back ({ GetUnitRight (units - 1), GetEdgeY (firstRow) });

        if (lastUnit == units - 1)
        {
            points.push_back ({ GetUnitRight (units - 1), GetEdgeY (lastRow + 1) });
        }
        else
        {
            points.push_back ({ GetUnitRight (units - 1), GetEdgeY (lastRow) });
            points.push_back ({ GetUnitRight (lastUnit),  GetEdgeY (lastRow) });
            points.push_back ({ GetUnitRight (lastUnit),  GetEdgeY (lastRow + 1) });
        }

        points.push_back ({ GetUnitLeft (0), GetEdgeY (lastRow + 1) });

        if (firstUnit > 0)
        {
            points.push_back ({ GetUnitLeft (0),         GetEdgeY (firstRow + 1) });
            points.push_back ({ GetUnitLeft (firstUnit), GetEdgeY (firstRow + 1) });
        }

        DrawOutline (painter, points, stroke);
    }

    //  A run that began above the rows looked at has its label off the top.
    if (label.empty() || firstRow < 0 || !TryGetLabelSegment ((uint64_t) firstRow, firstUnit, segment))
    {
        return;
    }

    width  = (float) GetLabelWidth (region);
    height = (float) GetLabelLane();
    edgeY  = GetEdgeY ((int64_t) segment.row);
    left   = PlaceLabel (GetUnitLeft (segment.first), GetUnitRight (segment.last), width,
                         m_view.m_scaler.ToPxf ((float) kLabelLeadDip), GetUnitLeft (0), GetUnitRight (units - 1));

    font.sizeDip = m_view.m_scaler.ToPxf (font.sizeDip * m_view.m_painter.m_zoom) * kLabelScale;

    DxuiHexPainter::FillCell (text, RECT { (LONG) left, (LONG) (edgeY - height / 2.0f),
                                           (LONG) (left + width), (LONG) (edgeY + height / 2.0f) }, theme.TextViewBackground());

    hr = text.DrawString (label.c_str(), left, edgeY - height / 2.0f, width, height, argb,
                          font.sizeDip, font.face, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::PlaceLabel
//
////////////////////////////////////////////////////////////////////////////////

float DxuiHexRegions::PlaceLabel (float left, float right, float width, float lead, float rowLeft, float rowRight)
{
    float  place = (left + right - width) / 2.0f;



    place = (std::max) (place, left + lead);
    place = (std::min) (place, rowRight - width);
    place = (std::max) (place, rowLeft + lead);

    return place;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexRegions::DrawOutline
//
//  A closed outline whose corners are rounded, as DxuiRoundedOutline has it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexRegions::DrawOutline (IDxuiPainter & painter, const std::vector<DxuiPointF> & points, uint32_t argb) const
{
    float  thickness = m_view.m_scaler.ToPxf (1.0f);



    for (const DxuiRoundedOutline::Segment & segment : DxuiRoundedOutline::GetSegments (points, m_view.m_scaler.ToPxf ((float) kCornerDip)))
    {
        painter.DrawLine (segment.from.x, segment.from.y, segment.to.x, segment.to.y, thickness, argb);
    }
}
