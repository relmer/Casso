#include "Pch.h"

#include "Ui/DiskInspector/SectorByteView.h"
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
    float               gap    = m_scaler.ToPxf (s_kHeaderGapDip);
    float               rowH   = m_scaler.ToPxf (static_cast<float> (kHeaderDip));
    float               textPx = m_scaler.ToPxf (kSmallDip);
    float               x      = static_cast<float> (m_boundsDip.left);
    float               w      = 0;
    float               h      = 0;
    std::wstring        line;



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
////////////////////////////////////////////////////////////////////////////////

void SectorByteView::PaintBytes (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const AnalyzedSector & sector, const TrackAnalysis & track, float y)
{
    const DataFieldDecode &  data    = track.fields[sector.dataField].data;
    float                    needed  = s_kOffsetCharsDip + kBytesPerRow * (s_kByteDip + s_kCharDip) + s_kTextGapDip;
    float                    fit     = std::min (1.0f, GetWidth() / m_scaler.ToPxf (needed));
    float                    rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip)) * std::max (fit, 0.75f);
    float                    textPx  = m_scaler.ToPxf (kTextDip) * fit;
    float                    byteW   = m_scaler.ToPxf (s_kByteDip) * fit;
    float                    charW   = m_scaler.ToPxf (s_kCharDip) * fit;
    float                    left    = static_cast<float> (m_boundsDip.left);
    float                    hexLeft = left + m_scaler.ToPxf (s_kOffsetCharsDip) * fit;
    float                    txtLeft = hexLeft + kBytesPerRow * byteW + m_scaler.ToPxf (s_kTextGapDip) * fit;
    bool                     isBad   = sector.isDataCheck && !sector.isDataGood;
    size_t                   i       = 0;
    int                      row     = 0;
    int                      col     = 0;
    uint32_t                 color   = 0;
    wchar_t                  ch[2]   = {};



    if (isBad)
    {
        painter.FillRect (left, y, GetWidth(), rowH, (m_context.palette.colors.failedChecksum & 0x00FFFFFF) | 0x40000000);
        text.DrawString (L"Data field checksum failed; these bytes are as decoded", left, y, GetWidth(), rowH, theme.ErrorForeground(),
                         m_scaler.ToPxf (kSmallDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        y += rowH * 1.25f;
    }

    m_grid = { y, rowH, hexLeft, txtLeft, byteW, charW, true };
    PaintSelection (painter, theme);

    for (i = 0; i < data.bytes.size(); i++)
    {
        row   = static_cast<int> (i) / kBytesPerRow;
        col   = static_cast<int> (i) % kBytesPerRow;
        color = (data.bytes[i] == 0) ? theme.ForegroundMuted() : ((data.bytes[i] & s_kHighBit) ? theme.Accent() : theme.Foreground());

        if (col == 0)
        {
            text.DrawString (std::format (L"{:02X}", i).c_str(), left, y + row * rowH, hexLeft - left, rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        text.DrawString (std::format (L"{:02X}", data.bytes[i]).c_str(), hexLeft + col * byteW, y + row * rowH, byteW, rowH, color, textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        ch[0] = GetTextChar (data.bytes[i]);
        text.DrawString (ch, txtLeft + col * charW, y + row * rowH, charW, rowH, color, textPx,
                         DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    }
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
