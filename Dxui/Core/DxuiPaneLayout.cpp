#include "Pch.h"

#include "DxuiPaneLayout.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::MakeSingle
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayout DxuiPaneLayout::MakeSingle (const std::wstring & pane)
{
    DxuiPaneLayout  layout;



    layout.m_root = MakeTabs (pane);
    return layout;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::CopyFrom
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::CopyFrom (const DxuiPaneLayout & other)
{
    if (&other == this)
    {
        return;
    }

    m_root       = CloneNode (other.m_root.get());
    m_floating   = other.m_floating;
    m_autoHidden = other.m_autoHidden;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::CloneNode
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<DxuiPaneLayoutNode> DxuiPaneLayout::CloneNode (const Node * node)
{
    std::unique_ptr<Node>  copy;



    if (node == nullptr)
    {
        return copy;
    }

    copy             = std::make_unique<Node>();
    copy->kind       = node->kind;
    copy->horizontal = node->horizontal;
    copy->ratio      = node->ratio;
    copy->panes      = node->panes;
    copy->active     = node->active;
    copy->first      = CloneNode (node->first.get());
    copy->second     = CloneNode (node->second.get());

    return copy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::MakeTabs
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<DxuiPaneLayoutNode> DxuiPaneLayout::MakeTabs (const std::wstring & pane)
{
    std::unique_ptr<Node>  node = std::make_unique<Node>();



    node->kind  = Node::Kind::Tabs;
    node->panes = { pane };
    return node;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::FindGroup
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneLayoutNode * DxuiPaneLayout::FindGroup (Node * node, const std::wstring & pane)
{
    Node *  found = nullptr;



    if (node == nullptr)
    {
        return nullptr;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        return (std::find (node->panes.begin(), node->panes.end(), pane) != node->panes.end()) ? node : nullptr;
    }

    found = FindGroup (node->first.get(), pane);
    return (found != nullptr) ? found : FindGroup (node->second.get(), pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::IsDocked
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::IsDocked (const std::wstring & pane) const
{
    return FindGroup (m_root.get(), pane) != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::IsFloating
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::IsFloating (const std::wstring & pane) const
{
    return std::any_of (m_floating.begin(), m_floating.end(), [&] (const Floating & f) { return f.pane == pane; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::IsAutoHidden
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::IsAutoHidden (const std::wstring & pane) const
{
    return std::any_of (m_autoHidden.begin(), m_autoHidden.end(), [&] (const AutoHidden & h) { return h.pane == pane; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Contains
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::Contains (const std::wstring & pane) const
{
    return IsDocked (pane) || IsFloating (pane) || IsAutoHidden (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::GetGroup
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DxuiPaneLayout::GetGroup (const std::wstring & pane) const
{
    const Node *  group = FindGroup (m_root.get(), pane);



    return (group != nullptr) ? group->panes : std::vector<std::wstring>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::RemoveFrom
//
//  Takes a pane out of the tree under `slot`. A group left empty goes, and a
//  split left with one side is replaced by that side.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::RemoveFrom (std::unique_ptr<Node> & slot, const std::wstring & pane)
{
    bool  removed = false;



    if (slot == nullptr)
    {
        return false;
    }

    if (slot->kind == Node::Kind::Tabs)
    {
        auto  found = std::find (slot->panes.begin(), slot->panes.end(), pane);

        if (found == slot->panes.end())
        {
            return false;
        }

        slot->panes.erase (found);
        slot->active = std::clamp (slot->active, 0, std::max (0, (int) slot->panes.size() - 1));

        if (slot->panes.empty())
        {
            slot.reset();
        }

        return true;
    }

    removed = RemoveFrom (slot->first, pane) || RemoveFrom (slot->second, pane);

    if (slot->first == nullptr)
    {
        slot = std::move (slot->second);
    }
    else if (slot->second == nullptr)
    {
        slot = std::move (slot->first);
    }

    return removed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Detach
//
//  Out of wherever the pane is: the tree, the floating list, the edges.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::Detach (const std::wstring & pane)
{
    bool    removed = RemoveFrom (m_root, pane);
    size_t  before  = m_floating.size() + m_autoHidden.size();



    std::erase_if (m_floating,   [&] (const Floating & f)   { return f.pane == pane; });
    std::erase_if (m_autoHidden, [&] (const AutoHidden & h) { return h.pane == pane; });

    return removed || (m_floating.size() + m_autoHidden.size()) != before;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Split
//
//  The group becomes a split of itself and the added node, the added node on
//  the given side.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::Split (Node & group, std::unique_ptr<Node> added, DxuiDockSide side)
{
    std::unique_ptr<Node>  existing = std::make_unique<Node> (std::move (group));
    bool                   isFirst  = (side == DxuiDockSide::Left || side == DxuiDockSide::Top);



    group.kind       = Node::Kind::Split;
    group.panes.clear();
    group.active     = 0;
    group.horizontal = (side == DxuiDockSide::Left || side == DxuiDockSide::Right);
    group.ratio      = 0.5f;
    group.first      = isFirst ? std::move (added)    : std::move (existing);
    group.second     = isFirst ? std::move (existing) : std::move (added);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::GetNeighbor
//
//  The pane a pane sits beside: another in its group, else the one next to
//  it in the tree. Where it goes back to when it docks again.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiPaneLayout::GetNeighbor (const std::wstring & pane) const
{
    std::vector<std::wstring>  order;
    std::function<void (const Node *)>  walk = [&] (const Node * node)
    {
        if (node == nullptr)
        {
            return;
        }

        if (node->kind == Node::Kind::Tabs)
        {
            order.insert (order.end(), node->panes.begin(), node->panes.end());
            return;
        }

        walk (node->first.get());
        walk (node->second.get());
    };
    const Node *  group = FindGroup (m_root.get(), pane);



    if (group != nullptr)
    {
        for (const std::wstring & other : group->panes)
        {
            if (other != pane)
            {
                return other;
            }
        }
    }

    walk (m_root.get());

    for (size_t i = 0; i < order.size(); i++)
    {
        if (order[i] == pane)
        {
            return (i + 1 < order.size()) ? order[i + 1] : ((i > 0) ? order[i - 1] : std::wstring());
        }
    }

    return std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::DockToSide
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::DockToSide (const std::wstring & pane, const std::wstring & target, DxuiDockSide side)
{
    Node *  group = nullptr;



    if (pane == target || !Contains (pane) || !IsDocked (target))
    {
        return false;
    }

    Detach (pane);
    group = FindGroup (m_root.get(), target);
    Split (*group, MakeTabs (pane), side);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::DockToEdge
//
//  Against the window's own edge: a split of the whole tree. A pane already
//  alone against that edge stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::DockToEdge (const std::wstring & pane, DxuiDockSide side)
{
    bool          isFirst    = (side == DxuiDockSide::Left || side == DxuiDockSide::Top);
    bool          horizontal = (side == DxuiDockSide::Left || side == DxuiDockSide::Right);
    const Node *  edge       = nullptr;



    if (!Contains (pane))
    {
        return false;
    }

    if (m_root != nullptr && m_root->kind == Node::Kind::Split && m_root->horizontal == horizontal)
    {
        edge = isFirst ? m_root->first.get() : m_root->second.get();

        if (edge->kind == Node::Kind::Tabs && edge->panes.size() == 1 && edge->panes[0] == pane)
        {
            return false;
        }
    }

    if (m_root != nullptr && m_root->kind == Node::Kind::Tabs && m_root->panes.size() == 1 && m_root->panes[0] == pane)
    {
        return false;
    }

    Detach (pane);

    if (m_root == nullptr)
    {
        m_root = MakeTabs (pane);
        return true;
    }

    Split (*m_root, MakeTabs (pane), side);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::TabWith
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::TabWith (const std::wstring & pane, const std::wstring & target)
{
    Node *  group = nullptr;



    if (pane == target || !Contains (pane) || !IsDocked (target) || FindGroup (m_root.get(), target) == FindGroup (m_root.get(), pane))
    {
        return false;
    }

    Detach (pane);
    group = FindGroup (m_root.get(), target);
    group->panes.push_back (pane);
    group->active = (int) group->panes.size() - 1;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Float
//
//  Into a window of its own. A floating pane moved elsewhere keeps the home
//  it had when it first left the tree.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::Float (const std::wstring & pane, const std::wstring & monitorKey, const RECT & rectDip)
{
    Floating  entry;



    if (!Contains (pane))
    {
        return false;
    }

    for (Floating & existing : m_floating)
    {
        if (existing.pane == pane)
        {
            existing.monitorKey = monitorKey;
            existing.rectDip    = rectDip;
            return true;
        }
    }

    entry.pane       = pane;
    entry.monitorKey = monitorKey;
    entry.rectDip    = rectDip;
    entry.homePane   = IsDocked (pane) ? GetNeighbor (pane) : std::wstring();

    for (const AutoHidden & hidden : m_autoHidden)
    {
        entry.homePane = (hidden.pane == pane) ? hidden.homePane : entry.homePane;
    }

    Detach (pane);
    m_floating.push_back (entry);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::AutoHide
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::AutoHide (const std::wstring & pane, DxuiDockSide edge)
{
    AutoHidden  entry;



    if (!Contains (pane) || IsAutoHidden (pane))
    {
        return false;
    }

    entry.pane     = pane;
    entry.edge     = edge;
    entry.homePane = IsDocked (pane) ? GetNeighbor (pane) : std::wstring();

    for (const Floating & floating : m_floating)
    {
        entry.homePane = (floating.pane == pane) ? floating.homePane : entry.homePane;
    }

    Detach (pane);
    m_autoHidden.push_back (entry);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::DockBack
//
//  A floating or auto-hidden pane returns to the pane it sat beside, tabbed
//  with it; when that pane is not docked any more, against the right edge.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::DockBack (const std::wstring & pane)
{
    std::wstring  home;
    Node *        group = nullptr;



    if (!IsFloating (pane) && !IsAutoHidden (pane))
    {
        return false;
    }

    for (const Floating & floating : m_floating)
    {
        home = (floating.pane == pane) ? floating.homePane : home;
    }

    for (const AutoHidden & hidden : m_autoHidden)
    {
        home = (hidden.pane == pane) ? hidden.homePane : home;
    }

    Detach (pane);
    group = home.empty() ? nullptr : FindGroup (m_root.get(), home);

    if (group != nullptr)
    {
        group->panes.push_back (pane);
        group->active = (int) group->panes.size() - 1;
    }
    else if (m_root == nullptr)
    {
        m_root = MakeTabs (pane);
    }
    else
    {
        Split (*m_root, MakeTabs (pane), DxuiDockSide::Right);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Activate
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::Activate (const std::wstring & pane)
{
    Node *  group = FindGroup (m_root.get(), pane);
    int     index = 0;



    if (group == nullptr)
    {
        return false;
    }

    index = (int) (std::find (group->panes.begin(), group->panes.end(), pane) - group->panes.begin());

    if (index == group->active)
    {
        return false;
    }

    group->active = index;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Close
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::Close (const std::wstring & pane)
{
    return Detach (pane);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Add
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::Add (const std::wstring & pane, const std::wstring & target)
{
    Node *  group = nullptr;



    if (pane.empty() || Contains (pane))
    {
        return false;
    }

    group = FindGroup (m_root.get(), target);

    if (group != nullptr)
    {
        group->panes.push_back (pane);
        group->active = (int) group->panes.size() - 1;
    }
    else if (m_root == nullptr)
    {
        m_root = MakeTabs (pane);
    }
    else
    {
        Split (*m_root, MakeTabs (pane), DxuiDockSide::Right);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::MoveByArrow
//
//  The nearest group whose area lies wholly in that direction from the pane's
//  own, measured between centers; the pane is tabbed into it. With none, the
//  pane goes against the window's edge on that side.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::MoveByArrow (const std::wstring & pane, DxuiDockSide direction, const RECT & areaDip,
                                  const ShownFn & shown, const MinSizeFn & minSize)
{
    std::vector<GroupRect>  groups  = Arrange (areaDip, shown, minSize);
    const GroupRect       * mine    = nullptr;
    const GroupRect       * nearest = nullptr;
    long                    best    = LONG_MAX;



    if (!IsDocked (pane))
    {
        return false;
    }

    for (const GroupRect & group : groups)
    {
        mine = (std::find (group.panes.begin(), group.panes.end(), pane) != group.panes.end()) ? &group : mine;
    }

    for (const GroupRect & group : groups)
    {
        long  dx      = 0;
        long  dy      = 0;
        bool  inPlace = false;

        if (mine == nullptr || &group == mine)
        {
            continue;
        }

        inPlace = (direction == DxuiDockSide::Left   && group.rect.right  <= mine->rect.left)   ||
                  (direction == DxuiDockSide::Right  && group.rect.left   >= mine->rect.right)  ||
                  (direction == DxuiDockSide::Top    && group.rect.bottom <= mine->rect.top)    ||
                  (direction == DxuiDockSide::Bottom && group.rect.top    >= mine->rect.bottom);

        if (!inPlace)
        {
            continue;
        }

        dx = ((group.rect.left + group.rect.right) - (mine->rect.left + mine->rect.right)) / 2;
        dy = ((group.rect.top + group.rect.bottom) - (mine->rect.top + mine->rect.bottom)) / 2;

        if (dx * dx + dy * dy < best)
        {
            best    = dx * dx + dy * dy;
            nearest = &group;
        }
    }

    if (nearest != nullptr)
    {
        return TabWith (pane, nearest->active);
    }

    return DockToEdge (pane, direction);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::HasShown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::HasShown (const Node * node, const ShownFn & shown)
{
    if (node == nullptr)
    {
        return false;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        return std::any_of (node->panes.begin(), node->panes.end(),
                            [&] (const std::wstring & pane) { return !shown || shown (pane); });
    }

    return HasShown (node->first.get(), shown) || HasShown (node->second.get(), shown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::GetMinimum
//
//  What a subtree needs: a group, the largest of its shown panes; a split,
//  the two sides added along its axis and the larger across it.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiPaneLayout::GetMinimum (const Node * node, const ShownFn & shown, const MinSizeFn & minSize)
{
    SIZE  result = {};
    SIZE  a      = {};
    SIZE  b      = {};



    if (!HasShown (node, shown))
    {
        return result;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        for (const std::wstring & pane : node->panes)
        {
            SIZE  one = (minSize && (!shown || shown (pane))) ? minSize (pane) : SIZE {};

            result.cx = std::max (result.cx, one.cx);
            result.cy = std::max (result.cy, one.cy);
        }

        return result;
    }

    a = GetMinimum (node->first.get(),  shown, minSize);
    b = GetMinimum (node->second.get(), shown, minSize);

    result.cx = node->horizontal ? a.cx + b.cx : std::max (a.cx, b.cx);
    result.cy = node->horizontal ? std::max (a.cy, b.cy) : a.cy + b.cy;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Arrange
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneLayout::GroupRect> DxuiPaneLayout::Arrange (const RECT & areaDip, const ShownFn & shown,
                                                                const MinSizeFn & minSize) const
{
    std::vector<GroupRect>  groups;



    ArrangeNode (m_root.get(), areaDip, shown, minSize, groups);
    return groups;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::ArrangeNode
//
//  The ratio places the split, then the split moves as little as it must for
//  each side to get its minimum. When the area cannot hold both minimums,
//  they share it in proportion to what each needs.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::ArrangeNode (const Node * node, const RECT & area, const ShownFn & shown,
                                  const MinSizeFn & minSize, std::vector<GroupRect> & out)
{
    GroupRect  group;
    bool       firstShown  = false;
    bool       secondShown = false;
    long       split       = 0;
    RECT       a           = area;
    RECT       b           = area;



    if (!HasShown (node, shown))
    {
        return;
    }

    if (node->kind == Node::Kind::Tabs)
    {
        for (const std::wstring & pane : node->panes)
        {
            if (!shown || shown (pane))
            {
                group.panes.push_back (pane);
            }
        }

        group.active = group.panes.front();

        if (node->active >= 0 && node->active < (int) node->panes.size() && (!shown || shown (node->panes[(size_t) node->active])))
        {
            group.active = node->panes[(size_t) node->active];
        }

        group.rect = area;
        out.push_back (std::move (group));
        return;
    }

    firstShown  = HasShown (node->first.get(),  shown);
    secondShown = HasShown (node->second.get(), shown);

    if (!firstShown || !secondShown)
    {
        ArrangeNode (firstShown ? node->first.get() : node->second.get(), area, shown, minSize, out);
        return;
    }

    split = GetSplitPosition (node, area, shown, minSize);

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

    ArrangeNode (node->first.get(),  a, shown, minSize, out);
    ArrangeNode (node->second.get(), b, shown, minSize, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::GetSplitPosition
//
//  How far along its axis a split divides its area. The ratio places the
//  division, then it moves as little as it must for each side to get its
//  minimum; when the area cannot hold both minimums, they share it in
//  proportion to what each needs.
//
////////////////////////////////////////////////////////////////////////////////

long DxuiPaneLayout::GetSplitPosition (const Node * node, const RECT & area, const ShownFn & shown, const MinSizeFn & minSize)
{
    long  total = node->horizontal ? (area.right - area.left) : (area.bottom - area.top);
    SIZE  a     = GetMinimum (node->first.get(),  shown, minSize);
    SIZE  b     = GetMinimum (node->second.get(), shown, minSize);
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
//  DxuiPaneLayout::ArrangeSplits
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneLayout::SplitRect> DxuiPaneLayout::ArrangeSplits (const RECT & areaDip, const ShownFn & shown,
                                                                      const MinSizeFn & minSize) const
{
    std::vector<SplitRect>  splits;



    ArrangeSplitsIn (m_root.get(), areaDip, shown, minSize, std::wstring(), splits);
    return splits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::ArrangeSplitsIn
//
//  The same walk as ArrangeNode, recording each split both of whose sides are
//  shown. A split with one side hidden gives its whole area to the other and
//  has nothing to drag.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::ArrangeSplitsIn (const Node * node, const RECT & area, const ShownFn & shown,
                                      const MinSizeFn & minSize, const std::wstring & path,
                                      std::vector<SplitRect> & out)
{
    SplitRect  split;
    bool       firstShown  = false;
    bool       secondShown = false;
    RECT       a           = area;
    RECT       b           = area;



    if (node == nullptr || node->kind != Node::Kind::Split || !HasShown (node, shown))
    {
        return;
    }

    firstShown  = HasShown (node->first.get(),  shown);
    secondShown = HasShown (node->second.get(), shown);

    if (!firstShown || !secondShown)
    {
        ArrangeSplitsIn (firstShown ? node->first.get() : node->second.get(), area, shown, minSize,
                         path + (firstShown ? L"0" : L"1"), out);
        return;
    }

    split.path       = path;
    split.horizontal = node->horizontal;
    split.area       = area;
    split.position   = (node->horizontal ? area.left : area.top) + GetSplitPosition (node, area, shown, minSize);
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

    ArrangeSplitsIn (node->first.get(),  a, shown, minSize, path + L"0", out);
    ArrangeSplitsIn (node->second.get(), b, shown, minSize, path + L"1", out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::SetRatio
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::SetRatio (const std::wstring & path, float ratio)
{
    Node  * node = m_root.get();



    for (wchar_t step : path)
    {
        if (node == nullptr || node->kind != Node::Kind::Split)
        {
            return false;
        }

        node = (step == L'0') ? node->first.get() : node->second.get();
    }

    if (node == nullptr || node->kind != Node::Kind::Split)
    {
        return false;
    }

    node->ratio = std::clamp (ratio, 0.05f, 0.95f);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::DropUnknown
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::DropUnknown (const std::function<bool (const std::wstring & pane)> & isKnown)
{
    DropUnknownIn (m_root, isKnown);

    std::erase_if (m_floating,   [&] (const Floating & f)   { return !isKnown (f.pane); });
    std::erase_if (m_autoHidden, [&] (const AutoHidden & h) { return !isKnown (h.pane); });

    for (Floating & floating : m_floating)
    {
        floating.homePane = isKnown (floating.homePane) ? floating.homePane : std::wstring();
    }

    for (AutoHidden & hidden : m_autoHidden)
    {
        hidden.homePane = isKnown (hidden.homePane) ? hidden.homePane : std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::DropUnknownIn
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::DropUnknownIn (std::unique_ptr<Node> & slot, const std::function<bool (const std::wstring & pane)> & isKnown)
{
    std::vector<std::wstring>  unknown;



    if (slot == nullptr)
    {
        return;
    }

    if (slot->kind == Node::Kind::Tabs)
    {
        for (const std::wstring & pane : slot->panes)
        {
            if (!isKnown (pane))
            {
                unknown.push_back (pane);
            }
        }

        for (const std::wstring & pane : unknown)
        {
            RemoveFrom (slot, pane);
        }

        return;
    }

    DropUnknownIn (slot->first,  isKnown);
    DropUnknownIn (slot->second, isKnown);

    if (slot->first == nullptr)
    {
        slot = std::move (slot->second);
    }
    else if (slot->second == nullptr)
    {
        slot = std::move (slot->first);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::PlaceOnMonitors
//
//  A floating pane whose monitor is gone opens at the primary's work area's
//  top-left corner at the size it was saved at (FR-044).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::PlaceOnMonitors (const std::vector<Monitor> & monitors)
{
    const Monitor  * primary = nullptr;



    if (monitors.empty())
    {
        return;
    }

    primary = &monitors.front();

    for (const Monitor & monitor : monitors)
    {
        primary = monitor.isPrimary ? &monitor : primary;
    }

    for (Floating & floating : m_floating)
    {
        bool  present = std::any_of (monitors.begin(), monitors.end(),
                                     [&] (const Monitor & m) { return m.key == floating.monitorKey; });
        long  width   = floating.rectDip.right - floating.rectDip.left;
        long  height  = floating.rectDip.bottom - floating.rectDip.top;

        if (present)
        {
            continue;
        }

        floating.monitorKey = primary->key;
        floating.rectDip    = { primary->workDip.left, primary->workDip.top,
                                primary->workDip.left + width, primary->workDip.top + height };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::GetSideName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiPaneLayout::GetSideName (DxuiDockSide side)
{
    static const wchar_t * const  kNames[] = { L"left", L"top", L"right", L"bottom" };



    return kNames[(int) side];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::TryGetSide
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::TryGetSide (const std::wstring & name, DxuiDockSide & side)
{
    for (int i = 0; i < 4; i++)
    {
        if (name == GetSideName ((DxuiDockSide) i))
        {
            side = (DxuiDockSide) i;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::ToText
//
//  `dxui-layout 1`, then `tree` and the tree as nested lists, then a `float`
//  line per floating pane and a `hide` line per auto-hidden one. Names are
//  quoted, with `\` before a quote or a backslash in them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiPaneLayout::ToText() const
{
    std::wstring  out   = std::format (L"dxui-layout {}\ntree ", kVersion);
    auto          quote = [] (const std::wstring & text)
    {
        std::wstring  q = L"\"";

        for (wchar_t c : text)
        {
            q += (c == L'"' || c == L'\\') ? std::wstring (L"\\") + c : std::wstring (1, c);
        }

        return q + L"\"";
    };



    if (m_root == nullptr)
    {
        out += L"none";
    }
    else
    {
        WriteNode (m_root.get(), out);
    }

    out += L"\n";

    for (const Floating & f : m_floating)
    {
        out += std::format (L"float {} {} {} {} {} {} {}\n", quote (f.pane), quote (f.monitorKey),
                            f.rectDip.left, f.rectDip.top, f.rectDip.right, f.rectDip.bottom, quote (f.homePane));
    }

    for (const AutoHidden & h : m_autoHidden)
    {
        out += std::format (L"hide {} {} {}\n", quote (h.pane), GetSideName (h.edge), quote (h.homePane));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::WriteNode
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneLayout::WriteNode (const Node * node, std::wstring & out)
{
    if (node->kind == Node::Kind::Split)
    {
        out += std::format (L"(split {} {:.4f} ", node->horizontal ? L"h" : L"v", node->ratio);
        WriteNode (node->first.get(), out);
        out += L" ";
        WriteNode (node->second.get(), out);
        out += L")";
        return;
    }

    out += std::format (L"(tabs {}", node->active);

    for (const std::wstring & pane : node->panes)
    {
        out += L" \"";

        for (wchar_t c : pane)
        {
            out += (c == L'"' || c == L'\\') ? std::wstring (L"\\") + c : std::wstring (1, c);
        }

        out += L"\"";
    }

    out += L")";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::Tokenize
//
//  Parentheses are tokens of their own; a quoted string is one token, kept
//  with its opening quote so it cannot be mistaken for a keyword.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> DxuiPaneLayout::Tokenize (const std::wstring & text)
{
    std::vector<std::wstring>  tokens;
    size_t                     at     = 0;



    while (at < text.size())
    {
        wchar_t       c = text[at];
        std::wstring  token;

        if (iswspace (c))
        {
            at++;
            continue;
        }

        if (c == L'(' || c == L')')
        {
            tokens.push_back (std::wstring (1, c));
            at++;
            continue;
        }

        if (c == L'"')
        {
            token = L"\"";

            for (at++; at < text.size() && text[at] != L'"'; at++)
            {
                if (text[at] == L'\\' && at + 1 < text.size())
                {
                    at++;
                }

                token += text[at];
            }

            if (at >= text.size())
            {
                return {};
            }

            at++;
            tokens.push_back (token);
            continue;
        }

        while (at < text.size() && !iswspace (text[at]) && text[at] != L'(' && text[at] != L')' && text[at] != L'"')
        {
            token += text[at++];
        }

        tokens.push_back (token);
    }

    return tokens;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::ReadNode
//
//  Null on anything malformed; the caller refuses the whole text.
//
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<DxuiPaneLayoutNode> DxuiPaneLayout::ReadNode (const std::vector<std::wstring> & tokens, size_t & at)
{
    std::unique_ptr<Node>  node   = std::make_unique<Node>();
    wchar_t              * end    = nullptr;
    std::wstring           kind;
    std::wstring           axis;
    std::wstring           ratio;
    std::wstring           active;
    auto                   next   = [&] () -> std::wstring { return (at < tokens.size()) ? tokens[at++] : std::wstring(); };



    if (next() != L"(")
    {
        return nullptr;
    }

    kind = next();

    if (kind == L"split")
    {
        axis  = next();
        ratio = next();

        node->kind       = Node::Kind::Split;
        node->horizontal = (axis == L"h");
        node->ratio      = wcstof (ratio.c_str(), &end);

        if ((axis != L"h" && axis != L"v") || ratio.empty() || *end != L'\0' || node->ratio < 0.0f || node->ratio > 1.0f)
        {
            return nullptr;
        }

        node->first  = ReadNode (tokens, at);
        node->second = (node->first != nullptr) ? ReadNode (tokens, at) : nullptr;

        return (node->second != nullptr && next() == L")") ? std::move (node) : nullptr;
    }

    if (kind != L"tabs")
    {
        return nullptr;
    }

    active = next();

    node->kind   = Node::Kind::Tabs;
    node->active = (int) wcstol (active.c_str(), &end, 10);

    if (active.empty() || *end != L'\0')
    {
        return nullptr;
    }

    while (at < tokens.size() && !tokens[at].empty() && tokens[at][0] == L'"')
    {
        node->panes.push_back (tokens[at++].substr (1));
    }

    if (node->panes.empty() || next() != L")")
    {
        return nullptr;
    }

    node->active = std::clamp (node->active, 0, (int) node->panes.size() - 1);
    return node;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneLayout::TryParse
//
//  All or nothing: a text with another version, a malformed tree, an unknown
//  line, or a pane named twice leaves `out` as it was.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneLayout::TryParse (const std::wstring & text, DxuiPaneLayout & out)
{
    std::vector<std::wstring>  tokens = Tokenize (text);
    DxuiPaneLayout             layout;
    std::vector<std::wstring>  seen;
    size_t                     at     = 2;
    wchar_t                  * end    = nullptr;
    std::function<void (const Node *)>  collect = [&] (const Node * node)
    {
        if (node == nullptr)
        {
            return;
        }

        seen.insert (seen.end(), node->panes.begin(), node->panes.end());
        collect (node->first.get());
        collect (node->second.get());
    };
    auto  isString = [&] (size_t i) { return i < tokens.size() && !tokens[i].empty() && tokens[i][0] == L'"'; };



    if (tokens.size() < 4 || tokens[0] != L"dxui-layout" || tokens[1] != std::to_wstring (kVersion) || tokens[2] != L"tree")
    {
        return false;
    }

    at = 3;

    if (tokens[at] == L"none")
    {
        at++;
    }
    else
    {
        layout.m_root = ReadNode (tokens, at);

        if (layout.m_root == nullptr)
        {
            return false;
        }
    }

    while (at < tokens.size())
    {
        if (tokens[at] == L"float" && isString (at + 1) && isString (at + 2) && isString (at + 7))
        {
            Floating  floating;
            long      values[4] = {};

            for (int i = 0; i < 4; i++)
            {
                values[i] = wcstol (tokens[at + 3 + (size_t) i].c_str(), &end, 10);

                if (*end != L'\0' || tokens[at + 3 + (size_t) i].empty())
                {
                    return false;
                }
            }

            floating.pane       = tokens[at + 1].substr (1);
            floating.monitorKey = tokens[at + 2].substr (1);
            floating.rectDip    = { values[0], values[1], values[2], values[3] };
            floating.homePane   = tokens[at + 7].substr (1);
            layout.m_floating.push_back (floating);
            at += 8;
            continue;
        }

        if (tokens[at] == L"hide" && isString (at + 1) && at + 3 < tokens.size() && isString (at + 3))
        {
            AutoHidden  hidden;

            if (!TryGetSide (tokens[at + 2], hidden.edge))
            {
                return false;
            }

            hidden.pane     = tokens[at + 1].substr (1);
            hidden.homePane = tokens[at + 3].substr (1);
            layout.m_autoHidden.push_back (hidden);
            at += 4;
            continue;
        }

        return false;
    }

    collect (layout.m_root.get());

    for (const Floating & floating : layout.m_floating)
    {
        seen.push_back (floating.pane);
    }

    for (const AutoHidden & hidden : layout.m_autoHidden)
    {
        seen.push_back (hidden.pane);
    }

    std::sort (seen.begin(), seen.end());

    if (std::adjacent_find (seen.begin(), seen.end()) != seen.end())
    {
        return false;
    }

    out = std::move (layout);
    return true;
}
