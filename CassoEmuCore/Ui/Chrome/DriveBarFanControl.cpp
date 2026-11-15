#include "Pch.h"

#include "DriveBarFanControl.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::Scale
//
////////////////////////////////////////////////////////////////////////////////

int DriveBarFanControl::Scale (int value, UINT dpi)
{
    return MulDiv (value, (int) dpi, (int) kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::CutName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveBarFanControl::CutName (const std::wstring & name, size_t maxChars)
{
    std::wstring  cut = name;



    if (maxChars > 1 && name.size() > maxChars)
    {
        cut  = name.substr (0, maxChars - 1);
        cut += s_kchEllipsis;
    }

    return cut;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::Layout
//
//  The shell places the slots through the fan, so only the bounds are
//  stored here.
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFanControl::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetDpi    (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::Paint
//
//  Each class's box, then its fan drives, cropped above the widget while
//  they slide out from behind it, then the widget, so the widget is in front
//  of them.
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFanControl::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & dxuiTheme)
{
    HRESULT             hr     = S_OK;
    const CassoTheme  & theme  = static_cast<const CassoTheme &> (dxuiTheme);
    float               radius = (float) Scale (kPanelRadiusPx, m_dpi);



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    for (int c = 0; c < m_fan.GetClassCount() && c < (int) m_views.size(); c++)
    {
        RECT   panel   = m_fan.GetPanelRect (c);
        RECT   head    = m_fan.GetSlotRect (c, 0);
        float  w       = (float) (panel.right  - panel.left);
        float  h       = (float) (panel.bottom - panel.top);
        bool   sliding = m_fan.IsClassOpen (c) && m_fan.GetFanProgress() < 1.0f;



        painter.FillRoundedRect    ((float) panel.left, (float) panel.top, w, h, radius, theme.dropdownBg);
        painter.OutlineRoundedRect ((float) panel.left, (float) panel.top, w, h, radius, 1.0f, theme.dropdownBorder);

        if (sliding)
        {
            painter.PushClipRect ((float) panel.left, (float) panel.top, w, (float) (head.top - panel.top));
            hr = text.PushClipRect ((float) panel.left, (float) panel.top, w, (float) (head.top - panel.top));
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        for (int i = (int) m_views[(size_t) c].size() - 1; i >= 1; i--)
        {
            if (i < m_fan.GetEntryCount (c) && m_fan.IsSlotShown (c, i) && !m_views[(size_t) c][(size_t) i].diskII)
            {
                PaintEntry (painter, text, theme, m_fan.GetSlotRect (c, i), m_views[(size_t) c][(size_t) i],
                            c == m_hoverClass && i == m_hoverEntry);
            }
        }

        if (sliding)
        {
            painter.PopClipRect();
            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (!m_views[(size_t) c].empty() && !m_views[(size_t) c][0].diskII)
        {
            PaintEntry (painter, text, theme, head, m_views[(size_t) c][0], c == m_hoverClass && m_hoverEntry == 0);
        }

        if (m_fan.GetEntryCount (c) > 1 && !m_fan.IsClassOpen (c))
        {
            PaintArrow (painter, theme, head, panel);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::PaintEntry
//
//  The drive's name level with the image's name, its form factor and
//  capacity bottom aligned with the rail, the image's name centered over the
//  band, and the rail lit in the middle while the drive is busy.
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFanControl::PaintEntry (
    IDxuiPainter             & painter,
    IDxuiTextRenderer        & text,
    const CassoTheme         & theme,
    const RECT               & slot,
    const DriveBarEntryView  & view,
    bool                       hovered)
{
    HRESULT  hr       = S_OK;
    int      bandX    = slot.left + Scale (m_captionWidthDip + kCaptionGapXPx, m_dpi);
    int      bandW    = Scale (kBodyWidthPx,  m_dpi);
    int      nameH    = Scale (kNameHeightPx, m_dpi);
    int      railTop  = slot.top + nameH + Scale (kBarGapPx, m_dpi);
    int      railH    = Scale (kBarHeightPx, m_dpi);
    int      capH     = Scale (kCaptionHeightPx, m_dpi);
    float    scale    = (float) m_dpi / (float) kBaseDpi;
    float    third    = (float) bandW / 3.0f;



    if (hovered)
    {
        painter.FillRect ((float) bandX, (float) slot.top, (float) bandW, (float) (railTop + railH - slot.top), theme.buttonHover);
    }

    painter.FillRect ((float) bandX, (float) railTop, (float) bandW, (float) railH, theme.ledIdle);
    painter.FillRect ((float) bandX, (float) railTop, (float) bandW, (float) railH, DxuiColor::ScaleAlpha (theme.driveLabel, kIdleRailAlpha));

    if (view.busy)
    {
        painter.FillRect ((float) bandX + third, (float) railTop, third, (float) railH, theme.ledActive);
    }

    hr = text.DrawString (view.title.c_str(),
                          (float) slot.left,
                          (float) slot.top,
                          (float) Scale (m_captionWidthDip, m_dpi),
                          (float) nameH,
                          theme.driveLabel,
                          kCaptionFontDip * scale,
                          DxuiTheme::kBodyFace,
                          DxuiTextRenderer::HAlign::Left,
                          DxuiTextRenderer::VAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (view.detail.c_str(),
                          (float) slot.left,
                          (float) (railTop + railH - capH + Scale (2, m_dpi)),
                          (float) Scale (m_captionWidthDip, m_dpi),
                          (float) capH,
                          theme.dropdownAccel,
                          kCaptionFontDip * scale,
                          DxuiTheme::kBodyFace,
                          DxuiTextRenderer::HAlign::Left,
                          DxuiTextRenderer::VAlign::Bottom);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (view.name.empty() ? L"(empty)" : CutName (view.name, kMaxNameChars).c_str(),
                          (float) bandX,
                          (float) slot.top,
                          (float) bandW,
                          (float) nameH,
                          view.name.empty() ? theme.dropdownAccel : theme.driveLabel,
                          kNameFontDip * scale,
                          DxuiTheme::kBodyFace,
                          DxuiTextRenderer::HAlign::Center,
                          DxuiTextRenderer::VAlign::Center);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::PaintArrow
//
//  An up arrow in the room at the right of a closed class's box, level with
//  the widget's name row, marking the drives that fan out above it.
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFanControl::PaintArrow (IDxuiPainter & painter, const CassoTheme & theme, const RECT & slot, const RECT & panel)
{
    float  half  = (float) Scale (kArrowHalfPx, m_dpi);
    float  cx    = (float) (slot.right + panel.right) * 0.5f;
    float  cy    = (float) slot.top + (float) Scale (kNameHeightPx, m_dpi) * 0.5f;
    float  thick = std::max (1.0f, (float) m_dpi / (float) kBaseDpi * 1.5f);



    painter.DrawLine (cx - half, cy + half * 0.5f, cx, cy - half * 0.5f, thick, theme.dropdownAccel);
    painter.DrawLine (cx, cy - half * 0.5f, cx + half, cy + half * 0.5f, thick, theme.dropdownAccel);
}
