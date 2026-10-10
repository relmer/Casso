#include "Pch.h"

#include "Ui/DiskInspector/FileMapGridView.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"





static constexpr float     s_kLabelDip      = 26.0f;
static constexpr float     s_kHeaderDip     = 16.0f;
static constexpr float     s_kNoteDip       = 34.0f;
static constexpr float     s_kMinRowDip     = 9.0f;
static constexpr float     s_kMaxRowDip     = 16.0f;
static constexpr float     s_kLetterMinDip  = 12.0f;
static constexpr float     s_kOutlineDip    = 2.0f;
static constexpr float     s_kDiffBandDip   = 3.0f;
static constexpr uint32_t  s_kMarkAlpha     = 0xE0000000u;





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
        }
    }

    for (int c = 0; c < m_map->cellsPerTrack; c++)
    {
        std::wstring  label = (m_map->cellsPerTrack == 8) ? std::to_wstring (c) : std::format (L"{:X}", c);

        text.DrawString (label.c_str(), g.left + g.labelW + c * g.cellW, g.top - m_scaler.ToPxf (s_kHeaderDip), g.cellW, m_scaler.ToPxf (s_kHeaderDip),
                         theme.ForegroundMuted(), textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    //  Rows too thin for every number are labeled every few tracks.
    for (int t = 0; t < m_map->tracks; t += std::max (1, static_cast<int> (std::ceil (textPx / g.rowH))))
    {
        text.DrawString (std::to_wstring (t).c_str(), g.left, g.top + t * g.rowH, g.labelW - m_scaler.ToPxf (4.0f), g.rowH, theme.ForegroundMuted(), textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }

    for (cell = 0; cell < static_cast<int> (m_map->cells.size()); cell++)
    {
        PaintCell (painter, text, theme, g, cell, numbers[cell]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::PaintCell
//
////////////////////////////////////////////////////////////////////////////////

void FileMapGridView::PaintCell (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const Grid & g, int cell, int number)
{
    const MapCell &  at     = m_map->cells[cell];
    float            x      = g.left + g.labelW + m_map->GetColumn (cell) * g.cellW;
    float            y      = g.top + m_map->GetTrack (cell) * g.rowH;
    float            w      = g.cellW - 1.0f;
    float            h      = g.rowH - 1.0f;
    uint32_t         color  = (at.result == SectorResult::Missing) ? m_context.palette.colors.nothingRecorded
                                                                    : m_context.palette.GetRoleColor (static_cast<MapRole> (at.role));
    uint32_t         ink    = (DiskInspectorPalette::GetTextColorOn (color) & 0x00FFFFFFu) | s_kMarkAlpha;
    float            textPx = m_scaler.ToPxf (kSmallDip);
    std::wstring     label;



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

    if (number > 0)
    {
        painter.OutlineRect (x, y, w, h, 1.0f, theme.Accent());
    }

    //  While comparing, a cell of the selected file pair that differs (FR-120).
    if (m_context.diffCells != nullptr && std::find (m_context.diffCells->begin(), m_context.diffCells->end(), cell) != m_context.diffCells->end())
    {
        painter.FillRect    (x, y + h - m_scaler.ToPxf (s_kDiffBandDip), w, m_scaler.ToPxf (s_kDiffBandDip), m_context.palette.colors.difference);
        painter.OutlineRect (x + 1, y + 1, w - 2, h - 2, m_scaler.ToPxf (s_kOutlineDip), m_context.palette.colors.difference);
    }

    if (cell == m_selectedCell)
    {
        painter.OutlineRect (x - 1, y - 1, w + 2, h + 2, m_scaler.ToPxf (s_kOutlineDip), theme.FocusRing());
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
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return cell >= 0;
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
    float  header = m_scaler.ToPxf (s_kHeaderDip) + ((m_map != nullptr && m_map->notMapped != NotMappedReason::None) ? m_scaler.ToPxf (s_kNoteDip) : 0.0f);
    float  rowH   = std::clamp (static_cast<float> (widthPx) / 40.0f, m_scaler.ToPxf (s_kMinRowDip), m_scaler.ToPxf (s_kMaxRowDip));



    return static_cast<int> (header + 35.0f * rowH + 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileMapGridView::MakeGrid
//
////////////////////////////////////////////////////////////////////////////////

FileMapGridView::Grid FileMapGridView::MakeGrid() const
{
    Grid   g;
    float  header  = m_scaler.ToPxf (s_kHeaderDip);
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
