#include "Pch.h"

#include "Ui/DiskInspector/SectorByteView.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Ui/DiskInspector/FileMapText.h"
#include "Ui/DiskInspector/InspectorText.h"





static constexpr Byte   s_kHighBit         = 0x80;
static constexpr Byte   s_kFirstPrintable  = 0x20;
static constexpr Byte   s_kLastPrintable   = 0x7E;
static constexpr float  s_kOffsetCharsDip  = 30.0f;
static constexpr float  s_kByteDip         = 22.0f;
static constexpr float  s_kTextGapDip      = 12.0f;
static constexpr float  s_kCharDip         = 8.0f;
static constexpr float  s_kHeaderGapDip    = 20.0f;
static constexpr uint32_t  s_kSelectionAlpha = 0x50000000u;
static constexpr uint32_t  s_kDiffAlpha      = 0x50000000u;
static constexpr float     s_kDiffLineDip    = 2.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::Paint
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *   track  = m_context.GetTrack();
    const AnalyzedSector *  sector = (track != nullptr) ? m_context.model->GetSector() : nullptr;
    float                   y      = static_cast<float> (m_boundsDip.top);
    std::wstring            empty;



    m_grid.isShown = false;

    if (m_context.hasDisk && m_context.analysis != nullptr)
    {
        if (sector != nullptr)
        {
            y = PaintHeader (text, theme, *sector, *track, y);
        }

        if (sector != nullptr && sector->dataField >= 0)
        {
            PaintBytes (painter, text, theme, *sector, *track, y);
        }
        else
        {
            empty = InspectorText::FormatNoSectorData (m_context.analysis->entries[m_context.model->GetQuarterTrack()], track);
            text.DrawString (empty.c_str(), static_cast<float> (m_boundsDip.left), y, GetWidth(), m_scaler.ToPxf (static_cast<float> (kRowDip * 3)),
                             theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetTextChar
//
////////////////////////////////////////////////////////////////////////////////

wchar_t SectorByteView::GetTextChar (Byte value)
{
    Byte  masked = static_cast<Byte> (value & ~s_kHighBit);



    return (masked >= s_kFirstPrintable && masked <= s_kLastPrintable) ? static_cast<wchar_t> (masked) : L'.';
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintHeader
//
//  Labeled values, wrapping onto as many lines as the width needs; returns
//  where the bytes start.
//
////////////////////////////////////////////////////////////////////////////////

float SectorByteView::PaintHeader (IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y)
{
    vector<HeaderItem>  items  = InspectorText::BuildSectorHeader (sector, track);
    vector<HeaderItem>  owners;
    float               gap    = m_scaler.ToPxf (s_kHeaderGapDip);
    float               rowH   = m_scaler.ToPxf (static_cast<float> (kHeaderDip));
    float               textPx = m_scaler.ToPxf (kSmallDip);
    float               x      = static_cast<float> (m_boundsDip.left);
    float               w      = 0;
    float               h      = 0;
    std::wstring        line;



    //  On a whole track of a mapped volume, the sector's role and owners.
    if (m_context.fileMap != nullptr && m_context.model->GetQuarterTrack() % DiskImage::kQuarterTracksPerWholeTrack == 0)
    {
        owners = FileMapText::BuildSectorItems (*m_context.fileMap, m_context.model->GetQuarterTrack() / DiskImage::kQuarterTracksPerWholeTrack, sector.sector);
        items.insert (items.end(), owners.begin(), owners.end());
    }

    for (const HeaderItem & item : items)
    {
        line = item.label + L": " + item.value;
        text.MeasureString (line.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

        if (x + w > m_boundsDip.right && x > m_boundsDip.left)
        {
            x  = static_cast<float> (m_boundsDip.left);
            y += rowH;
        }

        text.DrawString (line.c_str(), x, y, w + 1.0f, rowH, (item.value == L"Bad") ? theme.ErrorForeground() : theme.Foreground(),
                         textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        x += w + gap;
    }

    return y + rowH * 1.5f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintBytes
//
//  The sector's bytes; while comparing, B's paired sector below them, both
//  grids sized to fit, with each byte that differs marked in both (FR-121).
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintBytes (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y)
{
    const DataFieldDecode &  data    = track.fields[sector.dataField].data;
    std::span<const Byte>    bytesB  = GetPairedBytes (track);
    bool                     isB     = m_context.trackB != nullptr;
    float                    needed  = s_kOffsetCharsDip + kBytesPerRow * (s_kByteDip + s_kCharDip) + s_kTextGapDip;
    float                    fit     = std::min (1.0f, GetWidth() / m_scaler.ToPxf (needed));
    float                    rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip)) * std::max (fit, 0.75f);
    float                    textPx  = 0;
    float                    left    = static_cast<float> (m_boundsDip.left);
    bool                     isBad   = sector.isDataCheck && !sector.isDataGood;
    int                      rows    = static_cast<int> (data.bytes.size() + kBytesPerRow - 1) / kBytesPerRow;
    ByteGrid                 grid;



    //  Both grids and B's title share the height.
    if (isB)
    {
        rowH = std::min (rowH, (static_cast<float> (m_boundsDip.bottom) - y) / (2.0f * rows + 2.5f));
        fit  = std::min (fit, rowH / m_scaler.ToPxf (static_cast<float> (kRowDip)) / 0.75f);
    }

    textPx = m_scaler.ToPxf (kTextDip) * fit;
    grid   = { y, rowH, left + m_scaler.ToPxf (s_kOffsetCharsDip) * fit, 0.0f, m_scaler.ToPxf (s_kByteDip) * fit, m_scaler.ToPxf (s_kCharDip) * fit, true };
    grid.textLeft = grid.hexLeft + kBytesPerRow * grid.byteW + m_scaler.ToPxf (s_kTextGapDip) * fit;

    if (isBad)
    {
        painter.FillRect (left, y, GetWidth(), rowH, (m_context.palette.colors.failedChecksum & 0x00FFFFFF) | 0x40000000);
        text.DrawString (L"Data field checksum failed; these bytes are as decoded", left, y, GetWidth(), rowH, theme.ErrorForeground(),
                         m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        grid.top += rowH * 1.25f;
    }

    m_grid = grid;
    PaintSelection (painter, theme);
    PaintGrid      (painter, text, theme, grid, data.bytes, bytesB, textPx);

    if (isB)
    {
        grid.top += rows * rowH + rowH * 0.5f;
        text.DrawString (!bytesB.empty() ? L"B" : L"B holds no sector to pair with this one", left, grid.top, GetWidth(), rowH, theme.HeadingForeground(),
                         m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        grid.top += rowH;

        if (!bytesB.empty())
        {
            PaintGrid (painter, text, theme, grid, bytesB, data.bytes, textPx);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintGrid
//
//  Rows of 16 under their offsets, hex then text. A byte that differs from
//  the other side's is tinted and underlined, so it shows without color;
//  with no other side, nothing is marked.
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintGrid (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const ByteGrid & grid, std::span<const Byte> bytes,
                                std::span<const Byte> other, float textPx)
{
    float     left   = static_cast<float> (m_boundsDip.left);
    float     line   = m_scaler.ToPxf (s_kDiffLineDip);
    size_t    i      = 0;
    int       row    = 0;
    int       col    = 0;
    float     rowTop = 0;
    uint32_t  color  = 0;
    wchar_t   ch[2]  = {};



    for (i = 0; i < bytes.size(); i++)
    {
        row    = static_cast<int> (i) / kBytesPerRow;
        col    = static_cast<int> (i) % kBytesPerRow;
        rowTop = grid.top + row * grid.rowH;
        color  = (bytes[i] == 0) ? theme.ForegroundMuted() : ((bytes[i] & s_kHighBit) ? theme.Accent() : theme.Foreground());

        if (col == 0)
        {
            text.DrawString (std::format (L"{:02X}", i).c_str(), left, rowTop, grid.hexLeft - left, grid.rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        if (!other.empty() && (i >= other.size() || other[i] != bytes[i]))
        {
            painter.FillRect (grid.hexLeft + col * grid.byteW, rowTop, grid.byteW * 0.8f, grid.rowH, (m_context.palette.colors.difference & 0x00FFFFFFu) | s_kDiffAlpha);
            painter.FillRect (grid.hexLeft + col * grid.byteW, rowTop + grid.rowH - line, grid.byteW * 0.8f, line, m_context.palette.colors.difference);
            painter.FillRect (grid.textLeft + col * grid.charW, rowTop + grid.rowH - line, grid.charW, line, m_context.palette.colors.difference);
        }

        text.DrawString (std::format (L"{:02X}", bytes[i]).c_str(), grid.hexLeft + col * grid.byteW, rowTop, grid.byteW, grid.rowH, color, textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        ch[0] = GetTextChar (bytes[i]);
        text.DrawString (ch, grid.textLeft + col * grid.charW, rowTop, grid.charW, grid.rowH, color, textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::GetPairedBytes
//
//  B's bytes for the selected sector while comparing: the sector B pairs
//  with it on the same quarter track, when that sector has data.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const Byte> SectorByteView::GetPairedBytes (const TrackAnalysis & track) const
{
    const TrackAnalysis *  trackB = m_context.trackB;
    int                    paired = (trackB != nullptr) ? DiskComparer::FindPairedSector (track, m_context.model->GetSectorIndex(), *trackB) : -1;
    int                    field  = (paired >= 0) ? trackB->sectors[paired].dataField : -1;



    return (field >= 0) ? std::span<const Byte> (trackB->fields[field].data.bytes) : std::span<const Byte>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::PaintSelection
//
//  The selected bytes shaded in both columns, a run per row.
//
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintSelection (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    int       first = m_context.model->GetFirstByte();
    int       end   = first + m_context.model->GetByteCount();
    int       row   = 0;
    int       a     = 0;
    int       b     = 0;
    float     y     = 0;
    uint32_t  shade = (theme.Accent() & 0x00FFFFFFu) | s_kSelectionAlpha;



    for (row = (first >= 0) ? first / kBytesPerRow : kBytesPerRow; row * kBytesPerRow < end; row++)
    {
        a = std::max (first, row * kBytesPerRow) % kBytesPerRow;
        b = (std::min (end, (row + 1) * kBytesPerRow) - 1) % kBytesPerRow + 1;
        y = m_grid.top + row * m_grid.rowH;

        painter.FillRect (m_grid.hexLeft + a * m_grid.byteW, y, (b - a) * m_grid.byteW, m_grid.rowH, shade);
        painter.FillRect (m_grid.textLeft + a * m_grid.charW, y, (b - a) * m_grid.charW, m_grid.rowH, shade);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::OnMouse
//
//  A press on a byte in either column selects it, Shift extends the
//  selection to it, and a drag extends it as it goes (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

bool SectorByteView::OnMouse (const DxuiMouseEvent & ev)
{
    bool  isText    = false;
    int   index     = HitTest (ev.positionDip, isText);
    bool  isHandled = false;



    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left && index >= 0)
    {
        if (ev.shift && m_context.model->GetByteCount() > 0)
        {
            m_context.model->ExtendBytes (index);
        }
        else
        {
            m_context.model->SelectBytes (index, 1, isText);
        }

        m_isDragging = true;
        isHandled    = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Move && m_isDragging)
    {
        if (index >= 0)
        {
            m_context.model->ExtendBytes (index);
        }

        isHandled = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && m_isDragging)
    {
        m_isDragging = false;
        isHandled    = true;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorByteView::HitTest
//
//  The byte under a point and which column it is in, or -1. A drag past
//  either column's edge takes the byte at that edge.
//
////////////////////////////////////////////////////////////////////////////////

int SectorByteView::HitTest (POINT pointPx, bool & outIsText) const
{
    int    index    = -1;
    int    row      = 0;
    int    col      = 0;
    float  hexRight = m_grid.hexLeft + kBytesPerRow * m_grid.byteW;
    float  txtRight = m_grid.textLeft + kBytesPerRow * m_grid.charW;



    outIsText = pointPx.x >= m_grid.textLeft - (m_grid.textLeft - hexRight) / 2;

    if (m_grid.isShown && pointPx.y >= m_grid.top && pointPx.y < m_grid.top + kBytesPerRow * m_grid.rowH && pointPx.x < txtRight + m_grid.charW &&
        pointPx.x >= m_grid.hexLeft - m_grid.byteW)
    {
        row   = std::clamp (static_cast<int> ((pointPx.y - m_grid.top) / m_grid.rowH), 0, kBytesPerRow - 1);
        col   = outIsText ? static_cast<int> ((pointPx.x - m_grid.textLeft) / m_grid.charW) : static_cast<int> ((pointPx.x - m_grid.hexLeft) / m_grid.byteW);
        col   = std::clamp (col, 0, kBytesPerRow - 1);
        index = row * kBytesPerRow + col;
    }

    return index;
}
