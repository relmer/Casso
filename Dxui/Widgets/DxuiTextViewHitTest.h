#pragma once

#include "Pch.h"
#include "DxuiTextPosition.h"
#include "DxuiTextRow.h"
#include "DxuiTextViewport.h"


class DxuiTextLayout;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest
//
//  What lies under a point in a text view: the nearest character edge, as a
//  selection takes it, the character itself, and the link. Made for each
//  query, with the view's layout as it is then. A point over a scroll bar
//  is ruled out by the view.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewHitTest
{
public:
    using Row      = DxuiTextRow;
    using Line     = DxuiTextLine;
    using Span     = DxuiTextSpan;
    using Position = DxuiTextPosition;

    DxuiTextViewHitTest (const DxuiTextLayout & layout, const DxuiTextViewport & viewport) : m_layout (layout), m_viewport (viewport) {}

    Position  HitTest      (POINT point) const;
    bool      TryGetCharAt (POINT point, int & outLine, int & outFlat, int & outColumn) const;
    bool      TryGetLinkAt (POINT point, int & outRow, int & outSpan, RECT & outRect) const;

private:
    int  GetLineOffsetPx (int lineIndex) const;

    const DxuiTextLayout  & m_layout;
    DxuiTextViewport        m_viewport;
};
