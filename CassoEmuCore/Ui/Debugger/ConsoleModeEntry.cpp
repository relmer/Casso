#include "Pch.h"

#include "Ui/Debugger/ConsoleModeEntry.h"
#include "Core/UnicodeSymbols.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::IsInside
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleModeEntry::IsInside (const RECT & rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::GetWidthPx
//
//  The same width in both forms: a drop-down box has no icon to collapse to.
//
////////////////////////////////////////////////////////////////////////////////

int ConsoleModeEntry::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    (void) labeled;
    (void) text;

    return scaler.ToPx (kLabelDip + kBoxDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::Layout
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleModeEntry::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    int  inset = scaler.ToPx (kInsetYDip);



    (void) labeled;

    m_scaler = scaler;
    m_rc     = rc;
    m_box    = RECT { (std::min) (rc.right, rc.left + scaler.ToPx (kLabelDip)), rc.top + inset, rc.right, rc.bottom - inset };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::Paint
//
//  The label in the strip's ink, then the box as a combo box draws: a
//  framed field holding the mode, with a chevron at its right end.
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleModeEntry::Paint (
    IDxuiPainter       & painter,
    IDxuiTextRenderer  & text,
    const IDxuiTheme   & theme,
    bool                 hovered,
    bool                 pressed,
    bool                 labeled)
{
    HRESULT  hr    = S_OK;
    float    x     = (float) m_box.left;
    float    y     = (float) m_box.top;
    float    w     = (float) (m_box.right - m_box.left);
    float    h     = (float) (m_box.bottom - m_box.top);
    float    pad   = m_scaler.ToPxf ((float) kTextPadDip);
    float    arrow = m_scaler.ToPxf ((float) kArrowDip);



    (void) hovered;
    (void) labeled;

    hr = text.DrawString (L"Mode:",
                          (float) m_rc.left,
                          (float) m_rc.top,
                          (float) (m_box.left - m_rc.left),
                          (float) (m_rc.bottom - m_rc.top),
                          theme.Foreground(),
                          m_scaler.ToPxf (kFontDip),
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    painter.FillRect    (x, y, w, h, pressed ? theme.ButtonPressed() : (m_hover ? theme.ButtonHover() : theme.ButtonIdle()));
    painter.OutlineRect (x, y, w, h, 1.0f, theme.ButtonBorder());

    hr = text.DrawString (m_modeText.c_str(),
                          x + pad,
                          y,
                          (std::max) (0.0f, w - pad - arrow),
                          h,
                          theme.Foreground(),
                          m_scaler.ToPxf (kFontDip),
                          DxuiTheme::kBodyFace,
                          DxuiTextHAlign::Left,
                          DxuiTextVAlign::Center,
                          DxuiFontWeight::Normal,
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = text.DrawString (s_kpszMdl2ChevronDown,
                          x + w - arrow,
                          y,
                          arrow,
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
//  ConsoleModeEntry::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ConsoleModeEntry::GetTooltipAt (int x, int y, RECT & anchor) const
{
    if (!IsInside (m_box, x, y) || m_tip.empty())
    {
        return nullptr;
    }

    anchor = m_box;

    return m_tip.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::OnClick
//
//  A click on the box is not consumed, so the toolbar opens the entry's
//  drop-down of modes. The label takes a click and does nothing with it.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleModeEntry::OnClick (int x, int y)
{
    return !IsInside (m_box, x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleModeEntry::OnMouseMove (int x, int y)
{
    m_hover = IsInside (m_box, x, y);

    return m_hover;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleModeEntry::OnMouseLeave()
{
    m_hover = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleModeEntry::OnLButtonDown
//
//  The box is the entry's one part that takes a press.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleModeEntry::OnLButtonDown (int x, int y)
{
    return IsInside (m_box, x, y);
}




