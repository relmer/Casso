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

        // Caret placement needs glyph measurement; with the host-supplied
        // renderer the caret lands under the cursor, without one the end of
        // the text is the safe fallback.
        m_caret  = (m_renderer != nullptr) ? CaretFromX (*m_renderer, x)
                                           : m_text.size();
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
//  Drag selection: the caret follows the cursor while the anchor stays at
//  the press point. Needs the measurement hook; inert without it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::OnMouseMove (int x, int y)
{
    (void) y;

    if (m_dragging && m_renderer != nullptr)
    {
        m_caret = CaretFromX (*m_renderer, x);
        ResetBlink();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Caret movement, editing, and the clipboard chords -- everything except the
//  characters themselves, which arrive through OnChar.
//
//  Selection is modeled as a caret plus an ANCHOR, and every movement key ends
//  the same way: move the caret, then collapse the anchor onto it UNLESS Shift
//  is held. That one rule produces the whole selection behavior, so there is
//  no separate "am I selecting" state to fall out of step with the keys.
//
//  Backspace and Delete both check for a selection FIRST. With a selection
//  live, either key deletes the selection rather than a single character,
//  which is what every text field does and what makes typing over selected
//  text work.
//
//  Ctrl+X reuses copy plus delete rather than having its own path, so cut and
//  copy can never disagree about what "the selection" is.
//
//  Nothing happens unless the control is focused AND enabled: a disabled field
//  must not silently accept edits it will not display.
//
//  Any consumed key resets the blink, so the caret stays solid while the user
//  is actively typing instead of flickering mid-word.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTextInput::OnKey (WPARAM vk)
{
    HRESULT  hr       = S_OK;
    bool     consumed = false;
    bool     shift    = IsShiftKeyDown();
    bool     ctrl     = IsControlKeyDown();
    bool     isActive = m_focused && m_enabled;



    BAIL_OUT_IF (!isActive, S_OK);

    switch (vk)
    {
        case VK_LEFT:
            if (ctrl)
            {
                m_caret = GetWordBoundary (m_caret, false);
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
            if (ctrl)
            {
                m_caret = GetWordBoundary (m_caret, true);
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
//  Draws the field: chrome, horizontal scroll, selection band, text,
//  placeholder, and the blinking caret.
//
//  Horizontal scroll is RECOMPUTED here rather than maintained by the edit
//  operations, because it depends on measured text width -- which only the
//  renderer knows, and only at paint time. The four clamps are applied in
//  order and each fixes the previous one's overshoot:
//
//    caret left of view   scroll to the caret
//    caret right of view  scroll so the caret sits at the right edge
//    text ends early      pull back so no blank gap trails the text
//    negative             clamp to zero for text shorter than the field
//
//  That is what keeps the caret visible while typing past the right edge and
//  stops the field scrolling into empty space after a delete.
//
//  The selection band is positioned by MEASURING the substring before it and
//  the substring itself, since the renderer reports no per-character
//  positions. The same technique places the caret.
//
//  Text is drawn at a negative offset inside a clip rect rather than being
//  truncated to what fits, so the glyph shaping is identical whether or not
//  the field is scrolled.
//
//  The placeholder is drawn only when the text is EMPTY, and is deliberately
//  not scroll-adjusted -- it has no caret to follow.
//
//  Blink timing comes from GetCaretBlinkTime, so the field matches the user's
//  system setting (including "no blink"), with a fallback only for an invalid
//  zero from the OS.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTextInput::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT      hr        = S_OK;
    float        x         = (float) m_boundsDip.left;
    float        y         = (float) m_boundsDip.top;
    float        w         = (float) (m_boundsDip.right  - m_boundsDip.left);
    float        h         = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float        padL      = m_scaler.ToPxf (s_kPadLeftDip);
    float        padR      = m_scaler.ToPxf (s_kPadRightDip);
    float        fontPx    = m_scaler.ToPxf (s_kFontDip);
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
    hr = text.MeasureString (caretPrefix.c_str(), fontPx, DxuiTheme::kBodyFace, caretX,    textMeasH);
    IGNORE_RETURN_VALUE (hr, S_OK);
    hr = text.MeasureString (m_text.c_str(),      fontPx, DxuiTheme::kBodyFace, fullTextW, textMeasH);
    IGNORE_RETURN_VALUE (hr, S_OK);

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

    hr = text.PushClipRect (x + padL, y, innerW, h);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (selStart != selEnd)
    {
        float bx = 0.0f;
        float sx = 0.0f;

        before.assign (m_text, 0, selStart);
        sel.assign    (m_text, selStart, selEnd - selStart);

        hr = text.MeasureString (before.c_str(), fontPx, DxuiTheme::kBodyFace, bx, textMeasH);
        IGNORE_RETURN_VALUE (hr, S_OK);
        hr = text.MeasureString (sel.c_str(),    fontPx, DxuiTheme::kBodyFace, sx, textMeasH);
        IGNORE_RETURN_VALUE (hr, S_OK);

        text.FillRect (x + padL + bx - m_scrollPx, y + 2.0f, sx, h - 4.0f, selArgb);
    }

    hr = text.DrawString (m_text.c_str(),
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
                          false);
    IGNORE_RETURN_VALUE (hr, S_OK);

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

    hr = text.PopClipRect();
    IGNORE_RETURN_VALUE (hr, S_OK);
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
//  GetWordBoundary
//
//  Ctrl+arrow target: the start of the previous / next word, where a word
//  is a run of alphanumerics or underscores.
//
////////////////////////////////////////////////////////////////////////////////

size_t DxuiTextInput::GetWordBoundary (size_t from, bool forward) const
{
    size_t  i = from;



    if (forward)
    {
        // To the start of the next word: leave this word, cross the gap.
        while (i < m_text.size() && IsWordChar (m_text[i]))  { i++; }
        while (i < m_text.size() && !IsWordChar (m_text[i])) { i++; }
    }
    else
    {
        // To the start of this (or the previous) word.
        while (i > 0 && !IsWordChar (m_text[i - 1])) { i--; }
        while (i > 0 && IsWordChar (m_text[i - 1]))  { i--; }
    }

    return i;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CaretFromX
//
//  Maps a click position to the caret index it should land on.
//
//  Every prefix is measured and the NEAREST boundary wins, rather than the
//  last one the click passed. That is what makes clicking on the right half of
//  a character place the caret after it -- the behavior every text field has,
//  and the one users rely on when clicking at the end of a word.
//
//  Prefix measurement is used because the renderer exposes no per-character
//  positions; this is the same technique Paint uses to place the caret and the
//  selection band, so click position and painted position agree by
//  construction rather than by two implementations happening to match.
//
//  The click is converted into TEXT space first -- minus the bounds, minus the
//  padding, plus the current scroll -- so it is correct in a scrolled field.
//
//  Left of the first glyph short-circuits to caret 0 with nothing measured,
//  which is both the common case for a click in an empty field and a guard
//  against a negative target.
//
////////////////////////////////////////////////////////////////////////////////

size_t DxuiTextInput::CaretFromX (IDxuiTextRenderer & text, int xPx) const
{
    HRESULT       hr       = S_OK;
    float         padL     = m_scaler.ToPxf (s_kPadLeftDip);
    float         fontPx   = m_scaler.ToPxf (s_kFontDip);
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
            float  dist = 0.0f;

            prefix.assign (m_text, 0, i);
            hr = text.MeasureString (prefix.c_str(), fontPx, DxuiTheme::kBodyFace, w, h);
            IGNORE_RETURN_VALUE (hr, S_OK);

            dist = std::abs (w - target);

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
    size_t  room = 0;
    size_t  take = 0;



    if (m_caret != m_anchor)
    {
        DeleteSelection();
    }

    room = (m_maxLen > m_text.size()) ? (m_maxLen - m_text.size()) : 0;
    take = std::min (ins.size(), room);


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
//  Copies the selection as CF_UNICODETEXT. An empty selection copies nothing
//  and, importantly, leaves the clipboard ALONE -- Ctrl+C with no selection
//  must not wipe whatever the user copied earlier.
//
//  ownsGlobal tracks who is responsible for the memory. The clipboard takes
//  ownership only when SetClipboardData succeeds: freeing after a successful
//  set corrupts the clipboard, and not freeing after a failed one leaks. The
//  flag is raised at allocation and lowered exactly on success, so the single
//  cleanup block does the right thing from every exit.
//
//  Failures are silent by design. Another application holding the clipboard
//  open is routine, and an error dialog for a failed Ctrl+C would be worse
//  than the failure.
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
//  Pastes CF_UNICODETEXT at the caret, replacing any selection.
//
//  The clipboard text is copied into a local string and the clipboard is
//  CLOSED before anything is inserted. Insertion fires the change callback,
//  which runs arbitrary caller code -- and running that while holding the
//  clipboard open would let a re-entrant copy deadlock against our own lock.
//
//  Only CF_UNICODETEXT is requested. Windows synthesizes it from ANSI text,
//  so asking for the wide format costs no compatibility and avoids a codepage
//  conversion here.
//
//  Length limiting is left to InsertText, so a paste that overflows is
//  truncated by the same rule that governs typing rather than by a second one
//  that could disagree.
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
    m_scaler.SetDpi (scaler.GetDpi());
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
//  DxuiTextInput::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers, which take plain coordinates and are therefore
//  testable without constructing framework events.
//
//  A move is routed by DRAG STATE, not by position. While a selection drag is
//  in progress it extends the selection and is claimed; otherwise it is only
//  a hover update and is reported unhandled, so a pointer passing over the
//  field does not swallow moves other widgets want.
//
//  Only the left button acts. A right-click belongs to the host's context
//  menu, if any.
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
