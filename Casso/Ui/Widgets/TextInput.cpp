#include "Pch.h"

#include "TextInput.h"


namespace
{
    constexpr float     s_kFontDip       = 13.0f;
    constexpr float     s_kPadLeftDp     = 6.0f;
    constexpr float     s_kPadRightDp    = 6.0f;
    constexpr float     s_kCaretWidthDp  = 1.0f;
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

bool TextInput::HitTest (int x, int y) const
{
    return m_enabled &&
           x >= m_rect.left && x < m_rect.right &&
           y >= m_rect.top  && y < m_rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMouseHover
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::SetMouseHover (int x, int y)
{
    m_hover = HitTest (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool TextInput::OnLButtonDown (int x, int y)
{
    HRESULT  hr       = S_OK;
    int64_t  nowMs    = (int64_t) GetTickCount64();
    size_t   hitCaret = 0;
    bool     isDouble = false;
    bool     hit      = false;
    bool     result   = false;



    hit = HitTest (x, y);
    if (!hit)
    {
        m_focused = false;
    }
    BAIL_OUT_IF (!hit, S_OK);

    m_focused  = true;
    m_dragging = true;
    hitCaret   = CaretFromClientX (x);

    // Second click on (or very near) the same index within the system
    // double-click time selects the word under the caret.
    isDouble = (nowMs - m_lastClickMs) <= (int64_t) GetDoubleClickTime() &&
               (hitCaret == m_lastClickCaret ||
                (hitCaret > 0 && hitCaret - 1 == m_lastClickCaret) ||
                hitCaret + 1 == m_lastClickCaret);

    if (isDouble)
    {
        SelectWordAt (hitCaret);
        m_dragging   = false;            // word already selected; no drag
        m_lastClickMs = 0;               // reset so a triple-click isn't a double
    }
    else
    {
        m_caret = hitCaret;

        // Shift-click extends the existing selection; a plain click drops
        // the anchor at the caret (no selection).
        if (!Shift())
        {
            m_anchor = m_caret;
        }

        m_lastClickMs    = nowMs;
        m_lastClickCaret = hitCaret;
    }

    ResetBlink();
    result = true;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool TextInput::OnLButtonUp (int x, int y)
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    (void) x;
    (void) y;

    BAIL_OUT_IF (!m_dragging, S_OK);

    m_dragging = false;
    result     = true;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
//  Extends the selection while dragging: the anchor stays put and the
//  caret follows the cursor.
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::OnMouseMove (int x, int y)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_dragging || !m_enabled, S_OK);

    m_caret = CaretFromClientX (x);
    ResetBlink();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool TextInput::OnKey (WPARAM vk)
{
    HRESULT  hr       = S_OK;
    bool     consumed = false;
    bool     shift    = Shift   ();
    bool     ctrl     = Control ();



    BAIL_OUT_IF (!m_focused || !m_enabled, S_OK);

    switch (vk)
    {
        case VK_LEFT:
            if (!shift && m_caret != m_anchor)
            {
                // Collapse an existing selection to its left edge.
                m_caret = std::min (m_caret, m_anchor);
            }
            else if (ctrl)
            {
                m_caret = WordLeft (m_caret);
            }
            else if (m_caret > 0)
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
            if (!shift && m_caret != m_anchor)
            {
                m_caret = std::max (m_caret, m_anchor);
            }
            else if (ctrl)
            {
                m_caret = WordRight (m_caret);
            }
            else if (m_caret < m_text.size())
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
            else if (ctrl && m_caret > 0)
            {
                size_t  wl = WordLeft (m_caret);

                m_text.erase (wl, m_caret - wl);
                m_caret  = wl;
                m_anchor = wl;
                FireChange();
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
            else if (ctrl && m_caret < m_text.size())
            {
                size_t  wr = WordRight (m_caret);

                m_text.erase (m_caret, wr - m_caret);
                FireChange();
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

bool TextInput::OnChar (wchar_t ch)
{
    HRESULT       hr     = S_OK;
    std::wstring  ins;
    bool          result = false;



    BAIL_OUT_IF (!m_focused || !m_enabled, S_OK);
    BAIL_OUT_IF (ch < 0x20 || ch == 0x7F, S_OK);

    ins.assign (1, ch);
    InsertText (ins);
    ResetBlink();
    result = true;

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::Paint (DxUiPainter & painter, DwriteTextRenderer & text) const
{
    constexpr int64_t  s_kBlinkHalfMs = 530;          // Windows default caret blink

    HRESULT      hr         = S_OK;
    float        x          = (float) m_rect.left;
    float        y          = (float) m_rect.top;
    float        w          = (float) (m_rect.right  - m_rect.left);
    float        h          = (float) (m_rect.bottom - m_rect.top);
    float        padL       = m_scaler.Pxf (s_kPadLeftDp);
    float        padR       = m_scaler.Pxf (s_kPadRightDp);
    float        fontPx     = m_scaler.Pxf (s_kFontDip);
    float        innerW     = w - padL - padR;
    uint32_t     bgArgb     = s_kFallbackBg;
    uint32_t     fgArgb     = s_kFallbackFg;
    uint32_t     selArgb    = s_kFallbackSel;
    uint32_t     edgeArgb   = s_kFallbackEdge;
    uint32_t     focusArgb  = s_kFallbackFocus;
    float        measW      = 0.0f;
    float        measH      = 0.0f;
    float        caretX     = 0.0f;
    float        fullTextW  = 0.0f;
    float        lineH      = 0.0f;
    size_t       selStart   = std::min (m_caret, m_anchor);
    size_t       selEnd     = std::max (m_caret, m_anchor);
    std::wstring prefix;
    int64_t      blinkPhase = 0;
    bool         caretOn    = false;



    if (m_theme != nullptr)
    {
        bgArgb    = m_theme->dropdownBgArgb;
        fgArgb    = m_theme->dropdownItemTextArgb;
        selArgb   = m_theme->navHoverArgb;
        edgeArgb  = (fgArgb & 0x00FFFFFFu) | 0x30000000u;
        focusArgb = m_theme->linkArgb;
    }

    painter.FillRect    (x, y, w, h, bgArgb);
    painter.OutlineRect (x, y, w, h, 1.0f, m_focused ? focusArgb : edgeArgb);

    // Rebuild the per-boundary x cache so mouse hit-testing matches what is
    // drawn (same font / DPI). Index i = width of m_text[0..i).
    m_glyphX.assign (m_text.size() + 1, 0.0f);
    for (size_t i = 1; i <= m_text.size(); i++)
    {
        prefix.assign (m_text, 0, i);
        IGNORE_RETURN_VALUE (hr, text.MeasureString (prefix.c_str(), fontPx, L"Segoe UI", measW, measH));
        m_glyphX[i] = measW;
    }

    fullTextW = m_glyphX.empty() ? 0.0f : m_glyphX.back();
    caretX    = (m_caret < m_glyphX.size()) ? m_glyphX[m_caret] : fullTextW;

    // A reliable line height for caret sizing -- measH is only set when the
    // text is non-empty, so fall back to a reference measurement so an empty
    // field still shows a correctly sized caret.
    lineH = measH;
    if (lineH <= 0.0f)
    {
        float  refW = 0.0f;
        IGNORE_RETURN_VALUE (hr, text.MeasureString (L"Mg", fontPx, L"Segoe UI", refW, lineH));
    }

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
        float  selL    = m_glyphX[selStart];
        float  selR    = m_glyphX[selEnd];
        float  selH    = std::min (lineH, h - 2.0f);
        float  selTop  = y + (h - selH) * 0.5f;

        text.FillRect (x + padL + selL - m_scrollPx, selTop, selR - selL, selH, selArgb);
    }

    IGNORE_RETURN_VALUE (hr, text.DrawString (m_text.c_str(),
                                              x + padL - m_scrollPx,
                                              y,
                                              std::max (innerW + m_scrollPx, fullTextW + 1.0f),
                                              h,
                                              fgArgb,
                                              fontPx,
                                              L"Segoe UI",
                                              DwriteTextRenderer::HAlign::Left,
                                              DwriteTextRenderer::VAlign::Center,
                                              DWRITE_FONT_WEIGHT_NORMAL,
                                              false));

    // Blinking caret. Seed the phase anchor on first paint after focus so
    // the caret starts solid; toggle every half-period thereafter.
    if (m_focused)
    {
        int64_t  nowMs = (int64_t) GetTickCount64();

        if (m_blinkAnchorMs == 0)
        {
            m_blinkAnchorMs = nowMs;
        }

        blinkPhase = (nowMs - m_blinkAnchorMs) / s_kBlinkHalfMs;
        caretOn    = (blinkPhase % 2) == 0;

        if (caretOn)
        {
            // Win32-style caret: ~1 logical px wide (DPI-scaled, snapped to a
            // whole pixel so it stays crisp) and as tall as the text line,
            // vertically centred in the field rather than spanning its full
            // height.
            float  caretW   = std::max (1.0f, std::floor (m_scaler.Pxf (s_kCaretWidthDp) + 0.5f));
            float  caretH   = std::min (lineH, h - 2.0f);
            float  caretTop = y + (h - caretH) * 0.5f;

            text.FillRect (x + padL + caretX - m_scrollPx, caretTop, caretW, caretH, fgArgb);
        }
    }

    IGNORE_RETURN_VALUE (hr, text.PopClipRect());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampCaret
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::ClampCaret ()
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
//  CaretFromClientX
//
//  Maps a client x to the nearest caret boundary using the glyph offsets
//  cached by the most recent Paint. Picks the boundary whose drawn x is
//  closest to the cursor (midpoint rule), so clicking the left half of a
//  glyph lands before it and the right half after it.
//
////////////////////////////////////////////////////////////////////////////////

size_t TextInput::CaretFromClientX (int xPx) const
{
    float   padL   = m_scaler.Pxf (s_kPadLeftDp);
    float   target = (float) xPx - (float) m_rect.left - padL + m_scrollPx;
    size_t  best   = 0;
    bool    found  = false;



    if (m_glyphX.empty() || target <= 0.0f)
    {
        best = 0;
    }
    else if (target >= m_glyphX.back())
    {
        best = m_glyphX.size() - 1;
    }
    else
    {
        for (size_t i = 0; i + 1 < m_glyphX.size() && !found; i++)
        {
            float  mid = (m_glyphX[i] + m_glyphX[i + 1]) * 0.5f;

            if (target < mid)
            {
                best  = i;
                found = true;
            }
            else
            {
                best = i + 1;
            }
        }
    }

    return best;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WordLeft / WordRight
//
//  Word-boundary navigation matching the common Windows edit behavior:
//  WordLeft skips any whitespace immediately left of `pos`, then the word
//  characters, landing on the word start. WordRight skips the current word
//  characters, then the whitespace, landing on the next word start.
//
////////////////////////////////////////////////////////////////////////////////

size_t TextInput::WordLeft (size_t pos) const
{
    while (pos > 0 && !IsWordChar (m_text[pos - 1]))
    {
        pos--;
    }

    while (pos > 0 && IsWordChar (m_text[pos - 1]))
    {
        pos--;
    }

    return pos;
}





size_t TextInput::WordRight (size_t pos) const
{
    size_t  n = m_text.size();



    while (pos < n && IsWordChar (m_text[pos]))
    {
        pos++;
    }

    while (pos < n && !IsWordChar (m_text[pos]))
    {
        pos++;
    }

    return pos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SelectWordAt
//
//  Selects the run of word characters containing `pos` (anchor at its
//  start, caret at its end). When `pos` sits on a non-word character the
//  single character under it is selected instead, so a double-click always
//  selects something.
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::SelectWordAt (size_t pos)
{
    HRESULT  hr    = S_OK;
    size_t   n     = m_text.size();
    size_t   start = pos;
    size_t   end   = pos;



    if (n == 0)
    {
        m_anchor = 0;
        m_caret  = 0;
    }
    BAIL_OUT_IF (n == 0, S_OK);

    if (pos >= n)
    {
        pos = n - 1;
    }

    if (IsWordChar (m_text[pos]))
    {
        start = pos;
        end   = pos;

        while (start > 0 && IsWordChar (m_text[start - 1]))
        {
            start--;
        }

        while (end < n && IsWordChar (m_text[end]))
        {
            end++;
        }
    }
    else
    {
        start = pos;
        end   = pos + 1;
    }

    m_anchor = start;
    m_caret  = end;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ResetBlink
//
//  Restarts the caret blink phase so the caret is solid immediately after
//  any caret move or edit.
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::ResetBlink ()
{
    m_blinkAnchorMs = (int64_t) GetTickCount64();

    if (m_blinkAnchorMs == 0)
    {
        m_blinkAnchorMs = 1;             // 0 means "unseeded" in Paint
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeleteSelection
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::DeleteSelection ()
{
    HRESULT  hr       = S_OK;
    size_t   selStart = std::min (m_caret, m_anchor);
    size_t   selEnd   = std::max (m_caret, m_anchor);



    BAIL_OUT_IF (selStart == selEnd, S_OK);

    m_text.erase (selStart, selEnd - selStart);
    m_caret  = selStart;
    m_anchor = selStart;
    FireChange();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InsertText
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::InsertText (const std::wstring & ins)
{
    HRESULT  hr   = S_OK;
    size_t   room = 0;
    size_t   take = 0;



    if (m_caret != m_anchor)
    {
        DeleteSelection();
    }

    room = (m_maxLen > m_text.size()) ? (m_maxLen - m_text.size()) : 0;
    take = std::min (ins.size(), room);

    BAIL_OUT_IF (take == 0, S_OK);

    m_text.insert (m_caret, ins, 0, take);
    m_caret += take;
    m_anchor = m_caret;
    FireChange();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::CopyToClipboard () const
{
    HRESULT       hr       = S_OK;
    size_t        selStart = std::min (m_caret, m_anchor);
    size_t        selEnd   = std::max (m_caret, m_anchor);
    std::wstring  sel;



    BAIL_OUT_IF (selStart == selEnd, S_OK);

    sel.assign (m_text, selStart, selEnd - selStart);
    WriteTextToClipboard (sel);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyAllToClipboard
//
//  Copies the entire field contents to the clipboard regardless of the
//  current selection. Used by host copy affordances (e.g. a copy button).
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::CopyAllToClipboard () const
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_text.empty(), S_OK);

    WriteTextToClipboard (m_text);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteTextToClipboard
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::WriteTextToClipboard (const std::wstring & s) const
{
    HRESULT  hr      = S_OK;
    HGLOBAL  hGlobal = nullptr;
    void   * pBuf    = nullptr;
    HANDLE   setData = nullptr;
    size_t   bytes   = (s.size() + 1) * sizeof (wchar_t);
    BOOL     opened  = FALSE;
    BOOL     emptied = FALSE;



    opened = OpenClipboard (m_hwnd);
    CWR (opened);

    emptied = EmptyClipboard();
    CWR (emptied);

    hGlobal = GlobalAlloc (GMEM_MOVEABLE, bytes);
    CWR (hGlobal);

    pBuf = GlobalLock (hGlobal);
    CWR (pBuf);

    memcpy (pBuf, s.c_str(), bytes);
    GlobalUnlock (hGlobal);

    setData = SetClipboardData (CF_UNICODETEXT, hGlobal);
    CWR (setData);
    hGlobal = nullptr;               // clipboard owns the block on success

Error:
    if (hGlobal != nullptr)
    {
        GlobalFree (hGlobal);
    }
    if (opened)
    {
        CloseClipboard();
    }
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PasteFromClipboard
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::PasteFromClipboard ()
{
    HRESULT       hr     = S_OK;
    HANDLE        hData  = nullptr;
    wchar_t     * pBuf   = nullptr;
    BOOL          opened = FALSE;
    std::wstring  ins;



    opened = OpenClipboard (m_hwnd);
    CWR (opened);

    hData = GetClipboardData (CF_UNICODETEXT);
    CWR (hData);

    pBuf = (wchar_t *) GlobalLock (hData);
    CWR (pBuf);

    ins.assign (pBuf);
    GlobalUnlock (hData);
    CloseClipboard();
    opened = FALSE;                  // closed in the normal path; don't double-close

    // Strip newlines for single-line input.
    for (auto & c : ins)
    {
        if (c == L'\r' || c == L'\n' || c == L'\t')
        {
            c = L' ';
        }
    }

    InsertText (ins);

Error:
    if (opened)
    {
        CloseClipboard();
    }
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FireChange
//
////////////////////////////////////////////////////////////////////////////////

void TextInput::FireChange ()
{
    if (m_change)
    {
        m_change (m_text);
    }
}
