#include "Pch.h"

#include "Core/DxuiFocusManager.h"
#include "Core/DxuiPanel.h"
#include "Core/DxuiThread.h"
#include "Theme/IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFocusManager
//
////////////////////////////////////////////////////////////////////////////////

DxuiFocusManager::DxuiFocusManager()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DxuiFocusManager
//
////////////////////////////////////////////////////////////////////////////////

DxuiFocusManager::~DxuiFocusManager()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Attach
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::Attach (DxuiPanel * root)
{
    DXUI_ASSERT_UI_THREAD();

    m_root = root;
    m_scopes.clear();
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::SetTheme (const IDxuiTheme * theme)
{
    DXUI_ASSERT_UI_THREAD();

    m_theme = theme;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowEpsilonDip
//
//  Returns the explicit test-seam override if set, otherwise pulls
//  BodyLineHeightDip() from the attached theme, otherwise falls back
//  to a hard-coded constant.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiFocusManager::GetRowEpsilonDip() const
{
    constexpr float  s_kDefaultRowEpsilonDip = 16.0f;
    float            eps                     = s_kDefaultRowEpsilonDip;



    // Test seam wins over the theme, which wins over the constant.
    if (m_rowEpsilonOverridden)
    {
        eps = m_rowEpsilonOverrideDip;
    }
    else if (m_theme != nullptr)
    {
        eps = m_theme->BodyLineHeightDip();
    }

    return eps;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CollectFocusables
//
//  Depth-first walk of the visible / enabled subtree. Skips
//  kTabIndexExcluded controls and any control marked !Visible /
//  !Enabled / !Focusable.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::CollectFocusables (IDxuiControl * root, std::vector<IDxuiControl *> & out) const
{
    size_t  i = 0;



    // Hiding or disabling a container takes its whole subtree out of the tab
    // order, so this prunes rather than merely skipping the node itself.
    if (root != nullptr && root->IsVisible() && root->IsEnabled())
    {
        if (root->IsFocusable() && root->GetTabIndex() != IDxuiControl::kTabIndexExcluded)
        {
            out.push_back (root);
        }

        for (i = 0; i < root->GetChildCount(); ++i)
        {
            CollectFocusables (root->GetChild (i), out);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Rebuild
//
//  Rebuilds the tab order, sorted as IsBeforeInTabOrder gives.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::Rebuild()
{
    std::vector<IDxuiControl *>  raw;
    std::vector<IDxuiControl *>  prior     = m_tabOrder;
    IDxuiControl *               scopeRoot = nullptr;
    float                        eps       = 1.0f;



    DXUI_ASSERT_UI_THREAD();

    m_tabOrder.clear();

    // No root means no tree to walk, and the cleared order above is already
    // the right answer.
    if (m_root != nullptr)
    {
        scopeRoot = m_scopes.empty() ? static_cast<IDxuiControl *> (m_root) : m_scopes.back().root;
        if (scopeRoot == nullptr)
        {
            scopeRoot = m_root;
        }

        CollectFocusables (scopeRoot, raw);

        eps = GetRowEpsilonDip();
        if (eps <= 0.0f)
        {
            eps = 1.0f;
        }

        std::sort (raw.begin(), raw.end(),
            [eps] (IDxuiControl * a, IDxuiControl * b) -> bool
            {
                return IsBeforeInTabOrder (a, b, eps);
            });

        m_tabOrder = std::move (raw);

        RecoverFocus (prior);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecoverFocus
//
//  A focused control that has left the order, hidden or removed, passes
//  focus to the next control in the order it left that is still there, or,
//  when none follows it, to the nearest one before it: focus goes on from
//  where it was rather than around to the top. The control that left is not
//  told, since it may no longer exist. With nothing left, focus is dropped.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::RecoverFocus (const std::vector<IDxuiControl *> & priorOrder)
{
    auto            it        = std::find (priorOrder.begin(), priorOrder.end(), m_focused);
    size_t          at        = (size_t) (it - priorOrder.begin());
    size_t          i         = 0;
    IDxuiControl  * successor = nullptr;



    if (m_focused == nullptr || IsInTabOrder (m_focused))
    {
        return;
    }

    for (i = at + 1; i < priorOrder.size() && successor == nullptr; i++)
    {
        successor = IsInTabOrder (priorOrder[i]) ? priorOrder[i] : nullptr;
    }

    for (i = (at < priorOrder.size()) ? at : 0; i > 0 && successor == nullptr; i--)
    {
        successor = IsInTabOrder (priorOrder[i - 1]) ? priorOrder[i - 1] : nullptr;
    }

    m_focused = nullptr;

    if (successor != nullptr)
    {
        ChangeFocus (successor, m_isCueShown);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInTabOrder
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::IsInTabOrder (const IDxuiControl * ctl) const
{
    return std::find (m_tabOrder.begin(), m_tabOrder.end(), ctl) != m_tabOrder.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocused
//
//  Focus moved by the keyboard or by the program, so the focus rectangle
//  shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::SetFocused (IDxuiControl * ctl)
{
    ChangeFocus (ctl, true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsClippedByAncestor
//
//  A control scrolled out of its container's viewport keeps its bounds, so
//  the bounds alone would let a press on the tab strip or the button row
//  focus a page control laid out underneath it.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::IsClippedByAncestor (const IDxuiControl * ctl, POINT pointPx)
{
    const IDxuiControl *  node    = (ctl != nullptr) ? ctl->GetParent() : nullptr;
    bool                  clipped = false;



    for ( ; node != nullptr && !clipped; node = node->GetParent())
    {
        clipped = node->IsPointClipped (pointPx);
    }

    return clipped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTabPlaces
//
//  Where a control sorts in the tab order, level by level: the place of each
//  tab group it sits in, outermost first, then its own bounds. A row
//  scrolled out of a list's viewport keeps its place with the list, and a
//  control scrolled out of a page's keeps its place with the page, rather
//  than with whatever it lies under.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::GetTabPlaces (const IDxuiControl * ctl, std::vector<RECT> & places)
{
    const IDxuiControl  * node = ctl->GetParent();



    places.clear();
    places.push_back (ctl->GetBounds());

    for ( ; node != nullptr; node = node->GetParent())
    {
        if (node->IsTabGroup())
        {
            places.push_back (node->GetTabGroupPlace());
        }
    }

    std::reverse (places.begin(), places.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsBeforeInTabOrder
//
//  Controls with explicit non-negative GetTabIndex() values come first, by
//  ascending index. The rest compare their places level by level, as
//  GetTabPlaces gives them, by (top / rowEpsilon, left) at the first level
//  where they differ; controls at the same places sort by their own top,
//  then left.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::IsBeforeInTabOrder (const IDxuiControl * a, const IDxuiControl * b, float eps)
{
    int                taIdx   = a->GetTabIndex();
    int                tbIdx   = b->GetTabIndex();
    bool               aExpl   = (taIdx >= 0);
    bool               bExpl   = (tbIdx >= 0);
    RECT               ra      = a->GetBounds();
    RECT               rb      = b->GetBounds();
    std::vector<RECT>  placesA;
    std::vector<RECT>  placesB;
    size_t             levels  = 0;
    size_t             i       = 0;



    if (aExpl && bExpl)
    {
        return taIdx < tbIdx;
    }

    if (aExpl != bExpl)
    {
        return aExpl;
    }

    GetTabPlaces (a, placesA);
    GetTabPlaces (b, placesB);
    levels = std::min (placesA.size(), placesB.size());

    for (i = 0; i < levels; i++)
    {
        int  bandA = (int) std::floor ((float) placesA[i].top / eps);
        int  bandB = (int) std::floor ((float) placesB[i].top / eps);

        if (bandA != bandB)
        {
            return bandA < bandB;
        }

        if (placesA[i].left != placesB[i].left)
        {
            return placesA[i].left < placesB[i].left;
        }
    }

    if (ra.top != rb.top)
    {
        return ra.top < rb.top;
    }

    return ra.left < rb.left;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RevealInAncestors
//
//  A control focused from the keyboard is scrolled into view by every
//  scrolling container it sits in, innermost first, so each outer one
//  reveals where the inner ones left it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::RevealInAncestors (const IDxuiControl * ctl)
{
    IDxuiControl  * node = (ctl != nullptr) ? ctl->GetParent() : nullptr;



    for ( ; node != nullptr; node = node->GetParent())
    {
        node->RevealDescendant (*ctl);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusAtPoint
//
//  A press gives focus to the focusable control under it, without its focus
//  rectangle, so the next Tab moves on from the control the user was just in.
//  The last control in the order that contains the point wins, since a later
//  control paints over an earlier one. A press on nothing focusable leaves
//  focus where it was.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::FocusAtPoint (POINT pointDip)
{
    auto  it = m_tabOrder.rbegin();



    DXUI_ASSERT_UI_THREAD();

    for ( ; it != m_tabOrder.rend(); ++it)
    {
        RECT  bounds = (*it)->GetBounds();

        if (pointDip.x >= bounds.left && pointDip.x < bounds.right &&
            pointDip.y >= bounds.top  && pointDip.y < bounds.bottom &&
            !IsClippedByAncestor (*it, pointDip))
        {
            ChangeFocus (*it, false);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeFocus
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::ChangeFocus (IDxuiControl * ctl, bool showCue)
{
    IDxuiControl *  prior = m_focused;



    DXUI_ASSERT_UI_THREAD();

    m_isCueShown = showCue;

    if (ctl != nullptr)
    {
        ctl->SetFocusCueVisible (showCue);
    }

    // Focus moved by the keyboard can land on a control scrolled out of its
    // container's view. A click lands on one in view, so it scrolls nothing.
    if (showCue)
    {
        RevealInAncestors (ctl);
    }

    // Re-focusing the already-focused control must not fire the notifications
    // again -- a control that rebuilds state on focus-in would do it twice.
    if (prior != ctl)
    {
        m_focused = ctl;

        if (prior != nullptr)
        {
            prior->OnFocusChanged (false);
        }

        if (ctl != nullptr)
        {
            ctl->OnFocusChanged (true);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveFocus
//
//  Advances focus by +1 (Tab) or -1 (Shift+Tab) through the tab
//  order. Wraps at both ends.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::MoveFocus (int direction)
{
    size_t  count = m_tabOrder.size();
    size_t  idx   = 0;
    size_t  cur   = count;      // count doubles as the "not found" sentinel
    size_t  next  = 0;
    bool    moved = (count != 0);



    if (moved)
    {
        for (idx = 0; idx < count && cur == count; ++idx)
        {
            if (m_tabOrder[idx] == m_focused)
            {
                cur = idx;
            }
        }

        if (cur == count)
        {
            // No current focus -- pick the first (forward) or last (backward).
            next = (direction > 0) ? 0 : (count - 1);
        }
        else if (direction > 0)
        {
            next = (cur + 1) % count;
        }
        else
        {
            next = (cur == 0) ? (count - 1) : (cur - 1);
        }

        SetFocused (m_tabOrder[next]);

        if (m_tabOrder[next] != nullptr)
        {
            m_tabOrder[next]->OnFocusEntered (direction > 0);
        }
    }

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MoveFocusSpatial
//
//  Spatial arrow navigation: picks the nearest focusable in the
//  arrow's direction using bounding-box centroids. Distance is the
//  squared Euclidean distance between centroids; candidates that are
//  not in the correct half-plane are filtered out.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::MoveFocusSpatial (DxuiFocusKey arrow)
{
    IDxuiControl *  best     = nullptr;
    long            bestDist = 0;
    RECT            curR     = {};
    long            curCx    = 0;
    long            curCy    = 0;
    bool            moved    = false;



    // Nothing focused yet: there is no "from" point to measure against, so
    // an arrow behaves like a first Tab.
    if (m_focused == nullptr)
    {
        moved = MoveFocus (+1);
    }
    else
    {
        curR  = m_focused->GetBounds();
        curCx = (curR.left + curR.right)  / 2;
        curCy = (curR.top  + curR.bottom) / 2;

        for (IDxuiControl * candidate : m_tabOrder)
        {
            RECT  rr   = {};
            long  cx   = 0;
            long  cy   = 0;
            long  dx   = 0;
            long  dy   = 0;
            long  dist = 0;
            bool  keep = false;

            if (candidate == m_focused)
            {
                continue;
            }

            rr = candidate->GetBounds();
            cx = (rr.left + rr.right)  / 2;
            cy = (rr.top  + rr.bottom) / 2;
            dx = cx - curCx;
            dy = cy - curCy;

            switch (arrow)
            {
            case DxuiFocusKey::ArrowLeft:   keep = (dx < 0); break;
            case DxuiFocusKey::ArrowRight:  keep = (dx > 0); break;
            case DxuiFocusKey::ArrowUp:     keep = (dy < 0); break;
            case DxuiFocusKey::ArrowDown:   keep = (dy > 0); break;
            default:                        keep = false;    break;
            }

            if (!keep)
            {
                continue;
            }

            dist = dx * dx + dy * dy;
            if (best == nullptr || dist < bestDist)
            {
                best     = candidate;
                bestDist = dist;
            }
        }

        // Nothing in that half-plane: the arrow is a no-op rather than a wrap,
        // so focus stays where the user left it.
        if (best != nullptr)
        {
            SetFocused (best);
            moved = true;
        }
    }

    return moved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
//  Handles the navigation keys the focus manager owns: Tab, Shift+Tab, and the
//  arrows.
//
//  Tab moves in TREE ORDER while the arrows move SPATIALLY, matching what
//  users expect of each: Tab follows the declared sequence, an arrow goes
//  toward the thing that looks that way on screen. They are separate walks
//  because a tree that reads sensibly can still be laid out in a grid.
//
//  Escape is claimed ONLY while a scope is pushed. At the outermost level it
//  belongs to the dialog as cancel or close, and swallowing it there would
//  leave a dialog that cannot be dismissed from the keyboard.
//
//  Enter and Space are listed and deliberately do nothing. Activation belongs
//  to the FOCUSED CONTROL through its own OnKey -- what they mean depends
//  entirely on what has focus -- and naming them here documents that rather
//  than leaving a reader to wonder where they went.
//
//  The return value reports whether focus actually moved, so a caller can fall
//  through to its own handling when the walk declined.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiFocusManager::HandleKey (DxuiFocusKey key)
{
    bool  handled = false;



    DXUI_ASSERT_UI_THREAD();

    switch (key)
    {
    case DxuiFocusKey::Tab:
        handled = MoveFocus (+1);
        break;

    case DxuiFocusKey::ShiftTab:
        handled = MoveFocus (-1);
        break;

    case DxuiFocusKey::ArrowUp:
    case DxuiFocusKey::ArrowDown:
    case DxuiFocusKey::ArrowLeft:
    case DxuiFocusKey::ArrowRight:
        handled = MoveFocusSpatial (key);
        break;

    case DxuiFocusKey::Escape:
        // Only claimed while a scope is pushed; at the outermost level
        // Escape belongs to the dialog (cancel / close).
        if (!m_scopes.empty())
        {
            PopScope();
            handled = true;
        }

        break;

    case DxuiFocusKey::Enter:
    case DxuiFocusKey::Space:
        // Activation is routed via the focused control's OnKey, not here.
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushScope
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::PushScope (IDxuiControl * scopeRoot)
{
    Scope  scope;



    DXUI_ASSERT_UI_THREAD();

    scope.root       = scopeRoot;
    scope.priorFocus = m_focused;
    m_scopes.push_back (scope);
    m_focused = nullptr;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopScope
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::PopScope()
{
    Scope  scope;



    DXUI_ASSERT_UI_THREAD();

    if (m_scopes.empty())
    {
        return;
    }

    scope = m_scopes.back();
    m_scopes.pop_back();
    Rebuild();
    SetFocused (scope.priorFocus);
}
