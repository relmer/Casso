#include "Pch.h"

#include "Ui/DiskInspector/FileMapGridView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorMarquee.h"





static constexpr float     s_kLabelDip      = 34.0f;
static constexpr float     s_kHeaderDip     = 16.0f;
static constexpr float     s_kAxisDip       = 16.0f;
static constexpr float     s_kNoteDip       = 34.0f;
//  Rows no shorter than a label's text, so every cell has room for its
//  number or role letter.
static constexpr float     s_kMinRowDip     = InspectorView::kSmallDip;
static constexpr float     s_kMaxRowDip     = 16.0f;
static constexpr float     s_kLetterMinDip  = 12.0f;
static constexpr float     s_kOutlineDip    = 2.0f;
static constexpr float     s_kDiffBandDip   = 3.0f;
static constexpr uint32_t  s_kMarkAlpha     = 0xE0000000u;
static constexpr float     s_kShimmerSlicePx = 2.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_map != nullptr)
    {
        PaintMap (painter, text, theme);
    }
    else if (m_context.hasDisk)
    {
        text.DrawString (L"The map is built once every track has been analyzed.", static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top),
                         GetWidth(), m_scaler.ToPxf (s_kNoteDip), theme.ForegroundMuted(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintMap
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintMap (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    Grid          g      = MakeGrid();
    float         textPx = m_scaler.ToPxf (kSmallDip);
    int           cell   = 0;
    int           number = 0;
    bool          isFile = false;
    float         labelH = textPx * 1.4f;
    vector<int>   numbers;



    if (m_map->notMapped != NotMappedReason::None)
    {
        text.DrawString (FileMapText::FormatNotMapped (*m_map).c_str(), static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top), GetWidth(),
                         m_scaler.ToPxf (s_kNoteDip), theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left,
                         DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
    }

    //  The selected file's sectors numbered in file order.
    numbers.assign (m_map->cells.size(), 0);

    for (int k = 0; m_selectedFile >= 0 && m_selectedFile < static_cast<int> (m_map->files.size()) && k < static_cast<int> (m_map->files[m_selectedFile].sectors.size()); k++)
    {
        int  at = m_map->files[m_selectedFile].sectors[k].cell;

        number += (at >= 0) ? 1 : 0;

        if (at >= 0 && numbers[at] == 0)
        {
            numbers[at] = number;
            isFile      = true;
        }
    }

    text.DrawString (FileMapText::FormatColumnAxis (*m_map).c_str(), g.left + g.labelW, g.top - m_scaler.ToPxf (s_kHeaderDip + s_kAxisDip), GetWidth() - g.labelW,
                     m_scaler.ToPxf (s_kAxisDip), theme.Foreground(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                     DxuiFontWeight::SemiBold, false);
    text.DrawString (L"Track", g.left, g.top - m_scaler.ToPxf (s_kHeaderDip), g.labelW - m_scaler.ToPxf (4.0f), m_scaler.ToPxf (s_kHeaderDip), theme.Foreground(),
                     textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

    for (int c = 0; c < m_map->cellsPerTrack; c++)
    {
        std::wstring  label = (m_map->cellsPerTrack == 8) ? std::to_wstring (c) : std::format (L"{:X}", c);

        text.DrawString (label.c_str(), g.left + g.labelW + c * g.cellW, g.top - m_scaler.ToPxf (s_kHeaderDip), g.cellW, m_scaler.ToPxf (s_kHeaderDip),
                         theme.ForegroundMuted(), textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    //  Rows too thin for every number are labeled every few tracks, each
    //  label a line of text tall, centered on its row, so a row thinner
    //  than the text does not clip it.
    for (int t = 0; t < m_map->tracks; t += std::max (1, static_cast<int> (std::ceil (textPx / g.rowH))))
    {
        text.DrawString (std::to_wstring (t).c_str(), g.left, g.top + (t + 0.5f) * g.rowH - labelH / 2, g.labelW - m_scaler.ToPxf (4.0f), labelH, theme.ForegroundMuted(), textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    for (cell = 0; cell < static_cast<int> (m_map->cells.size()); cell++)
    {
        PaintCell (painter, text, theme, g, cell, numbers[cell], isFile && numbers[cell] == 0);
    }

    PaintMarquee (painter, g, numbers);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintCell
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintCell (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, int cell, int number, bool isDimmed)
{
    const MapCell &  at     = m_map->cells[cell];
    float            x      = g.left + g.labelW + m_map->GetColumn (cell) * g.cellW;
    float            y      = g.top + m_map->GetTrack (cell) * g.rowH;
    float            w      = g.cellW - 1.0f;
    float            h      = g.rowH - 1.0f;
    uint32_t         color  = GetCellColor (cell);
    uint32_t         ink    = (DiskInspectorPalette::GetTextColorOn (color) & 0x00FFFFFFu) | s_kMarkAlpha;
    float            textPx = m_scaler.ToPxf (kSmallDip);
    std::wstring     label;



    //  Every sector but the selected file's is dimmed, so the file's are the
    //  brightest in the grid wherever they lie.
    color = isDimmed ? InspectorMarquee::Dim (m_context.palette, color) : color;
    ink   = isDimmed ? InspectorMarquee::Dim (m_context.palette, ink)   : ink;

    painter.FillRect (x, y, w, h, color);

    if (at.result == SectorResult::Bad)
    {
        painter.DrawLine (x + 1, y + 1, x + w - 1, y + h - 1, 1.5f, ink);
        painter.DrawLine (x + w - 1, y + 1, x + 1, y + h - 1, 1.5f, ink);
    }
    else if (at.result == SectorResult::Missing)
    {
        painter.OutlineRect (x + 1, y + 1, w - 2, h - 2, 1.0f, theme.ForegroundMuted());
    }
    else if (at.result == SectorResult::NotChecked)
    {
        painter.FillCircle (x + w - 3.0f, y + 3.0f, 1.5f, ink);
    }

    label = (number > 0) ? std::to_wstring (number) : (g.cellW >= m_scaler.ToPxf (s_kLetterMinDip) ? std::wstring (1, FileMapText::GetRoleLetter (at.role)) : L"");

    if (!label.empty() && at.result != SectorResult::Bad && g.rowH >= textPx)
    {
        text.DrawString (label.c_str(), x, y, w, h, number > 0 ? theme.Foreground() : ink, textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                         number > 0 ? DxuiFontWeight::SemiBold : DxuiFontWeight::Normal, false);
    }

    //  While comparing, a cell of the selected file pair that differs (FR-120).
    if (m_context.diffCells != nullptr && std::find (m_context.diffCells->begin(), m_context.diffCells->end(), cell) != m_context.diffCells->end())
    {
        painter.FillRect    (x, y + h - m_scaler.ToPxf (s_kDiffBandDip), w, m_scaler.ToPxf (s_kDiffBandDip), m_context.palette.colors.difference);
        painter.OutlineRect (x + 1, y + 1, w - 2, h - 2, m_scaler.ToPxf (s_kOutlineDip), m_context.palette.colors.difference);
    }

    //  The selected cell lit by the window's shimmer, in slices two pixels
    //  wide (FR-038).
    for (float sx = x; cell == m_selectedCell && sx < x + w; sx += s_kShimmerSlicePx)
    {
        painter.FillRect (sx, y, std::min (s_kShimmerSlicePx, x + w - sx), h,
                          InspectorMarquee::GetShimmer (static_cast<int64_t> (GetTickCount64()), sx + s_kShimmerSlicePx / 2, y + h / 2, m_context.scenePx));
    }

    //  The cell under the pointer lit faintly and outlined, so the tooltip's
    //  subject is plain (FR-148).
    if (cell == HitTest (m_context.hoverPx))
    {
        painter.FillRect    (x, y, w, h, InspectorMarquee::GetHoverFill());
        painter.OutlineRect (x, y, w, h, 1.0f, InspectorMarquee::GetHoverLine());
    }

    if (cell == m_selectedCell)
    {
        painter.OutlineRect (x - 1, y - 1, w + 2, h + 2, m_scaler.ToPxf (s_kOutlineDip), theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintMarquee
//
//  A marching marquee around the selected file's sectors: an edge of a
//  numbered cell is drawn where its neighbor is not numbered, each edge in
//  the direction that runs clockwise around the file's sectors, its dashes
//  counted from the grid's origin so that edges in a line join up.
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintMarquee (IDxuiPainter & painter, const Grid & g, const vector<int> & numbers) const
{
    InspectorMarquee  marquee (m_context.palette, m_scaler.ToPxf (InspectorMarquee::kDashDip), InspectorMarquee::kThicknessPx,
                               static_cast<int64_t> (GetTickCount64()));
    int               tracks  = m_map->tracks;
    int               columns = m_map->cellsPerTrack;
    float             x0      = 0;
    float             y0      = 0;
    float             x1      = 0;
    float             y1      = 0;
    auto              isMarked = [&] (int t, int c) { return t >= 0 && t < tracks && c >= 0 && c < columns && t * columns + c < static_cast<int> (numbers.size()) && numbers[t * columns + c] > 0; };



    for (int t = 0; t < tracks; t++)
    {
        for (int c = 0; c < columns; c++)
        {
            if (!isMarked (t, c))
            {
                continue;
            }

            //  On pixel centers, so each one-pixel line covers one pixel.
            x0 = std::floor (g.left + g.labelW + c * g.cellW) + 0.5f;
            y0 = std::floor (g.top + t * g.rowH) + 0.5f;
            x1 = std::floor (g.left + g.labelW + (c + 1) * g.cellW) - 0.5f;
            y1 = std::floor (g.top + (t + 1) * g.rowH) - 0.5f;

            if (!isMarked (t - 1, c))
            {
                marquee.DrawLine (painter, x0, y0, x1, y0, x0);
            }

            if (!isMarked (t, c + 1))
            {
                marquee.DrawLine (painter, x1, y0, x1, y1, y0);
            }

            if (!isMarked (t + 1, c))
            {
                marquee.DrawLine (painter, x1, y1, x0, y1, -x1);
            }

            if (!isMarked (t, c - 1))
            {
                marquee.DrawLine (painter, x0, y1, x0, y0, -y1);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::OnMouse (const DxuiMouseEvent & ev)
{
    int   cell      = HitTest (ev.positionDip);
    bool  isHandled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && cell >= 0)
    {
        isHandled = true;

        if (m_choose && m_map != nullptr && m_map->notMapped == NotMappedReason::None)
        {
            m_choose (cell);
        }
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetTooltip
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    int  cell = HitTest (pointPx);



    if (cell >= 0)
    {
        outText     = (m_map->notMapped == NotMappedReason::None) ? FileMapText::FormatTooltip (*m_map, cell)
                                                                  : std::format (L"Track {}, physical sector {}\n{}", m_map->GetTrack (cell),
                                                                                 InspectorFormat::FormatSector (m_map->GetColumn (cell)),
                                                                                 FileMapText::FormatResult (m_map->cells[cell].result));
        outText    += IsDimmed (cell) ? L"\nDimmed: not part of " + m_map->files[m_selectedFile].path : L"";
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return cell >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetTooltipSwatches
//
//  The cell's color beside the line with its role, or with its result when
//  the disk is not mapped.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    int  cell = HitTest (pointPx);



    if (cell >= 0)
    {
        outSwatches = { { 1, IsDimmed (cell) ? InspectorMarquee::Dim (m_context.palette, GetCellColor (cell)) : GetCellColor (cell) } };
    }

    return cell >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::IsDimmed
//
//  A cell outside the selected file, while that file has sectors to mark.
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::IsDimmed (int cell) const
{
    bool  isFile = m_map != nullptr && m_selectedFile >= 0 && m_selectedFile < static_cast<int> (m_map->files.size()) &&
                   std::any_of (m_map->files[m_selectedFile].sectors.begin(), m_map->files[m_selectedFile].sectors.end(), [] (const FilePlace & p) { return p.cell >= 0; });



    return isFile && std::find (m_map->cells[cell].owners.begin(), m_map->cells[cell].owners.end(), m_selectedFile) == m_map->cells[cell].owners.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetCellColor
//
//  A cell's role color, or for a missing sector the platter's color for an
//  empty quarter track.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t FileMapGridView::GetCellColor (int cell) const
{
    return (m_map->cells[cell].result == SectorResult::Missing) ? m_context.palette.colors.nothingRecorded
                                                                : m_context.palette.GetRoleColor (static_cast<MapRole> (m_map->cells[cell].role));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetHeightFor
//
//  Rows as tall as the room allows, between a minimum and a maximum.
//
////////////////////////////////////////////////////////////////////////////////

int FileMapGridView::GetHeightFor (int widthPx) const
{
    float  header = m_scaler.ToPxf (s_kHeaderDip + s_kAxisDip) + ((m_map != nullptr && m_map->notMapped != NotMappedReason::None) ? m_scaler.ToPxf (s_kNoteDip) : 0.0f);
    float  rowH   = std::clamp (static_cast<float> (widthPx) / 40.0f, m_scaler.ToPxf (s_kMinRowDip), m_scaler.ToPxf (s_kMaxRowDip));



    return static_cast<int> (header + 35.0f * rowH + 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

bool FileMapGridView::GetCellRect (int cell, RECT & outRectPx) const
{
    Grid  g      = MakeGrid();
    bool  isCell = m_map != nullptr && cell >= 0 && cell < static_cast<int> (m_map->cells.size());



    if (isCell)
    {
        outRectPx = { static_cast<LONG> (g.left + g.labelW + m_map->GetColumn (cell) * g.cellW), static_cast<LONG> (g.top + m_map->GetTrack (cell) * g.rowH),
                      static_cast<LONG> (g.left + g.labelW + (m_map->GetColumn (cell) + 1) * g.cellW), static_cast<LONG> (g.top + (m_map->GetTrack (cell) + 1) * g.rowH) };
    }

    return isCell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::MakeGrid
//
////////////////////////////////////////////////////////////////////////////////

FileMapGridView::Grid FileMapGridView::MakeGrid() const
{
    Grid   g;
    float  header  = m_scaler.ToPxf (s_kHeaderDip + s_kAxisDip);
    float  note    = (m_map != nullptr && m_map->notMapped != NotMappedReason::None) ? m_scaler.ToPxf (s_kNoteDip) : 0.0f;
    int    columns = (m_map != nullptr) ? m_map->cellsPerTrack : 16;
    int    tracks  = (m_map != nullptr) ? m_map->tracks : 35;



    g.labelW = m_scaler.ToPxf (s_kLabelDip);
    g.left   = static_cast<float> (m_boundsDip.left);
    g.top    = static_cast<float> (m_boundsDip.top) + note + header;
    g.rowH   = std::max (1.0f, (GetHeight() - note - header) / tracks);
    g.cellW  = std::max (1.0f, (GetWidth() - g.labelW) / columns);

    return g;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::HitTest
//
////////////////////////////////////////////////////////////////////////////////

int FileMapGridView::HitTest (POINT pointPx) const
{
    Grid  g      = MakeGrid();
    int   column = 0;
    int   row    = 0;
    int   cell   = -1;



    if (m_map != nullptr && PtInRect (&m_boundsDip, pointPx))
    {
        column = static_cast<int> ((pointPx.x - g.left - g.labelW) / g.cellW);
        row    = static_cast<int> ((pointPx.y - g.top) / g.rowH);

        if (pointPx.x >= g.left + g.labelW && pointPx.y >= g.top && column < m_map->cellsPerTrack && row < m_map->tracks)
        {
            cell = row * m_map->cellsPerTrack + column;
        }
    }

    return cell;
}
