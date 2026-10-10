#include "Pch.h"

#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"





static constexpr float  s_kSwatchDip = 10.0f;
static constexpr float  s_kGapDip    = 6.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::Paint
//
//  Entries fill a grid of kColumns columns, row by row, and the note takes
//  the row after the last, so the legend's height never depends on how
//  wide its labels are.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterLegendView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float                rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float                swatch  = m_scaler.ToPxf (s_kSwatchDip);
    float                gap     = m_scaler.ToPxf (s_kGapDip);
    float                textPx  = m_scaler.ToPxf (kSmallDip);
    float                columnW = GetWidth() / kColumns;
    float                left    = static_cast<float> (m_boundsDip.left);
    float                top     = static_cast<float> (m_boundsDip.top);
    vector<LegendEntry>  entries = BuildEntries (m_context.palette, m_context.isTimingMode, m_context.timingRange);
    std::wstring         note    = GetNote (m_context.isTimingMode);
    int                  rows    = static_cast<int> ((entries.size() + kColumns - 1) / kColumns);



    if (m_context.hasDisk)
    {
        for (size_t i = 0; i < entries.size(); i++)
        {
            float  x = left + static_cast<float> (i % kColumns) * columnW;
            float  y = top  + static_cast<float> (i / kColumns) * rowH;

            painter.FillRect    (x, y + (rowH - swatch) / 2, swatch, swatch, entries[i].argb);
            painter.OutlineRect (x, y + (rowH - swatch) / 2, swatch, swatch, 1.0f, theme.Border());
            text.DrawString (entries[i].label.c_str(), x + swatch + gap, y, columnW - swatch - gap, rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        text.DrawString (note.c_str(), left, top + static_cast<float> (rows) * rowH, GetWidth(), rowH, theme.ForegroundMuted(), textPx,
                         DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
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
        entries.push_back ({ c.timingNominal, L"Nominal 3.91 " + std::wstring (s_kpszMicro) + L"s" });
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
