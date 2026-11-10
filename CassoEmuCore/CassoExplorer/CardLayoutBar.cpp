#include "Pch.h"

#include "CassoExplorer/CardLayoutBar.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::CardLayoutBar
//
////////////////////////////////////////////////////////////////////////////////

CardLayoutBar::CardLayoutBar (const CardLayoutModel & model) :
    m_model (model)
{
    m_focusable = true;
    m_position  = model.GetDefaultPosition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::SetPosition
//
//  Places the puck with no animation, as at the dialog's start.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::SetPosition (int position)
{
    m_position      = std::clamp (position, 1, std::max (m_model.GetPositionCount(), 1));
    m_fat32Progress = m_model.IsFat32 (m_position) ? 1.0f : 0.0f;
    m_animationMs   = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::Choose
//
//  A move into or out of FAT32 starts the slot growing or shrinking from
//  where it is.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::Choose (int position)
{
    bool  wasFat32 = m_model.IsFat32 (m_position);



    position = std::clamp (position, 1, std::max (m_model.GetPositionCount(), 1));

    if (position == m_position)
    {
        return;
    }

    m_position = position;

    if (m_model.IsFat32 (m_position) != wasFat32)
    {
        m_fromProgress = m_fat32Progress;
        m_animationMs  = (int64_t) GetTickCount64();
    }

    if (m_change)
    {
        m_change (m_position);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::AdvanceAnimation
//
//  Eased out, so the slot slows near its final size.
//
////////////////////////////////////////////////////////////////////////////////

bool CardLayoutBar::AdvanceAnimation (int64_t nowMs)
{
    float  target = m_model.IsFat32 (m_position) ? 1.0f : 0.0f;
    float  t      = 0.0f;



    if (m_animationMs == 0)
    {
        m_fat32Progress = target;
        return false;
    }

    t = std::clamp ((float) (nowMs - m_animationMs) / (float) kAnimationMs, 0.0f, 1.0f);
    t = 1.0f - (1.0f - t) * (1.0f - t);

    m_fat32Progress = m_fromProgress + (target - m_fromProgress) * t;

    if (t >= 1.0f)
    {
        m_animationMs = 0;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::MeasureHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int CardLayoutBar::MeasureHeightPx (int widthPx) const
{
    std::vector<CardLayoutModel::Row>  rows;
    size_t                             most = 0;



    UNREFERENCED_PARAMETER (widthPx);

    //  The grid is as tall as its most rows, so moving the puck moves
    //  nothing below the control.
    for (int position = 1; position <= m_model.GetPositionCount(); position++)
    {
        m_model.GetRows (position, rows);
        most = std::max (most, rows.size());
    }

    return (int) std::ceil (m_scaler.ToPxf (kBarHeightDip + kPuckHeightDip + kGapDip + kLineDip * 3.0f + kGapDip + kRowDip * (float) most));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler = scaler;

    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::GetSlotWidthPx
//
//  The volume slots and the space past them share the bar to scale, less
//  the FAT32 slot's share at the right.
//
////////////////////////////////////////////////////////////////////////////////

float CardLayoutBar::GetSlotWidthPx() const
{
    RECT      bounds = GetBounds();
    float     width  = (float) (bounds.right - bounds.left) * (1.0f - (m_model.CanFat32() ? kFat32Share : 0.0f));
    uint64_t  card   = (uint64_t) m_model.GetVolumeLimit() * CardLayoutModel::kSliceBytes + m_model.GetUnavailableBytes();



    return card == 0 ? 0.0f : width * (float) ((double) CardLayoutModel::kSliceBytes / (double) card);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::GetPuckCenterPx
//
////////////////////////////////////////////////////////////////////////////////

float CardLayoutBar::GetPuckCenterPx() const
{
    RECT   bounds = GetBounds();
    float  slot   = GetSlotWidthPx();



    if (m_model.IsFat32 (m_position))
    {
        return (float) bounds.right - m_scaler.ToPxf (kPuckWidthDip) * 0.5f;
    }

    return (float) bounds.left + slot * ((float) m_position - 0.5f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::GetPositionAt
//
////////////////////////////////////////////////////////////////////////////////

int CardLayoutBar::GetPositionAt (int xPx) const
{
    RECT   bounds   = GetBounds();
    float  slot     = GetSlotWidthPx();
    float  offset   = (float) (xPx - bounds.left);
    float  volumes  = slot * (float) m_model.GetVolumeLimit();
    bool   sticky   = m_dragging && m_model.IsFat32 (m_position) && offset > volumes * 0.5f;



    if (m_model.CanFat32() && (sticky || offset >= volumes))
    {
        return m_model.GetVolumeLimit() + 1;
    }

    return slot <= 0.0f ? 1 : std::clamp ((int) std::ceil (offset / slot), 1, std::max (m_model.GetVolumeLimit(), 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CardLayoutBar::OnMouse (const DxuiMouseEvent & ev)
{
    RECT  bounds = GetBounds();
    int   bottom = bounds.top + (int) m_scaler.ToPxf (kBarHeightDip + kPuckHeightDip);
    bool  inBar  = ev.positionDip.x >= bounds.left && ev.positionDip.x < bounds.right && ev.positionDip.y >= bounds.top && ev.positionDip.y < bottom;



    switch (ev.kind)
    {
        case DxuiMouseEventKind::Down:
            if (!inBar || ev.button != DxuiMouseButton::Left)
            {
                return false;
            }

            m_dragging = true;
            Choose (GetPositionAt (ev.positionDip.x));
            return true;

        case DxuiMouseEventKind::Move:
            if (!m_dragging)
            {
                return false;
            }

            Choose (GetPositionAt (ev.positionDip.x));
            return true;

        case DxuiMouseEventKind::Up:
            if (!m_dragging)
            {
                return false;
            }

            m_dragging = false;
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool CardLayoutBar::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    switch (ev.vk)
    {
        case VK_LEFT:   Choose (m_position - 1);                  return true;
        case VK_RIGHT:  Choose (m_position + 1);                  return true;
        case VK_HOME:   Choose (1);                               return true;
        case VK_END:    Choose (m_model.GetPositionCount());      return true;
        default:        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::GetAccessibleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CardLayoutBar::GetAccessibleName() const
{
    return m_model.DescribePosition (m_position);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::Paint
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT   bounds = GetBounds();
    float  top    = (float) bounds.top + m_scaler.ToPxf (kBarHeightDip + kPuckHeightDip + kGapDip);
    float  width  = (float) (bounds.right - bounds.left);
    float  font   = m_scaler.ToPxf (kFontDip);
    float  line   = m_scaler.ToPxf (kLineDip);



    PaintBar  (painter, text);
    PaintPuck (painter, theme);

    text.DrawString (m_model.DescribePosition (m_position).c_str(), (float) bounds.left, top, width, line, theme.Foreground(), font, DxuiTheme::GetUiFace(),
                     DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::SemiBold, false);
    text.DrawString (m_model.DescribeSpace (m_position).c_str(), (float) bounds.left, top + line, width, line * 2.0f, theme.ForegroundMuted(), font, DxuiTheme::GetUiFace(),
                     DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);

    PaintGrid (painter, text, theme, top + line * 3.0f + m_scaler.ToPxf (kGapDip));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::PaintBar
//
//  The volume slots and the space past them shrink as the FAT32 slot grows,
//  each by the same share, so the bar reads as one partition taking the
//  card over.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::PaintBar (IDxuiPainter & painter, IDxuiTextRenderer & text)
{
    RECT      bounds    = GetBounds();
    float     left      = (float) bounds.left;
    float     top       = (float) bounds.top;
    float     height    = m_scaler.ToPxf (kBarHeightDip);
    float     width     = (float) (bounds.right - bounds.left);
    float     fatShare  = m_model.CanFat32() ? kFat32Share + (1.0f - kFat32Share) * m_fat32Progress : 0.0f;
    float     rest      = 1.0f - fatShare;
    float     baseRest  = 1.0f - (m_model.CanFat32() ? kFat32Share : 0.0f);
    float     scale     = baseRest <= 0.0f ? 0.0f : rest / baseRest;
    float     slot      = GetSlotWidthPx() * scale;
    float     x         = left;
    float     gap       = 1.0f;
    float     smallFont = m_scaler.ToPxf (kSmallFontDip);
    float     fatX      = 0.0f;
    float     restWidth = 0.0f;



    for (int slotIndex = 1; slotIndex <= m_model.GetVolumeLimit(); slotIndex++)
    {
        CardLayoutModel::Segment  kind = m_model.GetSlotKind (m_model.IsFat32 (m_position) ? 0 : m_position, slotIndex);
        float                     w    = std::max (slot - gap, 0.0f);

        if (kind == CardLayoutModel::Segment::NeedsSetup)
        {
            PaintHatch (painter, x, top, w, height);
        }
        else
        {
            painter.FillRect (x, top, w, height, kind == CardLayoutModel::Segment::Universal ? s_kUniversalArgb : s_kUnusedArgb);
        }

        x += slot;
    }

    restWidth = std::max (left + width * rest - x, 0.0f);

    if (restWidth > gap)
    {
        painter.FillRect (x, top, restWidth - gap, height, s_kUnavailableArgb);

        if (m_fat32Progress < 0.5f)
        {
            text.DrawString (std::format (L"{} beyond the CFFA layout", TextEncoding::NarrowToWide (PartitionAccess::FormatSize (m_model.GetUnavailableBytes()))).c_str(),
                             x, top, restWidth - gap, height, 0xFFD3D1C7, smallFont, DxuiTheme::GetUiFace(), DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, true);
        }
    }

    if (!m_model.CanFat32())
    {
        return;
    }

    fatX = left + width * rest;

    painter.FillRect (fatX, top, left + width - fatX, height, m_model.IsFat32 (m_position) ? s_kFat32OnArgb : s_kFat32Argb);
    text.DrawString (m_fat32Progress > 0.5f ? L"One FAT32 partition, the whole card" : L"FAT32", fatX, top, left + width - fatX, height,
                     m_model.IsFat32 (m_position) ? s_kLightTextArgb : s_kDarkTextArgb, smallFont, DxuiTheme::GetUiFace(),
                     DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::PaintHatch
//
//  Amber with lighter diagonal stripes, clipped to the slot.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::PaintHatch (IDxuiPainter & painter, float x, float y, float width, float height)
{
    float  pitch = m_scaler.ToPxf (8.0f);
    float  pen   = m_scaler.ToPxf (3.0f);



    painter.FillRect (x, y, width, height, s_kSetupArgb);
    painter.PushClipRect (x, y, width, height);

    for (float start = x - height; start < x + width; start += pitch)
    {
        painter.DrawLine (start, y + height, start + height, y, pen, s_kSetupStripeArgb);
    }

    painter.PopClipRect();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::PaintPuck
//
//  A pentagon under the bar, taller than it is wide, its tip at the chosen
//  slot. A focus ring is drawn around it when the bar has the focus.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::PaintPuck (IDxuiPainter & painter, const IDxuiTheme & theme)
{
    RECT      bounds = GetBounds();
    float     cx     = GetPuckCenterPx();
    float     half   = m_scaler.ToPxf (kPuckWidthDip) * 0.5f;
    float     top    = (float) bounds.top + m_scaler.ToPxf (kBarHeightDip) + 2.0f;
    float     tip    = m_scaler.ToPxf (kPuckTipDip);
    float     bottom = top + m_scaler.ToPxf (kPuckHeightDip) - 2.0f;
    uint32_t  color  = theme.Foreground();



    painter.FillConvexQuad (cx, top, cx + half, top + tip, cx - half, top + tip, cx, top, color);
    painter.FillRect (cx - half, top + tip, half * 2.0f, bottom - top - tip, color);

    if (m_focused && m_focusCueVisible)
    {
        painter.OutlineRect (cx - half - 3.0f, top - 3.0f, half * 2.0f + 6.0f, bottom - top + 6.0f, 1.0f, theme.FocusRing());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CardLayoutBar::PaintGrid
//
//  A row for each adapter: its name, an icon for its support, and a note.
//  The icons are drawn rather than read from a font: a check in a green
//  circle, a warning triangle, a cross in a red circle, and a gray circle
//  for unconfirmed.
//
////////////////////////////////////////////////////////////////////////////////

void CardLayoutBar::PaintGrid (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, float top)
{
    RECT                               bounds  = GetBounds();
    float                              left    = (float) bounds.left;
    float                              width   = (float) (bounds.right - bounds.left);
    float                              row     = m_scaler.ToPxf (kRowDip);
    float                              icon    = m_scaler.ToPxf (kIconDip);
    float                              font    = m_scaler.ToPxf (kFontDip);
    float                              nameW   = width * kAdapterShare;
    float                              noteX   = left + nameW + icon + m_scaler.ToPxf (kGapDip);
    float                              y       = top;
    float                              r       = icon * 0.33f;
    std::vector<CardLayoutModel::Row>  rows;



    m_model.GetRows (m_position, rows);

    for (const CardLayoutModel::Row & each : rows)
    {
        float  cx = left + nameW + icon * 0.5f;
        float  cy = y + m_scaler.ToPxf (kLineDip) * 0.5f;

        painter.FillRect (left, y, width, 1.0f, theme.Divider());

        text.DrawString (each.adapter.c_str(), left, y + 2.0f, nameW, row, theme.Foreground(), font, DxuiTheme::GetUiFace(),
                         DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);
        text.DrawString (each.note.c_str(), noteX, y + 2.0f, left + width - noteX, row, theme.ForegroundMuted(), font, DxuiTheme::GetUiFace(),
                         DxuiTextHAlign::Left, DxuiTextVAlign::Top, DxuiFontWeight::Normal, true);

        switch (each.support)
        {
            case CardLayoutModel::Support::Works:
                painter.FillCircle (cx, cy, r, s_kWorksArgb);
                painter.DrawLine (cx - r * 0.5f, cy, cx - r * 0.1f, cy + r * 0.4f, 2.0f, 0xFFFFFFFF);
                painter.DrawLine (cx - r * 0.1f, cy + r * 0.4f, cx + r * 0.55f, cy - r * 0.4f, 2.0f, 0xFFFFFFFF);
                break;

            case CardLayoutModel::Support::Step:
                painter.FillConvexQuad (cx, cy - r, cx + r * 1.1f, cy + r * 0.85f, cx - r * 1.1f, cy + r * 0.85f, cx, cy - r, s_kStepArgb);
                painter.DrawLine (cx, cy - r * 0.35f, cx, cy + r * 0.3f, 2.0f, 0xFFFFFFFF);
                painter.FillCircle (cx, cy + r * 0.6f, 1.2f, 0xFFFFFFFF);
                break;

            case CardLayoutModel::Support::Fails:
                painter.FillCircle (cx, cy, r, theme.ErrorForeground());
                painter.DrawLine (cx - r * 0.45f, cy - r * 0.45f, cx + r * 0.45f, cy + r * 0.45f, 2.0f, 0xFFFFFFFF);
                painter.DrawLine (cx - r * 0.45f, cy + r * 0.45f, cx + r * 0.45f, cy - r * 0.45f, 2.0f, 0xFFFFFFFF);
                break;

            default:
                painter.FillCircle (cx, cy, r, theme.ForegroundDisabled());
                text.DrawString (L"?", cx - r, cy - r, r * 2.0f, r * 2.0f, 0xFFFFFFFF, font, DxuiTheme::GetUiFace(),
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
                break;
        }

        y += row;
    }
}
