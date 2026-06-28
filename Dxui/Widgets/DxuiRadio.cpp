#include "Pch.h"

#include "DxuiRadio.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetSelected
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::SetSelected (int index)
{
    if (index < 0 || index >= (int) m_options.size())
    {
        m_selected = -1;
        return;
    }

    m_selected = index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

int DxuiRadioGroup::HitTest (int x, int y) const
{
    int     i = 0;
    size_t  n = m_options.size();



    if (!m_enabled)
    {
        return -1;
    }

    for (i = 0; i < (int) n; ++i)
    {
        const RECT & r = m_options[(size_t) i].rect;

        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
        {
            return i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);

    if (m_pressedIdx >= 0 && m_pressedIdx != m_hover)
    {
        m_pressedIdx = -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnLButtonDown (int x, int y)
{
    int  hit = HitTest (x, y);



    if (hit < 0)
    {
        return false;
    }

    m_pressedIdx = hit;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnLButtonUp (int x, int y)
{
    int  hit      = HitTest (x, y);
    bool consumed = m_pressedIdx >= 0 && hit == m_pressedIdx;



    m_pressedIdx = -1;

    if (consumed)
    {
        Commit (hit);
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnKey (WPARAM vk)
{
    int     next = m_selected;
    size_t  n    = m_options.size();



    if (!m_enabled || !m_focused || n == 0)
    {
        return false;
    }

    if (vk == VK_LEFT || vk == VK_UP)
    {
        next = (m_selected <= 0) ? (int) (n - 1) : m_selected - 1;
        Commit (next);
        return true;
    }

    if (vk == VK_RIGHT || vk == VK_DOWN)
    {
        next = (m_selected < 0 || m_selected >= (int) n - 1) ? 0 : m_selected + 1;
        Commit (next);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Commit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::Commit (int newIndex)
{
    bool  changed = (newIndex != m_selected);



    m_selected = newIndex;

    if (changed && m_change)
    {
        m_change (newIndex);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    constexpr uint32_t  s_kBoxIdle      = 0xFF606060;
    constexpr uint32_t  s_kBoxHover     = 0xFF808080;
    constexpr uint32_t  s_kBoxDisabled  = 0xFF303030;
    constexpr uint32_t  s_kDot          = 0xFFFFFFFF;
    constexpr uint32_t  s_kDotDisabled  = 0xFF707070;
    constexpr uint32_t  s_kFocusRing    = 0xFFAACCFF;
    constexpr uint32_t  s_kTextIdle     = 0xFFE8EEF4;
    constexpr uint32_t  s_kTextDisabled = 0xFF707070;
    constexpr float     s_kBoxSizeDip    = 16.0f;
    constexpr float     s_kDotInsetDip   = 4.0f;
    constexpr float     s_kFocusInsetDip = -2.0f;
    constexpr float     s_kFocusThickDip = 1.0f;
    constexpr float     s_kLabelGapDip   = 6.0f;
    constexpr float     s_kFontDip      = 13.0f;

    HRESULT  hr         = S_OK;
    int      i          = 0;
    size_t   n          = m_options.size();
    float    boxSize    = m_scaler.Pxf (s_kBoxSizeDip);
    float    dotInset   = m_scaler.Pxf (s_kDotInsetDip);
    float    focusInset = m_scaler.Pxf (s_kFocusInsetDip);
    float    focusThick = m_scaler.Pxf (s_kFocusThickDip);
    float    labelGap   = m_scaler.Pxf (s_kLabelGapDip);
    float    fontDip    = m_scaler.Pxf (s_kFontDip);
    uint32_t textColor  = m_enabled ? s_kTextIdle : s_kTextDisabled;
    uint32_t dotColor   = m_enabled ? s_kDot      : s_kDotDisabled;



    for (i = 0; i < (int) n; ++i)
    {
        const DxuiRadioOption & opt      = m_options[(size_t) i];
        float               boxLeft  = (float) opt.rect.left;
        float               boxTop   = (float) opt.rect.top
                                       + ((float) (opt.rect.bottom - opt.rect.top) - boxSize) * 0.5f;
        float               cx       = boxLeft + boxSize * 0.5f;
        float               cy       = boxTop  + boxSize * 0.5f;
        float               outerR   = boxSize * 0.5f;
        float               innerR   = (boxSize - dotInset * 2.0f) * 0.5f;
        uint32_t            boxColor = m_enabled
                                            ? (m_hover == i ? s_kBoxHover : s_kBoxIdle)
                                            : s_kBoxDisabled;

        painter.FillCircleApprox (cx, cy, outerR, boxColor);

        if (m_selected == i)
        {
            painter.FillCircleApprox (cx, cy, innerR, dotColor);
        }

        if (m_focused && m_selected == i)
        {
            painter.OutlineRect (boxLeft + focusInset,
                                 boxTop  + focusInset,
                                 boxSize - focusInset * 2.0f,
                                 boxSize - focusInset * 2.0f,
                                 focusThick,
                                 s_kFocusRing);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (opt.label.c_str(),
                                                  boxLeft + boxSize + labelGap,
                                                  (float) opt.rect.top,
                                                  (float) (opt.rect.right - opt.rect.left) - boxSize - labelGap,
                                                  (float) (opt.rect.bottom - opt.rect.top),
                                                  textColor,
                                                  fontDip,
                                                  L"Segoe UI",
                                                  DxuiTextHAlign::Left,
                                                  DxuiTextVAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::Layout  (IDxuiControl override)
//
//  Snaps the group's bounding box; per-option rects were already
//  populated by the caller via SetOptions and remain unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (theme);
    static_cast<const DxuiRadioGroup *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        return false;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            return OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return OnKey (ev.vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::AccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected option (or empty if no selection).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiRadioGroup::AccessibleName () const
{
    if (m_selected < 0 || m_selected >= (int) m_options.size())
    {
        return L"";
    }

    return m_options[(size_t) m_selected].label;
}
