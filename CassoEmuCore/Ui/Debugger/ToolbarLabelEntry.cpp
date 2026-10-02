#include "Pch.h"

#include "Ui/Debugger/ToolbarLabelEntry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry::ToolbarLabelEntry
//
////////////////////////////////////////////////////////////////////////////////

ToolbarLabelEntry::ToolbarLabelEntry (std::wstring label) :
    m_label   (std::move (label)),
    m_command (std::make_shared<DxuiCommand>())
{
    m_command->label = m_label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry::GetWidthPx
//
//  The label with room to either side, the same in both forms.
//
////////////////////////////////////////////////////////////////////////////////

int ToolbarLabelEntry::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    HRESULT  hr     = S_OK;
    float    labelW = (float) (m_label.size() * kLabelGuess);
    float    labelH = 0.0f;



    (void) labeled;

    if (text != nullptr)
    {
        hr = text->MeasureString (m_label.c_str(), scaler.ToPxf (kFontDip), DxuiTheme::kBodyFace, labelW, labelH);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
    else
    {
        labelW = scaler.ToPxf (labelW);
    }

    return scaler.ToPx (2 * kPadDip) + (int) std::ceil (labelW);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ToolbarLabelEntry::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    (void) labeled;

    m_rc     = rc;
    m_scaler = scaler;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry::Paint
//
//  The label in the body face and the theme's text color, centered on the
//  strip's height; it takes no hover or press chrome.
//
////////////////////////////////////////////////////////////////////////////////

void ToolbarLabelEntry::Paint (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    bool                 hovered,
    bool                 pressed,
    bool                 labeled)
{
    HRESULT  hr   = S_OK;
    float    pad  = m_scaler.ToPxf ((float) kPadDip);
    float    left = (float) m_rc.left + pad;



    (void) painter;
    (void) hovered;
    (void) pressed;
    (void) labeled;

    hr = text.DrawString (m_label.c_str(),
                          left,
                          (float) m_rc.top,
                          (std::max) (0.0f, (float) m_rc.right - pad - left),
                          (float) (m_rc.bottom - m_rc.top),
                          theme.Foreground(),
                          m_scaler.ToPxf (kFontDip),
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ToolbarLabelEntry::GetTooltipAt (int x, int y, RECT & anchor) const
{
    (void) x;
    (void) y;
    (void) anchor;

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarLabelEntry::OnClick
//
//  Consumed, so a click on the label runs nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool ToolbarLabelEntry::OnClick (int x, int y)
{
    (void) x;
    (void) y;

    return true;
}
