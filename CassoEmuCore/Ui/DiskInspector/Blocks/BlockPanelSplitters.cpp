#include "Pch.h"

#include "BlockPanelSplitters.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::PlaceColumns
//
//  The map's edge stays at least two of the tree's least shares right of
//  the tree's edge.
//
////////////////////////////////////////////////////////////////////////////////

int BlockPanelSplitters::PlaceColumns (const RECT & bounds, int & outMapX)
{
    int  width = (int) (bounds.right - bounds.left);



    m_treeShare = std::clamp (m_treeShare, kMinTree, kMaxTree);
    m_split     = std::clamp (m_split, std::max (kMinSplit, m_treeShare + kMinTree * 2), kMaxSplit);
    outMapX     = bounds.left + (int) (width * m_split);

    return bounds.left + (int) (width * m_treeShare);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::PlaceListEnd
//
////////////////////////////////////////////////////////////////////////////////

int BlockPanelSplitters::PlaceListEnd (const RECT & column)
{
    m_listShare = std::clamp (m_listShare, kMinList, kMaxList);

    return column.top + (int) ((column.bottom - column.top) * m_listShare);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::PlaceHexEnd
//
////////////////////////////////////////////////////////////////////////////////

int BlockPanelSplitters::PlaceHexEnd (const RECT & column)
{
    m_hexShare = std::clamp (m_hexShare, kMinHex, kMaxHex);

    return (int) column.left + (int) ((column.right - column.left) * m_hexShare);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::SetBarRect
//
////////////////////////////////////////////////////////////////////////////////

void BlockPanelSplitters::SetBarRect (Bar bar, const RECT & rect)
{
    m_bars[GetIndex (bar)] = rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::GetBarRect
//
////////////////////////////////////////////////////////////////////////////////

const RECT & BlockPanelSplitters::GetBarRect (Bar bar) const
{
    return m_bars[GetIndex (bar)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::FitHex
//
////////////////////////////////////////////////////////////////////////////////

void BlockPanelSplitters::FitHex (float share, float minShare)
{
    m_hexShare = std::clamp (share, std::max (kMinHex, minShare), kMaxHex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::OnMouse
//
//  A press is checked against the tree's bar, the hex view's, the list's
//  and the map's, in that order.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockPanelSplitters::OnMouse (const DxuiMouseEvent & ev, const RECT & bounds, const RECT & list, int footPx, bool & outIsMoved)
{
    POINT  p = ev.positionDip;



    outIsMoved = false;

    if (ev.kind == DxuiMouseEventKind::Down && ev.button == DxuiMouseButton::Left)
    {
        for (Bar bar : { Bar::Tree, Bar::Hex, Bar::List, Bar::Map })
        {
            if (PtInRect (&GetBarRect (bar), p))
            {
                m_dragged = bar;
                return true;
            }
        }

        return false;
    }

    if (m_dragged == Bar::None)
    {
        return false;
    }

    if (ev.kind == DxuiMouseEventKind::Up)
    {
        m_dragged = Bar::None;
        return true;
    }

    if (ev.kind != DxuiMouseEventKind::Move)
    {
        return false;
    }

    if (m_dragged == Bar::Tree && bounds.right > bounds.left)
    {
        m_treeShare = std::clamp ((float) (p.x - bounds.left) / (float) (bounds.right - bounds.left), kMinTree, kMaxTree);
        outIsMoved  = true;
    }
    else if (m_dragged == Bar::Map && bounds.right > bounds.left)
    {
        m_split    = std::clamp ((float) (p.x - bounds.left) / (float) (bounds.right - bounds.left), kMinSplit, kMaxSplit);
        outIsMoved = true;
    }
    else if (m_dragged == Bar::Hex && list.right > list.left)
    {
        m_hexShare = std::clamp ((float) (p.x - list.left) / (float) (list.right - list.left), kMinHex, kMaxHex);
        outIsMoved = true;
    }
    else if (m_dragged == Bar::List && footPx > list.top)
    {
        m_listShare = std::clamp ((float) (p.y - list.top) / (float) (footPx - list.top), kMinList, kMaxList);
        outIsMoved  = true;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::GetCursor
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockPanelSplitters::GetCursor (POINT pointPx) const
{
    bool  isList   = m_dragged == Bar::List || PtInRect (&GetBarRect (Bar::List), pointPx);
    bool  isColumn = (m_dragged != Bar::None && m_dragged != Bar::List)
                  || PtInRect (&GetBarRect (Bar::Tree), pointPx) || PtInRect (&GetBarRect (Bar::Map), pointPx) || PtInRect (&GetBarRect (Bar::Hex), pointPx);



    return isList ? IDC_SIZENS : isColumn ? IDC_SIZEWE : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockPanelSplitters::Paint
//
//  The tree's and the map's bars are filled. The list's and the hex view's
//  are a line through their middle. The bar being dragged is drawn in the
//  accent color.
//
////////////////////////////////////////////////////////////////////////////////

void BlockPanelSplitters::Paint (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    const RECT  & map  = GetBarRect (Bar::Map);
    const RECT  & tree = GetBarRect (Bar::Tree);
    const RECT  & list = GetBarRect (Bar::List);
    const RECT  & hex  = GetBarRect (Bar::Hex);
    auto          tint = [&] (Bar bar) { return m_dragged == bar ? theme.Accent() : theme.Divider(); };



    painter.FillRect ((float) map.left, (float) map.top, (float) (map.right - map.left), (float) (map.bottom - map.top), tint (Bar::Map));
    painter.FillRect ((float) tree.left, (float) tree.top, (float) (tree.right - tree.left), (float) (tree.bottom - tree.top), tint (Bar::Tree));
    painter.FillRect ((float) list.left, (float) (list.top + list.bottom) / 2.0f, (float) (list.right - list.left), 1.0f, tint (Bar::List));
    painter.FillRect ((float) (hex.left + hex.right) / 2.0f, (float) hex.top, 1.0f, (float) (hex.bottom - hex.top), tint (Bar::Hex));
}
