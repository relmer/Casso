#include "Pch.h"

#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr float  s_kSwatchDip = 10.0f;
static constexpr float  s_kGapDip    = 6.0f;
static constexpr float  s_kItemGapDip = 14.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::Paint
//
//  Entries flow left to right and onto further rows, then the note.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterLegendView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float         rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float         swatch  = m_scaler.ToPxf (s_kSwatchDip);
    float         gap     = m_scaler.ToPxf (s_kGapDip);
    float         itemGap = m_scaler.ToPxf (s_kItemGapDip);
    float         textPx  = m_scaler.ToPxf (kSmallDip);
    float         x       = static_cast<float> (m_boundsDip.left);
    float         y       = static_cast<float> (m_boundsDip.top);
    float         w       = 0;
    float         h       = 0;
    std::wstring  note    = GetNote (m_context.isTimingMode);



    if (m_context.hasDisk)
    {
        for (const LegendEntry & entry : BuildEntries (m_context.palette, m_context.isTimingMode, m_context.timingRange))
        {
            text.MeasureString (entry.label.c_str(), textPx, DxuiTheme::kBodyFace, w, h);

            if (x + swatch + gap + w > m_boundsDip.right && x > m_boundsDip.left)
            {
                x  = static_cast<float> (m_boundsDip.left);
                y += rowH;
            }

            painter.FillRect    (x, y + (rowH - swatch) / 2, swatch, swatch, entry.argb);
            painter.OutlineRect (x, y + (rowH - swatch) / 2, swatch, swatch, 1.0f, theme.Border());
            text.DrawString (entry.label.c_str(), x + swatch + gap, y, w + 1.0f, rowH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

            x += swatch + gap + w + itemGap;
        }

        if (!note.empty())
        {
            text.DrawString (note.c_str(), static_cast<float> (m_boundsDip.left), y + rowH, GetWidth(), rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

vector<LegendEntry> PlatterLegendView::BuildEntries (const DiskInspectorPalette & palette, bool isTimingMode, double range)
{
    const DiskInspectorColors &  c       = palette.colors;
    vector<LegendEntry>          entries;
    std::wstring                 percent = InspectorFormat::FormatPercent (range).substr (1);



    if (isTimingMode)
    {
        entries.push_back ({ c.timingFast,    L"Fast cells (" + std::wstring (s_kpszMinus) + percent + L")" });
        entries.push_back ({ c.timingNominal, L"Nominal 3.91" + std::wstring (s_kpszMicro) + L"s" });
        entries.push_back ({ c.timingSlow,    L"Slow cells (+" + percent + L")" });
    }
    else
    {
        entries.push_back ({ c.sync,            L"Sync" });
        entries.push_back ({ c.addressMark,     L"Address marks" });
        entries.push_back ({ c.addressField,    L"Address field" });
        entries.push_back ({ c.dataMark,        L"Data marks" });
        entries.push_back ({ c.dataField,       L"Data field" });
        entries.push_back ({ c.failedChecksum,  L"Failed checksum" });
        entries.push_back ({ c.other,           L"Other" });
        entries.push_back ({ c.noise,           L"Noise" });
        entries.push_back ({ c.randomBits,      L"Random bits" });
        entries.push_back ({ c.nothingRecorded, L"Nothing recorded" });
        entries.push_back ({ c.damageHatch,     L"Damaged" });
        entries.push_back ({ c.pendingPattern,  L"Analyzing" });
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::GetNote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PlatterLegendView::GetNote (bool isTimingMode)
{
    return isTimingMode ? L"Only flux tracks record timing; other tracks are dimmed" : L"Rings past the head's reach are dimmed";
}
