#pragma once

#include "Pch.h"
#include "IDxuiTextLayoutSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewHitTest
//
//  What lies under a point in a text view: the nearest character edge, as a
//  selection takes it, the character itself, and the link. Made for each
//  query, with the view's layout as it is then.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextViewHitTest
{
public:
    using Row      = DxuiTextRow;
    using Line     = DxuiTextLine;
    using Span     = DxuiTextSpan;
    using Position = DxuiTextPosition;

    explicit DxuiTextViewHitTest (const IDxuiTextLayoutSource & view) : m_view (view), m_look (view.GetLayoutLook()) {}

    Position  HitTest      (POINT point) const;
    bool      TryGetCharAt (POINT point, int & outLine, int & outFlat, int & outColumn) const;
    bool      TryGetLinkAt (POINT point, int & outRow, int & outSpan, RECT & outRect) const;

private:
    const IDxuiTextLayoutSource  & m_view;
    IDxuiTextLayoutSource::Look    m_look;
};
