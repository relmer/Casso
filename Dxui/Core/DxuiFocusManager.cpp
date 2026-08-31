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
//  Rebuilds the tab order. Controls with explicit non-negative
//  GetTabIndex() values sort first by ascending index. Remaining
//  geometry-mode controls sort by (top / rowEpsilon, left).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::Rebuild()
{
    std::vector<IDxuiControl *>  raw;
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
                int   taIdx = a->GetTabIndex();
                int   tbIdx = b->GetTabIndex();
                bool  aExpl = (taIdx >= 0);
                bool  bExpl = (tbIdx >= 0);
                RECT  ra    = {};
                RECT  rb    = {};
                int   ba    = 0;
                int   bb    = 0;

                if (aExpl && bExpl)
                {
                    return taIdx < tbIdx;
                }

                if (aExpl != bExpl)
                {
                    return aExpl;  // explicit indices come first
                }

                ra = a->GetBounds();
                rb = b->GetBounds();
                ba = (int) ((float) ra.top / eps);
                bb = (int) ((float) rb.top / eps);
                if (ba != bb)
                {
                    return ba < bb;
                }

                return ra.left < rb.left;
            });

        m_tabOrder = std::move (raw);

        // Drop focus if previously-focused control is no longer in the order.
        if (m_focused != nullptr)
        {
            bool  stillThere = false;

            for (IDxuiControl * ctl : m_tabOrder)
            {
                if (ctl == m_focused)
                {
                    stillThere = true;
                    break;
                }
            }

            if (!stillThere)
            {
                m_focused = nullptr;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocused
//
////////////////////////////////////////////////////////////////////////////////

void DxuiFocusManager::SetFocused (IDxuiControl * ctl)
{
    IDxuiControl *  prior = m_focused;



    DXUI_ASSERT_UI_THREAD();

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
