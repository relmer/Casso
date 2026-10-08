#include "Pch.h"

#include "Ui/Settings/PaddleBarView.h"






static constexpr const wchar_t *  s_kpszBarFont      = L"Segoe UI";
static constexpr float            s_kBarFontDip      = 12.0f;
static constexpr float            s_kReadingWidthDip = 30.0f;
static constexpr float            s_kReadingGapDip   = 8.0f;
static constexpr float            s_kTrackHeightDip  = 14.0f;
static constexpr float            s_kMarkWidthDip    = 3.0f;
static constexpr float            s_kMarkOverhang    = 3.0f;
static constexpr float            s_kCornerDip       = 3.0f;
static constexpr float            s_kEdgeDip         = 1.0f;
static constexpr float            s_kPaddleMax       = 255.0f;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatReading
//
//  The value alone: the row's label gives the paddle.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PaddleBarView::FormatReading (const PaddleBar & bar)
{
    return std::to_wstring (bar.value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMarkX
//
//  0 at the track's left end and 255 at its right, the way a paddle reads.
//
////////////////////////////////////////////////////////////////////////////////

float PaddleBarView::GetMarkX (float left, float width, Byte value)
{
    return left + width * ((float) value / s_kPaddleMax);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void PaddleBarView::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    m_scaler.SetDpi (scaler.GetDpi());
    SetBounds (boundsDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The track centered in the row, stopping a gap short of the reading, which
//  is right-aligned at the view's right edge. The fill and the mark are the
//  accent color while a controller is read, and muted otherwise, so a bar at
//  rest is not mistaken for a knob left at that value.
//
////////////////////////////////////////////////////////////////////////////////

void PaddleBarView::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT   hr        = S_OK;
    RECT      bounds    = GetBounds();
    float     readingW  = m_scaler.ToPxf (s_kReadingWidthDip);
    float     readGap   = m_scaler.ToPxf (s_kReadingGapDip);
    float     trackH    = m_scaler.ToPxf (s_kTrackHeightDip);
    float     markW     = m_scaler.ToPxf (s_kMarkWidthDip);
    float     overhang  = m_scaler.ToPxf (s_kMarkOverhang);
    float     corner    = m_scaler.ToPxf (s_kCornerDip);
    float     edge      = m_scaler.ToPxf (s_kEdgeDip);
    float     fontPx    = m_scaler.ToPxf (s_kBarFontDip);
    float     left      = (float) bounds.left;
    float     right     = (float) bounds.right;
    float     top       = (float) bounds.top;
    float     height    = (float) (bounds.bottom - bounds.top);
    float     readingX  = right - readingW;
    float     width     = readingX - readGap - markW * 0.5f - left;
    float     trackTop  = top + (height - trackH) * 0.5f;
    float     markX     = GetMarkX (left, width, m_bar.value);
    uint32_t  fill      = m_isActive ? theme.Accent() : theme.ForegroundDisabled();



    if (!IsVisible() || width <= markW)
    {
        return;
    }

    painter.FillRoundedRect    (left, trackTop, width, trackH, corner, theme.BackgroundElevated());
    painter.FillRoundedRect    (left, trackTop, markX - left, trackH, corner, fill);
    painter.OutlineRoundedRect (left, trackTop, width, trackH, corner, edge, theme.Border());
    painter.FillRect           (markX - markW * 0.5f, trackTop - overhang, markW, trackH + overhang * 2.0f, fill);

    hr = text.DrawString (FormatReading (m_bar).c_str(), readingX, top, readingW, height,
                          theme.ForegroundMuted(), fontPx, s_kpszBarFont,
                          DxuiTextHAlign::Right, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
