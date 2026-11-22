#pragma once

#include "Pch.h"
#include "DxuiTextLayout.h"
#include "DxuiTextViewInterfaces.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextSelection
//
//  A text view's selection by character: the anchor and the caret, the
//  character under a point, the clicks and drags that select, and the
//  selected text.
//
//  A selection runs from any character to any character, across cells and
//  rows. Positions count characters in a row's text with its cells joined
//  by tabs, so a copy has a tab between cells and a line break between rows.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextSelection : public IDxuiTextSelectionRange
{
public:
    using Position = DxuiTextLayout::Position;

    DxuiTextSelection (const DxuiTextLayout & layout, const IDxuiTextGeometry & geometry, IDxuiTextViewport & viewport)
        : m_layout (layout), m_geometry (geometry), m_viewport (viewport) {}

    Position      HitTest          (POINT point) const;
    void          Select           (Position anchor, Position caret);
    void          SelectAll        ();
    void          ClearSelection   ();
    void          Reset            ();
    bool          HasSelection     () const override { return !(m_anchor == m_caret); }
    Position      GetFrom          () const override { return (std::min) (m_anchor, m_caret); }
    Position      GetTo            () const override { return (std::max) (m_anchor, m_caret); }
    std::wstring  GetSelectionText () const;
    void          CopySelection    (HWND hwnd) const;
    bool          IsDragging       () const { return m_dragging; }

    //  A left press on the text, the drag that follows, and its release.
    //  OnDrag and OnRelease return whether a drag is under way.
    void  OnPress   (POINT point, bool shift, int64_t nowMs);
    bool  OnDrag    (POINT point);
    bool  OnRelease ();

private:
    //  Word characters, spaces and tabs are three kinds; a run of one kind is
    //  what a double-click selects.
    static int  GetCharClass  (wchar_t ch) { return (ch == L'\t') ? 2 : (ch == L' ') ? 1 : 0; }
    void        SelectWordAt  (Position pos);
    void        GetWordBounds (Position pos, Position & outFirst, Position & outLast) const;

    const DxuiTextLayout     & m_layout;
    const IDxuiTextGeometry  & m_geometry;
    IDxuiTextViewport        & m_viewport;
    Position                   m_anchor;
    Position                   m_caret;
    Position                   m_lastClick;
    int64_t                    m_lastClickMs = 0;
    int                        m_clicks      = 0;   // the clicks in a row on one spot, each within the double-click time
    bool                       m_dragging    = false;
    bool                       m_wordDrag    = false;
    Position                   m_wordFirst;
    Position                   m_wordLast;
};
