#include "Pch.h"

#include "Ui/Settings/PaddleBarsView.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





static constexpr const wchar_t *  s_kpszBarFont     = L"Segoe UI";
static constexpr float            s_kBarFontDip     = 12.0f;
static constexpr float            s_kLabelBandDip   = 18.0f;
static constexpr float            s_kTrackHeightDip = 14.0f;
static constexpr float            s_kBarGapDip      = 14.0f;
static constexpr float            s_kMarkWidthDip   = 3.0f;
static constexpr float            s_kMarkOverhang   = 3.0f;
static constexpr float            s_kCornerDip      = 3.0f;
static constexpr float            s_kEdgeDip        = 1.0f;
static constexpr float            s_kPaddleMax      = 255.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatLabel
//
//  The paddle's name and its value two spaces apart, as the stick's circle
//  labels its axes.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PaddleBarsView::FormatLabel (const PaddleBar & bar)
{
    return std::format (L"{}  {}", bar.name, bar.value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMarkX
//
//  0 at the track's left end and 255 at its right, the way a paddle reads.
//
////////////////////////////////////////////////////////////////////////////////

float PaddleBarsView::GetMarkX (float left, float width, Byte value)
{
    return left + width * ((float) value / s_kPaddleMax);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void PaddleBarsView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The bars stack from the top of the bounds, each a label band and then its
//  track. The fill and the mark are the accent color while a controller is
//  read, and muted otherwise, so a bar at rest is not mistaken for a knob
//  left at that value.
//
////////////////////////////////////////////////////////////////////////////////

void PaddleBarsView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT   hr        = S_OK;
    RECT      bounds    = GetBounds();
    float     band      = m_scaler.ToPxf (s_kLabelBandDip);
    float     trackH    = m_scaler.ToPxf (s_kTrackHeightDip);
    float     gap       = m_scaler.ToPxf (s_kBarGapDip);
    float     markW     = m_scaler.ToPxf (s_kMarkWidthDip);
    float     overhang  = m_scaler.ToPxf (s_kMarkOverhang);
    float     corner    = m_scaler.ToPxf (s_kCornerDip);
    float     edge      = m_scaler.ToPxf (s_kEdgeDip);
    float     fontPx    = m_scaler.ToPxf (s_kBarFontDip);
    float     left      = (float) bounds.left;
    float     width     = (float) (bounds.right - bounds.left);
    float     y         = (float) bounds.top;
    uint32_t  fill      = m_isActive ? theme.Accent() : theme.ForegroundDisabled();



    if (!IsVisible() || width <= markW)
    {
        return;
    }

    for (const PaddleBar & bar : m_bars)
    {
        float  trackTop = y + band;
        float  markX    = GetMarkX (left, width, bar.value);

        hr = text.DrawString (FormatLabel (bar).c_str(), left, y, width, band,
                              theme.ForegroundMuted(), fontPx, s_kpszBarFont,
                              DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        painter.FillRoundedRect    (left, trackTop, width, trackH, corner, theme.BackgroundElevated());
        painter.FillRoundedRect    (left, trackTop, markX - left, trackH, corner, fill);
        painter.OutlineRoundedRect (left, trackTop, width, trackH, corner, edge, theme.Border());
        painter.FillRect           (markX - markW * 0.5f, trackTop - overhang, markW, trackH + overhang * 2.0f, fill);

        y = trackTop + trackH + gap;
    }
}
