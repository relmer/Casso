#include "Pch.h"

#include "DxuiPaneLayoutArranger.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::Arrange
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneLayout::GroupRect> DxuiPaneLayoutArranger::Arrange (
    const Node  * root,
    const RECT  & areaDip) const
{
    std::vector<GroupRect>  groups;



    ArrangeNode (root, areaDip, groups);
    return groups;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::ArrangeSplits
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneLayout::SplitRect> DxuiPaneLayoutArranger::ArrangeSplits (
    const Node  * root,
    const RECT  & areaDip) const
{
    std::vector<SplitRect>  splits;



    ArrangeSplitsIn (root, areaDip, std::wstring(), splits);
    return splits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::HasShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayoutArranger::HasShown (const Node * node) const
{
    if (node == nullptr)
    {
        return false;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        return std::any_of (node->panes.begin(), node->panes.end(),
                            [this] (const std::wstring & pane) { return IsShown (pane); });
    }

    return HasShown (node->first.get()) || HasShown (node->second.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::GetMinimum
//
//  What a subtree needs: a group, the largest of its shown panes; a split,
//  the two sides added along its axis and the larger across it.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiPaneLayoutArranger::GetMinimum (const Node * node) const
{
    SIZE  result = {};
    SIZE  a      = {};
    SIZE  b      = {};



    if (!HasShown (node))
    {
        return result;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        for (const std::wstring & pane : node->panes)
        {
            SIZE  one = (m_minSize && IsShown (pane)) ? m_minSize (pane) : SIZE {};

            result.cx = std::max (result.cx, one.cx);
            result.cy = std::max (result.cy, one.cy);
        }

        return result;
    }

    a = GetMinimum (node->first.get());
    b = GetMinimum (node->second.get());

    result.cx = node->horizontal ? a.cx + b.cx : std::max (a.cx, b.cx);
    result.cy = node->horizontal ? std::max (a.cy, b.cy) : a.cy + b.cy;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::ArrangeNode
//
//  The ratio places the split, then the split moves as little as it must for
//  each side to get its minimum. When the area cannot hold both minimums,
//  they share it in proportion to what each needs.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayoutArranger::ArrangeNode (
    const Node              * node,
    const RECT              & area,
    std::vector<GroupRect>  & out) const
{
    GroupRect  group;
    bool       firstShown  = false;
    bool       secondShown = false;
    long       split       = 0;
    RECT       a           = area;
    RECT       b           = area;



    if (!HasShown (node))
    {
        return;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        for (const std::wstring & pane : node->panes)
        {
            if (IsShown (pane))
            {
                group.panes.push_back (pane);
            }
        }

        group.active = group.panes.front();

        //  When the active pane is hidden, as one just closed is, the front
        //  goes to the next visible pane after it in the group, or to the one
        //  before it when it was the last.
        for (int at = node->active, step = 1; node->active >= 0 && node->active < (int) node->panes.size(); at += step)
        {
            if (at >= (int) node->panes.size())
            {
                at   = node->active;
                step = -1;
                continue;
            }

            if (at < 0)
            {
                break;
            }

            if (IsShown (node->panes[(size_t) at]))
            {
                group.active = node->panes[(size_t) at];
                break;
            }
        }

        group.rect = area;
        out.push_back (std::move (group));
        return;
    }

    firstShown  = HasShown (node->first.get());
    secondShown = HasShown (node->second.get());

    if (!firstShown || !secondShown)
    {
        ArrangeNode (firstShown ? node->first.get() : node->second.get(), area, out);
        return;
    }

    split = GetSplitPosition (node, area);

    if (node->horizontal)
    {
        a.right = area.left + split;
        b.left  = a.right;
    }
    else
    {
        a.bottom = area.top + split;
        b.top    = a.bottom;
    }

    ArrangeNode (node->first.get(),  a, out);
    ArrangeNode (node->second.get(), b, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::GetSplitPosition
//
//  How far along its axis a split divides its area. The ratio places the
//  division, then it moves as little as it must for each side to get its
//  minimum; when the area cannot hold both minimums, they share it in
//  proportion to what each needs.
//
////////////////////////////////////////////////////////////////////////////////

long DxuiPaneLayoutArranger::GetSplitPosition (
    const Node  * node,
    const RECT  & area) const
{
    long  total = node->horizontal ? (area.right - area.left) : (area.bottom - area.top);
    SIZE  a     = GetMinimum (node->first.get());
    SIZE  b     = GetMinimum (node->second.get());
    long  needA = node->horizontal ? a.cx : a.cy;
    long  needB = node->horizontal ? b.cx : b.cy;
    long  split = (long) std::lround (total * node->ratio);



    if (needA + needB <= total)
    {
        split = std::clamp (split, needA, total - needB);
    }
    else if (needA + needB > 0)
    {
        split = total * needA / (needA + needB);
    }

    return split;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayoutArranger::ArrangeSplitsIn
//
//  The same walk as ArrangeNode, recording each split both of whose sides are
//  shown. A split with one side hidden gives its whole area to the other and
//  has nothing to drag.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayoutArranger::ArrangeSplitsIn (
    const Node              * node,
    const RECT              & area,
    const std::wstring      & path,
    std::vector<SplitRect>  & out) const
{
    SplitRect  split;
    bool       firstShown  = false;
    bool       secondShown = false;
    RECT       a           = area;
    RECT       b           = area;



    if (node == nullptr || node->kind != Node::Kind::Split || !HasShown (node))
    {
        return;
    }

    firstShown  = HasShown (node->first.get());
    secondShown = HasShown (node->second.get());

    if (!firstShown || !secondShown)
    {
        ArrangeSplitsIn (firstShown ? node->first.get() : node->second.get(), area, path + (firstShown ? L"0" : L"1"), out);
        return;
    }

    split.path       = path;
    split.horizontal = node->horizontal;
    split.area       = area;
    split.position   = (node->horizontal ? area.left : area.top) + GetSplitPosition (node, area);
    out.push_back (split);

    if (node->horizontal)
    {
        a.right = split.position;
        b.left  = split.position;
    }
    else
    {
        a.bottom = split.position;
        b.top    = split.position;
    }

    ArrangeSplitsIn (node->first.get(),  a, path + L"0", out);
    ArrangeSplitsIn (node->second.get(), b, path + L"1", out);
}
