#include "Pch.h"

#include "DxuiToolbarEditBox.h"

#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox  (constructor)
//
//  The input draws no frame of its own, since the field is the entry's, and
//  its hint uses Explorer's italic, disabled-looking style.
//
////////////////////////////////////////////////////////////////////////////////

DxuiToolbarEditBox::DxuiToolbarEditBox()
{
    m_input.SetChromeless        (true);
    m_input.SetPlaceholderItalic (true);
    m_input.SetFont              (DxuiTheme::kBodyFace, kFontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::SetFocused
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarEditBox::SetFocused (bool focused)
{
    m_focused = focused;
    m_input.SetFocused (focused);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::OnKey
//
//  Enter submits and Escape cancels. Characters and editing keys go to the
//  input, and any change to the text is reported.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarEditBox::OnKey (const DxuiKeyEvent & ev)
{
    bool  consumed = false;



    if (ev.kind == DxuiKeyEventKind::Char)
    {
        //  Enter, Escape and Tab arrive as characters too; their key-down
        //  events already did what they do.
        if (ev.vk < 0x20)
        {
            return ev.vk != L'\t';
        }

        EditThenNotify ([this, &ev, &consumed]() { consumed = m_input.OnChar ((wchar_t) ev.vk); });
        return consumed;
    }

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    switch (ev.vk)
    {
    case VK_RETURN:
        if (m_onSubmit)
        {
            m_onSubmit();
        }

        return true;

    case VK_ESCAPE:
        if (m_onCancel)
        {
            m_onCancel();
        }

        return true;

    case VK_TAB:
        return false;

    default:
        EditThenNotify ([this, &ev, &consumed]() { consumed = m_input.OnKey (ev.vk); });
        return consumed;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::EditThenNotify
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarEditBox::EditThenNotify (const std::function<void()> & edit)
{
    std::wstring  before = m_input.GetText();



    edit();

    if (m_input.GetText() != before && m_onChange)
    {
        m_onChange (m_input.GetText());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::GetWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiToolbarEditBox::GetWidthPx (bool labeled, const DxuiDpiScaler & scaler, IDxuiTextRenderer * text) const
{
    (void) labeled;
    (void) text;

    return scaler.ToPx (m_widthDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::Layout
//
//  The input takes the field less a small inset on the left and, with a
//  glyph, the glyph's slot on the right.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarEditBox::Layout (const RECT & rc, bool labeled, const DxuiDpiScaler & scaler)
{
    int  right = m_scaler.ToPx ((m_glyph != nullptr) ? s_kGlyphSlotDip : s_kInsetDip);



    (void) labeled;

    m_rc = rc;
    m_scaler.SetDpi (scaler.GetDpi());
    m_input.SetDpi  (scaler.GetDpi());

    right = m_scaler.ToPx ((m_glyph != nullptr) ? s_kGlyphSlotDip : s_kInsetDip);

    m_input.SetRect (RECT { rc.left + m_scaler.ToPx (s_kInsetDip),
                            rc.top,
                            (std::max) (rc.left, rc.right - right),
                            rc.bottom });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarEditBox::Paint (IDxuiPainter      & painter,
                                IDxuiTextRenderer & text,
                                const IDxuiTheme  & theme,
                                bool                hovered,
                                bool                pressed,
                                bool                labeled)
{
    HRESULT   hr     = S_OK;
    float     x      = (float) m_rc.left;
    float     y      = (float) m_rc.top;
    float     w      = (float) (m_rc.right - m_rc.left);
    float     h      = (float) (m_rc.bottom - m_rc.top);
    float     radius = m_scaler.ToPxf ((float) DxuiTheme::kCornerRadiusDip);
    float     line   = (std::max) (1.0f, m_scaler.ToPxf (2.0f));
    float     slot   = m_scaler.ToPxf ((float) s_kGlyphSlotDip);
    uint32_t  fill   = m_focused ? theme.ContentBackground()
                     : m_hover   ? theme.ButtonHover()
                                 : theme.ButtonIdle();



    (void) hovered;
    (void) pressed;
    (void) labeled;

    painter.FillRoundedRect    (x, y, w, h, radius, fill);
    painter.OutlineRoundedRect (x, y, w, h, radius, 1.0f, m_error ? theme.ErrorForeground() : theme.ButtonBorder());

    if (m_focused)
    {
        painter.FillRect (x + radius, y + h - line, w - radius * 2.0f, line, m_error ? theme.ErrorForeground() : theme.Accent());
    }

    m_input.SetTheme (&theme);
    m_input.Paint (painter, text);

    if (m_glyph != nullptr)
    {
        hr = text.DrawString (m_glyph,
                              x + w - slot,
                              y,
                              slot,
                              h,
                              theme.ForegroundMuted(),
                              m_scaler.ToPxf (12.0f),
                              DxuiToolbar::kMdl2IconFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::GetTooltipAt
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiToolbarEditBox::GetTooltipAt (int x, int y, RECT & anchor) const
{
    if (m_tooltip.empty() || !Contains (x, y))
    {
        return nullptr;
    }

    anchor = m_rc;

    return m_tooltip.c_str();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::OnLButtonDown
//
//  Focus comes first, since taking focus resets the input's drag state.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarEditBox::OnLButtonDown (int x, int y)
{
    if (!Contains (x, y))
    {
        return false;
    }

    if (m_onFocusRequest)
    {
        m_onFocusRequest();
    }

    m_input.OnLButtonDown (x, y);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::OnClick
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarEditBox::OnClick (int x, int y)
{
    m_input.OnLButtonUp (x, y);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarEditBox::OnMouseMove (int x, int y)
{
    m_hover = Contains (x, y);
    m_input.OnMouseMove (x, y);

    return m_hover;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::OnMouseLeave
//
////////////////////////////////////////////////////////////////////////////////

void DxuiToolbarEditBox::OnMouseLeave()
{
    m_hover = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarEditBox::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiToolbarEditBox::Contains (int x, int y) const
{
    return x >= m_rc.left && x < m_rc.right && y >= m_rc.top && y < m_rc.bottom;
}
