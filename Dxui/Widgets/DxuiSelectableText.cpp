#include "Pch.h"
#include "DxuiSelectableText.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Core/DxuiClipboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::GetSelectedText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiSelectableText::GetSelectedText() const
{
    size_t  from = (std::min) (m_anchor, m_caret);
    size_t  to   = (std::min) ((std::max) (m_anchor, m_caret), m_text.size());



    return (from < to) ? m_text.substr (from, to - from) : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::Copy
//
//  The selection, or all of it when nothing is selected, since a message is
//  the one thing there is to copy.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSelectableText::Copy() const
{
    DxuiClipboard::SetText (m_hwnd, HasSelection() ? GetSelectedText() : m_text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSelectableText::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.GetDpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::Paint
//
//  The selection's boxes under the text, then the text over them, both laid
//  out in the same box so they agree.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSelectableText::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT                                         hr     = S_OK;
    DxuiFontHandle                                  font   = theme.BodyFont();
    float                                           x      = (float) m_boundsDip.left;
    float                                           y      = (float) m_boundsDip.top;
    float                                           w      = (float) (m_boundsDip.right  - m_boundsDip.left);
    float                                           h      = (float) (m_boundsDip.bottom - m_boundsDip.top);
    std::vector<IDxuiTextRenderer::TextRangeRect>   boxes;



    if (!IsVisible() || m_text.empty() || w <= 0.0f || h <= 0.0f)
    {
        return;
    }

    m_face   = (font.face != nullptr) ? font.face : L"Segoe UI";
    m_fontPx = m_scaler.ToPxf (font.sizeDip);

    if (HasSelection())
    {
        hr = text.GetTextRangeRects (m_text.c_str(), m_fontPx, m_face.c_str(), w, h, m_hAlign, m_vAlign,
                                     (std::min) (m_anchor, m_caret), (std::max) (m_anchor, m_caret) - (std::min) (m_anchor, m_caret), boxes);
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (const IDxuiTextRenderer::TextRangeRect & box : boxes)
        {
            painter.FillRect (x + box.x, y + box.y, box.width, box.height, theme.SelectionBackground());
        }
    }

    hr = text.DrawString (m_text.c_str(), x, y, w, h, theme.TextColor (m_role), m_fontPx, m_face.c_str(),
                          m_hAlign, m_vAlign, DxuiFontWeight::Normal, true);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSelectableText::HitTest (POINT clientPx, size_t & outIndex) const
{
    HRESULT  hr = E_FAIL;



    if (m_renderer != nullptr && m_fontPx > 0.0f)
    {
        hr = m_renderer->HitTestText (m_text.c_str(), m_fontPx, m_face.c_str(),
                                      (float) (m_boundsDip.right - m_boundsDip.left), (float) (m_boundsDip.bottom - m_boundsDip.top),
                                      m_hAlign, m_vAlign,
                                      (float) (clientPx.x - m_boundsDip.left), (float) (clientPx.y - m_boundsDip.top), outIndex);
    }

    outIndex = (std::min) (outIndex, m_text.size());

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::SelectWord
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSelectableText::SelectWord (size_t index)
{
    size_t  from = (std::min) (index, m_text.size());
    size_t  to   = from;



    while (from > 0 && IsWordChar (m_text[from - 1]))
    {
        from--;
    }

    while (to < m_text.size() && IsWordChar (m_text[to]))
    {
        to++;
    }

    m_anchor = from;
    m_caret  = to;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::OnMouse
//
//  A press sets where a selection starts, a drag carries it, and a second
//  press inside the double-click time takes the word.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSelectableText::OnMouse (const DxuiMouseEvent & ev)
{
    size_t     index = 0;
    ULONGLONG  now   = GetTickCount64();



    if (ev.button != DxuiMouseButton::Left && ev.kind != DxuiMouseEventKind::Move)
    {
        return false;
    }

    switch (ev.kind)
    {
        case DxuiMouseEventKind::Down:
            if (!HitTest (ev.positionDip, index))
            {
                return false;
            }

            if (now - m_lastClick <= GetDoubleClickTime())
            {
                SelectWord (index);
                m_lastClick = 0;
                return true;
            }

            m_lastClick = now;
            m_anchor    = index;
            m_caret     = index;
            m_dragging  = true;
            return true;

        case DxuiMouseEventKind::Move:
            if (m_dragging && HitTest (ev.positionDip, index))
            {
                m_caret = index;
                return true;
            }

            return false;

        case DxuiMouseEventKind::Up:
            m_dragging = false;
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::QueryCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSelectableText::QueryCommand (DxuiStandardCommand command, bool & outEnabled) const
{
    switch (command)
    {
        case DxuiStandardCommand::Copy:
        case DxuiStandardCommand::SelectAll:
            outEnabled = !m_text.empty();
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::InvokeCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSelectableText::InvokeCommand (DxuiStandardCommand command)
{
    switch (command)
    {
        case DxuiStandardCommand::Copy:
            Copy();
            return true;

        case DxuiStandardCommand::SelectAll:
            SelectAll();
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSelectableText::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiSelectableText::GetCursorForPoint (POINT clientPx) const
{
    bool  inside = clientPx.x >= m_boundsDip.left && clientPx.x < m_boundsDip.right
                && clientPx.y >= m_boundsDip.top  && clientPx.y < m_boundsDip.bottom;



    return (IsVisible() && inside && !m_text.empty()) ? IDC_IBEAM : nullptr;
}
