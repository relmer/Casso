#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerPickerChips.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerChips::Paint
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerChips::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT        hr       = S_OK;
    RECT           bounds   = GetBounds();
    float          fontPx   = m_scaler.ToPxf (kFontDip);
    float          height   = m_scaler.ToPxf (kHeightDip);
    float          pad      = m_scaler.ToPxf (kPadDip);
    float          top      = (float) bounds.top + ((float) (bounds.bottom - bounds.top) - height) / 2.0f;
    float          x        = (float) bounds.left;
    float          labelW   = MeasureWidth (text, kpszLabel, fontPx);
    std::wstring   face     = DxuiTheme::GetUiFace();



    if (m_terms.empty() || !IsVisible())
    {
        return;
    }

    hr = text.DrawString (kpszLabel, x, (float) bounds.top, labelW, (float) (bounds.bottom - bounds.top), theme.ForegroundMuted(), fontPx, face.c_str(),
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal);
    IGNORE_RETURN_VALUE (hr, S_OK);

    x += labelW + m_scaler.ToPxf (kLabelGapDip);

    for (const std::wstring & term : m_terms)
    {
        float  width = MeasureWidth (text, term, fontPx) + pad * 2.0f;



        if (x + width > (float) bounds.right)
        {
            break;
        }

        //  The edge is a capsule in the border's color, and the fill a capsule
        //  a pixel inside it.
        FillCapsule (text, x, top, width, height, theme.Border());
        FillCapsule (text, x + 1.0f, top + 1.0f, width - 2.0f, height - 2.0f, theme.ControlBackground());

        hr = text.DrawString (term.c_str(), x, top, width, height, theme.Foreground(), fontPx, face.c_str(),
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal);
        IGNORE_RETURN_VALUE (hr, S_OK);

        x += width + m_scaler.ToPxf (kGapDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerChips::GetAccessibleName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerPickerChips::GetAccessibleName() const
{
    std::wstring  name = kpszLabel;



    for (size_t i = 0; i < m_terms.size(); i++)
    {
        name += (i == 0) ? L" " : L", ";
        name += m_terms[i];
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerChips::MeasureWidth
//
////////////////////////////////////////////////////////////////////////////////

float ExplorerPickerChips::MeasureWidth (IDxuiTextRenderer & text, const std::wstring & value, float fontPx) const
{
    float    width  = 0.0f;
    float    height = 0.0f;
    HRESULT  hr     = text.MeasureString (value.c_str(), fontPx, DxuiTheme::GetUiFace(), width, height);



    IGNORE_RETURN_VALUE (hr, S_OK);

    return std::ceil (width);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerChips::FillCapsule
//
//  Through the text renderer, which draws over the command bar's background;
//  the painter draws under it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerChips::FillCapsule (IDxuiTextRenderer & text, float x, float y, float width, float height, uint32_t argb)
{
    HRESULT  hr     = S_OK;
    float    radius = height / 2.0f;



    hr = text.FillRect (x + radius, y, width - height, height, argb);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillEllipse (x + radius, y + radius, radius, radius, argb);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.FillEllipse (x + width - radius, y + radius, radius, radius, argb);
    IGNORE_RETURN_VALUE (hr, S_OK);
}
