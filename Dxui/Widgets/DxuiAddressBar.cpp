#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiAddressBar.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::SetSegments
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::SetSegments (std::vector<std::wstring> labels)
{
    m_labels = std::move (labels);
    m_hover  = Hit();

    LayoutSegments();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::BeginEdit
//
//  The path, all selected, so what is typed replaces it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::BeginEdit()
{
    bool  selected = false;



    m_editing = true;
    m_input.SetText    (m_path);
    m_input.SetFocused (true);

    selected = m_input.InvokeCommand (DxuiStandardCommand::SelectAll);
    IGNORE_RETURN_VALUE (selected, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::EndEdit
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::EndEdit()
{
    m_editing = false;
    m_pressed = Hit();
    m_input.SetFocused (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::HitTest
//
////////////////////////////////////////////////////////////////////////////////

DxuiAddressBar::Hit DxuiAddressBar::HitTest (int x, int y) const
{
    Hit   hit;
    int   i      = 0;
    bool  inside = x >= m_boundsDip.left && x < m_boundsDip.right && y >= m_boundsDip.top && y < m_boundsDip.bottom;



    if (inside)
    {
        hit.part = Part::Blank;

        if (x >= m_overflow.left && x < m_overflow.right)
        {
            hit = Hit { Part::Overflow, -1 };
        }
        else if (x >= m_overflowSep.left && x < m_overflowSep.right)
        {
            hit = Hit { Part::Separator, -1 };
        }

        for (i = m_firstShown; i < (int) m_rects.size(); i++)
        {
            if (x >= m_rects[(size_t) i].left && x < m_rects[(size_t) i].right)
            {
                hit = Hit { Part::Segment, i };
            }
            else if (x >= m_separators[(size_t) i].left && x < m_separators[(size_t) i].right)
            {
                hit = Hit { Part::Separator, i };
            }
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  padX = 0;



    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());

    padX = m_scaler.ToPx (s_kPadXDip);

    m_input.Layout (RECT { boundsDip.left + padX, boundsDip.top, boundsDip.right - padX, boundsDip.bottom }, m_scaler);
    LayoutSegments();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::LayoutSegments
//
//  Each segment is its label and padding, followed by its separator. Leading
//  segments are dropped until the rest fit; the last always shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::LayoutSegments()
{
    int               padX     = m_scaler.ToPx (s_kPadXDip);
    int               segPad   = m_scaler.ToPx (s_kSegmentPadDip);
    int               sep      = m_scaler.ToPx (s_kSeparatorDip);
    int               overflow = m_scaler.ToPx (s_kOverflowDip);
    int               avail    = (int) (m_boundsDip.right - m_boundsDip.left) - padX * 2;
    int               total    = 0;
    int               x        = (int) m_boundsDip.left + padX;
    int               i        = 0;
    std::vector<int>  widths;



    m_rects.assign      (m_labels.size(), RECT {});
    m_separators.assign (m_labels.size(), RECT {});
    m_overflow    = RECT {};
    m_overflowSep = RECT {};
    m_firstShown  = 0;

    for (const std::wstring & label : m_labels)
    {
        widths.push_back (MeasurePx (label) + segPad * 2);
        total += widths.back() + sep;
    }

    //  What does not fit goes behind the overflow button, which takes its own
    //  room from what is left.
    if (total > avail)
    {
        avail -= overflow + sep;
    }

    while (m_firstShown + 1 < (int) m_labels.size() && total > avail)
    {
        total -= widths[(size_t) m_firstShown] + sep;
        m_firstShown++;
    }

    if (m_firstShown > 0)
    {
        m_overflow    = RECT { x, m_boundsDip.top, x + overflow, m_boundsDip.bottom };
        x            += overflow;
        m_overflowSep = RECT { x, m_boundsDip.top, x + sep, m_boundsDip.bottom };
        x            += sep;
    }

    for (i = m_firstShown; i < (int) m_labels.size(); i++)
    {
        m_rects[(size_t) i]      = RECT { x, m_boundsDip.top, x + widths[(size_t) i], m_boundsDip.bottom };
        x                       += widths[(size_t) i];
        m_separators[(size_t) i] = RECT { x, m_boundsDip.top, x + sep, m_boundsDip.bottom };
        x                       += sep;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::MeasurePx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiAddressBar::MeasurePx (const std::wstring & label) const
{
    HRESULT  hr     = E_FAIL;
    float    width  = 0.0f;
    float    height = 0.0f;



    if (m_renderer != nullptr)
    {
        hr = m_renderer->MeasureString (label.c_str(), m_scaler.ToPxf (m_fontDip), (m_face != nullptr) ? m_face : DxuiTheme::kBodyFace, width, height);
    }

    return SUCCEEDED (hr) ? (int) std::ceil (width) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::Paint
//
//  A raised, rounded field, outlined in the accent while it is being edited
//  and in the focus color while it only has focus.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT          hr      = S_OK;
    float            x       = (float) m_boundsDip.left;
    float            y       = (float) m_boundsDip.top;
    float            w       = (float) (m_boundsDip.right - m_boundsDip.left);
    float            h       = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float            radius  = m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip);
    float            accent  = (float) m_scaler.ToPx (2);   // the editing field's accent underline
    const wchar_t  * face    = (m_face != nullptr) ? m_face : DxuiTheme::kBodyFace;
    int              i       = 0;



    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }

    //  Editing, the field takes Windows' focused text box look: a darker fill,
    //  a hairline edge and the accent along its bottom. With focus but no edit,
    //  the focus ring marks it as it marks the window's other controls.
    painter.FillRoundedRect (x, y, w, h, radius, m_editing ? theme.ContentBackground() : theme.BackgroundElevated());

    if (m_editing)
    {
        painter.OutlineRoundedRect (x, y, w, h, radius, 1.0f, (theme.Foreground() & 0x00FFFFFFu) | 0x18000000u);
        painter.FillRect (x + radius, y + h - accent, w - radius * 2.0f, accent, theme.Accent());
    }
    else if (m_focused)
    {
        painter.OutlineRoundedRect (x, y, w, h, radius, (float) m_scaler.ToPx (2), theme.FocusRing());
    }

    if (m_editing)
    {
        m_input.Paint (painter, text, theme);
        return;
    }

    hr = text.PushClipRect (x, y, w, h);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_firstShown > 0)
    {
        PaintHover (painter, theme, m_overflow, Hit { Part::Overflow, -1 });

        hr = text.DrawString (s_kpszMdl2More,
                              (float) m_overflow.left, y, (float) (m_overflow.right - m_overflow.left), h,
                              theme.Foreground(),
                              m_scaler.ToPxf (s_kChevronDip),
                              m_iconFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (s_kpszMdl2ChevronRight,
                              (float) m_overflowSep.left, y, (float) (m_overflowSep.right - m_overflowSep.left), h,
                              theme.Foreground(),
                              m_scaler.ToPxf (s_kChevronDip),
                              m_iconFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    for (i = m_firstShown; i < (int) m_labels.size(); i++)
    {
        const RECT & seg = m_rects[(size_t) i];
        const RECT & sep = m_separators[(size_t) i];

        PaintHover (painter, theme, seg, Hit { Part::Segment,   i });
        PaintHover (painter, theme, sep, Hit { Part::Separator, i });

        hr = text.DrawString (m_labels[(size_t) i].c_str(),
                              (float) seg.left, y, (float) (seg.right - seg.left), h,
                              theme.Foreground(),
                              m_scaler.ToPxf (m_fontDip),
                              face,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (s_kpszMdl2ChevronRight,
                              (float) sep.left, y, (float) (sep.right - sep.left), h,
                              theme.Foreground(),
                              m_scaler.ToPxf (s_kChevronDip),
                              m_iconFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::PaintHover
//
//  A rounded card inset from the bar's top and bottom, in Windows' subtle
//  hover fill, or its pressed fill while the button is down.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::PaintHover (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & rc, const Hit & hit) const
{
    float  inset = (float) m_scaler.ToPx (s_kHoverInsetDip);



    if (hit == m_hover || hit == m_pressed)
    {
        painter.FillRoundedRect ((float) rc.left,
                                 (float) rc.top + inset,
                                 (float) (rc.right - rc.left),
                                 (float) (rc.bottom - rc.top) - inset * 2.0f,
                                 m_scaler.ToPxf (DxuiTheme::kCornerRadiusDip),
                                 (hit == m_pressed) ? theme.SystemButtonPressed() : theme.SystemButtonHover());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::OnMouse
//
//  While editing, the text field takes the pointer. Otherwise what was
//  pressed acts on release over the same part: a segment navigates, a
//  separator opens its menu, and the blank past them starts an edit.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiAddressBar::OnMouse (const DxuiMouseEvent & ev)
{
    Hit   hit     = HitTest (ev.positionDip.x, ev.positionDip.y);
    Hit   pressed = m_pressed;
    Hit   hover;
    bool  left    = ev.button == DxuiMouseButton::Left;
    bool  handled = false;



    if (m_editing)
    {
        if (ev.kind == DxuiMouseEventKind::Down && left)
        {
            m_pressed = Hit { Part::Blank, -1 };
        }
        else if (ev.kind == DxuiMouseEventKind::Up && left)
        {
            m_pressed = Hit();
        }

        handled = m_input.OnMouse (ev) || ev.kind != DxuiMouseEventKind::Move;
        m_input.SetFocused (true);
    }
    else if (ev.kind == DxuiMouseEventKind::Move)
    {
        hover   = (hit.part == Part::Segment || hit.part == Part::Overflow || (hit.part == Part::Separator && hit.index >= 0)) ? hit : Hit();
        handled = !(hover == m_hover);
        m_hover = hover;
    }
    else if (ev.kind == DxuiMouseEventKind::Down && left && hit.part != Part::None)
    {
        m_pressed = hit;
        handled   = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && left && pressed.part != Part::None)
    {
        m_pressed = Hit();
        handled   = true;

        if (hit == pressed && hit.part == Part::Segment && m_onSegment)
        {
            m_onSegment (hit.index);
        }
        else if (hit == pressed && hit.part == Part::Overflow && m_onOverflow)
        {
            m_onOverflow (m_overflow);
        }
        else if (hit == pressed && hit.part == Part::Separator && hit.index >= 0 && m_onSeparator)
        {
            m_onSeparator (hit.index, m_separators[(size_t) hit.index]);
        }
        else if (hit == pressed && hit.part == Part::Blank)
        {
            BeginEdit();
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::OnKey
//
//  Enter and F4 open the path for editing. While editing, Enter reports the
//  text and Escape cancels; Tab is left to the host, and every other key and
//  character goes to the text field.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiAddressBar::OnKey (const DxuiKeyEvent & ev)
{
    bool  down    = ev.kind == DxuiKeyEventKind::Down;
    bool  handled = false;



    if (!m_editing)
    {
        if (down && (ev.vk == VK_RETURN || ev.vk == VK_F4))
        {
            BeginEdit();
            handled = true;
        }
    }
    else if (down && ev.vk == VK_RETURN)
    {
        handled = true;

        if (m_onSubmit)
        {
            m_onSubmit (m_input.GetText());
        }
    }
    else if (down && ev.vk == VK_ESCAPE)
    {
        EndEdit();
        handled = true;
    }
    else if (!(down && ev.vk == VK_TAB))
    {
        handled = m_input.OnKey (ev);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::OnFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::OnFocusChanged (bool focused)
{
    m_focused = focused;

    if (!focused)
    {
        EndEdit();
    }
}
