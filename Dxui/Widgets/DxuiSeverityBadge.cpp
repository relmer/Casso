#include "Pch.h"

#include "Widgets/DxuiSeverityBadge.h"
#include "Theme/DxuiColor.h"
#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiWarningBadge.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverityBadge::Draw
//
//  The ring is walked as chords, as the info banner's badge is, since the
//  painter has no arc.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSeverityBadge::Draw (IDxuiPainter & painter, DxuiSeverity severity, float left, float top, float size, const IDxuiTheme & theme, uint32_t errorArgb)
{
    static constexpr int  kSegments = 20;



    float  r      = size * 0.5f;
    float  cx     = left + r;
    float  cy     = top + r;
    float  stroke = (std::max) (1.0f, size * s_kStrokeEm);



    switch (severity)
    {
        case DxuiSeverity::Error:
        {
            float  stem = stroke * 1.4f;

            painter.FillCircle (cx, cy, r, GetColor (DxuiSeverity::Error, theme, errorArgb));
            painter.FillRect   (cx - stem * 0.5f, cy - r * 0.55f, stem, r * 0.7f, GetInk (DxuiSeverity::Error, theme, errorArgb));
            painter.FillCircle (cx, cy + r * 0.42f, stem * 0.6f, GetInk (DxuiSeverity::Error, theme, errorArgb));
            break;
        }

        case DxuiSeverity::Warning:
            DxuiWarningBadge::Draw (painter, left, top, size, size, theme.WarningAccent(), theme.WarningEdge(), theme.WarningMark());
            break;

        case DxuiSeverity::Info:
        {
            float  ring = r - stroke * 0.5f;

            for (int i = 0; i < kSegments; i++)
            {
                float  a0 = 6.2831853f * static_cast<float> (i)     / kSegments;
                float  a1 = 6.2831853f * static_cast<float> (i + 1) / kSegments;

                painter.DrawLine (cx + ring * std::cos (a0), cy + ring * std::sin (a0), cx + ring * std::cos (a1), cy + ring * std::sin (a1), stroke, theme.Accent());
            }

            painter.FillCircle (cx, cy - r * 0.42f, stroke * 0.55f, theme.Accent());
            painter.FillRect   (cx - stroke * 0.5f, cy - r * 0.12f, stroke, r * 0.58f, theme.Accent());
            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverityBadge::GetCountWidth
//
//  A disc for one digit, and a pill as wide as the count and its padding for
//  more.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiSeverityBadge::GetCountWidth (IDxuiTextRenderer & text, const std::wstring & count, float height)
{
    float  w = 0.0f;
    float  h = 0.0f;



    text.MeasureString (count.c_str(), height * s_kCountEm, DxuiTheme::kBodyFace, w, h);

    return (std::max) (height, w + height * 0.6f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverityBadge::DrawCount
//
//  The count in GetInk's ink, so it reads on each color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSeverityBadge::DrawCount (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    DxuiSeverity         severity,
    const std::wstring & count,
    float                left,
    float                cy,
    float                height,
    const IDxuiTheme   & theme,
    uint32_t             errorArgb)
{
    float     width = GetCountWidth (text, count, height);
    uint32_t  ink   = GetInk (severity, theme, errorArgb);
    HRESULT   hr    = S_OK;



    painter.FillRoundedRect (left, cy - height * 0.5f, width, height, height * 0.5f, GetColor (severity, theme, errorArgb));

    hr = text.DrawString (count.c_str(), left, cy - height * 0.5f, width, height, ink, height * s_kCountEm, DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::SemiBold, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverityBadge::GetColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiSeverityBadge::GetColor (DxuiSeverity severity, const IDxuiTheme & theme, uint32_t errorArgb)
{
    uint32_t  color = theme.Accent();



    switch (severity)
    {
        case DxuiSeverity::Error:   color = (errorArgb != 0) ? errorArgb : theme.ErrorForeground(); break;
        case DxuiSeverity::Warning: color = theme.WarningAccent();   break;
        case DxuiSeverity::Info:                                     break;
    }

    return color;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSeverityBadge::GetInk
//
//  The error color differs by theme, light on a dark theme and dark on a
//  light one, so no one ink has a contrast of 4.5:1 with all of them.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiSeverityBadge::GetInk (DxuiSeverity severity, const IDxuiTheme & theme, uint32_t errorArgb)
{
    return (severity == DxuiSeverity::Warning) ? theme.WarningMark() : DxuiColor::GetContrastingInk (GetColor (severity, theme, errorArgb));
}
