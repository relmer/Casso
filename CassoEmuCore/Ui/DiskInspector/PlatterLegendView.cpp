#include "Pch.h"

#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"





static constexpr float  s_kSwatchDip = 10.0f;
static constexpr float  s_kGapDip    = 6.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::Paint
//
//  Each section under its title, its entries in a grid of kColumns columns
//  filled row by row, then its note.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterLegendView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    float  rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float  swatch  = m_scaler.ToPxf (s_kSwatchDip);
    float  gap     = m_scaler.ToPxf (s_kGapDip);
    float  pad     = m_scaler.ToPxf (static_cast<float> (kPadDip));
    float  textPx  = m_scaler.ToPxf (kSmallDip);
    float  left    = static_cast<float> (m_boundsDip.left) + pad;
    float  y       = static_cast<float> (m_boundsDip.top) + pad;
    float  columnW = (GetWidth() - 2 * pad) / kColumns;



    for (const LegendSection & section : BuildSections (m_context))
    {
        text.DrawString (section.title.c_str(), left, y, GetWidth() - 2 * pad, rowH, theme.HeadingForeground(), textPx, DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        y += rowH;

        for (size_t i = 0; i < section.entries.size(); i++)
        {
            float  x  = left + static_cast<float> (i % kColumns) * columnW;
            float  ey = y + static_cast<float> (i / kColumns) * rowH;

            painter.FillRect    (x, ey + (rowH - swatch) / 2, swatch, swatch, section.entries[i].argb);
            painter.OutlineRect (x, ey + (rowH - swatch) / 2, swatch, swatch, 1.0f, theme.Border());
            text.DrawString (section.entries[i].label.c_str(), x + swatch + gap, ey, columnW - swatch - gap, rowH, theme.Foreground(), textPx,
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        y += static_cast<float> ((section.entries.size() + kColumns - 1) / kColumns) * rowH;

        if (!section.note.empty())
        {
            text.DrawString (section.note.c_str(), left, y, GetWidth() - 2 * pad, rowH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                             DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            y += rowH;
        }

        y += rowH / 2;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::GetHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int PlatterLegendView::GetHeightDip() const
{
    int  rows = 0;



    for (const LegendSection & section : BuildSections (m_context))
    {
        rows += 2 + static_cast<int> ((section.entries.size() + kColumns - 1) / kColumns) * 2 + (section.note.empty() ? 0 : 2) + 1;
    }

    return rows * kRowDip / 2 + 2 * kPadDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::BuildSections
//
//  The platter for its mode, then the file map's roles while the Files
//  overlay colors the platter.
//
////////////////////////////////////////////////////////////////////////////////

vector<LegendSection> PlatterLegendView::BuildSections (const InspectorViewContext & context)
{
    vector<LegendSection>  sections;
    LegendSection          files;
    int                    role     = 0;



    sections.push_back ({ context.isTimingMode ? L"Platter: Timing" : L"Platter: Structure",
                          BuildEntries (context.palette, context.isTimingMode, context.timingRange), GetNote (context.isTimingMode) });

    if (context.isFilesOverlay && context.fileMap != nullptr && context.fileMap->notMapped == NotMappedReason::None)
    {
        files.title = L"Files overlay: " + FileMapText::FormatVolume (*context.fileMap);

        for (role = 0; role < static_cast<int> (MapRole::Count); role++)
        {
            files.entries.push_back ({ context.palette.GetRoleColor (static_cast<MapRole> (role)),
                                       FileMapText::FormatRole (context.fileMap->fileSystem, static_cast<SectorRole> (role)) });
        }

        sections.push_back (files);
    }

    return sections;
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
