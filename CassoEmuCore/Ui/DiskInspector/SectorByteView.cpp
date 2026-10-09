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
