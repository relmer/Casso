#include "Pch.h"

#include "Ui/Debugger/ToolbarBoxEntry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool ToolbarBoxEntry::IsInside (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry::GetWidthPx
//
//  The same width in both forms: a box has no icon to collapse to.
//
////////////////////////////////////////////////////////////////////////////////

int ToolbarBoxEntry::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    (void) labeled;
    (void) text;

    return scaler.ToPx (kGapDip + m_labelWidthDip + m_boxWidthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ToolbarBoxEntry::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    int  gap   = scaler.ToPx (kGapDip);
    int  label = scaler.ToPx (m_labelWidthDip);



    (void) labeled;

    m_scaler  = scaler;
    m_rc      = rc;
    m_labelRc = RECT { (std::min) (rc.right, rc.left + gap), rc.top, (std::min) (rc.right, rc.left + gap + label), rc.bottom };

    if (m_box != nullptr)
    {
        m_box->Layout (RECT { m_labelRc.right, rc.top, rc.right, rc.bottom }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry::Paint
//
//  The label only; the box is the window's control and paints itself over
//  the strip.
//
////////////////////////////////////////////////////////////////////////////////

void ToolbarBoxEntry::Paint (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    bool                 hovered,
    bool                 pressed,
    bool                 labeled)
{
    HRESULT  hr = S_OK;



    (void) painter;
    (void) hovered;
    (void) pressed;
    (void) labeled;

    hr = text.DrawString (m_label.c_str(),
                          (float) m_labelRc.left,
                          (float) m_labelRc.top,
                          (float) (m_labelRc.right - m_labelRc.left),
                          (float) (m_labelRc.bottom - m_labelRc.top),
                          theme.Foreground(),
                          m_scaler.ToPxf (kLabelFontDip),
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ToolbarBoxEntry::GetTooltipAt (int x, int y, RECT & anchor) const
{
    if (!IsInside (m_rc, x, y))
    {
        return nullptr;
    }

    anchor = m_rc;

    return m_tip.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToolbarBoxEntry::OnClick
//
//  Consumed, so the toolbar runs nothing for the entry.
//
////////////////////////////////////////////////////////////////////////////////

bool ToolbarBoxEntry::OnClick (int x, int y)
{
    return IsInside (m_rc, x, y);
}
