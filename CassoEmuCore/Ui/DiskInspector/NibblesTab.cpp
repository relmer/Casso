#include "Pch.h"

#include "Ui/DiskInspector/NibblesTab.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Ui/DiskInspector/InspectorText.h"
#include "Ui/DiskInspector/PlatterCells.h"





static constexpr uint32_t  s_kRangeAlpha = 0x60000000u;
static constexpr int    s_kWheelRows   = 3;
static constexpr float  s_kSelectDip   = 2.0f;
static constexpr Byte   s_kSyncValue   = 0xFF;
static constexpr int    s_kSyncCells   = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::Paint
//
////////////////////////////////////////////////////////////////////////////////

void NibblesTab::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    const TrackAnalysis *  track   = m_context.GetTrack();
    int                    perRow  = GetPerRow();
    int                    rows    = GetVisibleRows();
    float                  rowH    = m_scaler.ToPxf (static_cast<float> (kRowDip));
    float                  cellW   = m_scaler.ToPxf (static_cast<float> (kCellDip));
    float                  offW    = m_scaler.ToPxf (static_cast<float> (kOffsetDip));
    float                  textPx  = m_scaler.ToPxf (kTextDip);
    float                  smallPx = m_scaler.ToPxf (kSmallDip - 2.0f);
    float                  x       = 0;
    float                  y       = 0;
    int                    row     = 0;
    int                    col     = 0;
    int                    n       = 0;
    int                    count   = 0;
    uint32_t               color   = 0;
    bool                   failed  = false;
    std::wstring           extra;



    if (track == nullptr || track->framed.nibbles.empty())
    {
        if (m_context.hasDisk)
        {
            text.DrawString (L"Nothing recorded.", static_cast<float> (m_boundsDip.left), static_cast<float> (m_boundsDip.top), GetWidth(), rowH,
                             theme.Foreground(), textPx, DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        }
    }
    else
    {
        count = static_cast<int> (track->framed.nibbles.size());

        for (row = 0; row < rows && (m_firstRow + row) * perRow < count; row++)
        {
            y = m_boundsDip.top + row * rowH;
            n = (m_firstRow + row) * perRow;

            text.DrawString (std::format (L"{:04X}", n).c_str(), static_cast<float> (m_boundsDip.left), y, offW, rowH, theme.ForegroundMuted(), textPx,
                             DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

            for (col = 0; col < perRow && n < count; col++, n++)
            {
                const FramedNibble &  nibble = track->framed.nibbles[n];

                x      = m_boundsDip.left + offW + col * cellW;
                failed = n < static_cast<int> (track->isFailedChecksum.size()) && track->isFailedChecksum[n] != 0;
                color  = m_context.palette.GetKindColor (failed ? PlatterKind::FailedChecksum : PlatterCells::GetKindOf (track->nibbleKinds[n]));

                if (IsInSelection (*track, n))
                {
                    painter.FillRect (x, y, cellW, rowH, theme.SelectionBackground());
                }

                if (IsInRange (n))
                {
                    painter.FillRect (x, y, cellW, rowH, (theme.Accent() & 0x00FFFFFFu) | s_kRangeAlpha);
                }

                painter.FillRect (x + 1, y + rowH - m_scaler.ToPxf (3.0f), cellW - 2, m_scaler.ToPxf (2.0f), color);

                text.DrawString (std::format (L"{:02X}", nibble.value).c_str(), x, y, cellW * 0.6f, rowH,
                                 IsOutsideTable (*track, n) ? theme.ErrorForeground() : theme.Foreground(),
                                 textPx, DxuiTheme::kMonoFace, DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);

                extra.clear();

                if (nibble.value == s_kSyncValue && nibble.extraZeroCells > 0)
                {
                    extra = std::to_wstring (s_kSyncCells + nibble.extraZeroCells);
                }
                else if (nibble.extraZeroCells > 0)
                {
                    extra = L"+" + std::to_wstring (nibble.extraZeroCells);
                }

                if (nibble.isNoise)
                {
                    painter.OutlineRect (x + 1, y + 1, cellW - 2, rowH - 2, m_scaler.ToPxf (1.0f), m_context.palette.GetKindColor (PlatterKind::Noise));
                }

                if (!extra.empty())
                {
                    text.DrawString (extra.c_str(), x + cellW * 0.6f, y, cellW * 0.4f, rowH * 0.6f, theme.ForegroundMuted(), smallPx,
                                     DxuiTheme::kBodyFace, DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, false);
                }

            }
        }

        if (m_context.model->GetFirstNibble() >= 0)
        {
            n = m_context.model->GetFirstNibble() - m_firstRow * perRow;

            if (n >= 0 && n < rows * perRow)
            {
                painter.OutlineRect (m_boundsDip.left + offW + (n % perRow) * cellW, m_boundsDip.top + (n / perRow) * rowH, cellW, rowH,
                                     m_scaler.ToPxf (s_kSelectDip), theme.Accent());
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesTab::OnMouse (const DxuiMouseEvent & ev)
{
    const TrackAnalysis *  track     = m_context.GetTrack();
    bool                   isInside  = PtInRect (&m_boundsDip, ev.positionDip) != FALSE;
    bool                   isHandled = false;
    int                    nibble    = -1;
    int                    lastRow   = 0;



    if (track != nullptr && isInside)
    {
        lastRow = std::max (0, (static_cast<int> (track->framed.nibbles.size()) + GetPerRow() - 1) / GetPerRow() - GetVisibleRows());

        if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal)
        {
            m_firstRow = std::clamp (m_firstRow - static_cast<int> (ev.wheelDelta * s_kWheelRows), 0, lastRow);
            isHandled  = true;
        }
        else if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
        {
            nibble       = HitTest (ev.positionDip);
            m_isDragging = nibble >= 0;

            if (nibble >= 0 && ev.shift && m_context.model->GetNibbleCount() > 0)
            {
                m_context.model->ExtendNibbles (nibble);
            }
            else if (nibble >= 0)
            {
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
//  NibblesTab::GetTooltip
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesTab::GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    int                    nibble = (track != nullptr) ? HitTest (pointPx) : -1;



    if (nibble >= 0)
    {
        outText     = InspectorText::FormatNibbleTooltip (*track, nibble);
        outAnchorPx = { pointPx.x, pointPx.y, pointPx.x + 1, pointPx.y + 1 };
    }

    return nibble >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::ScrollTo
//
////////////////////////////////////////////////////////////////////////////////

void NibblesTab::ScrollTo (int nibble)
{
    if (nibble >= 0)
    {
        m_firstRow = std::max (0, nibble / GetPerRow() - GetVisibleRows() / 3);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::GetNibblesPerRow
//
//  As many whole steps of 8 as fit after the offset column, and at least one.
//
////////////////////////////////////////////////////////////////////////////////

int NibblesTab::GetNibblesPerRow (float widthPx, float cellPx, float offsetPx)
{
    int  fit = static_cast<int> ((widthPx - offsetPx) / std::max (cellPx, 1.0f));



    return std::max (kNibbleStep, fit / kNibbleStep * kNibbleStep);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::GetPerRow
//
////////////////////////////////////////////////////////////////////////////////

int NibblesTab::GetPerRow() const
{
    return GetNibblesPerRow (GetWidth(), m_scaler.ToPxf (static_cast<float> (kCellDip)), m_scaler.ToPxf (static_cast<float> (kOffsetDip)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::GetVisibleRows
//
////////////////////////////////////////////////////////////////////////////////

int NibblesTab::GetVisibleRows() const
{
    return std::max (1, static_cast<int> (GetHeight() / m_scaler.ToPxf (static_cast<float> (kRowDip))));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::HitTest
//
////////////////////////////////////////////////////////////////////////////////

int NibblesTab::HitTest (POINT pointPx) const
{
    const TrackAnalysis *  track  = m_context.GetTrack();
    float                  offW   = m_scaler.ToPxf (static_cast<float> (kOffsetDip));
    float                  cellW  = m_scaler.ToPxf (static_cast<float> (kCellDip));
    float                  rowH   = m_scaler.ToPxf (static_cast<float> (kRowDip));
    int                    col    = static_cast<int> ((pointPx.x - m_boundsDip.left - offW) / cellW);
    int                    row    = static_cast<int> ((pointPx.y - m_boundsDip.top) / rowH);
    int                    nibble = -1;



    if (track != nullptr && PtInRect (&m_boundsDip, pointPx) && pointPx.x >= m_boundsDip.left + offW && col < GetPerRow())
    {
        nibble = (m_firstRow + row) * GetPerRow() + col;
        nibble = (nibble < static_cast<int> (track->framed.nibbles.size())) ? nibble : -1;
    }

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::IsInSelection
//
//  Whether a nibble belongs to the selected sector's fields.
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesTab::IsInSelection (const TrackAnalysis & track, int nibble) const
{
    const AnalyzedSector *  sector = m_context.model->GetSector();
    int                     field  = (nibble < static_cast<int> (track.fieldOfNibble.size())) ? track.fieldOfNibble[nibble] : -1;



    return sector != nullptr && field >= 0 && (field == sector->addressField || field == sector->dataField);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibblesTab::IsOutsideTable
//
//  A data field nibble that is not in its encoding's table, so it cannot be
//  decoded.
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesTab::IsOutsideTable (const TrackAnalysis & track, int nibble)
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
//  NibblesTab::IsInRange
//
//  Whether a nibble is in the selected run of nibbles (FR-043).
//
////////////////////////////////////////////////////////////////////////////////

bool NibblesTab::IsInRange (int nibble) const
{
    int  first = m_context.model->GetFirstNibble();



    return first >= 0 && nibble >= first && nibble < first + m_context.model->GetNibbleCount();
}
