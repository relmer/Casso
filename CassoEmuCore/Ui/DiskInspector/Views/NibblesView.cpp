#include "Pch.h"

#include "Ui/DiskInspector/Views/NibblesView.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Ui/DiskInspector/ComparisonText.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/InspectorTip.h"
#include "Ui/DiskInspector/PlatterCells.h"





static constexpr uint32_t  s_kRangeAlpha      = 0x60000000u;
static constexpr uint32_t  s_kDiffAlpha       = 0x50000000u;
static constexpr uint32_t  s_kHitAlpha        = 0x60000000u;
static constexpr int       s_kWheelLines      = 3;
static constexpr float     s_kSelectDip       = 2.0f;
static constexpr Byte      s_kSyncValue       = 0xFF;
static constexpr int       s_kSyncCells       = 8;
static constexpr uint32_t  s_kPanelArgb       = 0xFF0C140Fu;
static constexpr uint32_t  s_kHeaderArgb      = 0xFF1A2530u;
static constexpr float     s_kPanelRadiusDip  = 6.0f;
static constexpr float     s_kPanelPadDip     = 8.0f;
static constexpr float     s_kStripeDip       = 3.0f;
static constexpr float     s_kChevronDip      = 22.0f;
static constexpr float     s_kHeaderGapDip    = 2.0f;
static constexpr LPCWSTR   s_kpszGlyphFace    = L"Segoe MDL2 Assets";
static constexpr LPCWSTR   s_kpszChevronOpen  = L"\xE70D";
static constexpr LPCWSTR   s_kpszChevronShut  = L"\xE76C";





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::Rebuild
//
//  The sections and lines for the selected track, rebuilt when the track,
//  the row width or a kind's open state changes.
//
////////////////////////////////////////////////////////////////////////////////

void NibblesView::Rebuild() const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    perRow = GetPerRow();
    uint32_t               open   = 0;



    for (size_t k = 0; k < m_isOpen.size(); k++)
    {
        open |= m_isOpen[k] ? (1u << k) : 0u;
    }

    if (track != m_builtFor || perRow != m_builtPerRow || open != m_builtOpen)
    {
        m_builtFor    = track;
        m_builtPerRow = perRow;
        m_builtOpen   = open;
        m_lines.clear();
        m_sections = (track != nullptr && m_context.analysis != nullptr) ? NibbleSections::Build (*m_context.analysis, m_context.model->GetQuarterTrack()) : vector<NibbleSection>();

        for (int s = 0; s < static_cast<int> (m_sections.size()); s++)
        {
            const NibbleSection &  section = m_sections[static_cast<size_t> (s)];

            m_lines.push_back ({ s, section.firstNibble, section.count, true });

            for (int n = 0; m_isOpen[static_cast<size_t> (section.kind)] && n < section.count; n += perRow)
            {
                m_lines.push_back ({ s, section.firstNibble + n, std::min (perRow, section.count - n), false });
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::Paint
//
//  The sections on a near-black panel with a green tint, drawn in the colors
//  for a dark surface whatever the window's theme (FR-150).
//
////////////////////////////////////////////////////////////////////////////////

void NibblesView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & surface)
{
    const IDxuiTheme &     theme  = GetPanelTheme (surface);
    const TrackAnalysis *  track  = m_context.GetTrack();
    float                  rowH   = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float                  y      = static_cast<float> (m_boundsDip.top);
    int                    line   = 0;



    Rebuild();
    m_firstLine = std::clamp (m_firstLine, 0, GetLastFirstLine());

    if (track == nullptr || track->framed.nibbles.empty())
    {
        if (m_context.hasDisk)
        {
            text.DrawString (L"Nothing recorded.", static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top), GetWidth(), rowH,
                             theme.Foreground(), m_scaler.ToPxf (kTextDip), DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
    else
    {
        painter.FillRoundedRect (m_boundsDip.left - m_scaler.ToPxf (s_kPanelPadDip), m_boundsDip.top - m_scaler.ToPxf (s_kPanelPadDip / 2.0f),
                                 GetWidth() + m_scaler.ToPxf (s_kPanelPadDip), GetHeight() + m_scaler.ToPxf (s_kPanelPadDip / 2.0f),
                                 m_scaler.ToPxf (s_kPanelRadiusDip), s_kPanelArgb);

        for (line = m_firstLine; line < static_cast<int> (m_lines.size()) && y + GetLineHeight (line) <= m_boundsDip.bottom; line++)
        {
            const Line &  l = m_lines[static_cast<size_t> (line)];

            if (l.isHeader)
            {
                PaintHeader (painter, text, theme, *track, m_sections[static_cast<size_t> (l.section)], y);
            }
            else
            {
                PaintRow (painter, text, theme, *track, l, y);
            }

            y += GetLineHeight (line);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::PaintHeader
//
//  A bar with a stripe in the section's kind color, the chevron, the field's
//  state symbol, the title, and the facts at the right end.
//
////////////////////////////////////////////////////////////////////////////////

void NibblesView::PaintHeader (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const NibbleSection & section, float y)
{
    static constexpr PlatterKind  kKindColors[] = { PlatterKind::Sync, PlatterKind::AddressField, PlatterKind::DataField, PlatterKind::Other };



    float         x       = static_cast<float> (m_boundsDip.left);
    float         w       = GetWidth();
    float         h       = m_scaler.ToPxf (static_cast<float> (kHeaderDip)) - m_scaler.ToPxf (s_kHeaderGapDip);
    float         chevron = m_scaler.ToPxf (s_kChevronDip);
    float         textPx  = m_scaler.ToPxf (kTextDip);
    float         titleW  = 0;
    float         titleH  = 0;
    bool          isOpen  = m_isOpen[static_cast<size_t> (section.kind)];
    bool          isSel   = m_selectedHeader >= 0 && &m_sections[static_cast<size_t> (m_selectedHeader)] == &section;
    std::wstring  title   = section.title;



    if (section.sectorIndex >= 0 && section.sectorIndex < static_cast<int> (track.sectors.size()))
    {
        title = std::wstring (DiskInspectorPalette::GetStateSymbol (track.sectors[static_cast<size_t> (section.sectorIndex)].state)) + L" " + title;
    }

    painter.FillRect (x, y, w, h, isSel ? theme.SelectionBackground() : s_kHeaderArgb);
    painter.FillRect (x, y, m_scaler.ToPxf (s_kStripeDip), h, m_context.palette.GetKindColor (kKindColors[static_cast<size_t> (section.kind)]));

    text.DrawString (isOpen ? s_kpszChevronOpen : s_kpszChevronShut, x + m_scaler.ToPxf (s_kStripeDip), y, chevron, h, theme.ForegroundMuted(), textPx * 0.8f,
                     s_kpszGlyphFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

    text.MeasureString (title.c_str(), textPx, DxuiTheme::kBodyFace, titleW, titleH);
    text.DrawString (title.c_str(), x + chevron + m_scaler.ToPxf (s_kStripeDip), y, w - chevron, h, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                     DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);

    //  The facts are drawn in the room after the title; the tooltip has them all.
    text.DrawString (section.facts.c_str(), x + chevron + titleW + 2 * chevron, y, std::max (0.0f, w - chevron - titleW - 2 * chevron - m_scaler.ToPxf (s_kPanelPadDip)), h,
                     theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::PaintRow
//
//  A row of a section's nibbles under the hex offset of its first.
//
////////////////////////////////////////////////////////////////////////////////

void NibblesView::PaintRow (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const TrackAnalysis & track, const Line & line, float y)
{
    float         rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float         cellW   = m_scaler.ToPxf (static_cast<float> (kCellDip));
    float         offW    = m_scaler.ToPxf (static_cast<float> (kOffsetDip));
    float         textPx  = m_scaler.ToPxf (kTextDip);
    float         smallPx = m_scaler.ToPxf (kSmallDip - 2.0f);
    float         x       = 0;
    int           nibbleB = -1;
    bool          failed  = false;
    uint32_t      color   = 0;
    std::wstring  extra;



    text.DrawString (std::format (L"{:04X}", line.firstNibble).c_str(), static_cast<float> (m_boundsDip.left), y, offW, rowH, theme.ForegroundMuted(), textPx,
                     DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

    for (int col = 0; col < line.count; col++)
    {
        int                   n      = line.firstNibble + col;
        const FramedNibble &  nibble = track.framed.nibbles[static_cast<size_t> (n)];

        x      = m_boundsDip.left + offW + col * cellW;
        failed = n < static_cast<int> (track.isFailedChecksum.size()) && track.isFailedChecksum[static_cast<size_t> (n)] != 0;
        color  = m_context.palette.GetKindColor (failed ? PlatterKind::FailedChecksum : PlatterCells::GetKindOf (track.nibbleKinds[static_cast<size_t> (n)]));

        if (IsInSelection (track, n))
        {
            painter.FillRect (x, y, cellW, rowH, theme.SelectionBackground());
        }

        if (IsInRange (n))
        {
            painter.FillRect (x, y, cellW, rowH, (theme.Accent() & 0x00FFFFFFu) | s_kRangeAlpha);
        }

        //  A nibble of a find hit (FR-056).
        if (m_context.findMarks != nullptr && m_context.findMarks->IsNibbleMarked (&track, n))
        {
            painter.FillRect    (x, y, cellW, rowH, (m_context.palette.colors.findHit & 0x00FFFFFFu) | s_kHitAlpha);
            painter.OutlineRect (x + 1, y + 1, cellW - 2, rowH - 2, m_scaler.ToPxf (1.0f), m_context.palette.colors.findHit);
        }

        //  While comparing, a nibble that differs from B's (FR-121).
        if (m_context.nibbleDiffs != nullptr && ComparisonText::FindInHunk (*m_context.nibbleDiffs, n, nibbleB))
        {
            painter.FillRect    (x, y, cellW, rowH, (m_context.palette.colors.difference & 0x00FFFFFFu) | s_kDiffAlpha);
            painter.OutlineRect (x + 1, y + 1, cellW - 2, rowH - 2, m_scaler.ToPxf (1.0f), m_context.palette.colors.difference);
        }

        painter.FillRect (x + 1, y + rowH - m_scaler.ToPxf (3.0f), cellW - 2, m_scaler.ToPxf (2.0f), color);

        text.DrawString (std::format (L"{:02X}", nibble.value).c_str(), x, y, cellW * 0.6f, rowH, IsOutsideTable (track, n) ? theme.ErrorForeground() : theme.Foreground(),
                         textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

        extra = (nibble.value == s_kSyncValue && nibble.extraZeroCells > 0) ? std::to_wstring (s_kSyncCells + nibble.extraZeroCells)
              : (nibble.extraZeroCells > 0)                                 ? L"+" + std::to_wstring (nibble.extraZeroCells)
              :                                                               std::wstring();

        if (nibble.isNoise)
        {
            painter.OutlineRect (x + 1, y + 1, cellW - 2, rowH - 2, m_scaler.ToPxf (1.0f), m_context.palette.GetKindColor (PlatterKind::Noise));
        }

        if (!extra.empty())
        {
            text.DrawString (extra.c_str(), x + cellW * 0.6f, y, cellW * 0.4f, rowH * 0.6f, theme.ForegroundMuted(), smallPx,
                             DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
        }

        if (n == m_context.model->GetFirstNibble())
        {
            painter.OutlineRect (x, y, cellW, rowH, m_scaler.ToPxf (s_kSelectDip), theme.Accent());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::OnMouse (const DxuiMouseEvent & ev)
{
    const TrackAnalysis *  track     = m_context.GetTrack();
    bool                   isInside  = PtInRect (&m_boundsDip, ev.positionDip) != FALSE;
    bool                   isHandled = false;
    int                    nibble    = -1;
    int                    line      = -1;



    if (track != nullptr && isInside)
    {
        Rebuild();

        if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal)
        {
            m_firstLine = std::clamp (m_firstLine - static_cast<int> (ev.wheelDelta * s_kWheelLines), 0, GetLastFirstLine());
            isHandled   = true;
        }
        else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
        {
            line         = HitLine (ev.positionDip);
            nibble       = HitTest (ev.positionDip);
            m_isDragging = nibble >= 0;

            if (line >= 0 && m_lines[static_cast<size_t> (line)].isHeader)
            {
                const NibbleSection &  section = m_sections[static_cast<size_t> (m_lines[static_cast<size_t> (line)].section)];

                m_selectedHeader = m_lines[static_cast<size_t> (line)].section;

                if (IsOnChevron (ev.positionDip))
                {
                    m_isOpen[static_cast<size_t> (section.kind)] = !m_isOpen[static_cast<size_t> (section.kind)];
                }
                else if (section.sectorIndex >= 0)
                {
                    m_context.model->SelectSector (m_context.model->GetQuarterTrack(), section.sectorIndex);
                    NotifySelection();
                }
                else
                {
                    m_context.model->SelectNibbles (m_context.model->GetQuarterTrack(), section.firstNibble, section.count);
                    NotifySelection();
                }
            }
            else if (nibble >= 0 && ev.shift && m_context.model->GetNibbleCount() > 0)
            {
                m_selectedHeader = -1;
                m_context.model->ExtendNibbles (nibble);
            }
            else if (nibble >= 0)
            {
                m_selectedHeader = -1;
                m_context.model->SelectNibbles (m_context.model->GetQuarterTrack(), nibble, 1);
                NotifySelection();
            }

            isHandled = true;
        }
        else if (ev.kind == DxuiMouseEventKind::Move)
        {
            nibble = m_isDragging ? HitTest (ev.positionDip) : -1;

            if (nibble >= 0)
            {
                m_context.model->ExtendNibbles (nibble);
            }

            isHandled = true;
        }
    }

    if (ev.kind == DxuiMouseEventKind::Up && m_isDragging)
    {
        m_isDragging = false;
        isHandled    = true;
    }

    return isHandled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetTooltip
//
//  Over a header, the section's facts and, for a field, every column of the
//  field's details (FR-042); over a nibble, the nibble's.
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track   = m_context.GetTrack();
    const TrackAnalysis *  trackB  = m_context.trackB;
    int                    line    = (track != nullptr) ? HitLine (pointPx) : -1;
    int                    nibble  = (track != nullptr) ? HitTest (pointPx) : -1;
    int                    nibbleB = -1;



    if (line >= 0 && m_lines[static_cast<size_t> (line)].isHeader)
    {
        const NibbleSection &  section = m_sections[static_cast<size_t> (m_lines[static_cast<size_t> (line)].section)];

        outText     = section.title + L"\n" + (section.details.empty() ? section.facts : section.details);
        outAnchorPx = { m_boundsDip.left, static_cast<LONG> (GetLineTop (line)), m_boundsDip.right, static_cast<LONG> (GetLineTop (line) + GetLineHeight (line)) };
    }
    else if (nibble >= 0)
    {
        outText = InspectorTip::BuildNibbleTip (m_context, *track, nibble, track->framed.nibbles[nibble].startCell, GetOwnCells (*track, nibble), false).GetText();

        //  For a nibble that differs, the tooltip has B's value (FR-121).
        if (m_context.nibbleDiffs != nullptr && trackB != nullptr && ComparisonText::FindInHunk (*m_context.nibbleDiffs, nibble, nibbleB))
        {
            outText += (nibbleB >= 0 && !trackB->framed.nibbles.empty())
                     ? std::format (L"\nB: {:02X}", trackB->framed.nibbles[static_cast<size_t> (nibbleB) % trackB->framed.nibbles.size()].value)
                     : std::wstring (L"\nB has no nibble here");
        }

        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return !outText.empty() && (nibble >= 0 || line >= 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetTooltipSwatches
//
//  The colors of the nibble's cells, the cells its bar is drawn for.
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    nibble = (track != nullptr) ? HitTest (pointPx) : -1;



    if (nibble >= 0)
    {
        outSwatches = InspectorTip::BuildNibbleTip (m_context, *track, nibble, track->framed.nibbles[nibble].startCell, GetOwnCells (*track, nibble), false).GetSwatches();
    }

    return nibble >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetOwnCells
//
//  A nibble's eight bits and any extra zero cells after them.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t NibblesView::GetOwnCells (const TrackAnalysis & track, int nibble)
{
    static constexpr uint32_t  kBitsCells = 8;



    return kBitsCells + track.framed.nibbles[nibble].extraZeroCells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::ScrollTo
//
//  To the line with the nibble, or to its section's header when the section
//  is folded.
//
////////////////////////////////////////////////////////////////////////////////

void NibblesView::ScrollTo (int nibble)
{
    int  target = -1;



    Rebuild();

    for (int line = 0; nibble >= 0 && line < static_cast<int> (m_lines.size()); line++)
    {
        const Line &  l = m_lines[static_cast<size_t> (line)];

        if (nibble >= l.firstNibble && nibble < l.firstNibble + l.count && (target < 0 || !l.isHeader))
        {
            target = line;
        }
    }

    if (target >= 0)
    {
        m_firstLine = std::clamp (target - GetVisibleLines() / 3, 0, GetLastFirstLine());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetSelectedHeaderText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring NibblesView::GetSelectedHeaderText() const
{
    return (m_selectedHeader >= 0 && m_selectedHeader < static_cast<int> (m_sections.size())) ? NibbleSections::FormatHeader (m_sections[static_cast<size_t> (m_selectedHeader)])
                                                                                              : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetNibblesPerRow
//
//  As many whole steps of 8 as fit after the offset column, and at least one.
//
////////////////////////////////////////////////////////////////////////////////

int NibblesView::GetNibblesPerRow (float widthPx, float cellPx, float offsetPx)
{
    int  fit = static_cast<int> ((widthPx - offsetPx) / std::max (cellPx, 1.0f));



    return std::max (kNibbleStep, fit / kNibbleStep * kNibbleStep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetPerRow
//
////////////////////////////////////////////////////////////////////////////////

int NibblesView::GetPerRow() const
{
    return GetNibblesPerRow (GetWidth(), m_scaler.ToPxf (static_cast<float> (kCellDip)), m_scaler.ToPxf (static_cast<float> (kOffsetDip)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetLineHeight
//
////////////////////////////////////////////////////////////////////////////////

float NibblesView::GetLineHeight (int line) const
{
    return m_scaler.ToPxf (static_cast<float> (m_lines[static_cast<size_t> (line)].isHeader ? kHeaderDip : kRowDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetLineTop
//
////////////////////////////////////////////////////////////////////////////////

float NibblesView::GetLineTop (int line) const
{
    float  y = static_cast<float> (m_boundsDip.top);



    for (int l = m_firstLine; l < line; l++)
    {
        y += GetLineHeight (l);
    }

    return y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetVisibleLines
//
//  How many lines fit, at the height of a nibble row.
//
////////////////////////////////////////////////////////////////////////////////

int NibblesView::GetVisibleLines() const
{
    return std::max (1, static_cast<int> (GetHeight() / m_scaler.ToPxf (static_cast<float> (kRowDip))));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::GetLastFirstLine
//
//  The first line scrolled to when the last line is at the bottom.
//
////////////////////////////////////////////////////////////////////////////////

int NibblesView::GetLastFirstLine() const
{
    float  room = GetHeight();
    int    line = static_cast<int> (m_lines.size());



    while (line > 0 && room >= GetLineHeight (line - 1))
    {
        room -= GetLineHeight (line - 1);
        line--;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::HitLine
//
////////////////////////////////////////////////////////////////////////////////

int NibblesView::HitLine (POINT pointPx) const
{
    float  y   = static_cast<float> (m_boundsDip.top);
    int    hit = -1;



    for (int line = m_firstLine; hit < 0 && PtInRect (&m_boundsDip, pointPx) && line < static_cast<int> (m_lines.size()) && y <= m_boundsDip.bottom; line++)
    {
        hit = (pointPx.y >= y && pointPx.y < y + GetLineHeight (line)) ? line : -1;
        y  += GetLineHeight (line);
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::HitTest
//
//  The nibble under the point, on a row of nibbles.
//
////////////////////////////////////////////////////////////////////////////////

int NibblesView::HitTest (POINT pointPx) const
{
    float  offW   = m_scaler.ToPxf (static_cast<float> (kOffsetDip));
    float  cellW  = m_scaler.ToPxf (static_cast<float> (kCellDip));
    int    line   = HitLine (pointPx);
    int    col    = static_cast<int> ((pointPx.x - m_boundsDip.left - offW) / cellW);
    int    nibble = -1;



    if (line >= 0 && !m_lines[static_cast<size_t> (line)].isHeader && pointPx.x >= m_boundsDip.left + offW && col < m_lines[static_cast<size_t> (line)].count)
    {
        nibble = m_lines[static_cast<size_t> (line)].firstNibble + col;
    }

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::IsOnChevron
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::IsOnChevron (POINT pointPx) const
{
    return pointPx.x < m_boundsDip.left + m_scaler.ToPxf (s_kStripeDip + s_kChevronDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::IsInSelection
//
//  Whether a nibble is in the selected sector's fields.
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::IsInSelection (const TrackAnalysis & track, int nibble) const
{
    const AnalyzedSector *  sector = m_context.model->GetSector();
    int                     field  = (nibble < static_cast<int> (track.fieldOfNibble.size())) ? track.fieldOfNibble[nibble] : -1;



    return sector != nullptr && field >= 0 && (field == sector->addressField || field == sector->dataField);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::IsOutsideTable
//
//  A data field nibble that is not in its encoding's table, so it cannot be
//  decoded.
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::IsOutsideTable (const TrackAnalysis & track, int nibble)
{
    int   field     = (nibble < static_cast<int> (track.fieldOfNibble.size())) ? track.fieldOfNibble[nibble] : -1;
    bool  isOutside = false;



    if (field >= 0 && track.nibbleKinds[nibble] == NibbleKind::DataField)
    {
        isOutside = DiskFieldFormat::TranslateNibble (track.fields[field].kind, track.framed.nibbles[nibble].value) == DiskFieldFormat::kNotInTable;
    }

    return isOutside;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesView::IsInRange
//
//  Whether a nibble is in the selected run of nibbles (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesView::IsInRange (int nibble) const
{
    int  first = m_context.model->GetFirstNibble();



    return first >= 0 && nibble >= first && nibble < first + m_context.model->GetNibbleCount();
}
