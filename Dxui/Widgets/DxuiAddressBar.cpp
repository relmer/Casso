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
    m_hover  = s_kNoSegment;

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
    m_pressed = s_kNoSegment;
    m_input.SetFocused (false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::HitTestSegment
//
////////////////////////////////////////////////////////////////////////////////

int DxuiAddressBar::HitTestSegment (int x, int y) const
{
    int   hit    = s_kNoSegment;
    int   i      = 0;
    bool  inside = x >= m_boundsDip.left && x < m_boundsDip.right && y >= m_boundsDip.top && y < m_boundsDip.bottom;



    if (inside)
    {
        hit = (x < m_segmentsRight) ? s_kSeparator : s_kBlank;

        for (i = m_firstShown; i < (int) m_rects.size(); i++)
        {
            const RECT & r = m_rects[(size_t) i];

            if (x >= r.left && x < r.right)
            {
                hit = i;
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
//  Each segment is its label and padding, followed by a separator. Leading
//  segments are dropped until the rest fit; the last always shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::LayoutSegments()
{
    int               padX   = m_scaler.ToPx (s_kPadXDip);
    int               segPad = m_scaler.ToPx (s_kSegmentPadDip);
    int               sep    = m_scaler.ToPx (s_kSeparatorDip);
    int               avail  = (int) (m_boundsDip.right - m_boundsDip.left) - padX * 2;
    int               total  = 0;
    int               x      = (int) m_boundsDip.left + padX;
    int               i      = 0;
    std::vector<int>  widths;



    m_rects.assign (m_labels.size(), RECT {});
    m_firstShown = 0;

    for (const std::wstring & label : m_labels)
    {
        widths.push_back (MeasurePx (label) + segPad * 2);
        total += widths.back() + sep;
    }

    while (m_firstShown + 1 < (int) m_labels.size() && total > avail)
    {
        total -= widths[(size_t) m_firstShown] + sep;
        m_firstShown++;
    }

    for (i = m_firstShown; i < (int) m_labels.size(); i++)
    {
        m_rects[(size_t) i] = RECT { x, m_boundsDip.top, x + widths[(size_t) i], m_boundsDip.bottom };
        x                  += widths[(size_t) i] + sep;
    }

    m_segmentsRight = x;
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
        hr = m_renderer->MeasureString (label.c_str(), m_scaler.ToPxf (s_kFontDip), DxuiTheme::kBodyFace, width, height);
    }

    return SUCCEEDED (hr) ? (int) std::ceil (width) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar::Paint
//
//  A raised field, outlined in the accent while it is being edited and in
//  the focus color while it only has focus.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiAddressBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr     = S_OK;
    float    x      = (float) m_boundsDip.left;
    float    y      = (float) m_boundsDip.top;
    float    w      = (float) (m_boundsDip.right - m_boundsDip.left);
    float    h      = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float    fontPx = m_scaler.ToPxf (s_kFontDip);
    float    sep    = (float) m_scaler.ToPx (s_kSeparatorDip);
    int      i      = 0;



    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }

    painter.FillRect (x, y, w, h, theme.BackgroundElevated());

    if (m_editing || m_focused)
    {
        painter.OutlineRoundedRect (x, y, w, h, m_scaler.ToPxf (s_kRadiusDip), 1.0f, m_editing ? theme.Accent() : theme.FocusRing());
    }

    if (m_editing)
    {
        m_input.Paint (painter, text, theme);
        return;
    }

    hr = text.PushClipRect (x, y, w, h);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (i = m_firstShown; i < (int) m_labels.size(); i++)
    {
        const RECT & r = m_rects[(size_t) i];

        if (i == m_hover || i == m_pressed)
        {
            painter.FillRect ((float) r.left, y, (float) (r.right - r.left), h, theme.HoverBackground());
        }

        hr = text.DrawString (m_labels[(size_t) i].c_str(),
                              (float) r.left, y, (float) (r.right - r.left), h,
                              theme.Foreground(),
                              fontPx,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        hr = text.DrawString (s_kpszChevronRight,
                              (float) r.right, y, sep, h,
                              theme.ForegroundMuted(),
                              fontPx,
                              DxuiTheme::kBodyFace,
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
//  DxuiAddressBar::OnMouse
//
//  While editing, the text field takes the pointer. Otherwise a segment or
//  the blank past them acts on release over what was pressed; a separator
//  does nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiAddressBar::OnMouse (const DxuiMouseEvent & ev)
{
    int   hit     = HitTestSegment (ev.positionDip.x, ev.positionDip.y);
    int   pressed = m_pressed;
    bool  left    = ev.button == DxuiMouseButton::Left;
    bool  handled = false;



    if (m_editing)
    {
        if (ev.kind == DxuiMouseEventKind::Down && left)
        {
            m_pressed = s_kBlank;
        }
        else if (ev.kind == DxuiMouseEventKind::Up && left)
        {
            m_pressed = s_kNoSegment;
        }

        handled = m_input.OnMouse (ev) || ev.kind != DxuiMouseEventKind::Move;
        m_input.SetFocused (true);
    }
    else if (ev.kind == DxuiMouseEventKind::Move)
    {
        handled = (hit >= 0 ? hit : s_kNoSegment) != m_hover;
        m_hover = (hit >= 0) ? hit : s_kNoSegment;
    }
    else if (ev.kind == DxuiMouseEventKind::Down && left && hit != s_kNoSegment)
    {
        m_pressed = hit;
        handled   = true;
    }
    else if (ev.kind == DxuiMouseEventKind::Up && left && pressed != s_kNoSegment)
    {
        m_pressed = s_kNoSegment;
        handled   = true;

        if (hit == pressed && hit >= 0 && m_onSegment)
        {
            m_onSegment (hit);
        }
        else if (hit == pressed && hit == s_kBlank)
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
