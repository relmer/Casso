#include "Pch.h"
#include "Theme/DxuiTheme.h"

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
    int     i   = 0;
    size_t  n   = m_options.size();
    int     hit = -1;



    if (m_enabled)
    {
        for (i = 0; i < (int) n && hit < 0; ++i)
        {
            const RECT & r = m_options[(size_t) i].rect;

            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            {
                hit = i;
            }
        }
    }

    return hit;
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
    int   hit      = HitTest (x, y);
    bool  wasHit   = (hit >= 0);



    if (wasHit)
    {
        m_pressedIdx = hit;
    }

    return wasHit;
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
//  Arrow-key navigation within the group.
//
//  All four arrows work, in pairs. A radio group may be laid out horizontally
//  or vertically and the widget does not know which, so binding only one axis
//  would leave half the layouts unnavigable.
//
//  Selection WRAPS at both ends, matching the Windows radio-group convention.
//
//  Moving the selection COMMITS it immediately rather than merely previewing
//  it, which is how a radio group differs from a list: there is no separate
//  activation step, so arrowing through options is choosing them.
//
//  An unselected group (index -1) enters at the first option going forward and
//  the last going backward, so the first arrow press always lands somewhere
//  sensible.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnKey (WPARAM vk)
{
    int     next     = m_selected;
    size_t  n        = m_options.size();
    bool    isActive = false;
    bool    handled  = false;



    isActive = m_enabled && m_focused && n != 0;

    if (isActive && (vk == VK_LEFT || vk == VK_UP))
    {
        next    = (m_selected <= 0) ? (int) (n - 1) : m_selected - 1;
        handled = true;
    }
    else if (isActive && (vk == VK_RIGHT || vk == VK_DOWN))
    {
        next    = (m_selected < 0 || m_selected >= (int) n - 1) ? 0 : m_selected + 1;
        handled = true;
    }

    if (handled)
    {
        Commit (next);
    }

    return handled;
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
//  Draws each option: the circle, the selected dot, the focus ring, and the
//  label.
//
//  The circle centers on the LABEL, not on the option rect. For a plain
//  option those are the same thing. For a described one they are not: the box
//  is two lines tall, and centering on it would float the circle down between
//  the label and the description, pointing at neither.
//
//  Circles are drawn with the painter's polygon approximation because the
//  painter has no true circle primitive -- it is a solid-triangle batcher, and
//  keeping it that way is what makes it cheap.
//
//  The focus ring is drawn only on the SELECTED option, not on every option or
//  on a separately-tracked focused one. A radio group commits as it navigates,
//  so selection and keyboard position are always the same thing, and a second
//  indicator would imply a distinction that does not exist.
//
//  Disabled state changes three colors together -- circle, dot, and label -- so
//  the row reads as disabled as a whole rather than as a grayed label beside a
//  live-looking control.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRadioGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    constexpr float  s_kBoxSizeDip    = 16.0f;
    constexpr float  s_kDotInsetDip   = 4.0f;
    constexpr float  s_kFocusInsetDip = -2.0f;
    constexpr float  s_kFocusThickDip = 1.0f;
    constexpr float  s_kLabelGapDip   = 6.0f;
    constexpr float  s_kFontDip       = 13.0f;
    // The first line of a described option. The label sits in this band, the
    // BUTTON CENTERS ON IT, and the description gets whatever the caller's
    // rect has left over. Centering the button on the whole box instead would
    // float it down beside the description, pointing at neither line.
    constexpr float  s_kLabelLineDip  = 20.0f;



    HRESULT  hr         = S_OK;
    int      i          = 0;
    size_t   n          = m_options.size();
    float    boxSize    = m_scaler.ToPxf (s_kBoxSizeDip);
    float    dotInset   = m_scaler.ToPxf (s_kDotInsetDip);
    float    focusInset = m_scaler.ToPxf (s_kFocusInsetDip);
    float    focusThick = m_scaler.ToPxf (s_kFocusThickDip);
    float    labelGap   = m_scaler.ToPxf (s_kLabelGapDip);
    float    fontDip    = m_scaler.ToPxf (s_kFontDip);
    uint32_t textColor  = m_enabled ? theme.Foreground() : theme.ForegroundDisabled();
    uint32_t dotColor   = m_enabled ? theme.ButtonText() : theme.ForegroundDisabled();



    for (i = 0; i < (int) n; ++i)
    {
        const DxuiRadioOption  & opt     = m_options[(size_t) i];
        float                    boxLeft = (float) opt.rect.left;
        float                    cx      = 0.0f;
        float                    cy      = 0.0f;
        float                    outerR  = 0.0f;
        float                    innerR  = 0.0f;
        float               labelBand = opt.description.empty()
                                        ? (float) (opt.rect.bottom - opt.rect.top)
                                        : m_scaler.ToPxf (s_kLabelLineDip);
        float               boxTop    = (float) opt.rect.top + (labelBand - boxSize) * 0.5f;
        cx = boxLeft + boxSize * 0.5f;
        cy = boxTop  + boxSize * 0.5f;
        outerR = boxSize * 0.5f;
        innerR = (boxSize - dotInset * 2.0f) * 0.5f;
        uint32_t            boxColor = m_enabled
                                            ? (m_hover == i ? theme.ButtonHover() : theme.ButtonIdle())
                                            : theme.PressedBackground();

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
                                 theme.FocusRing());
        }

        float   textLeft  = boxLeft + boxSize + labelGap;
        float   textWidth = (float) (opt.rect.right - opt.rect.left) - boxSize - labelGap;
        float   rowHeight = (float) (opt.rect.bottom - opt.rect.top);
        float   labelH    = opt.description.empty() ? rowHeight : m_scaler.ToPxf (s_kLabelLineDip);

        //  An undescribed option centers its label in the whole row, exactly
        //  as before. A described one puts the label on the first line and
        //  gives the rest to the description, so adding descriptions to one
        //  option in a group does not shift the others.
        hr = text.DrawString (opt.label.c_str(),
                              textLeft,
                              (float) opt.rect.top,
                              textWidth,
                              labelH,
                              textColor,
                              fontDip,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (!opt.description.empty())
        {
            //  Muted, and disabled-muted when the group is: a description
            //  that stayed sharp while its label dimmed would read as the
            //  live part of a dead control.
            uint32_t   descColor = m_enabled ? theme.ForegroundMuted() : theme.ForegroundDisabled();

            hr = text.DrawString (opt.description.c_str(),
                                  textLeft,
                                  (float) opt.rect.top + labelH,
                                  textWidth,
                                  rowHeight - labelH,
                                  descColor,
                                  fontDip,
                                  DxuiTheme::kBodyFace,
                                  DxuiTextHAlign::Left,
                                  DxuiTextVAlign::Top);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
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
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are testable without
//  framework events.
//
//  A move only updates hover and is reported unhandled, so the pointer
//  crossing the group does not consume moves other widgets want.
//
//  Only the left button acts; a right-click belongs to the host.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        break;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::OnKey  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiRadioGroup::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRadioGroup::GetAccessibleName  (IDxuiControl override)
//
//  Returns the label of the selected option (or empty if no selection),
//  followed by its description where it has one.
//
//  THE DESCRIPTION IS PART OF THE NAME, not decoration. It exists precisely
//  because the label alone does not say what the option means -- so a reader
//  given the label by itself is left with the same guess the description was
//  added to remove, which is the one user who can least afford it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiRadioGroup::GetAccessibleName() const
{
    std::wstring  name;
    bool          hasSelection = false;



    hasSelection = m_selected >= 0 && m_selected < (int) m_options.size();

    if (hasSelection)
    {
        const DxuiRadioOption &  opt = m_options[(size_t) m_selected];

        name = opt.label;

        if (!opt.description.empty())
        {
            name += L". ";
            name += opt.description;
        }
    }

    return name;
}
