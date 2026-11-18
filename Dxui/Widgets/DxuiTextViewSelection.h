#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"


class IDxuiTextSelectionHost;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewSelection
//
//  A text view's selection, from any character to any character, across
//  cells and rows; its text, with a tab between cells and a line break
//  between rows; and the run of characters a double-click takes.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewSelection
{
public:
    using Position = DxuiTextPosition;
    using ClampFn  = std::function<void (Position & at)>;

    explicit DxuiTextViewSelection (const IDxuiTextSelectionHost & host) : m_host (host) {}

    Position      GetAnchor     () const { return m_anchor; }
    Position      GetCaret      () const { return m_caret; }
    bool          HasSelection  () const { return !(m_anchor == m_caret); }

    void          Select        (Position anchor, Position caret) { m_anchor = anchor; m_caret = caret; }
    void          SetCaret      (Position caret)                  { m_caret = caret; }
    void          SelectAll     ();
    void          Clear         ()                                { m_anchor = m_caret; }
    void          SelectWordAt  (Position pos)                    { GetWordBounds (pos, m_anchor, m_caret); }
    void          GetWordBounds (Position pos, Position & outFirst, Position & outLast) const;
    std::wstring  GetText       () const;
    void          Copy          (HWND hwnd) const;

    //  The rows replaced, the same rows changed, and rows dropped from the
    //  front of the text.
    void          Reset         () { m_anchor = Position(); m_caret = Position(); }
    void          ClampTo       (const ClampFn & clamp) { clamp (m_anchor); clamp (m_caret); }
    void          DropFront     (int dropped);

private:
    //  Word characters, spaces and tabs are three kinds; a run of one kind is
    //  what a double-click selects.
    static int  GetCharClass (wchar_t ch) { return (ch == L'\t') ? 2 : (ch == L' ') ? 1 : 0; }

    const IDxuiTextSelectionHost  & m_host;
    Position                        m_anchor;
    Position                        m_caret;
};
