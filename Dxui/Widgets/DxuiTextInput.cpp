#include "Pch.h"

#include "Widgets/DxuiTextInput.h"


namespace
{
    constexpr float     s_kFontDip       = 13.0f;
    constexpr float     s_kPadLeftDip     = 6.0f;
    constexpr float     s_kPadRightDip    = 6.0f;
    constexpr float     s_kCaretWidthPx  = 1.0f;
    constexpr uint32_t  s_kFallbackBg    = 0xFF1A1F26;
    constexpr uint32_t  s_kFallbackFg    = 0xFFE8EEF4;
    constexpr uint32_t  s_kFallbackSel   = 0xFF335577;
    constexpr uint32_t  s_kFallbackEdge  = 0xFF445566;
    constexpr uint32_t  s_kFallbackFocus = 0xFFAACCFF;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::HitTest (int x, int y) const
{
    if (!m_enabled)
    {
        return false;
    }
    return x >= m_boundsDip.left && x < m_boundsDip.right && y >= m_boundsDip.top && y < m_boundsDip.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnLButtonDown (int x, int y)
{
    if (!HitTest (x, y))
    {
        m_focused = false;
        return false;
    }

    m_focused  = true;
    m_dragging = true;

    // Caret placement requires the text renderer for hit-testing. We
    // don't have one in mouse-down context; place at end as a safe
    // fallback. A future enhancement could measure on first paint and
    // store glyph offsets, but for filter inputs the user almost
    // always Tabs in / Ctrl+A's anyway.
    m_caret  = m_text.size();
    m_anchor = m_caret;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnLButtonUp (int x, int y)
{
    (void) x;
    (void) y;
    if (!m_dragging)
    {
        return false;
    }
    m_dragging = false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::OnMouseMove (int x, int y)
{
    (void) x;
    (void) y;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnKey (WPARAM vk)
{
    bool  consumed = false;
    bool  shift    = Shift   ();
    bool  ctrl     = Control ();


    if (!m_focused || !m_enabled)
    {
        return false;
    }

    switch (vk)
    {
        case VK_LEFT:
            if (m_caret > 0)
            {
                m_caret--;
            }
            if (!shift)
            {
                m_anchor = m_caret;
            }
            consumed = true;
            break;

        case VK_RIGHT:
            if (m_caret < m_text.size())
            {
                m_caret++;
            }
            if (!shift)
            {
                m_anchor = m_caret;
            }
            consumed = true;
            break;

        case VK_HOME:
            m_caret = 0;
            if (!shift)
            {
                m_anchor = m_caret;
            }
            consumed = true;
            break;

        case VK_END:
            m_caret = m_text.size();
            if (!shift)
            {
                m_anchor = m_caret;
            }
            consumed = true;
            break;

        case VK_BACK:
            if (m_caret != m_anchor)
            {
                DeleteSelection();
            }
            else if (m_caret > 0)
            {
                m_text.erase (m_caret - 1, 1);
                m_caret--;
                m_anchor = m_caret;
                FireChange();
            }
            consumed = true;
            break;

        case VK_DELETE:
            if (m_caret != m_anchor)
            {
                DeleteSelection();
            }
            else if (m_caret < m_text.size())
            {
                m_text.erase (m_caret, 1);
                FireChange();
            }
            consumed = true;
            break;

        case 'A':
            if (ctrl)
            {
                m_anchor = 0;
                m_caret  = m_text.size();
                consumed = true;
            }
            break;

        case 'C':
            if (ctrl)
            {
                CopyToClipboard();
                consumed = true;
            }
            break;

        case 'X':
            if (ctrl)
            {
                CopyToClipboard();
                if (m_caret != m_anchor)
                {
                    DeleteSelection();
                }
                consumed = true;
            }
            break;

        case 'V':
            if (ctrl)
            {
                PasteFromClipboard();
                consumed = true;
            }
            break;

        default:
            break;
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnChar (wchar_t ch)
{
    std::wstring  ins;


    if (!m_focused || !m_enabled)
    {
        return false;
    }

    if (ch < 0x20 || ch == 0x7F)
    {
        return false;
    }

    ins.assign (1, ch);
    InsertText (ins);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT      hr        = S_OK;
    float        x         = (float) m_boundsDip.left;
    float        y         = (float) m_boundsDip.top;
    float        w         = (float) (m_boundsDip.right  - m_boundsDip.left);
    float        h         = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float        padL      = m_scaler.Pxf (s_kPadLeftDip);
    float        padR      = m_scaler.Pxf (s_kPadRightDip);
    float        fontPx    = m_scaler.Pxf (s_kFontDip);
    float        innerW    = w - padL - padR;
    uint32_t     bgArgb    = s_kFallbackBg;
    uint32_t     fgArgb    = s_kFallbackFg;
    uint32_t     selArgb   = s_kFallbackSel;
    uint32_t     edgeArgb  = s_kFallbackEdge;
    uint32_t     focusArgb = s_kFallbackFocus;
    float        textMeasW = 0.0f;
    float        textMeasH = 0.0f;
    float        caretX    = 0.0f;
    float        fullTextW = 0.0f;
    size_t       selStart  = std::min (m_caret, m_anchor);
    size_t       selEnd    = std::max (m_caret, m_anchor);
    std::wstring before;
    std::wstring sel;
    std::wstring caretPrefix;


    if (m_theme != nullptr)
    {
        bgArgb    = m_theme->BackgroundElevated();
        fgArgb    = m_theme->Foreground();
        selArgb   = m_theme->SelectionBackground();
        edgeArgb  = (fgArgb & 0x00FFFFFFu) | 0x30000000u;
        focusArgb = m_theme->FocusRing();
    }

    painter.FillRect    (x, y, w, h, bgArgb);
    painter.OutlineRect (x, y, w, h, 1.0f, m_focused ? focusArgb : edgeArgb);

    caretPrefix.assign (m_text, 0, m_caret);
    IGNORE_RETURN_VALUE (hr, text.MeasureString (caretPrefix.c_str(), fontPx, L"Segoe UI", caretX,    textMeasH));
    IGNORE_RETURN_VALUE (hr, text.MeasureString (m_text.c_str(),      fontPx, L"Segoe UI", fullTextW, textMeasH));

    if (innerW <= 0.0f)
    {
        m_scrollPx = 0.0f;
    }
    else
    {
        if (caretX - m_scrollPx < 0.0f)            { m_scrollPx = caretX; }
        if (caretX - m_scrollPx > innerW)          { m_scrollPx = caretX - innerW; }
        if (fullTextW - m_scrollPx < innerW)       { m_scrollPx = fullTextW - innerW; }
        if (m_scrollPx < 0.0f)                     { m_scrollPx = 0.0f; }
    }

    IGNORE_RETURN_VALUE (hr, text.PushClipRect (x + padL, y, innerW, h));

    if (selStart != selEnd)
    {
        before.assign (m_text, 0, selStart);
        sel.assign    (m_text, selStart, selEnd - selStart);

        float bx = 0.0f;
        float sx = 0.0f;
        IGNORE_RETURN_VALUE (hr, text.MeasureString (before.c_str(), fontPx, L"Segoe UI", bx, textMeasH));
        IGNORE_RETURN_VALUE (hr, text.MeasureString (sel.c_str(),    fontPx, L"Segoe UI", sx, textMeasH));

        text.FillRect (x + padL + bx - m_scrollPx, y + 2.0f, sx, h - 4.0f, selArgb);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              x + padL - m_scrollPx,
                                              y,
                                              std::max (innerW + m_scrollPx, fullTextW + 1.0f),
                                              h,
                                              fgArgb,
                                              fontPx,
                                              L"Segoe UI",
                                              DxuiTextHAlign::Left,
                                              DxuiTextVAlign::Center,
                                              DWRITE_FONT_WEIGHT_NORMAL,
                                              false));

    if (m_focused)
    {
        text.FillRect (x + padL + caretX - m_scrollPx, y + 2.0f, (float) s_kCaretWidthPx, h - 4.0f, fgArgb);
    }

    IGNORE_RETURN_VALUE (hr, text.PopClipRect());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampCaret
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::ClampCaret ()
{
    if (m_caret > m_text.size())
    {
        m_caret = m_text.size();
    }
    if (m_anchor > m_text.size())
    {
        m_anchor = m_text.size();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaretFromX
//
////////////////////////////////////////////////////////////////////////////////

size_t DxuiTextInput::CaretFromX (IDxuiTextRenderer & text, int xPx) const
{
    HRESULT       hr       = S_OK;
    float         padL     = m_scaler.Pxf (s_kPadLeftDip);
    float         fontPx   = m_scaler.Pxf (s_kFontDip);
    float         target   = (float) xPx - (float) m_boundsDip.left - padL + m_scrollPx;
    float         w        = 0.0f;
    float         h        = 0.0f;
    std::wstring  prefix;
    size_t        best     = 0;
    float         bestDist = 1e9f;


    if (target <= 0.0f)
    {
        return 0;
    }

    for (size_t i = 0; i <= m_text.size(); i++)
    {
        prefix.assign (m_text, 0, i);
        IGNORE_RETURN_VALUE (hr, text.MeasureString (prefix.c_str(), fontPx, L"Segoe UI", w, h));

        float dist = std::abs (w - target);
        if (dist < bestDist)
        {
            bestDist = dist;
            best     = i;
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeleteSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::DeleteSelection ()
{
    size_t  selStart = std::min (m_caret, m_anchor);
    size_t  selEnd   = std::max (m_caret, m_anchor);


    if (selStart == selEnd)
    {
        return;
    }

    m_text.erase (selStart, selEnd - selStart);
    m_caret  = selStart;
    m_anchor = selStart;
    FireChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  InsertText
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::InsertText (const std::wstring & ins)
{
    if (m_caret != m_anchor)
    {
        DeleteSelection();
    }

    size_t  room = (m_maxLen > m_text.size()) ? (m_maxLen - m_text.size()) : 0;
    size_t  take = std::min (ins.size(), room);


    if (take == 0)
    {
        return;
    }

    m_text.insert (m_caret, ins, 0, take);
    m_caret += take;
    m_anchor = m_caret;
    FireChange();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::CopyToClipboard () const
{
    size_t        selStart = std::min (m_caret, m_anchor);
    size_t        selEnd   = std::max (m_caret, m_anchor);
    std::wstring  sel;
    HGLOBAL       hGlobal  = nullptr;
    void        * pBuf     = nullptr;
    BOOL          opened   = FALSE;


    if (selStart == selEnd)
    {
        return;
    }

    sel.assign (m_text, selStart, selEnd - selStart);

    opened = OpenClipboard (m_hwnd);
    if (!opened)
    {
        return;
    }

    if (!EmptyClipboard())
    {
        CloseClipboard();
        return;
    }

    hGlobal = GlobalAlloc (GMEM_MOVEABLE, (sel.size() + 1) * sizeof (wchar_t));
    if (hGlobal == nullptr)
    {
        CloseClipboard();
        return;
    }

    pBuf = GlobalLock (hGlobal);
    if (pBuf == nullptr)
    {
        GlobalFree (hGlobal);
        CloseClipboard();
        return;
    }

    memcpy (pBuf, sel.c_str(), (sel.size() + 1) * sizeof (wchar_t));
    GlobalUnlock (hGlobal);

    if (SetClipboardData (CF_UNICODETEXT, hGlobal) == nullptr)
    {
        GlobalFree (hGlobal);
    }

    CloseClipboard();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PasteFromClipboard
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::PasteFromClipboard ()
{
    HANDLE        hData = nullptr;
    wchar_t     * pBuf  = nullptr;
    BOOL          opened = FALSE;
    std::wstring  ins;


    opened = OpenClipboard (m_hwnd);
    if (!opened)
    {
        return;
    }

    hData = GetClipboardData (CF_UNICODETEXT);
    if (hData == nullptr)
    {
        CloseClipboard();
        return;
    }

    pBuf = (wchar_t *) GlobalLock (hData);
    if (pBuf == nullptr)
    {
        CloseClipboard();
        return;
    }

    ins.assign (pBuf);
    GlobalUnlock (hData);
    CloseClipboard();

    // Strip newlines for single-line input.
    for (auto & c : ins)
    {
        if (c == L'\r' || c == L'\n' || c == L'\t')
        {
            c = L' ';
        }
    }

    InsertText (ins);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FireChange
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::FireChange ()
{
    if (m_change)
    {
        m_change (m_text);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextInput::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextInput::Paint  (IDxuiControl override)
//
//  The legacy Paint takes (painter, text); the theme parameter mirrors
//  whatever was installed earlier via SetTheme.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_theme == nullptr)
    {
        m_theme = &theme;
    }
    static_cast<const DxuiTextInput *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextInput::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnMouse (const DxuiMouseEvent & ev)
{
    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_dragging)
        {
            OnMouseMove (ev.positionDip.x, ev.positionDip.y);
            return true;
        }
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
//  DxuiTextInput::OnKey  (IDxuiControl override)
//
//  Down events dispatch to OnKey(vk); Char events dispatch to OnChar.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Char)
    {
        return OnChar ((wchar_t) ev.vk);
    }

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        return OnKey (ev.vk);
    }

    return false;
}
