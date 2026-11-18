#include "Pch.h"

#include "Ui/DiskInspector/PlatterLegendView.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/InspectorFormat.h"
#include "Ui/DiskInspector/FileMapText.h"





static constexpr float  s_kSwatchDip = 10.0f;
static constexpr float  s_kGapDip    = 6.0f;
static constexpr float  s_kColumnGapDip = 16.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::Paint
//
//  Each section under its title, its entries in a grid of kColumns columns
//  filled row by row or, with descriptions, one to a row, then its note.
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
    float  columnW = m_scaler.ToPxf (GetColumnDip (text, m_context));



    for (const LegendSection & section : BuildSections (m_context))
    {
        text.DrawString (section.title.c_str(), left, y, GetWidth() - 2 * pad, rowH, theme.HeadingForeground(), textPx, DxuiTheme::kBodyFace,
                         DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
        y += rowH;

        int  columns = GetColumnCount (section);

        for (size_t i = 0; i < section.entries.size(); i++)
        {
            float  x  = left + static_cast<float> (i % columns) * columnW;
            float  ey = y + static_cast<float> (i / columns) * rowH;

            painter.FillRect    (x, ey + (rowH - swatch) / 2, swatch, swatch, section.entries[i].argb);
            painter.OutlineRect (x, ey + (rowH - swatch) / 2, swatch, swatch, 1.0f, theme.Border());
            text.DrawString (section.entries[i].label.c_str(), x + swatch + gap, ey, columnW - swatch - gap, rowH, theme.Foreground(), textPx,
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
            text.DrawString (section.entries[i].description.c_str(), x + columnW, ey, GetWidth() - pad - x - columnW, rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }

        y += static_cast<float> ((section.entries.size() + columns - 1) / columns) * rowH;

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
//  PlatterLegendView::GetColumnDip
//
//  A column as wide as its widest entry: the swatch, the gap, the label and
//  a gap before the next column.
//
////////////////////////////////////////////////////////////////////////////////

float PlatterLegendView::GetColumnDip (IDxuiTextRenderer & text, const InspectorViewContext & context)
{
    float  widest = 0;
    float  w      = 0;
    float  h      = 0;



    for (const LegendSection & section : BuildSections (context))
    {
        for (const LegendEntry & entry : section.entries)
        {
            text.MeasureString (entry.label.c_str(), kSmallDip, DxuiTheme::kBodyFace, w, h);
            widest = std::max (widest, w);
        }
    }

    return s_kSwatchDip + s_kGapDip + std::ceil (widest) + s_kColumnGapDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::GetWidthDip
//
//  The columns side by side, a label's column and its description, or a title
//  or note, whichever is widest, and the padding.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterLegendView::GetWidthDip (IDxuiTextRenderer & text, const InspectorViewContext & context)
{
    float  column = GetColumnDip (text, context);
    float  widest = 0;
    float  w      = 0;
    float  h      = 0;



    for (const LegendSection & section : BuildSections (context))
    {
        for (const std::wstring & line : { section.title, section.note })
        {
            text.MeasureString (line.c_str(), kSmallDip, DxuiTheme::kBodyFace, w, h);
            widest = std::max (widest, w);
        }

        widest = std::max (widest, column * GetColumnCount (section));

        for (const LegendEntry & entry : section.entries)
        {
            text.MeasureString (entry.description.c_str(), kSmallDip, DxuiTheme::kBodyFace, w, h);
            widest = std::max (widest, entry.description.empty() ? 0.0f : column + w);
        }
    }

    return static_cast<int> (std::ceil (widest)) + 2 * kPadDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::GetColumnCount
//
////////////////////////////////////////////////////////////////////////////////

int PlatterLegendView::GetColumnCount (const LegendSection & section)
{
    bool  hasDescriptions = std::any_of (section.entries.begin(), section.entries.end(), [] (const LegendEntry & entry) { return !entry.description.empty(); });



    return hasDescriptions ? 1 : kColumns;
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
        rows += 2 + static_cast<int> ((section.entries.size() + GetColumnCount (section) - 1) / GetColumnCount (section)) * 2 + (section.note.empty() ? 0 : 2) + 1;
    }

    return rows * kRowDip / 2 + 2 * kPadDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterLegendView::BuildSections
//
//  The platter for its mode, then the file map's roles while the Files
//  overlay colors the platter, then the find hits' color while the views
//  mark them.
//
////////////////////////////////////////////////////////////////////////////////

vector<LegendSection> PlatterLegendView::BuildSections (const InspectorViewContext & context)
{
    vector<LegendSection>  sections;
    LegendSection          files;
    int                    role     = 0;



    sections.push_back ({ context.isTimingMode ? L"Platter timing" : L"Platter structure",
                          BuildEntries (context.palette, context.isTimingMode, context.timingRange), GetNote (context.isTimingMode) });

    sections.push_back ({ L"Head", { { context.palette.colors.headIdle, L"Idle", L"The machine isn't reading or writing the disk" },
                                     { context.palette.colors.headReading, L"Reading", L"The machine is reading the track under the head" },
                                     { context.palette.colors.headWriting, L"Writing", L"The machine is writing the track under the head" } },
                          L"A cross on the head indicates a blocked write." });

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

    if (context.findMarks != nullptr)
    {
        sections.push_back ({ L"Find", { { context.palette.colors.findHit, context.findMarks->FormatHitLine (1) } }, L"" });
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
        entries.push_back ({ c.timingFast,    L"Fast cells (" + std::wstring (s_kpszMinus) + percent + L")", L"Cells this much shorter than nominal or more" });
        entries.push_back ({ c.timingNominal, L"Nominal 3.91 " + std::wstring (s_kpszMicro) + L"s", L"The standard cell length; closer cells blend toward this color" });
        entries.push_back ({ c.timingSlow,    L"Slow cells (+" + percent + L")", L"Cells this much longer than nominal or more" });
    }
    else
    {
        entries.push_back ({ c.sync,            L"Sync",             L"$FF nibbles with extra 0 bits, which bring a read into step with the nibbles" });
        entries.push_back ({ c.addressMark,     L"Address marks",    L"The bytes before and after an address field" });
        entries.push_back ({ c.addressField,    L"Address field",    L"A sector's volume, track, sector number and checksum" });
        entries.push_back ({ c.dataMark,        L"Data marks",       L"The bytes before and after a data field" });
        entries.push_back ({ c.dataField,       L"Data field",       L"A sector's contents" });
        entries.push_back ({ c.failedChecksum,  L"Failed checksum",  L"A field whose contents don't match its stored checksum" });
        entries.push_back ({ c.other,           L"Other",            L"Valid nibbles outside any standard field, such as gaps or copy protection" });
        entries.push_back ({ c.noise,           L"Noise",            L"A nibble with three 0 cells in a row, which no valid disk byte has" });
        entries.push_back ({ c.randomBits,      L"Random bits",      L"A long stretch with no flux transitions, where a real drive reads random bits" });
        entries.push_back ({ c.nothingRecorded, L"Nothing recorded", L"The image has no data for this quarter track" });
        entries.push_back ({ c.damageHatch,     L"Damaged",          L"The image's data for this track can't be read" });
        entries.push_back ({ c.pendingPattern,  L"Analyzing",        L"This track's analysis hasn't finished" });
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
