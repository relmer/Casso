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
////////////////////////////////////////////////////////////////////////////////

void DriveBarFanControl::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & dxuiTheme)
{
    const CassoTheme  & theme = static_cast<const CassoTheme &> (dxuiTheme);
    RECT                panel = m_fan.GetPanelRect();



    _ASSERTE (dynamic_cast<const CassoTheme *> (&dxuiTheme) != nullptr);

    if (panel.right > panel.left)
    {
        float  radius = (float) Scale (kPanelRadiusPx, m_dpi);
        float  w      = (float) (panel.right  - panel.left);
        float  h      = (float) (panel.bottom - panel.top);

        painter.FillRoundedRect    ((float) panel.left, (float) panel.top, w, h, radius, theme.dropdownBg);
        painter.OutlineRoundedRect ((float) panel.left, (float) panel.top, w, h, radius, 1.0f, theme.dropdownBorder);
    }

    for (int c = 0; c < m_fan.GetClassCount() && c < (int) m_views.size(); c++)
    {
        for (int i = 0; i < m_fan.GetEntryCount (c) && i < (int) m_views[(size_t) c].size(); i++)
        {
            RECT  slot = m_fan.GetSlotRect (c, i);

            if (!m_fan.IsSlotShown (c, i))
            {
                continue;
            }

            if (!m_views[(size_t) c][(size_t) i].diskII)
            {
                PaintEntry (painter, text, theme, slot, m_views[(size_t) c][(size_t) i], c == m_hoverClass && i == m_hoverEntry);
            }

            if (i == 0 && m_fan.GetEntryCount (c) > 1)
            {
                PaintArrow (painter, theme, slot);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveBarFanControl::PaintEntry
//
//  The caption bottom aligned with the rail, the name centered over the
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
    int      bandX    = slot.left + Scale (kCaptionWidthPx + kCaptionGapXPx, m_dpi);
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

    hr = text.DrawString (view.caption.c_str(),
                          (float) slot.left,
                          (float) (railTop + railH - capH + Scale (2, m_dpi)),
                          (float) Scale (kCaptionWidthPx, m_dpi),
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
//  An up arrow at the right end of a class widget's name row, marking the
//  drives that fan out above it.
//
////////////////////////////////////////////////////////////////////////////////

void DriveBarFanControl::PaintArrow (IDxuiPainter & painter, const CassoTheme & theme, const RECT & slot)
{
    float  half  = (float) Scale (kArrowHalfPx, m_dpi);
    float  cx    = (float) slot.right - half * 1.5f;
    float  cy    = (float) slot.top + (float) Scale (kNameHeightPx, m_dpi) * 0.5f;
    float  thick = std::max (1.0f, (float) m_dpi / (float) kBaseDpi * 1.5f);



    painter.DrawLine (cx - half, cy + half * 0.5f, cx, cy - half * 0.5f, thick, theme.dropdownAccel);
    painter.DrawLine (cx, cy - half * 0.5f, cx + half, cy + half * 0.5f, thick, theme.dropdownAccel);
}
