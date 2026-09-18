#include "Pch.h"

#include "Widgets/DxuiFieldError.h"
#include "Render/DxuiStroke.h"
#include "Theme/DxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldError::GetHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiFieldError::GetHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, const DxuiDpiScaler & scaler, int widthPx) const
{
    HRESULT  hr     = S_OK;
    float    fontPx = scaler.ToPxf (theme.BodyFont().sizeDip);
    float    textW  = 0.0f;
    float    textH  = 0.0f;
    int      markPx = scaler.ToPx (kMarkDip);
    int      indent = scaler.ToPx (kMarkDip + kGapDip);



    if (m_message.empty())
    {
        return 0;
    }

    hr = text.MeasureStringWrapped (m_message.c_str(), fontPx, DxuiTheme::kBodyFace, (float) (std::max) (widthPx - indent, 1), textW, textH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    return (std::max) (markPx, (int) std::ceil (textH));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldError::GetChannelLuminance
//
//  One sRGB channel's contribution to relative luminance, before weighting.
//
////////////////////////////////////////////////////////////////////////////////

double DxuiFieldError::GetChannelLuminance (uint32_t channel)
{
    double  c = (double) (channel & 0xFFu) / 255.0;



    return (c <= 0.03928) ? c / 12.92 : std::pow ((c + 0.055) / 1.055, 2.4);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldError::GetMarkGlyphColor
//
//  The contrast ratio each glyph color would have against the fill, from the
//  fill's relative luminance; the larger wins.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DxuiFieldError::GetMarkGlyphColor (uint32_t fillArgb)
{
    double  luminance = 0.2126 * GetChannelLuminance (fillArgb >> 16)
                      + 0.7152 * GetChannelLuminance (fillArgb >> 8)
                      + 0.0722 * GetChannelLuminance (fillArgb);
    double  onWhite   = 1.05 / (luminance + 0.05);
    double  onBlack   = (luminance + 0.05) / 0.05;



    return (onWhite >= onBlack) ? 0xFFFFFFFFu : 0xFF000000u;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldError::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFieldError::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldError::Paint
//
//  The mark is centered on the message's first line however many lines follow,
//  and the message wraps in the room beside it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFieldError::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT   hr     = S_OK;
    float     left   = (float) m_boundsDip.left;
    float     top    = (float) m_boundsDip.top;
    float     width  = (float) (m_boundsDip.right - m_boundsDip.left);
    float     height = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float     markPx = m_scaler.ToPxf ((float) kMarkDip);
    float     indent = m_scaler.ToPxf ((float) (kMarkDip + kGapDip));
    float     fontPx = m_scaler.ToPxf (theme.BodyFont().sizeDip);
    float     radius = markPx * 0.5f;
    float     thick  = (std::max) (1.0f, m_scaler.ToPxf (1.5f));
    float     lineW  = 0.0f;
    float     lineH  = 0.0f;
    float     cx     = 0.0f;
    float     cy     = 0.0f;
    float     arm    = 0.0f;
    uint32_t  fill   = theme.ErrorForeground();
    uint32_t  glyph  = GetMarkGlyphColor (fill);



    if (m_message.empty())
    {
        return;
    }

    hr = text.MeasureString (L"Ag", fontPx, DxuiTheme::kBodyFace, lineW, lineH);
    IGNORE_RETURN_VALUE (hr, S_OK);

    cx  = left + radius;
    cy  = top + (std::max) (lineH, markPx) * 0.5f;
    arm = radius * 0.4f;

    painter.FillRoundedRect (cx - radius, cy - radius, markPx, markPx, radius, fill);
    DxuiStroke::Segment (painter, cx - arm, cy - arm, cx + arm, cy + arm, thick, glyph);
    DxuiStroke::Segment (painter, cx - arm, cy + arm, cx + arm, cy - arm, thick, glyph);

    hr = text.DrawString (m_message.c_str(),
                          left + indent, top, (std::max) (width - indent, 1.0f), height,
                          fill,
                          fontPx,
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Top,
                          DxuiFontWeight::Normal,
                          true);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldValidator::Revalidate
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFieldValidator::Revalidate()
{
    bool  allValid = true;



    for (Field & field : m_fields)
    {
        std::wstring  message = field.check ? field.check() : std::wstring();

        if (field.error != nullptr)
        {
            field.error->SetMessage (message);
        }

        allValid = allValid && message.empty();
    }

    if (m_confirm != nullptr)
    {
        m_confirm->SetEnabled (allValid);
    }

    return allValid;
}
