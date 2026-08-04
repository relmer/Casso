#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiTextInput.h"


static constexpr float     s_kFontDip             = 13.0f;
static constexpr float     s_kPadLeftDip          = 6.0f;
static constexpr float     s_kPadRightDip         = 6.0f;
static constexpr float     s_kCaretWidthPx        = 1.0f;
static constexpr uint32_t  s_kFallbackBg          = 0xFF1A1F26;
static constexpr uint32_t  s_kFallbackFg          = 0xFFE8EEF4;
static constexpr uint32_t  s_kFallbackSel         = 0xFF335577;
static constexpr uint32_t  s_kFallbackEdge        = 0xFF445566;
static constexpr uint32_t  s_kFallbackFocus       = 0xFFAACCFF;
static constexpr uint32_t  s_kFallbackPlaceholder = 0xFF6A7585;





////////////////////////////////////////////////////////////////////////////////
//
//  HitTest
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::HitTest (int x, int y) const
{
    return m_enabled
        && x >= m_boundsDip.left && x < m_boundsDip.right
        && y >= m_boundsDip.top  && y < m_boundsDip.bottom;
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
    bool  isHit = HitTest (x, y);



    m_focused = isHit;

    if (isHit)
    {
        m_dragging = true;

        // Caret placement requires the text renderer for hit-testing. We
        // don't have one in mouse-down context; place at end as a safe
        // fallback. A future enhancement could measure on first paint and
        // store glyph offsets, but for filter inputs the user almost
        // always Tabs in / Ctrl+A's anyway.
        m_caret  = m_text.size();
        m_anchor = m_caret;

        ResetBlink();
    }

    return isHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnLButtonUp (int x, int y)
{
    bool  wasDragging = m_dragging;



    (void) x;
    (void) y;

    m_dragging = false;

    return wasDragging;
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
    HRESULT  hr       = S_OK;
    bool     consumed = false;
    bool     shift    = Shift   ();
    bool     ctrl     = Control();
    bool     isActive = m_focused && m_enabled;



    BAIL_OUT_IF (!isActive, S_OK);

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

    if (consumed)
    {
        ResetBlink();
    }

Error:
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
    // Control characters and DEL are not text; the key handler owns those.
    bool          isTypable = m_focused && m_enabled && ch >= 0x20 && ch != 0x7F;



    if (isTypable)
    {
        ins.assign (1, ch);
        InsertText (ins);
        ResetBlink();
    }

    return isTypable;
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

    if (!m_chromeless)
    {
        painter.FillRect    (x, y, w, h, bgArgb);
        painter.OutlineRect (x, y, w, h, 1.0f, m_focused ? focusArgb : edgeArgb);
    }

    caretPrefix.assign (m_text, 0, m_caret);
    IGNORE_RETURN_VALUE (hr, text.MeasureString (caretPrefix.c_str(), fontPx, DxuiTheme::kBodyFace, caretX,    textMeasH));
    IGNORE_RETURN_VALUE (hr, text.MeasureString (m_text.c_str(),      fontPx, DxuiTheme::kBodyFace, fullTextW, textMeasH));

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
        IGNORE_RETURN_VALUE (hr, text.MeasureString (before.c_str(), fontPx, DxuiTheme::kBodyFace, bx, textMeasH));
        IGNORE_RETURN_VALUE (hr, text.MeasureString (sel.c_str(),    fontPx, DxuiTheme::kBodyFace, sx, textMeasH));

        text.FillRect (x + padL + bx - m_scrollPx, y + 2.0f, sx, h - 4.0f, selArgb);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              x + padL - m_scrollPx,
                                              y,
                                              std::max (innerW + m_scrollPx, fullTextW + 1.0f),
                                              h,
                                              fgArgb,
                                              fontPx,
                                              DxuiTheme::kBodyFace,
                                              DxuiTextHAlign::Left,
                                              DxuiTextVAlign::Center,
                                              DxuiFontWeight::Normal,
                                              false));

    if (m_text.empty() && !m_placeholder.empty())
    {
        uint32_t  phArgb = (m_theme != nullptr) ? m_theme->ForegroundMuted() : s_kFallbackPlaceholder;

        hr = text.DrawString (m_placeholder.c_str(),
                              x + padL,
                              y,
                              innerW,
                              h,
                              phArgb,
                              fontPx,
                              DxuiTheme::kBodyFace,
                              DxuiTextHAlign::Left,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_focused)
    {
        constexpr float    s_kEmptyCaretFactor = 1.3f;
        constexpr int64_t  s_kFallbackBlinkMs  = 530;   // only if the OS reports an invalid (zero) blink time

        int64_t  now     = (int64_t) GetTickCount64();
        UINT     blinkMs = GetCaretBlinkTime();
        bool     caretOn = true;

        if (m_blinkAnchorMs == 0)
        {
            m_blinkAnchorMs = now;
        }

        // INFINITE means the user disabled caret blinking -- keep it solid.
        if (blinkMs != INFINITE)
        {
            int64_t  halfMs = (blinkMs == 0) ? s_kFallbackBlinkMs : (int64_t) blinkMs;

            caretOn = (((now - m_blinkAnchorMs) / halfMs) % 2) == 0;
        }

        if (caretOn)
        {
            float  caretH   = (textMeasH > 1.0f) ? textMeasH : fontPx * s_kEmptyCaretFactor;
            float  caretTop = y + (h - caretH) * 0.5f;

            text.FillRect (x + padL + caretX - m_scrollPx, caretTop, (float) s_kCaretWidthPx, caretH, fgArgb);
        }
    }

    IGNORE_RETURN_VALUE (hr, text.PopClipRect());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampCaret
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::ClampCaret()
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



    // Left of the first glyph is caret 0 without measuring anything.
    if (target > 0.0f)
    {
        for (size_t i = 0; i <= m_text.size(); i++)
        {
            prefix.assign (m_text, 0, i);
            IGNORE_RETURN_VALUE (hr, text.MeasureString (prefix.c_str(), fontPx, DxuiTheme::kBodyFace, w, h));

            float dist = std::abs (w - target);

            if (dist < bestDist)
            {
                bestDist = dist;
                best     = i;
            }
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeleteSelection
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::DeleteSelection()
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

void DxuiTextInput::CopyToClipboard() const
{
    HRESULT       hr         = S_OK;
    size_t        selStart   = std::min (m_caret, m_anchor);
    size_t        selEnd     = std::max (m_caret, m_anchor);
    std::wstring  sel;
    HGLOBAL       hGlobal    = nullptr;
    void        * pBuf       = nullptr;
    bool          isOpen     = false;
    bool          wasEmptied = false;
    // True while WE still have to free hGlobal; cleared once the clipboard
    // takes ownership, which is the one path that must not free it.
    bool          ownsGlobal = false;



    BAIL_OUT_IF (selStart == selEnd, S_OK);   // nothing selected

    sel.assign (m_text, selStart, selEnd - selStart);

    isOpen = OpenClipboard (m_hwnd) != FALSE;

    BAIL_OUT_IF (!isOpen, S_OK);

    wasEmptied = EmptyClipboard() != FALSE;

    BAIL_OUT_IF (!wasEmptied, S_OK);

    hGlobal    = GlobalAlloc (GMEM_MOVEABLE, (sel.size() + 1) * sizeof (wchar_t));
    ownsGlobal = (hGlobal != nullptr);

    BAIL_OUT_IF (!ownsGlobal, S_OK);

    pBuf = GlobalLock (hGlobal);

    BAIL_OUT_IF (pBuf == nullptr, S_OK);

    memcpy (pBuf, sel.c_str(), (sel.size() + 1) * sizeof (wchar_t));
    GlobalUnlock (hGlobal);

    ownsGlobal = (SetClipboardData (CF_UNICODETEXT, hGlobal) == nullptr);

Error:
    if (ownsGlobal)
    {
        GlobalFree (hGlobal);
    }

    if (isOpen)
    {
        CloseClipboard();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PasteFromClipboard
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::PasteFromClipboard()
{
    HRESULT       hr      = S_OK;
    HANDLE        hData   = nullptr;
    wchar_t     * pBuf    = nullptr;
    std::wstring  ins;
    bool          isOpen  = false;
    bool          hasText = false;



    isOpen = OpenClipboard (m_hwnd) != FALSE;

    BAIL_OUT_IF (!isOpen, S_OK);

    hData = GetClipboardData (CF_UNICODETEXT);

    BAIL_OUT_IF (hData == nullptr, S_OK);

    pBuf = (wchar_t *) GlobalLock (hData);

    BAIL_OUT_IF (pBuf == nullptr, S_OK);

    ins.assign (pBuf);
    hasText = true;
    GlobalUnlock (hData);

Error:
    if (isOpen)
    {
        CloseClipboard();
    }

    // Insert AFTER releasing the clipboard: InsertText can raise callbacks,
    // and holding the clipboard open across them is asking for a deadlock.
    if (hasText)
    {
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  FireChange
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::FireChange()
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
    // Adopt the theme handed down by the paint pump every frame so a
    // theme change / preview takes effect on the next repaint without a
    // separate push. m_theme is read only within the synchronous const
    // Paint below, so pointing it at the passed theme is safe.
    m_theme = &theme;

    static_cast<const DxuiTextInput *> (this)->Paint (painter, text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextInput::OnMouse  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        if (m_dragging)
        {
            OnMouseMove (ev.positionDip.x, ev.positionDip.y);
            handled = true;
        }
        else
        {
            SetMouseHover (ev.positionDip.x, ev.positionDip.y);
        }
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
//  DxuiTextInput::OnKey  (IDxuiControl override)
//
//  Down events dispatch to OnKey(vk); Char events dispatch to OnChar.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnKey (const DxuiKeyEvent & ev)
{
    bool  handled = false;



    if (ev.kind == DxuiKeyEventKind::Char)
    {
        handled = OnChar ((wchar_t) ev.vk);
    }
    else if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = OnKey (ev.vk);
    }

    return handled;
}
