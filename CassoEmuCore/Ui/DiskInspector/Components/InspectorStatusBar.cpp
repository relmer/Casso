#include "Pch.h"

#include "Ui/DiskInspector/Components/InspectorStatusBar.h"
#include "Ui/DiskInspector/Components/InspectorMetrics.h"





static constexpr float  s_kChipPadDip    = 8.0f;
static constexpr float  s_kChipGapDip    = 6.0f;
static constexpr float  s_kChipHeightDip = 20.0f;
static constexpr float  s_kChipTextDip   = 12.0f;
static constexpr float     s_kHintTextDip   = 12.0f;
static constexpr uint32_t  s_kRgbMask       = 0x00FFFFFFu;
static constexpr uint32_t  s_kHotTintAlpha  = 0x4C000000u;   // a linked chip's fill under the pointer, its color at 30%





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::Create
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::Create (DxuiPanel & parent, ZoomFn onZoom, bool hasHelp)
{
    m_hasHelp     = hasHelp;
    m_zoomControl = parent.CreateChild<DxuiZoomControl>();
    m_zoomControl->SetOnZoom (std::move (onZoom));
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::Tick
//
//  The zoom's slider goes by where the cursor is, not the last mouse event, so
//  it stays through a tooltip that ends the window's hover.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::Tick (HWND owner)
{
    POINT  cursor = {};



    if (m_scaler.GetDpi() != 0 && owner != nullptr && GetCursorPos (&cursor) && ScreenToClient (owner, &cursor))
    {
        m_zoomControl->UpdateHover (cursor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int InspectorStatusBar::GetHeightPx (const DxuiDpiScaler & scaler) const
{
    return scaler.ToPx (DxuiStatusBar::GetBandDp());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::Layout
//
//  The zoom over the bar's last field, the "?" button in the field before
//  it, and the lead items and the hint in the rest.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::Layout (const RECT & area, const DxuiDpiScaler & scaler)
{
    int   pad   = scaler.ToPx (InspectorMetrics::kMarginDip);
    RECT  help  = {};
    RECT  zoom  = {};



    m_scaler = scaler;
    m_bandPx = area;

    m_bar.SetDividers (false);
    m_bar.Layout      (area, scaler);
    LayOutBar         (0);

    help = m_bar.GetFieldRect (kFieldHelp);
    zoom = m_bar.GetFieldRect (kFieldZoom);

    m_helpPx  = m_hasHelp ? RECT { help.left + pad, area.top + 3, help.right - pad, area.bottom - 3 } : RECT {};
    m_chipsPx = { area.left + pad, area.top, m_hasHelp ? help.left : zoom.left, area.bottom };
    m_zoomControl->Layout ({ zoom.left + pad, area.top + 1, zoom.right - pad, area.bottom - 1 }, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::LayOutBar
//
//  The bar's fields: the room the lead items take, the hint in the stretch
//  field, the "?" button's field, and the zoom's field.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::LayOutBar (int leadPx)
{
    int  pad   = m_scaler.ToPx (InspectorMetrics::kMarginDip);
    int  helpW = m_hasHelp ? (m_bandPx.bottom - m_bandPx.top) - 6 + 2 * pad : 0;



    m_bar.SetFields ({ { L"", 0, false, leadPx },
                       { m_hint, 0, true },
                       { L"", 0, false, helpW },
                       { L"", DxuiZoomControl::kWidthDip + 2 * InspectorMetrics::kMarginDip, false } });
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::SetZoom
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::SetZoom (double zoom)
{
    m_zoomControl->SetZoom (zoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::Paint
//
//  The bar with the hint in the room after the lead items, a divider along
//  the band's top, then the lead items and the "?" button over the bar.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, vector<LeadItem> items, uint32_t badColor)
{
    float  x    = PlaceLead (text, items);
    int    lead = (x > m_chipsPx.left) ? static_cast<int> (std::ceil (x)) - m_bandPx.left : 0;



    LayOutBar   (std::min (lead, static_cast<int> (m_chipsPx.right - m_bandPx.left)));
    m_bar.Paint (painter, text, theme);

    painter.FillRect (static_cast<float> (m_bandPx.left), static_cast<float> (m_bandPx.top), static_cast<float> (m_bandPx.right - m_bandPx.left), 1.0f,
                      theme.Divider());

    PaintLead (painter, text, theme, items, badColor);

    if (m_hasHelp)
    {
        PaintHelp (text, theme);
    }

    m_placed = std::move (items);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::GetLinkedChipAt
//
////////////////////////////////////////////////////////////////////////////////

const InspectorStatusBar::LeadItem * InspectorStatusBar::GetLinkedChipAt (POINT p) const
{
    float             chipH = m_scaler.ToPxf (s_kChipHeightDip);
    float             top   = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;
    const LeadItem  * found = nullptr;



    for (const LeadItem & item : m_placed)
    {
        bool  isOver = item.isLinked && item.isPlaced && p.x >= item.x && p.x < item.x + item.w && p.y >= top && p.y < top + chipH;

        found = (found == nullptr && isOver) ? &item : found;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::PlaceLead
//
//  Where each item goes, and where the hint's room starts. Room for the
//  selection is set aside first; summary chips that would crowd it are left
//  out, and so is a selection wider than the room.
//
////////////////////////////////////////////////////////////////////////////////

float InspectorStatusBar::PlaceLead (IDxuiTextRenderer & text, vector<LeadItem> & inOutItems) const
{
    float  pad        = m_scaler.ToPxf (s_kChipPadDip);
    float  gap        = m_scaler.ToPxf (s_kChipGapDip);
    float  x          = static_cast<float> (m_chipsPx.left);
    float  right      = static_cast<float> (m_chipsPx.right);
    float  selectionW = 0;
    float  h          = 0;
    bool   isFull     = false;



    for (LeadItem & item : inOutItems)
    {
        bool  isChip = item.kind == LeadKind::Chip || item.kind == LeadKind::BadChip;

        text.MeasureString (item.text.c_str(), m_scaler.ToPxf (item.kind == LeadKind::Selection ? s_kHintTextDip : s_kChipTextDip),
                            item.kind == LeadKind::Selection ? DxuiTheme::GetUiFace() : DxuiTheme::kBodyFace, item.w, h);

        item.w     += (isChip || item.kind == LeadKind::Result) ? 2 * pad : 0.0f;
        selectionW  = (item.kind == LeadKind::Selection) ? item.w : selectionW;
    }

    for (LeadItem & item : inOutItems)
    {
        bool  isChip = item.kind == LeadKind::Chip || item.kind == LeadKind::BadChip;

        if (item.kind == LeadKind::Selection)
        {
            x += (x > m_chipsPx.left) ? gap : 0.0f;
        }

        isFull        = isFull || (isChip && x + item.w > right - selectionW);
        item.isPlaced = isChip ? !isFull : (item.kind != LeadKind::Selection || x + item.w <= right);
        item.x        = x;

        if (item.isPlaced)
        {
            x += item.w + gap * (isChip || item.kind == LeadKind::Result ? 1.0f : 2.0f);
        }
    }

    return x;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::PaintLead
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::PaintLead (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, const vector<LeadItem> & items, uint32_t badColor) const
{
    float  chipH  = m_scaler.ToPxf (s_kChipHeightDip);
    float  textPx = m_scaler.ToPxf (s_kChipTextDip);
    float  y      = (m_chipsPx.top + m_chipsPx.bottom - chipH) / 2.0f;



    for (const LeadItem & item : items)
    {
        if (!item.isPlaced)
        {
            continue;
        }

        switch (item.kind)
        {
            case LeadKind::Result:
                painter.FillRoundedRect (item.x, y, item.w, chipH, chipH / 2, theme.SelectionBackground());
                text.DrawString (item.text.c_str(), item.x, y, item.w, chipH, theme.Foreground(), textPx, DxuiTheme::kBodyFace,
                                 DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
                break;

            case LeadKind::Chip:
            case LeadKind::BadChip:
            {
                uint32_t  ink = (item.kind == LeadKind::BadChip) ? badColor : (item.isLinked ? theme.Accent() : theme.Foreground());

                painter.FillRoundedRect (item.x, y, item.w, chipH, chipH / 2, theme.ButtonIdle());

                if (item.isLinked && m_isLinkHot)
                {
                    painter.FillRoundedRect (item.x, y, item.w, chipH, chipH / 2, (ink & s_kRgbMask) | s_kHotTintAlpha);
                }

                painter.OutlineRoundedRect (item.x, y, item.w, chipH, chipH / 2, m_scaler.ToPxf (1.0f),
                                            (item.kind == LeadKind::BadChip || item.isLinked) ? ink : theme.ButtonBorder());
                text.DrawString (item.text.c_str(), item.x, y, item.w, chipH, ink, textPx,
                                 DxuiTheme::kBodyFace, DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                break;
            }

            case LeadKind::Note:
                text.DrawString (item.text.c_str(), item.x, y, item.w + 1.0f, chipH, theme.ForegroundMuted(), textPx, DxuiTheme::kBodyFace,
                                 DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                break;

            case LeadKind::Selection:
                text.DrawString (item.text.c_str(), item.x, static_cast<float> (m_chipsPx.top), item.w + 1.0f, static_cast<float> (m_chipsPx.bottom - m_chipsPx.top),
                                 theme.Foreground(), m_scaler.ToPxf (s_kHintTextDip), DxuiTheme::GetUiFace(), DxuiTextHAlign::Left,
                                 DxuiTextVAlign::CenterOnCapHeight, DxuiFontWeight::Normal, false);
                break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusBar::PaintHelp
//
//  The "?" button in a circle.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusBar::PaintHelp (IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    float  cx     = (m_helpPx.left + m_helpPx.right) / 2.0f;
    float  cy     = (m_helpPx.top + m_helpPx.bottom) / 2.0f;
    float  radius = std::min (m_helpPx.right - m_helpPx.left, m_helpPx.bottom - m_helpPx.top) / 2.0f;



    text.DrawEllipse (cx, cy, radius - 0.5f, radius - 0.5f, 1.0f, theme.ForegroundMuted());
    text.DrawString  (L"?", static_cast<float> (m_helpPx.left), static_cast<float> (m_helpPx.top), static_cast<float> (m_helpPx.right - m_helpPx.left),
                      static_cast<float> (m_helpPx.bottom - m_helpPx.top), theme.Foreground(), radius * 1.3f, DxuiTheme::kBodyFace, DxuiTextHAlign::Center,
                      DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
}
