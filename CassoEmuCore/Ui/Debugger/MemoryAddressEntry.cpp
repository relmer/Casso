#include "Pch.h"

#include "Ui/Debugger/MemoryAddressEntry.h"
#include "Core/UnicodeSymbols.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"





static constexpr const wchar_t *  s_kGlyphChevronDown = s_kpszMdl2ChevronDown;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryAddressEntry::IsInside (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::GetWidthPx
//
//  The same width in both forms: a combo box has no icon to collapse to.
//
////////////////////////////////////////////////////////////////////////////////

int MemoryAddressEntry::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    (void) labeled;
    (void) text;

    return scaler.ToPx (kWidthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::Layout
//
////////////////////////////////////////////////////////////////////////////////

void MemoryAddressEntry::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    int  arrow = scaler.ToPx (kArrowDip);



    (void) labeled;

    m_scaler = scaler;
    m_rc     = rc;
    m_arrow  = RECT { (std::max) (rc.left, rc.right - arrow), rc.top, rc.right, rc.bottom };

    if (m_box != nullptr)
    {
        m_box->Layout (RECT { rc.left, rc.top, m_arrow.left, rc.bottom }, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::Paint
//
//  The arrow only; the box is the window's control and paints itself over
//  the strip.
//
////////////////////////////////////////////////////////////////////////////////

void MemoryAddressEntry::Paint (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    bool                 hovered,
    bool                 pressed,
    bool                 labeled)
{
    HRESULT  hr = S_OK;
    float    x  = (float) m_arrow.left;
    float    y  = (float) m_arrow.top;
    float    w  = (float) (m_arrow.right - m_arrow.left);
    float    h  = (float) (m_arrow.bottom - m_arrow.top);



    (void) hovered;
    (void) labeled;

    painter.FillRect    (x, y, w, h, pressed ? theme.ButtonPressed() : (m_hover ? theme.ButtonHover() : theme.ButtonIdle()));
    painter.OutlineRect (x, y, w, h, 1.0f, theme.ButtonBorder());

    hr = text.DrawString (s_kGlyphChevronDown,
                          x,
                          y,
                          w,
                          h,
                          theme.Foreground(),
                          m_scaler.ToPxf (kArrowGlyphDip),
                          DxuiToolbar::kMdl2IconFace,
                          DxuiTextHAlign::Center,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * MemoryAddressEntry::GetTooltipAt (int x, int y, RECT & anchor) const
{
    if (!IsInside (m_rc, x, y))
    {
        return nullptr;
    }

    anchor = m_rc;

    return IsInside (m_arrow, x, y) ? m_arrowTip.c_str() : m_boxTip.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::OnClick
//
//  A click on the arrow is not consumed, so the toolbar opens the entry's
//  drop-down of earlier addresses.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryAddressEntry::OnClick (int x, int y)
{
    return !IsInside (m_arrow, x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryAddressEntry::OnMouseMove (int x, int y)
{
    m_hover = IsInside (m_arrow, x, y);

    return m_hover;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void MemoryAddressEntry::OnMouseLeave()
{
    m_hover = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryAddressEntry::OnLButtonDown
//
//  The arrow is the entry's one part that takes a press.
//
////////////////////////////////////////////////////////////////////////////////

bool MemoryAddressEntry::OnLButtonDown (int x, int y)
{
    return IsInside (m_arrow, x, y);
}
